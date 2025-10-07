// SPDX-License-Identifier: GPL-2.0
/*
 * Maxim MAX9296A Quad GMSL2 Deserializer Driver
 *
 */

#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/regmap.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

#include "max_des.h"
#include "max_ser.h"

#define MAX_DES_PCLK					(25000000ull)

#define MAX_DES_LINK_FREQUENCY_MIN		(100000000ull)
#define MAX_DES_LINK_FREQUENCY_DEFAULT	(750000000ull)
#define MAX_DES_LINK_FREQUENCY_MAX		(1250000000ull)

#define MAX9296_N_GMSL			(2)

#define MAX9296A_PIPES_NUM		(4)
#define MAX9296A_NAME			"max9296a"
#define MAX9296A_GPIO_NUM		(13)

struct max9296a_priv {
	struct max_des_priv des_priv;
	const struct max9296a_chip_info *info;

	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;

	struct pinctrl_dev *pctldev;
	struct pinctrl_desc pctldesc;
	struct gpio_chip gc;

	bool i2c_addr_change;
	unsigned int i2c_addr;

	struct gpio_desc *reset_gpio;
};

struct max9296a_chip_info {
	unsigned int num_pipes;
	unsigned int pipe_hw_ids[MAX9296A_PIPES_NUM];
	unsigned int num_phys;
	unsigned int num_links;
	bool phy0_first_lanes_on_master_phy;
	bool polarity_on_physical_lanes;
	bool supports_tunnel_mode;
	bool fix_tx_ids;
};

#define des_to_priv(des) \
	container_of(des, struct max9296a_priv, des_priv)

static struct max_des_subdev_priv *next_subdev(struct max_des_priv *priv,
						struct max_des_subdev_priv *sd_priv)
{
	if (!sd_priv)
		sd_priv = &priv->sd_privs[0];
	else
		sd_priv++;

	for (; sd_priv < priv->sd_privs + priv->num_subdevs; sd_priv++) {
		if (sd_priv->node)
			return sd_priv;
	}

	return NULL;
}

#define for_each_subdev(priv, sd_priv) \
	for ((sd_priv) = NULL; ((sd_priv) = next_subdev((priv), (sd_priv))); )

static inline struct max_des_subdev_priv *sd_to_max_des(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max_des_subdev_priv, sd);
}

static int __max_des_mipi_update(struct max_des_priv *priv)
{
	struct max_des_subdev_priv *sd_priv;
	bool enable = 0;

	dev_dbg(priv->dev, "%s()\n", __func__);

	for_each_subdev(priv, sd_priv) {
		if (sd_priv->active) {
			enable = 1;
			break;
		}
	}

	if (enable == priv->active)
		return 0;

	priv->active = enable;

	return priv->ops->mipi_enable(priv, enable);
}

static int max_des_ch_enable(struct max_des_subdev_priv *sd_priv, bool enable)
{
	struct max_des_priv *priv = sd_priv->priv;
	int ret = 0;

	dev_dbg(priv->dev, "%s()\n", __func__);

	mutex_lock(&priv->lock);

	if (sd_priv->active == enable)
		goto exit;

	sd_priv->active = enable;

	ret = __max_des_mipi_update(priv);

exit:
	mutex_unlock(&priv->lock);

	return ret;
}

static int max_des_post_init(struct max_des_priv *priv)
{
	unsigned int i, mask = 0;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	for (i = 0; i < priv->ops->num_links; i++) {
		struct max_des_link *link = &priv->links[i];

		if (!link->enabled)
			continue;

		mask |= BIT(link->index);
	}

	ret = priv->ops->select_links(priv, mask);
	if (ret)
		return ret;

	if (priv->ops->post_init) {
		ret = priv->ops->post_init(priv);
		if (ret)
			return ret;
	}

	return 0;
}

static int max_des_i2c_mux_select(struct i2c_mux_core *muxc, u32 chan)
{
	return 0;
}

static int max_des_i2c_mux_init(struct max_des_priv *priv)
{
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (!i2c_check_functionality(priv->client->adapter, I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
		return -ENODEV;

	priv->mux_chan = -1;

	priv->mux = i2c_mux_alloc(priv->client->adapter, &priv->client->dev,
				  priv->gmsl_links_used, 0, I2C_MUX_LOCKED,
				  max_des_i2c_mux_select, NULL);
	if (!priv->mux)
		return -ENOMEM;

	priv->mux->priv = priv;

	for (i = 0; i < priv->ops->num_links; i++) {
		struct max_des_link *link = &priv->links[i];

		if (!link->enabled)
			continue;

		priv->ops->select_links(priv, BIT(link->index));

		ret = i2c_mux_add_adapter(priv->mux, 0, link->index, 0);
		dev_info(priv->dev, "%s() link [%d]\n", __func__, link->index);
		if (ret)
			goto err_add_adapters;
	}

	return 0;

err_add_adapters:
	i2c_mux_del_adapters(priv->mux);
	return ret;
}

static int max_des_update_pipe_remaps(struct max_des_priv *priv,
					struct max_des_pipe *pipe)
{
	struct max_des_link *link = &priv->links[pipe->link_id];
	struct max_des_subdev_priv *sd_priv;
	unsigned int i;

	dev_dbg(priv->dev, "%s()\n", __func__);

	pipe->num_remaps = 0;

	if (link->tunnel_mode)
		return 0;

	for_each_subdev(priv, sd_priv) {
		unsigned int num_remaps;

		if (sd_priv->pipe_id != pipe->index)
			continue;

		if (!sd_priv->fmt)
			continue;

		if (sd_priv->fmt->dt == MAX_DT_EMB8)
			num_remaps = 1;
		else
			num_remaps = 3;

		for (i = 0; i < num_remaps; i++) {
			struct max_des_dt_vc_remap *remap;
			unsigned int dt;

			if (pipe->num_remaps == MAX_DES_REMAPS_NUM) {
				dev_err(priv->dev, "Too many remaps\n");
				return -EINVAL;
			}

			remap = &pipe->remaps[pipe->num_remaps++];

			if (i == 0)
				dt = sd_priv->fmt->dt;
			else if (i == 1)
				dt = MAX_DT_FS;
			else
				dt = MAX_DT_FE;

			remap->from_dt = dt;
			remap->from_vc = sd_priv->src_vc_id;
			remap->to_dt = dt;
			remap->to_vc = sd_priv->dst_vc_id;
			remap->phy = sd_priv->phy_id;
		}
	}

	return priv->ops->update_pipe_remaps(priv, pipe);
}

static int max_des_init(struct max_des_priv *priv)
{
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = __max_des_mipi_update(priv);
	if (ret)
		return ret;

	ret = priv->ops->init(priv);
	if (ret)
		return ret;

	for (i = 0; i < priv->ops->num_phys; i++) {
		struct max_des_phy *phy = &priv->phys[i];

		if (!phy->enabled)
			continue;

		if (!phy->bus_config_parsed) {
			dev_err(priv->dev, "Cannot turn on unconfigured PHY\n");
			return -EINVAL;
		}

		ret = priv->ops->init_phy(priv, phy);
		if (ret)
			return ret;
	}

	for (i = 0; i < priv->ops->num_pipes; i++) {
		struct max_des_pipe *pipe = &priv->pipes[i];

		if (!pipe->enabled)
			continue;

		ret = priv->ops->init_pipe(priv, pipe);
		if (ret)
			return ret;

		ret = max_des_update_pipe_remaps(priv, pipe);
		if (ret)
			return ret;
	}

	if (priv->ops->init_link) {
		for (i = 0; i < priv->ops->num_links; i++) {
			struct max_des_link *link = &priv->links[i];

			if (!link->enabled)
				continue;

			ret = priv->ops->init_link(priv, link);
			if (ret)
				return ret;
		}
	}

	if (priv->ops->init_fsync) {
		ret = priv->ops->init_fsync(priv, priv->fsync);
		if (ret)
			return ret;
	}

	return 0;
}

static int max_ser_change_address(struct i2c_client *client, struct regmap *regmap, u8 addr, u8 id)
{
	int ret;

	ret = regmap_write(regmap, 0x0, addr << 1);
	if (ret)
		return ret;

	client->addr = addr;

	regmap_update_bits(regmap, 0x7b, GENMASK(2, 0), id);
	regmap_update_bits(regmap, 0x83, GENMASK(2, 0), id);
	regmap_update_bits(regmap, 0x8b, GENMASK(2, 0), id);
	regmap_update_bits(regmap, 0x93, GENMASK(2, 0), id);
	regmap_update_bits(regmap, 0xa3, GENMASK(2, 0), id);
	regmap_update_bits(regmap, 0xab, GENMASK(2, 0), id);

	return 0;
}

static int max_ser_reset(struct regmap *regmap)
{
	int ret;

	ret = regmap_update_bits(regmap, 0x10, 0x80, 0x80);
	if (ret)
		return ret;

	msleep(50);

	return 0;
}

static int max_ser_wait_for_multiple(struct i2c_client *client, struct regmap *regmap,
				u8 *addrs, unsigned int num_addrs)
{
	unsigned int i, j, val;
	int ret;

	dev_dbg(&client->dev, "%s()\n", __func__);

	for (i = 0; i < 10; i++) {
		for (j = 0; j < num_addrs; j++) {
			client->addr = addrs[j];

			ret = regmap_read(regmap, 0x0, &val);
			if (ret >= 0)
				return 0;
		}

		msleep(100);

		dev_dbg(&client->dev, "Retry %u waiting for serializer: %d\n", i, ret);
	}

	return ret;
}

static int max_ser_wait(struct i2c_client *client, struct regmap *regmap, u8 addr)
{
	return max_ser_wait_for_multiple(client, regmap, &addr, 1);
}

static int max_des_parse_fsync(struct max_des_priv *priv)
{
	struct device *dev = &priv->client->dev;
	char const *fsync_mode;
	u32 fsync_freq = 0;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (of_property_read_string(dev->of_node, "fsync-mode", &fsync_mode)) {
		return 0;
	}

	dev_dbg(priv->dev, "mode: %s\n", fsync_mode);

	if (!strcmp("internal", fsync_mode) ||
		!strcmp("internal-output", fsync_mode)) {
		ret = of_property_read_u32(dev->of_node, "fsync-freq", &fsync_freq);
		if (ret) {
			dev_err(priv->dev, "fsync-freq not found: %d\n", ret);
			return ret;
		}
		priv->fsync->freq = MAX_DES_PCLK / fsync_freq;

		if (!strcmp("internal-output", fsync_mode))
			priv->fsync->internal_output = true;
		else
			priv->fsync->internal = true;
	}
	else if (!strcmp("external", fsync_mode)) {
		priv->fsync->external = true;
	}
	else {
		dev_warn(priv->dev, "unknow fsync-mode");
	}

	return 0;
}

static int max_des_init_link_ser_xlate(struct max_des_priv *priv,
					struct max_des_link *link,
					u8 power_up_addr, u8 new_addr, u8 source_id)
{
	u8 addrs[] = { power_up_addr, new_addr };
	struct i2c_client *client;
	struct regmap *regmap;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	client = i2c_new_dummy_device(priv->client->adapter, power_up_addr);
	if (IS_ERR(client)) {
		ret = PTR_ERR(client);
		dev_err(priv->dev,
			"Failed to create I2C client: %d\n", ret);
		return ret;
	}

	regmap = regmap_init_i2c(client, &max_ser_i2c_regmap);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(priv->dev,
			"Failed to create I2C regmap: %d\n", ret);
		goto err_unregister_client;
	}

	ret = priv->ops->select_links(priv, BIT(link->index));
	if (ret)
		goto err_regmap_exit;

	ret = max_ser_wait_for_multiple(client, regmap, addrs, ARRAY_SIZE(addrs));
	if (ret) {
		link->enabled = false;
		dev_err(priv->dev,
			"Failed waiting for serializer with new or old address: %d\n", ret);
		goto err_regmap_exit;
	}

	ret = max_ser_reset(regmap);
	if (ret) {
		link->enabled = false;
		dev_err(priv->dev, "Failed to reset serializer: %d\n", ret);
		goto err_regmap_exit;
	}

	ret = max_ser_wait(client, regmap, power_up_addr);
	if (ret) {
		link->enabled = false;
		dev_err(priv->dev,
			"Failed waiting for serializer with new address: %d\n", ret);
		goto err_regmap_exit;
	}

	if (power_up_addr != new_addr) {
		ret = max_ser_change_address(client, regmap, new_addr, source_id);
		if (ret) {
			link->enabled = false;
			dev_err(priv->dev, "Failed to change serializer address: %d\n", ret);
			goto err_regmap_exit;
		}
	}

err_regmap_exit:
	regmap_exit(regmap);

err_unregister_client:
	i2c_unregister_device(client);

	return ret;
}

static int max_des_parse_link_ser_xlate(struct max_des_priv *priv)
{
	struct property *local;
	struct property *remote;
	struct property *source;
	struct max_i2c_xlate *xlate;
	unsigned int i;
	int ret;
	u32 local_addr;
	u32 remote_addr;
	u32 source_id;

	dev_dbg(priv->dev, "%s()\n", __func__);

	local = of_find_property(priv->dev->of_node,
				 "i2c-addr-alias-map-local",
				 &ret);
	remote = of_find_property(priv->dev->of_node,
				  "i2c-addr-alias-map-remote",
				  &ret);
	source = of_find_property(priv->dev->of_node,
				  "i2c-addr-alias-source-id",
				  &ret);

	if (local == NULL || remote == NULL || source == NULL) {
		dev_dbg(priv->dev, "find not property of alias map\n");
		return 0;
	}

	ret = 0;
	for (i = 0; i < priv->ops->num_links; i++) {
		struct max_des_link *link = &priv->links[i];

		if (!link->enabled)
			continue;

		ret = of_property_read_u32_index(priv->dev->of_node,
						 "i2c-addr-alias-map-local",
						 i, &local_addr);
		if (ret != 0 || local_addr > 0x7f)
			break;

		ret = of_property_read_u32_index(priv->dev->of_node,
						 "i2c-addr-alias-map-remote",
						 i, &remote_addr);
		if (ret != 0 || remote_addr > 0x7f)
			break;

		ret = of_property_read_u32_index(priv->dev->of_node,
						 "i2c-addr-alias-source-id",
						 i, &source_id);
		if (ret != 0 || source_id > 0x7)
			break;

		xlate = &link->ser_xlate;
		xlate->src = (u8)(local_addr & 0x7f);
		xlate->dst = (u8)(remote_addr & 0x7f);
		xlate->id = (u8)(source_id & 0x7);

		dev_info(priv->dev, "i2c address alias "
			"index: %d local: 0x%x remote: 0x%x id: 0x%x\n",
			i, xlate->dst, xlate->src, xlate->id);

		ret = max_des_init_link_ser_xlate(priv, link, xlate->dst, xlate->src, xlate->id);
		if (ret != 0) {
			link->enabled = false;
			continue;
		}
	}

	return 0;
}

static int max_des_parse_i2c_dt(struct max_des_priv *priv)
{
	struct device *dev = &priv->client->dev;
	struct device_node *i2c_mux;
	struct device_node *node = NULL;

	dev_dbg(priv->dev, "%s()\n", __func__);

	i2c_mux = of_find_node_by_name(dev->of_node, "i2c-mux");
	if (!i2c_mux) {
		dev_err(priv->dev, "Failed to find i2c-mux node\n");
		return -EINVAL;
	}

	/* Identify which i2c-mux channels are enabled */
	for_each_child_of_node(i2c_mux, node) {
		u32 id = 0;

		of_property_read_u32(node, "reg", &id);
		if (id >= MAX9296_N_GMSL)
			continue;

		if (!of_device_is_available(node)) {
			dev_dbg(priv->dev, "Skipping disabled I2C bus port %u\n", id);
			continue;
		}

		priv->gmsl_link_mask |= BIT(id);
		priv->gmsl_links_used++;
	}
	of_node_put(node);
	of_node_put(i2c_mux);
	of_node_put(dev->of_node);

	dev_info(priv->dev, "find [%d] node, mask [0x%x]\n", priv->gmsl_links_used, priv->gmsl_link_mask);

	return 0;
}

static int max_des_parse_ch_dt(struct max_des_subdev_priv *sd_priv,
				struct device_node *node)
{
	struct max_des_priv *priv = sd_priv->priv;
	struct max_des_pipe *pipe;
	struct max_des_link *link;
	struct max_des_phy *phy;
	u32 val;
	u64 link_frequency;
	struct v4l2_fwnode_endpoint v4l2_ep = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	struct v4l2_fwnode_bus_mipi_csi2 *mipi = &v4l2_ep.bus.mipi_csi2;
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	of_property_read_string(node, "label", &sd_priv->label);

	/* TODO: implement extended Virtual Channel. */
	val = sd_priv->src_vc_id;
	of_property_read_u32(node, "maxim,src-vc-id", &val);
	if (val >= MAX_SERDES_VC_ID_NUM) {
		dev_err(priv->dev, "Invalid source virtual channel %u\n", val);
		return -EINVAL;
	}
	sd_priv->src_vc_id = val;

	/* TODO: implement extended Virtual Channel. */
	val = sd_priv->dst_vc_id;
	of_property_read_u32(node, "maxim,dst-vc-id", &val);
	if (val >= MAX_SERDES_VC_ID_NUM) {
		dev_err(priv->dev, "Invalid destination virtual channel %u\n", val);
		return -EINVAL;
	}
	sd_priv->dst_vc_id = val;

	val = sd_priv->pipe_id;
	of_property_read_u32(node, "maxim,pipe-id", &val);
	if (val >= priv->ops->num_pipes) {
		dev_err(priv->dev, "Invalid pipe %u\n", val);
		return -EINVAL;
	}
	sd_priv->pipe_id = val;

	pipe = &priv->pipes[val];
	pipe->enabled = true;

	val = pipe->phy_id;
	of_property_read_u32(node, "maxim,phy-id", &val);
	if (val >= priv->ops->num_phys) {
		dev_err(priv->dev, "Invalid PHY %u\n", val);
		return -EINVAL;
	}
	sd_priv->phy_id = val;

	if (of_property_read_bool(node, "maxim,embedded-data"))
		sd_priv->fmt = max_format_by_dt(MAX_DT_EMB8);

	phy = &priv->phys[val];
	phy->enabled = true;

	link = &priv->links[pipe->link_id];
	link->enabled = true;

	val = of_property_read_bool(node, "maxim,tunnel-mode");
	if (val && !priv->ops->supports_tunnel_mode) {
		dev_err(priv->dev, "Tunnel mode is not supported\n");
		return -EINVAL;
	}
	link->tunnel_mode = val;

	if (v4l2_fwnode_endpoint_alloc_parse(of_fwnode_handle(node), &v4l2_ep)) {
		dev_err(priv->dev, "Could not parse v4l2 endpoint\n");
		return ret;
	}

	if (v4l2_ep.nr_of_link_frequencies == 0)
		link_frequency = MAX_DES_LINK_FREQUENCY_DEFAULT;
	else if (v4l2_ep.nr_of_link_frequencies == 1)
		link_frequency = v4l2_ep.link_frequencies[0];
	else {
		dev_err(priv->dev, "PHY configured with invalid number of link frequencies\n");
		return -EINVAL;
	}

	v4l2_fwnode_endpoint_free(&v4l2_ep);

	if (link_frequency < MAX_DES_LINK_FREQUENCY_MIN ||
		link_frequency > MAX_DES_LINK_FREQUENCY_MAX) {
		dev_err(priv->dev, "PHY configured with out of range link frequency\n");
		return -EINVAL;
	}

	for (i = 0; i < mipi->num_data_lanes; i++) {
		if (mipi->data_lanes[i] > mipi->num_data_lanes) {
			dev_err(priv->dev, "PHY configured with data lanes out of range\n");
			return -EINVAL;
		}
	}

	dev_info(priv->dev, "num_data_lanes [%d], clock lane [%d], link_freq [%lld]\n",
		mipi->num_data_lanes, mipi->clock_lane, link_frequency);

	if (!phy->bus_config_parsed) {
		phy->mipi = v4l2_ep.bus.mipi_csi2;
		phy->link_frequency = link_frequency;
		phy->bus_config_parsed = true;

		return 0;
	}

	if (phy->link_frequency != link_frequency) {
		dev_err(priv->dev, "PHY configured with differing link frequency\n");
		return -EINVAL;
	}

	if (phy->mipi.num_data_lanes != mipi->num_data_lanes) {
		dev_err(priv->dev, "PHY configured with differing number of data lanes\n");
		return -EINVAL;
	}

	for (i = 0; i < mipi->num_data_lanes; i++) {
		if (phy->mipi.data_lanes[i] != mipi->data_lanes[i]) {
			dev_err(priv->dev, "PHY configured with differing data lanes\n");
			return -EINVAL;
		}
	}

	if (phy->mipi.clock_lane != mipi->clock_lane) {
		dev_err(priv->dev, "PHY configured with differing clock lane\n");
		return -EINVAL;
	}

	return 0;
}

static int max_des_parse_pipe_link_remap_dt(struct max_des_priv *priv,
						struct max_des_pipe *pipe,
						struct device_node *node)
{
	u32 val;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	val = pipe->link_id;
	ret = of_property_read_u32(node, "maxim,link-id", &val);
	if (!ret && !priv->ops->supports_pipe_link_remap) {
		dev_err(priv->dev, "Pipe link remapping is not supported\n");
		return -EINVAL;
	}

	if (val >= priv->ops->num_links) {
		dev_err(priv->dev, "Invalid link %u\n", val);
		return -EINVAL;
	}

	pipe->link_id = val;

	return 0;
}

static int max_des_parse_pipe_dt(struct max_des_priv *priv,
				struct max_des_pipe *pipe,
				struct device_node *node)
{
	u32 val;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	val = pipe->phy_id;
	of_property_read_u32(node, "maxim,phy-id", &val);
	if (val >= priv->ops->num_phys) {
		dev_err(priv->dev, "Invalid PHY %u\n", val);
		return -EINVAL;
	}
	pipe->phy_id = val;

	val = pipe->stream_id;
	ret = of_property_read_u32(node, "maxim,stream-id", &val);
	if (!ret && priv->pipe_stream_autoselect) {
		dev_err(priv->dev, "Cannot select stream when using autoselect\n");
		return -EINVAL;
	}

	if (val >= MAX_SERDES_STREAMS_NUM) {
		dev_err(priv->dev, "Invalid stream %u\n", val);
		return -EINVAL;
	}

	pipe->stream_id = val;

	pipe->dbl8 = of_property_read_bool(node, "maxim,dbl8");
	pipe->dbl10 = of_property_read_bool(node, "maxim,dbl10");
	pipe->dbl12 = of_property_read_bool(node, "maxim,dbl12");

	pipe->dbl8mode = of_property_read_bool(node, "maxim,dbl8-mode");
	pipe->dbl10mode = of_property_read_bool(node, "maxim,dbl10-mode");

	ret = max_des_parse_pipe_link_remap_dt(priv, pipe, node);

	if (ret)
		return ret;

	return 0;
}

static int max_des_parse_phy_dt(struct max_des_priv *priv,
				struct max_des_phy *phy,
				struct device_node *node)
{
	phy->alt_mem_map8 = of_property_read_bool(node, "maxim,alt-mem-map8");
	phy->alt2_mem_map8 = of_property_read_bool(node, "maxim,alt2-mem-map8");
	phy->alt_mem_map10 = of_property_read_bool(node, "maxim,alt-mem-map10");
	phy->alt_mem_map12 = of_property_read_bool(node, "maxim,alt-mem-map12");

	return 0;
}

static int max_des_parse_dt(struct max_des_priv *priv)
{
	const char *channel_node_name = "channel";
	const char *pipe_node_name = "pipe";
	const char *phy_node_name = "phy";
	struct device_node *node;
	struct max_des_subdev_priv *sd_priv;
	struct max_des_link *link;
	struct max_des_pipe *pipe;
	struct max_des_phy *phy;
	unsigned int i;
	u32 index;
	u32 val;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	val = of_property_read_bool(priv->dev->of_node, "maxim,pipe-stream-autoselect");
	if (val && !priv->ops->supports_pipe_stream_autoselect) {
		dev_err(priv->dev, "Pipe stream autoselect is not supported\n");
		return -EINVAL;
	}
	priv->pipe_stream_autoselect = val;

	for (i = 0; i < priv->ops->num_phys; i++) {
		phy = &priv->phys[i];
		phy->index = i;
	}

	for (i = 0; i < priv->ops->num_pipes; i++) {
		pipe = &priv->pipes[i];
		pipe->index = i;
		pipe->phy_id = i % priv->ops->num_phys;
		pipe->stream_id = i % MAX_SERDES_STREAMS_NUM;
		pipe->link_id = i;
	}

	for (i = 0; i < priv->ops->num_links; i++) {
		link = &priv->links[i];
		link->index = i;
	}

	for_each_child_of_node(priv->dev->of_node, node) {
		if (of_node_name_eq(node, phy_node_name)) {
			ret = of_property_read_u32(node, "reg", &index);
			if (ret) {
				dev_err(priv->dev, "Failed to read phy reg: %d\n", ret);
				continue;
			}

			if (index >= priv->ops->num_phys) {
				dev_err(priv->dev, "Invalid PHY %u\n", index);
				of_node_put(node);
				return -EINVAL;
			}

			phy = &priv->phys[index];

			ret = max_des_parse_phy_dt(priv, phy, node);
			if (ret) {
				of_node_put(node);
				return ret;
			}
		}

		if (of_node_name_eq(node, pipe_node_name)) {
			ret = of_property_read_u32(node, "reg", &index);
			if (ret) {
				dev_err(priv->dev, "Failed to read pipe reg: %d\n", ret);
				continue;
			}

			if (index >= priv->ops->num_pipes) {
				dev_err(priv->dev, "Invalid pipe %u\n", index);
				of_node_put(node);
				return -EINVAL;
			}

			pipe = &priv->pipes[index];

			ret = max_des_parse_pipe_dt(priv, pipe, node);
			if (ret) {
				of_node_put(node);
				return ret;
			}
		}

		if (of_node_name_eq(node, channel_node_name)) {
			ret = of_property_read_u32(node, "reg", &index);
			if (ret) {
				dev_err(priv->dev, "Failed to read channel reg: %d\n", ret);
				continue;
			}

			priv->num_subdevs++;
		}
	}
	of_node_put(node);

	priv->sd_privs = devm_kcalloc(priv->dev, priv->num_subdevs,
					sizeof(*priv->sd_privs), GFP_KERNEL);
	if (!priv->sd_privs)
		return -ENOMEM;

	i = 0;
	for_each_child_of_node(priv->dev->of_node, node) {
		if (of_node_name_eq(node, channel_node_name)) {
			ret = of_property_read_u32(node, "reg", &index);
			if (ret) {
				dev_err(priv->dev, "Failed to read channel reg: %d\n", ret);
				continue;
			}

			sd_priv = &priv->sd_privs[i++];
			sd_priv->node = node;
			sd_priv->priv = priv;
			sd_priv->index = index;
			sd_priv->src_vc_id = 0;
			sd_priv->dst_vc_id = index % MAX_SERDES_VC_ID_NUM;
			sd_priv->pipe_id = index % priv->ops->num_pipes;

			ret = max_des_parse_ch_dt(sd_priv, node);
			if (ret) {
				of_node_put(node);
				return ret;
			}
		}
	}

	return 0;
}

static int max_des_allocate(struct max_des_priv *priv)
{
	priv->phys = devm_kcalloc(priv->dev, priv->ops->num_phys,
				sizeof(*priv->phys), GFP_KERNEL);
	if (!priv->phys)
		return -ENOMEM;

	priv->pipes = devm_kcalloc(priv->dev, priv->ops->num_pipes,
				sizeof(*priv->pipes), GFP_KERNEL);
	if (!priv->pipes)
		return -ENOMEM;

	priv->links = devm_kcalloc(priv->dev, priv->ops->num_links,
				sizeof(*priv->links), GFP_KERNEL);
	if (!priv->links)
		return -ENOMEM;

	priv->fsync = devm_kcalloc(priv->dev, 1,
				sizeof(*priv->fsync), GFP_KERNEL);
	if (!priv->fsync)
		return -ENOMEM;

	return 0;
}

static int max_des_probe(struct max_des_priv *priv)
{
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	mutex_init(&priv->lock);

	ret = max_des_allocate(priv);
	if (ret)
		return ret;

	ret = max_des_parse_dt(priv);
	if (ret)
		return ret;

	ret = max_des_parse_i2c_dt(priv);
	if (ret)
		return ret;

	ret = max_des_parse_link_ser_xlate(priv);
	if (ret)
		return ret;

	ret = max_des_parse_fsync(priv);
	if (ret)
		return ret;

	ret = max_des_init(priv);
	if (ret)
		return ret;

	ret = max_des_i2c_mux_init(priv);
	if (ret)
		return ret;

	ret = max_des_post_init(priv);
	if (ret)
		return ret;

	dev_info(priv->dev, "probe successful\n");
	return 0;
}

static int max_des_remove(struct max_des_priv *priv)
{
	i2c_mux_del_adapters(priv->mux);

	return 0;
}

static int max9296a_read(struct max9296a_priv *priv, int reg)
{
	int ret, val;

	ret = regmap_read(priv->regmap, reg, &val);
	dev_dbg(priv->dev, "read %d 0x%x = 0x%02x\n", ret, reg, val);

	if (ret) {
		dev_err(priv->dev, "read 0x%04x failed\n", reg);
		return ret;
	}

	return val;
}

static int max9296a_write(struct max9296a_priv *priv, unsigned int reg, u8 val)
{
	int ret;

	ret = regmap_write(priv->regmap, reg, val);
	dev_dbg(priv->dev, "write %d 0x%x = 0x%02x\n", ret, reg, val);

	if (ret)
		dev_err(priv->dev, "write 0x%04x failed\n", reg);

	return ret;
}

static int max9296a_update_bits(struct max9296a_priv *priv, unsigned int reg,
					u8 mask, u8 val)
{
	int ret;

	ret = regmap_update_bits(priv->regmap, reg, mask, val);
	dev_dbg(priv->dev, "update %d 0x%x 0x%02x = 0x%02x\n", ret, reg, mask, val);

	if (ret)
		dev_err(priv->dev, "update 0x%04x failed\n", reg);

	return ret;
}

static unsigned int max9296a_field_get(unsigned int val, unsigned int mask)
{
	return (val & mask) >> __ffs(mask);
}

static unsigned int max9296a_field_prep(unsigned int val, unsigned int mask)
{
	return (val << __ffs(mask)) & mask;
}

static int max9296a_change_address(struct max9296a_priv *priv)
{
	struct i2c_client *client;
	struct regmap *regmap;
	int ret;
	unsigned int max9296a_addr[4] = { 0x40, 0x4a, 0x68, 0x6c };
	unsigned int i;

	dev_dbg(priv->dev, "%s()\n", __func__);

	client = i2c_new_dummy_device(priv->client->adapter, priv->i2c_addr);
	if (IS_ERR(client)) {
		ret = PTR_ERR(client);
		dev_err(priv->dev,
			"Failed to create I2C client: %d\n", ret);
		return ret;
	}

	regmap = regmap_init_i2c(client, &max_des_i2c_regmap);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(priv->dev,
			"Failed to create I2C regmap: %d\n", ret);
		goto err_unregister_client;
	}

	ret = regmap_write(regmap, 0x0, priv->client->addr << 1);
	if (ret) {
		dev_err(priv->dev, "Failed to change deserializer address: %d\n", ret);
		goto err_regmap_exit;
	}

	for (i = 0; i < (sizeof(max9296a_addr)/sizeof(unsigned int)); i++) {
		if (max9296a_addr[i] == priv->client->addr) {
			dev_info(priv->dev, "change addr to [%d] 0x%x\n", i, max9296a_addr[i]);
			regmap_update_bits(priv->regmap, 0x5b, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x63, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x6b, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x73, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x7b, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x83, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x8b, GENMASK(2, 0), i);
		}
	}

err_regmap_exit:
	regmap_exit(regmap);

err_unregister_client:
	i2c_unregister_device(client);

	return ret;
}

static int max9296a_wait_for_device(struct max9296a_priv *priv)
{
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	for (i = 0; i < 10; i++) {
		ret = max9296a_read(priv, 0x0);
		if (ret >= 0)
			return 0;

		msleep(50);

		dev_dbg(priv->dev, "Retry %u waiting for deserializer: %d\n", i, ret);
	}

	return ret;
}

static int max9296a_reset(struct max9296a_priv *priv)
{
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max9296a_wait_for_device(priv);
	if (ret) {
		dev_err(priv->dev, "Failed waiting for MAX9296A, err: %d\n", ret);
		return ret;
	}

	ret = max9296a_update_bits(priv, 0x10, 0x80, 0x80);
	if (ret)
		return ret;

	msleep(50);

	if (priv->i2c_addr_change) {
		max9296a_change_address(priv);
		if (ret)
			return ret;
	}

	ret = max9296a_wait_for_device(priv);
	if (ret) {
		dev_err(priv->dev, "Failed waiting for MAX9296A, err: %d\n", ret);
		return ret;
	}

	return 0;
}

static int max9296a_mipi_enable(struct max_des_priv *des_priv, bool enable)
{
	struct max9296a_priv *priv = des_to_priv(des_priv);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (enable) {
		ret = max9296a_update_bits(priv, 0x313, 0x02, 0x02);
		if (ret)
			return ret;
	} else {
		ret = max9296a_update_bits(priv, 0x313, 0x02, 0x00);
		if (ret)
			return ret;
	}

	return 0;
}

static int max9296a_init(struct max_des_priv *des_priv)
{
	struct max9296a_priv *priv = des_to_priv(des_priv);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	/* Disable all PHYs. */
	ret = max9296a_update_bits(priv, 0x332, GENMASK(7, 4), 0x00);
	if (ret)
		return ret;

	/* Disable all pipes. */
	ret = max9296a_update_bits(priv, 0x2, GENMASK(7, 4), 0x00);
	if (ret)
		return ret;

	if (priv->info->num_pipes == 1) {
		ret = max9296a_update_bits(priv, 0x160, BIT(0), 0x00);
		if (ret)
			return ret;
	}

	/* Set I2C speed to 397Kbps */
	ret = max9296a_update_bits(priv, 0x41, GENMASK(6, 4), 0x50);
	if (ret)
		return ret;

	/* Disable all UART channels */
	ret = max9296a_update_bits(priv, 0x3, GENMASK(5, 4), 0x00);
	if (ret)
		return ret;

	return 0;
}

static int max9296a_init_phy(struct max_des_priv *des_priv,
				 struct max_des_phy *phy)
{
	struct max9296a_priv *priv = des_to_priv(des_priv);
	unsigned int num_data_lanes = phy->mipi.num_data_lanes;
	unsigned int dpll_freq = phy->link_frequency * 2;
	unsigned int master_phy, slave_phy;
	unsigned int master_shift, slave_shift;
	unsigned int reg, val, mask;
	unsigned int clk_bit, lane_0_bit, lane_2_bit;
	unsigned int used_data_lanes = 0;
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);
	/*
	 * MAX9296A has four PHYs, but does not support single-PHY configurations,
	 * only double-PHY configurations, even when only using two lanes.
	 * For PHY 0 + PHY 1, PHY 1 is the master PHY.
	 * For PHY 2 + PHY 3, PHY 2 is the master PHY.
	 * Clock is always on the master PHY.
	 * For first pair of PHYs, first lanes are on the master PHY.
	 * For second pair of PHYs, first lanes are on the master PHY too.
	 *
	 * PHY 0 + 1
	 * CLK = PHY 1
	 * PHY1 Lane 0 = D0
	 * PHY1 Lane 1 = D1
	 * PHY0 Lane 0 = D2
	 * PHY0 Lane 1 = D3
	 *
	 * PHY 2 + 3
	 * CLK = PHY 2
	 * PHY2 Lane 0 = D0
	 * PHY2 Lane 1 = D1
	 * PHY3 Lane 0 = D2
	 * PHY3 Lane 1 = D3
	 *
	 * MAX96714 only has two PHYs which cannot support single-PHY configurations.
	 * Clock is always on the master PHY, first lanes are on PHY 0, even if
	 * PHY 1 is the master PHY.
	 *
	 * PHY 0 + 1
	 * CLK = PHY 1
	 * PHY0 Lane 0 = D0
	 * PHY0 Lane 1 = D1
	 * PHY1 Lane 0 = D2
	 * PHY1 Lane 1 = D3
	 */
	if (phy->index == 0) {
		master_phy = 1;
		slave_phy = 0;
	} else if (phy->index == 1) {
		master_phy = 2;
		slave_phy = 3;
	} else {
		return -EINVAL;
	}

	/* Configure a lane count. */
	/* TODO: Add support CPHY mode. */
	ret = max9296a_update_bits(priv, 0x44a + 0x40 * phy->index, GENMASK(7, 6),
				   FIELD_PREP(GENMASK(7, 6), num_data_lanes - 1));
	if (ret)
		return ret;

	/* Configure lane mapping. */
	/*
	 * The lane of each PHY can be mapped to physical lanes 0, 1, 2,
	 * and 3. This mapping is exclusive, multiple lanes, even if unused
	 * cannot be mapped to the same physical lane.
	 * Each lane mapping is represented as two bits.
	 */
	reg = 0x333 + phy->index;

	master_shift = (master_phy % 2) * 4;
	slave_shift = (slave_phy % 2) * 4;

	if (phy->index == 0 && priv->info->phy0_first_lanes_on_master_phy) {
		lane_0_bit = master_shift;
		lane_2_bit = slave_shift;
	} else {
		lane_0_bit = slave_shift;
		lane_2_bit = master_shift;
	}

	val = 0;
	for (i = 0; i < 4 ; i++) {
		unsigned int shift;
		unsigned int map;

		if (i < num_data_lanes) {
			if (phy->mipi.data_lanes[i] < 1)
				return -EINVAL;

			map = phy->mipi.data_lanes[i] - 1;
		} else {
			map = ffz(used_data_lanes);
		}

		if (i < 2)
			shift = lane_0_bit;
		else
			shift = lane_2_bit;

		shift += (i % 2) * 2;

		val |= map << shift;

		used_data_lanes |= BIT(map);
	}

	ret = max9296a_update_bits(priv, reg, 0xff, val);
	if (ret)
		return ret;

	/*
	 * Configure lane polarity.
	 *
	 * PHY 0 and 1 are on register 0x335.
	 * PHY 1 and 2 are on register 0x336.
	 *
	 * Each PHY has 3 bits of polarity configuration.
	 *
	 * On MAX9296A, each bit represents the lane polarity of logical lanes.
	 * Each of these lanes can be mapped to any physical lane.
	 * 0th bit is for lane 0.
	 * 1st bit is for lane 1.
	 * 2nd bit is for clock lane.
	 *
	 * On MAX96714, each bit represents the lane polarity of physical lanes.
	 * 0th bit for physical lane 0.
	 * 1st bit for physical lane 1.
	 * 2nd bit for clock lane of PHY 0, the slave PHY, which is unused.
	 *
	 * 3rd bit for physical lane 2.
	 * 4th bit for physical lane 3.
	 * 5th bit for clock lane of PHY 1, the master PHY.
	 */
	reg = 0x335 + phy->index;

	master_shift = (master_phy % 2) * 3;
	slave_shift = (slave_phy % 2) * 3;
	clk_bit = master_shift + 2;

	if (phy->index == 0 && priv->info->phy0_first_lanes_on_master_phy) {
		lane_0_bit = master_shift;
		lane_2_bit = slave_shift;
	} else {
		lane_0_bit = slave_shift;
		lane_2_bit = master_shift;
	}

	val = 0;

	if (phy->mipi.lane_polarities[0])
		val |= BIT(clk_bit);

	for (i = 0; i < num_data_lanes; i++) {
		unsigned int shift;
		unsigned int map;

		if (!phy->mipi.lane_polarities[i + 1])
			continue;

		/*
		 * The numbers inside the data_lanes array specify the hardware
		 * lane each logical lane maps to.
		 * If polarity is set for the physical lanes, retrieve the
		 * physical lane matching the logical lane from data_lanes.
		 * Otherwise, when polarity is set for the logical lanes
		 * the index of the polarity can be used.
		 */

		if (priv->info->polarity_on_physical_lanes)
			map = phy->mipi.data_lanes[i];
		else
			map = i;

		if (map < 2)
			shift = lane_0_bit;
		else
			shift = lane_2_bit;

		shift += map % 2;

		val |= BIT(shift);
	}

	ret = max9296a_update_bits(priv, reg, 0x3f, val);
	if (ret)
		return ret;

	ret = max9296a_update_bits(priv, 0x325, 0x80, 0x80);
	if (ret)
		return ret;

	/* Put DPLL block into reset. */
	ret = max9296a_update_bits(priv, 0x1c00 + 0x100 * master_phy, BIT(0), 0x00);
	if (ret)
		return ret;

	/* Set DPLL frequency. */
	reg = 0x31d + 0x3 * master_phy;
	ret = max9296a_update_bits(priv, reg, GENMASK(4, 0),
				   div_u64(dpll_freq, 100000000));
	if (ret)
		return ret;

	/* Enable DPLL frequency. */
	ret = max9296a_update_bits(priv, reg, BIT(5), BIT(5));
	if (ret)
		return ret;

	ret = max9296a_update_bits(priv, reg, GENMASK(7, 6), 0xc0);
	if (ret)
		return ret;

	/* Pull DPLL block out of reset. */
	ret = max9296a_update_bits(priv, 0x1c00 + 0x100 * master_phy, BIT(0), 0x01);
	if (ret)
		return ret;


	if (dpll_freq > 1500000000ull) {
		/* Enable initial deskew with 2 x 32k UI. */
		ret = max9296a_write(priv, 0x443 + 0x40 * phy->index, 0x81);
		if (ret)
			return ret;

		/* Enable periodic deskew with 2 x 1k UI.. */
		ret = max9296a_write(priv, 0x444 + 0x40 * phy->index, 0x81);
		if (ret)
			return ret;
	} else {
		/* Disable initial deskew. */
		ret = max9296a_write(priv, 0x443 + 0x40 * phy->index, 0x07);
		if (ret)
			return ret;

		/* Disable periodic deskew. */
		ret = max9296a_write(priv, 0x444 + 0x40 * phy->index, 0x01);
		if (ret)
			return ret;
	}

	/* Set alternate memory map modes. */
	val  = phy->alt_mem_map12 ? BIT(0) : 0;
	val |= phy->alt_mem_map8 ? BIT(1) : 0;
	val |= phy->alt_mem_map10 ? BIT(2) : 0;
	ret = max9296a_update_bits(priv, 0x433 + 0x40 * master_phy, GENMASK(2, 0), val);

	/* Enable PHY. */
	mask = (BIT(master_phy) | BIT(slave_phy)) << 4;
	ret = max9296a_update_bits(priv, 0x332, mask, mask);
	if (ret)
		return ret;

	return 0;
}

static unsigned int max9296a_pipe_id(struct max9296a_priv *priv,
					 struct max_des_pipe *pipe)
{
	return priv->info->pipe_hw_ids[pipe->index];
}

static int max9296a_init_pipe(struct max_des_priv *des_priv,
				  struct max_des_pipe *pipe)
{
	struct max9296a_priv *priv = des_to_priv(des_priv);
	unsigned int index = max9296a_pipe_id(priv, pipe);
	unsigned int reg, mask;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	/* Enable pipe. */
	mask = BIT(index + 4);
	ret = max9296a_update_bits(priv, 0x2, mask, mask);
	if (ret)
		return ret;

	if (priv->info->num_pipes == 1) {
		mask = BIT(0);
		ret = max9296a_update_bits(priv, 0x160, mask, mask);
		if (ret)
			return ret;
	}

	/* Set source stream. */
	if (priv->info->num_pipes == 1)
		reg = 0x161;
	else
		reg = 0x50 + index;
	ret = max9296a_update_bits(priv, reg, GENMASK(1, 0), pipe->stream_id);
	if (ret)
		return ret;

	return 0;
}

static int max9296a_init_link(struct max_des_priv *des_priv,
				  struct max_des_link *link)
{
	struct max9296a_priv *priv = des_to_priv(des_priv);
	unsigned int index = link->index;
	unsigned int mask;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	/* PHY optimization */
	ret = max9296a_write(priv, 0x1458 + 0x100 * index, 0x28);
	ret += max9296a_write(priv, 0x1459 + 0x100 * index, 0x68);
	if (ret)
		return ret;

	// /* RLMS Register Setting for 6Gbps GMSL2 Rate */
	// ret = max9296a_write(priv, 0x143f + 0x100 * index, 0x3d);
	// if (ret)
	// 	return ret;

	// ret = max9296a_write(priv, 0x143e + 0x100 * index, 0xfd);
	// if (ret)
	// 	return ret;

	// ret = max9296a_write(priv, 0x1449 + 0x100 * index, 0xf5);
	// if (ret)
	// 	return ret;

	// ret = max9296a_write(priv, 0x14a3 + 0x100 * index, 0x30);
	// if (ret)
	// 	return ret;

	// ret = max9296a_write(priv, 0x14d8 + 0x100 * index, 0x07);
	// if (ret)
	// 	return ret;

	// ret = max9296a_write(priv, 0x14a5 + 0x100 * index, 0x70);
	// if (ret)
	// 	return ret;

	ret = max9296a_update_bits(priv, 0x10, BIT(5), BIT(5));
	if (ret)
		return ret;
	msleep(100);

	if (priv->info->supports_tunnel_mode) {
		mask = BIT(0);
		ret = max9296a_update_bits(priv, 0x474, mask,
					   link->tunnel_mode ? mask : 0);
		if (ret)
			return ret;
	}

	return 0;
}

static int max9296a_init_fsync(struct max_des_priv *des_priv,
				  struct max_des_fsync *fsync)
{
	struct max9296a_priv *priv = des_to_priv(des_priv);
	int ret = 0;

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (fsync->internal || fsync->internal_output) {
		ret = max9296a_write(priv, 0x3f1, 0x00);
		ret += max9296a_write(priv, 0x3e2, 0x00);
		ret += max9296a_write(priv, 0x3ea, 0x00);
		ret += max9296a_write(priv, 0x3eb, 0x00);
		ret += max9296a_write(priv, 0x3e7, (fsync->freq >> 16) & 0xff);
		ret += max9296a_write(priv, 0x3e6, (fsync->freq >> 8) & 0xff);
		ret += max9296a_write(priv, 0x3e5, (fsync->freq >> 0) & 0xff);
		ret += max9296a_write(priv, 0x3ef, 0xc0);
		ret += max9296a_write(priv, 0x3e0, fsync->internal_output << 2);
	}
	else if (fsync->external) {
		ret = max9296a_write(priv, 0x3e0, fsync->external << 3);
	}

	return ret;
}

static int max9296a_init_pipe_remap(struct max9296a_priv *priv,
					struct max_des_pipe *pipe,
					struct max_des_dt_vc_remap *remap,
					unsigned int i)
{
	unsigned int index = max9296a_pipe_id(priv, pipe);
	unsigned int reg, val, shift, mask;
	unsigned int phy_id;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (remap->phy == 0)
		phy_id = 1;
	else if (remap->phy == 1)
		phy_id = 2;

	/* Set source Data Type and Virtual Channel. */
	/* TODO: implement extended Virtual Channel. */
	reg = 0x40d + 0x40 * index + i * 2;
	ret = max9296a_write(priv, reg,
				 MAX_DES_DT_VC(remap->from_dt, remap->from_vc));
	if (ret)
		return ret;

	/* Set destination Data Type and Virtual Channel. */
	/* TODO: implement extended Virtual Channel. */
	reg = 0x40e + 0x40 * index + i * 2;
	ret = max9296a_write(priv, reg,
				 MAX_DES_DT_VC(remap->to_dt, remap->to_vc));
	if (ret)
		return ret;

	/* Set destination PHY. */
	reg = 0x42d + 0x40 * index + i / 4;
	shift = (i % 4) * 2;
	mask = 0x3 << shift;
	val = (phy_id & 0x3) << shift;
	ret = max9296a_update_bits(priv, reg, mask, val);
	if (ret)
		return ret;

	/* Enable remap. */
	reg = 0x40b + 0x40 * index + i / 8;
	val = BIT(i % 8);
	ret = max9296a_update_bits(priv, reg, val, val);
	if (ret)
		return ret;

	return 0;
}

static int max9296a_update_pipe_remaps(struct max_des_priv *des_priv,
					   struct max_des_pipe *pipe)
{
	struct max9296a_priv *priv = des_to_priv(des_priv);
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	for (i = 0; i < pipe->num_remaps; i++) {
		struct max_des_dt_vc_remap *remap = &pipe->remaps[i];

		ret = max9296a_init_pipe_remap(priv, pipe, remap, i);
		if (ret)
			return ret;
	}

	return 0;
}

static int max9296a_select_links(struct max_des_priv *des_priv,
				 unsigned int mask)
{
	struct max9296a_priv *priv = des_to_priv(des_priv);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (!mask) {
		dev_warn(priv->dev, "All links are disabled\n");
	}

	ret = max9296a_update_bits(priv, 0x10,
					BIT(5) | GENMASK(1, 0),
					BIT(5) | FIELD_PREP(GENMASK(1, 0), mask));

	/* delay to settle link */
	msleep(100);

	return ret;
}

static int max9296a_post_init(struct max_des_priv *des_priv)
{
	struct max_des_subdev_priv *sd_priv;
	struct max_des_pipe *pipe;
	const struct max_format *fmt;
	int ret;

	dev_dbg(des_priv->dev, "%s()\n", __func__);

	/* fix format to UYVY8_1X16 */
	fmt = max_format_by_code(MEDIA_BUS_FMT_UYVY8_1X16);
	if (!fmt)
		return -EINVAL;

	for_each_subdev(des_priv, sd_priv) {
		if (!des_priv->links[sd_priv->index].enabled)
			continue;

		pipe = &des_priv->pipes[sd_priv->pipe_id];
		sd_priv->fmt = fmt;

		dev_dbg(des_priv->dev, "pipe_id [%d], phy_id [%d], src_vc_id [%d], dst_vc_id [%d]\n",
			sd_priv->pipe_id, sd_priv->phy_id,
			sd_priv->src_vc_id, sd_priv->dst_vc_id);

		mutex_lock(&des_priv->lock);
		ret = max_des_update_pipe_remaps(des_priv, pipe);
		mutex_unlock(&des_priv->lock);
		if (ret)
			return -EINVAL;

		ret = max_des_ch_enable(sd_priv, true);
		if (ret)
			return -EINVAL;
	}

	return 0;
}

static const struct max_des_ops max9296a_ops = {
	.mipi_enable = max9296a_mipi_enable,
	.init = max9296a_init,
	.init_phy = max9296a_init_phy,
	.init_pipe = max9296a_init_pipe,
	.init_link = max9296a_init_link,
	.init_fsync = max9296a_init_fsync,
	.update_pipe_remaps = max9296a_update_pipe_remaps,
	.select_links = max9296a_select_links,
	.post_init = max9296a_post_init,
};

#define max9296a_PIN(n) PINCTRL_PIN(n, "mfp" __stringify(n))

static const struct pinctrl_pin_desc max9296a_pins[] = {
	max9296a_PIN(0), max9296a_PIN(1), max9296a_PIN(2),
	max9296a_PIN(3), max9296a_PIN(4), max9296a_PIN(5),
	max9296a_PIN(6), max9296a_PIN(7), max9296a_PIN(8),
	max9296a_PIN(9), max9296a_PIN(10), max9296a_PIN(11),
	max9296a_PIN(12),
};

#define max9296a_GROUP_PINS(name, ...)                                         \
	static const unsigned int name##_pins[] = { __VA_ARGS__ }

max9296a_GROUP_PINS(mfp0, 0);
max9296a_GROUP_PINS(mfp1, 1);
max9296a_GROUP_PINS(mfp2, 2);
max9296a_GROUP_PINS(mfp3, 3);
max9296a_GROUP_PINS(mfp4, 4);
max9296a_GROUP_PINS(mfp5, 5);
max9296a_GROUP_PINS(mfp6, 6);
max9296a_GROUP_PINS(mfp7, 7);
max9296a_GROUP_PINS(mfp8, 8);
max9296a_GROUP_PINS(mfp9, 9);
max9296a_GROUP_PINS(mfp10, 10);
max9296a_GROUP_PINS(mfp11, 11);
max9296a_GROUP_PINS(mfp12, 12);

#define max9296a_GROUP(name)                                                   \
	PINCTRL_PINGROUP(__stringify(name), name##_pins,                       \
			ARRAY_SIZE(name##_pins))

static const struct pingroup max9296a_ctrl_groups[] = {
	max9296a_GROUP(mfp0), max9296a_GROUP(mfp1), max9296a_GROUP(mfp2),
	max9296a_GROUP(mfp3), max9296a_GROUP(mfp4), max9296a_GROUP(mfp5),
	max9296a_GROUP(mfp6), max9296a_GROUP(mfp7), max9296a_GROUP(mfp8),
	max9296a_GROUP(mfp9), max9296a_GROUP(mfp10), max9296a_GROUP(mfp11),
	max9296a_GROUP(mfp12),
};

#define max9296a_FUNC_GROUPS(name, ...)                                        \
	static const char *const name##_groups[] = { __VA_ARGS__ }

max9296a_FUNC_GROUPS(gpio, "mfp0", "mfp1", "mfp2", "mfp3", "mfp4", "mfp5",
			"mfp6", "mfp7", "mfp8", "mfp9", "mfp10", "mfp11", "mfp12");

enum max9296a_func {
	max9296a_func_gpio,
};

#define max9296a_FUNC(name)                                                    \
	[max9296a_func_##name] = PINCTRL_PINFUNCTION(                          \
		__stringify(name), name##_groups, ARRAY_SIZE(name##_groups))

static const struct pinfunction max9296a_functions[] = {
	max9296a_FUNC(gpio),
};

enum max9296a_pinctrl_params {
	max9296a_PINCTRL_PULL_STRENGTH_WEAK = PIN_CONFIG_END + 1,
	max9296a_PINCTRL_JITTER_COMPENSATION_EN,
	max9296a_PINCTRL_GMSL_TX_EN,
	max9296a_PINCTRL_GMSL_RX_EN,
	max9296a_PINCTRL_GMSL_TX_ID,
	max9296a_PINCTRL_GMSL_RX_ID,
	max9296a_PINCTRL_INPUT_VALUE,
};

static const struct pinconf_generic_params max9296a_cfg_params[] = {
	{ "maxim,pull-strength-weak", max9296a_PINCTRL_PULL_STRENGTH_WEAK, 0 },
	{ "maxim,jitter-compensation", max9296a_PINCTRL_JITTER_COMPENSATION_EN, 0 },
	{ "maxim,gmsl-tx", max9296a_PINCTRL_GMSL_TX_EN, 0 },
	{ "maxim,gmsl-rx", max9296a_PINCTRL_GMSL_RX_EN, 0 },
	{ "maxim,gmsl-tx-id", max9296a_PINCTRL_GMSL_TX_ID, 0 },
	{ "maxim,gmsl-rx-id", max9296a_PINCTRL_GMSL_RX_ID, 0 },
};

static int max9296a_ctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(max9296a_ctrl_groups);
}

static const char *max9296a_ctrl_get_group_name(struct pinctrl_dev *pctldev,
						unsigned selector)
{
	return max9296a_ctrl_groups[selector].name;
}

static int max9296a_ctrl_get_group_pins(struct pinctrl_dev *pctldev,
					unsigned selector,
					const unsigned **pins,
					unsigned *num_pins)
{
	*pins = (unsigned *)max9296a_ctrl_groups[selector].pins;
	*num_pins = max9296a_ctrl_groups[selector].npins;

	return 0;
}

static int max9296a_get_pin_config_reg(unsigned int offset, u32 param,
					unsigned int *reg, unsigned int *mask,
					unsigned int *val)
{
	*reg = 0x2b0 + offset * 0x3;

	switch (param) {
	case PIN_CONFIG_OUTPUT_ENABLE:
		*mask = BIT(0);
		*val = 0b0;
		return 0;
	case PIN_CONFIG_INPUT_ENABLE:
		*mask = BIT(0);
		*val = 0b1;
		return 0;
	case max9296a_PINCTRL_GMSL_TX_EN:
		*mask = BIT(1);
		*val = 0b1;
		return 0;
	case max9296a_PINCTRL_GMSL_RX_EN:
		*mask = BIT(2);
		*val = 0b1;
		return 0;
	case max9296a_PINCTRL_INPUT_VALUE:
		*mask = BIT(3);
		*val = 0b1;
		return 0;
	case PIN_CONFIG_OUTPUT:
		*mask = BIT(4);
		*val = 0b1;
		return 0;
	case max9296a_PINCTRL_JITTER_COMPENSATION_EN:
		*mask = BIT(5);
		*val = 0b1;
		return 0;
	case max9296a_PINCTRL_PULL_STRENGTH_WEAK:
		*mask = BIT(7);
		*val = 0b0;
		return 0;
	case max9296a_PINCTRL_GMSL_TX_ID:
		*reg += 1;
		*mask = GENMASK(4, 0);
		return 0;
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		*reg += 1;
		*mask = BIT(5);
		*val = 0b0;
		return 0;
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		*reg += 1;
		*mask = BIT(5);
		*val = 0b1;
		return 0;
	case PIN_CONFIG_BIAS_DISABLE:
		*reg += 1;
		*mask = GENMASK(7, 6);
		*val = 0b00;
		return 0;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		*reg += 1;
		*mask = GENMASK(7, 6);
		*val = 0b10;
		return 0;
	case PIN_CONFIG_BIAS_PULL_UP:
		*reg += 1;
		*mask = GENMASK(7, 6);
		*val = 0b01;
		return 0;
	case max9296a_PINCTRL_GMSL_RX_ID:
		*reg += 2;
		*mask = GENMASK(4, 0);
		return 0;
	default:
		return -ENOTSUPP;
	}
}

static int max9296a_conf_pin_config_get(struct pinctrl_dev *pctldev,
					unsigned int offset,
					unsigned long *config)
{
	struct max9296a_priv *priv = pinctrl_dev_get_drvdata(pctldev);
	u32 param = pinconf_to_config_param(*config);
	unsigned int reg, mask, val;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max9296a_get_pin_config_reg(offset, param, &reg, &mask, &val);
	if (ret)
		return ret;

	switch (param) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
	case PIN_CONFIG_DRIVE_PUSH_PULL:
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		ret = max9296a_read(priv, reg);
		if (ret < 0)
			return ret;

		val = max9296a_field_get(ret, mask) == val;
		if (!val)
			return -EINVAL;

		break;
	case max9296a_PINCTRL_JITTER_COMPENSATION_EN:
	case max9296a_PINCTRL_PULL_STRENGTH_WEAK:
	case max9296a_PINCTRL_GMSL_TX_EN:
	case max9296a_PINCTRL_GMSL_RX_EN:
	case max9296a_PINCTRL_INPUT_VALUE:
	case PIN_CONFIG_OUTPUT_ENABLE:
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT:
		ret = max9296a_read(priv, reg);
		if (ret < 0)
			return ret;

		val = max9296a_field_get(ret, mask) == val;
		break;
	case max9296a_PINCTRL_GMSL_TX_ID:
	case max9296a_PINCTRL_GMSL_RX_ID:
		ret = max9296a_read(priv, reg);
		if (ret < 0)
			return ret;

		val = max9296a_field_get(val, mask);
		break;
	default:
		return -ENOTSUPP;
	}

	*config = pinconf_to_config_packed(param, val);

	return 0;
}

static int max9296a_conf_pin_config_set_one(struct max9296a_priv *priv,
						unsigned int offset,
						unsigned long config)
{
	u32 param = pinconf_to_config_param(config);
	u32 arg = pinconf_to_config_argument(config);
	unsigned int reg, mask, val;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max9296a_get_pin_config_reg(offset, param, &reg, &mask, &val);
	if (ret)
		return ret;

	switch (param) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
	case PIN_CONFIG_DRIVE_PUSH_PULL:
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		val = max9296a_field_prep(val, mask);

		ret = max9296a_update_bits(priv, reg, mask, val);
		break;
	case max9296a_PINCTRL_JITTER_COMPENSATION_EN:
	case max9296a_PINCTRL_PULL_STRENGTH_WEAK:
	case max9296a_PINCTRL_GMSL_TX_EN:
	case max9296a_PINCTRL_GMSL_RX_EN:
	case PIN_CONFIG_OUTPUT_ENABLE:
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT:
		val = max9296a_field_prep(arg ? val : ~val, mask);

		ret = max9296a_update_bits(priv, reg, mask, val);
		break;
	case max9296a_PINCTRL_GMSL_TX_ID:
	case max9296a_PINCTRL_GMSL_RX_ID:
		val = max9296a_field_prep(arg, mask);

		ret = max9296a_update_bits(priv, reg, mask, val);
		break;
	default:
		return -ENOTSUPP;
	}

	if (param == PIN_CONFIG_OUTPUT) {
		config = pinconf_to_config_packed(PIN_CONFIG_OUTPUT_ENABLE, 1);
		ret = max9296a_conf_pin_config_set_one(priv, offset, config);
		if (ret)
			return ret;
	}

	return ret;
}

static int max9296a_conf_pin_config_set(struct pinctrl_dev *pctldev,
					unsigned int offset,
					unsigned long *configs,
					unsigned int num_configs)
{
	struct max9296a_priv *priv = pinctrl_dev_get_drvdata(pctldev);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	while (num_configs--) {
		unsigned long config = *configs;

		ret = max9296a_conf_pin_config_set_one(priv, offset, config);
		if (ret)
			return ret;

		configs++;
	}

	return 0;
}

static int max9296a_mux_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(max9296a_functions);
}

static const char *max9296a_mux_get_function_name(struct pinctrl_dev *pctldev,
						unsigned selector)
{
	return max9296a_functions[selector].name;
}

static int max9296a_mux_get_groups(struct pinctrl_dev *pctldev,
				unsigned selector,
				const char *const **groups,
				unsigned *const num_groups)
{
	*groups = max9296a_functions[selector].groups;
	*num_groups = max9296a_functions[selector].ngroups;

	return 0;
}

static int max9296a_mux_set(struct pinctrl_dev *pctldev, unsigned selector,
				unsigned group)
{
	return 0;
}

static int max9296a_gpio_get_direction(struct gpio_chip *gc,
					unsigned int offset)
{
	unsigned long config =
		pinconf_to_config_packed(PIN_CONFIG_OUTPUT_ENABLE, 0);
	struct max9296a_priv *priv = gpiochip_get_data(gc);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max9296a_conf_pin_config_get(priv->pctldev, offset, &config);
	if (ret)
		return ret;

	return pinconf_to_config_argument(config) ? GPIO_LINE_DIRECTION_OUT :
							GPIO_LINE_DIRECTION_IN;
}

static int max9296a_gpio_direction_input(struct gpio_chip *gc,
					unsigned int offset)
{
	unsigned long config =
		pinconf_to_config_packed(PIN_CONFIG_INPUT_ENABLE, 1);
	struct max9296a_priv *priv = gpiochip_get_data(gc);

	dev_dbg(priv->dev, "%s()\n", __func__);

	return max9296a_conf_pin_config_set_one(priv, offset, config);
}

static int max9296a_gpio_direction_output(struct gpio_chip *gc,
					unsigned int offset, int value)
{
	unsigned long config =
		pinconf_to_config_packed(PIN_CONFIG_OUTPUT, value);
	struct max9296a_priv *priv = gpiochip_get_data(gc);

	dev_dbg(priv->dev, "%s()\n", __func__);

	return max9296a_conf_pin_config_set_one(priv, offset, config);
}

static int max9296a_gpio_get(struct gpio_chip *gc, unsigned int offset)
{
	unsigned long config =
		pinconf_to_config_packed(max9296a_PINCTRL_INPUT_VALUE, 0);
	struct max9296a_priv *priv = gpiochip_get_data(gc);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max9296a_conf_pin_config_get(priv->pctldev, offset, &config);
	if (ret)
		return ret;

	return pinconf_to_config_argument(config);
}

static void max9296a_gpio_set(struct gpio_chip *gc, unsigned int offset,
				int value)
{
	unsigned long config =
		pinconf_to_config_packed(PIN_CONFIG_OUTPUT, value);
	struct max9296a_priv *priv = gpiochip_get_data(gc);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max9296a_conf_pin_config_set_one(priv, offset, config);
	if (ret)
		dev_err(priv->dev,
			"Failed to set GPIO %u output value, err: %d\n", offset,
			ret);
}

static struct pinctrl_ops max9296a_ctrl_ops = {
	.get_groups_count = max9296a_ctrl_get_groups_count,
	.get_group_name = max9296a_ctrl_get_group_name,
	.get_group_pins = max9296a_ctrl_get_group_pins,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_pin,
	.dt_free_map = pinconf_generic_dt_free_map,
};

static const struct pinconf_ops max9296a_conf_ops = {
	.pin_config_get = max9296a_conf_pin_config_get,
	.pin_config_set = max9296a_conf_pin_config_set,
	.is_generic = true,
};

static const struct pinmux_ops max9296a_mux_ops = {
	.get_functions_count = max9296a_mux_get_functions_count,
	.get_function_name = max9296a_mux_get_function_name,
	.get_function_groups = max9296a_mux_get_groups,
	.set_mux = max9296a_mux_set,
	.strict = true,
};

static int max9296a_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct max9296a_priv *priv;
	struct max_des_ops *ops;
	int ret;

	dev_info(dev, "%s() device node: %s\n", __func__,
		 client->dev.of_node->full_name);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	ops = devm_kzalloc(dev, sizeof(*ops), GFP_KERNEL);
	if (!ops)
		return -ENOMEM;

	priv->info = of_device_get_match_data(dev);
	if (!priv->info) {
		dev_err(dev, "Failed to get match data\n");
		return -ENODEV;
	}

	priv->dev = dev;
	priv->client = client;
	i2c_set_clientdata(client, priv);

	priv->regmap = devm_regmap_init_i2c(client, &max_des_i2c_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	*ops = max9296a_ops;

	ops->fix_tx_ids = priv->info->fix_tx_ids;
	ops->num_phys = priv->info->num_phys;
	ops->num_pipes = priv->info->num_pipes;
	ops->num_links = priv->info->num_links;
	ops->supports_tunnel_mode = priv->info->supports_tunnel_mode;

	priv->des_priv.dev = dev;
	priv->des_priv.client = client;
	priv->des_priv.regmap = priv->regmap;
	priv->des_priv.ops = ops;

	priv->reset_gpio =
		devm_gpiod_get_optional(priv->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR_OR_NULL(priv->reset_gpio)) {
		ret = PTR_ERR(priv->reset_gpio);
		if (ret != -EPROBE_DEFER) {
			dev_warn(&client->dev, "reset-gpios not found: %d\n", ret);
		}
	}
	else {
		gpiod_set_value_cansleep(priv->reset_gpio, 0);
		usleep_range(500, 600);
		gpiod_set_value_cansleep(priv->reset_gpio, 1);
		usleep_range(500, 600);
	}
	msleep(50);

	priv->i2c_addr_change = false;
	ret = of_property_read_u32(dev->of_node, "phy-reg", &priv->i2c_addr);
	if (!ret) {
		if (priv->i2c_addr != priv->client->addr) {
			priv->i2c_addr_change = true;
			ret = max9296a_change_address(priv);
			if (ret)
				return ret;
		}
	}

	ret = max9296a_reset(priv);
	if (ret)
		return ret;

	/* Disable link auto-select. */
	ret = max9296a_update_bits(priv, 0x10, GENMASK(5, 4), 0x20);
	if (ret)
		return ret;
	msleep(100);

	priv->pctldesc = (struct pinctrl_desc){
		.owner = THIS_MODULE,
		.name = MAX9296A_NAME,
		.pins = max9296a_pins,
		.npins = ARRAY_SIZE(max9296a_pins),
		.pctlops = &max9296a_ctrl_ops,
		.confops = &max9296a_conf_ops,
		.pmxops = &max9296a_mux_ops,
		.custom_params = max9296a_cfg_params,
		.num_custom_params = ARRAY_SIZE(max9296a_cfg_params),
	};

	ret = devm_pinctrl_register_and_init(dev, &priv->pctldesc, priv,
						&priv->pctldev);
	if (ret)
		return ret;

	ret = pinctrl_enable(priv->pctldev);
	if (ret)
		return ret;

	priv->gc = (struct gpio_chip){
		.owner = THIS_MODULE,
		.label = MAX9296A_NAME,
		.base = -1,
		.ngpio = MAX9296A_GPIO_NUM,
		.parent = dev,
		.can_sleep = true,
		.request = gpiochip_generic_request,
		.free = gpiochip_generic_free,
		.set_config = gpiochip_generic_config,
		.get_direction = max9296a_gpio_get_direction,
		.direction_input = max9296a_gpio_direction_input,
		.direction_output = max9296a_gpio_direction_output,
		.get = max9296a_gpio_get,
		.set = max9296a_gpio_set,
	};

	ret = devm_gpiochip_add_data(dev, &priv->gc, priv);
	if (ret)
		return ret;

	return max_des_probe(&priv->des_priv);
}

static int max9296a_remove(struct i2c_client *client)
{
	struct max9296a_priv *priv = i2c_get_clientdata(client);

	return max_des_remove(&priv->des_priv);
}

static const struct max9296a_chip_info max9296a_info = {
	.phy0_first_lanes_on_master_phy = true,
	.supports_tunnel_mode = false,
	.fix_tx_ids = true,
	.num_pipes = 4,
	.pipe_hw_ids = { 0, 1, 2, 3 },
	.num_phys = 2,
	.num_links = 2,
};

static const struct max9296a_chip_info max96714_info = {
	.polarity_on_physical_lanes = true,
	.supports_tunnel_mode = true,
	.num_pipes = 1,
	.pipe_hw_ids = { 1 },
	.num_phys = 1,
	.num_links = 1,
};

static const struct of_device_id max9296a_of_table[] = {
	{ .compatible = "maxim,max9296a", .data = &max9296a_info },
	{ .compatible = "maxim,max96714", .data = &max96714_info },
	{ },
};
MODULE_DEVICE_TABLE(of, max9296a_of_table);

static struct i2c_driver max9296a_i2c_driver = {
	.driver	= {
		.name = "max9296a",
		.of_match_table	= of_match_ptr(max9296a_of_table),
	},
	.probe_new = max9296a_probe,
	.remove = max9296a_remove,
};

module_i2c_driver(max9296a_i2c_driver);

MODULE_DESCRIPTION("Maxim MAX9296A Dual GMSL2 Deserializer Driver");
MODULE_AUTHOR("TechNexion Inc.");
MODULE_LICENSE("GPL");