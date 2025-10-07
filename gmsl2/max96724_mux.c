// SPDX-License-Identifier: GPL-2.0
/*
* Maxim MAX96724 Quad GMSL2 Deserializer Driver
*
*/

#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/i2c-mux.h>
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

#define MAX96724_N_GMSL			    (4)

#define MAX96724_PHYS_NUM		    (4)
#define MAX96724_PHY1_ALT_CLOCK		(5)
#define MAX96724_NAME               "max96724"
#define MAX96724_GPIO_NUM           (9)

struct max96724_priv {
	struct max_des_priv des_priv;

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

#define des_to_priv(des) \
	container_of(des, struct max96724_priv, des_priv)

#define for_each_subdev(priv, sd_priv) \
	for ((sd_priv) = NULL; ((sd_priv) = next_subdev((priv), (sd_priv))); )

static inline struct max_des_subdev_priv *sd_to_max_des(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max_des_subdev_priv, sd);
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

static int __max_des_mipi_update(struct max_des_priv *priv)
{
	struct max_des_subdev_priv *sd_priv;
	bool enable = 0;

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
	// struct max_des_priv *des_priv = i2c_mux_priv(muxc);
	// struct max96724_priv *priv = des_to_priv(des_priv);
	// u8 val = 0xff;

	// if (des_priv->mux_chan == chan)
	// 	return 0;

	// val &= ~(0x3 << (chan * 2));
	// val |= 0x2 << (chan * 2);
	// regmap_write(priv->regmap, 0x3, val);

	// des_priv->mux_chan = chan;

	return 0;
}

static int max_des_i2c_mux_init(struct max_des_priv *priv)
{
	unsigned int i;
	u8 val = 0;
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

		ret = i2c_mux_add_adapter(priv->mux, 0, link->index, 0);
		if (ret)
			goto err_add_adapters;

		val |= 0x2 << (link->index * 2);
	}

	regmap_write(priv->regmap, 0x3, val);

	return 0;

err_add_adapters:
	i2c_mux_del_adapters(priv->mux);
	return ret;
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

		dev_info(priv->dev, "link [%d]: %s mode",
			pipe->link_id,
			priv->links[pipe->link_id].tunnel_mode ? "tunnel" : "pixel");
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

		dev_info(priv->dev, "%s mode",
			priv->fsync->internal_output ? "internal fsync with output" :
			priv->fsync->internal ? "internal fsync" :
			priv->fsync->external ? "external fsync" :
			"non-fsync");
	}

	return 0;
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
		if (id >= MAX96724_N_GMSL)
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

static int max96724_read(struct max96724_priv *priv, int reg)
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

static int max96724_write(struct max96724_priv *priv, unsigned int reg, u8 val)
{
	int ret;

	ret = regmap_write(priv->regmap, reg, val);
	dev_dbg(priv->dev, "write %d 0x%x = 0x%02x\n", ret, reg, val);
	if (ret)
		dev_err(priv->dev, "write 0x%04x failed\n", reg);

	return ret;
}

static int max96724_update_bits(struct max96724_priv *priv, unsigned int reg,
					u8 mask, u8 val)
{
	int ret;

	ret = regmap_update_bits(priv->regmap, reg, mask, val);
	dev_dbg(priv->dev, "update %d 0x%x 0x%02x = 0x%02x\n", ret, reg, mask, val);
	if (ret)
		dev_err(priv->dev, "update 0x%04x failed\n", reg);

	return ret;
}

static unsigned int max96724_field_get(unsigned int val, unsigned int mask)
{
	return (val & mask) >> __ffs(mask);
}

static unsigned int max96724_field_prep(unsigned int val, unsigned int mask)
{
	return (val << __ffs(mask)) & mask;
}

static int max96724_change_address(struct max96724_priv *priv)
{
	struct i2c_client *client;
	struct regmap *regmap;
	int ret;
	unsigned int max96724_addr[4] = { 0x27, 0x2e, 0x4e, 0x4f };
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

	for (i = 0; i < (sizeof(max96724_addr)/sizeof(unsigned int)); i++) {
		if (max96724_addr[i] == priv->client->addr) {
			dev_info(priv->dev, "change addr to [%d] 0x%x\n", i, max96724_addr[i]);
			regmap_update_bits(priv->regmap, 0x72, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x76, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x7a, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x7e, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0xa3, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0xab, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0xb3, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0xbb, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x503, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x513, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x523, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x533, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x563, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x573, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x583, GENMASK(2, 0), i);
			regmap_update_bits(priv->regmap, 0x593, GENMASK(2, 0), i);
		}
	}

err_regmap_exit:
	regmap_exit(regmap);

err_unregister_client:
	i2c_unregister_device(client);

	return ret;
}

static int max96724_wait_for_device(struct max96724_priv *priv)
{
	unsigned int i;
	int ret;

	for (i = 0; i < 10; i++) {
		ret = max96724_read(priv, 0x0);
		if (ret >= 0)
			return 0;

		msleep(100);

		dev_dbg(priv->dev, "Retry %u waiting for deserializer: %d\n", i, ret);
	}

	return ret;
}

static int max96724_log_pipe_status(struct max_des_priv *des_priv,
					struct max_des_pipe *pipe, const char *name)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	unsigned int index = pipe->index;
	unsigned int reg, mask;
	int ret;

	reg = 0x1dc + index * 0x20;
	mask = BIT(0);
	ret = max96724_read(priv, reg);
	if (ret < 0)
		return ret;

	ret = ret & mask;
	dev_info(priv->dev, "%s: \tvideo_lock: %u\n", name, ret);

	return 0;
}

static int max96724_log_phy_status(struct max_des_priv *des_priv,
				struct max_des_phy *phy, const char *name)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	unsigned int index = phy->index;
	unsigned int reg, mask, shift;
	int ret;

	reg = 0x8d0 + index / 2;
	shift = 4 * (index % 2);
	mask = GENMASK(3, 0);
	ret = max96724_read(priv, reg);
	if (ret < 0)
		return ret;

	ret = (ret >> shift) & mask;
	dev_info(priv->dev, "%s: \tcsi2_pkt_cnt: %u\n", name, ret);

	reg += 2;
	ret = max96724_read(priv, reg);
	if (ret < 0)
		return ret;

	ret = (ret >> shift) & mask;
	dev_info(priv->dev, "%s: \tphy_pkt_cnt: %u\n", name, ret);

	return 0;
}

static int max96724_mipi_enable(struct max_des_priv *des_priv, bool enable)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	int ret;

	dev_dbg(priv->dev, "%s() - [%d]\n", __func__, enable);

	if (enable) {
		ret = max96724_update_bits(priv, 0x40b, 0x02, 0x02);
		if (ret)
			return ret;
	} else {
		ret = max96724_update_bits(priv, 0x40b, 0x02, 0x00);
		if (ret)
			return ret;
	}

	return 0;
}

struct max96724_lane_config {
	unsigned int lanes[MAX96724_PHYS_NUM];
	unsigned int clock_lane[MAX96724_PHYS_NUM];
	unsigned int bit;
};

static const struct max96724_lane_config max96724_lane_configs[] = {

	/*
	* PHY 1 can be in 4-lane mode (combining lanes of PHY 0 and PHY 1)
	* but only use the data lanes of PHY0, while continuing to use the
	* clock lane of PHY 1.
	* Specifying clock-lanes as 5 turns on alternate clocking mode.
	*/
	{ { 0, 2, 4, 0 }, { 0, MAX96724_PHY1_ALT_CLOCK, 0, 0 }, BIT(2) },
	{ { 0, 2, 2, 2 }, { 0, MAX96724_PHY1_ALT_CLOCK, 0, 0 }, BIT(3) },

	{ { 2, 2, 2, 2 }, { 0, 0, 0, 0 }, BIT(0) },
	{ { 0, 4, 4, 0 }, { 0, 0, 0, 0 }, BIT(2) },
	{ { 0, 4, 2, 2 }, { 0, 0, 0, 0 }, BIT(3) },
	{ { 2, 2, 4, 0 }, { 0, 0, 0, 0 }, BIT(4) },
};

static int max96724_init_lane_config(struct max96724_priv *priv)
{
	unsigned int num_lane_configs = ARRAY_SIZE(max96724_lane_configs);
	struct max_des_priv *des_priv = &priv->des_priv;
	struct max_des_phy *phy;
	unsigned int i, j;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	for (i = 0; i < num_lane_configs; i++) {
		bool matching = true;

		for (j = 0; j < des_priv->ops->num_phys; j++) {
			phy = max_des_phy_by_id(des_priv, j);

			if (!phy->enabled)
				continue;

			if (phy->mipi.num_data_lanes == max96724_lane_configs[i].lanes[j] &&
				phy->mipi.clock_lane == max96724_lane_configs[i].clock_lane[j])
				continue;

			matching = false;
			break;
		}

		if (matching)
			break;
	}

	if (i == num_lane_configs) {
		dev_err(priv->dev, "Invalid lane configuration\n");
		return -EINVAL;
	}

	ret = max96724_update_bits(priv, 0x8a0, 0x1f,
				max96724_lane_configs[i].bit);
	if (ret)
		return ret;

	return 0;
}

static int max96724_reset(struct max96724_priv *priv)
{
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96724_wait_for_device(priv);
	if (ret) {
		dev_err(priv->dev, "Failed waiting for MAX96724, err: %d\n", ret);
		return ret;
	}

	ret = max96724_update_bits(priv, 0x13, 0x40, 0x40);
	if (ret)
		return ret;

	msleep(10);

	if (priv->i2c_addr_change) {
		max96724_change_address(priv);
		if (ret)
			return ret;
	}

	ret = max96724_wait_for_device(priv);
	if (ret) {
		dev_err(priv->dev, "Failed waiting for MAX96724, err: %d\n", ret);
		return ret;
	}

	return 0;
}

static int max96724_check_gmsl_links(struct max_des_priv *des_priv)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	unsigned int locked_links_mask = 0;
	unsigned int links_mask = des_priv->gmsl_link_mask;
	u16 link_lock_addr[4] = {
		0x1a,
		0x0a,
		0x0b,
		0x0c
	};
	unsigned long timeout;

	dev_dbg(priv->dev, "%s()\n", __func__);

	max96724_update_bits(priv, 0x6, GENMASK(3, 0), des_priv->gmsl_link_mask);

	timeout = jiffies + msecs_to_jiffies(100);

	while (!time_after(jiffies, timeout)) {
		int current_link = ffs(links_mask) - 1;

		if (current_link == -1)
			break;

		if ((max96724_read(priv, link_lock_addr[current_link]) & BIT(3)) == BIT(3))
			locked_links_mask |= BIT(current_link);

		links_mask &= ~BIT(current_link);

		if (!links_mask && des_priv->gmsl_link_mask == locked_links_mask)
			break;
		else if (!links_mask)
			links_mask = des_priv->gmsl_link_mask & ~locked_links_mask;

		usleep_range(1000, 2000);
	}

	return locked_links_mask;
}

static int max96724_init(struct max_des_priv *des_priv)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	unsigned int locked_links;
	int retries = 3;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	while (retries--) {
		locked_links = max96724_check_gmsl_links(des_priv);
		if (locked_links == des_priv->gmsl_link_mask)
			break;

		max96724_reset(priv);
		usleep_range(2000, 2500);
	}

	if (locked_links == 0) {
		dev_err(priv->dev, "No GMSL link has locked after 3 retries. Abort!\n");
		return -ENODEV;
	}

	dev_info(priv->dev, "GMSL link has locked - mask [0x%x]\n", locked_links);

	/* Disable all PHYs. */
	ret = max96724_update_bits(priv, 0x8a2, GENMASK(7, 4), 0x00);
	if (ret)
		return ret;

	/* Disable CSI output. */
	ret = max96724_update_bits(priv, 0x40b, 0x02, 0x00);
	if (ret)
		return ret;

	ret = max96724_update_bits(priv, 0xf4, BIT(4),
				des_priv->pipe_stream_autoselect
				? BIT(4) : 0x00);
	if (ret)
		return ret;

	/* Disable all pipes. */
	ret = max96724_update_bits(priv, 0xf4, GENMASK(3, 0), 0x00);
	if (ret)
		return ret;

	/* Set I2C speed to 397Kbps */
	ret = max96724_update_bits(priv, 0x641, GENMASK(6, 4), 0x50);
	ret = max96724_update_bits(priv, 0x651, GENMASK(6, 4), 0x50);
	ret = max96724_update_bits(priv, 0x661, GENMASK(6, 4), 0x50);
	ret = max96724_update_bits(priv, 0x671, GENMASK(6, 4), 0x50);

	ret = max96724_init_lane_config(priv);
	if (ret)
		return ret;

	return 0;
}

static int max96724_init_phy(struct max_des_priv *des_priv,
				struct max_des_phy *phy)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	unsigned int num_data_lanes = phy->mipi.num_data_lanes;
	unsigned int dpll_freq = phy->link_frequency * 2;
	unsigned int num_hw_data_lanes;
	unsigned int reg, val, shift, mask, clk_bit;
	unsigned int index = phy->index;
	unsigned int used_data_lanes = 0;
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);
	/* Configure a lane count. */
	/* TODO: Add support CPHY mode. */
	if (index == 1 && phy->mipi.clock_lane == MAX96724_PHY1_ALT_CLOCK &&
		phy->mipi.num_data_lanes == 2)
		num_hw_data_lanes = 4;
	else
		num_hw_data_lanes = phy->mipi.num_data_lanes;

	reg = 0x90a + 0x40 * index;
	shift = 6;
	mask = GENMASK(1, 0);
	val = num_data_lanes - 1;
	ret = max96724_update_bits(priv, reg, mask << shift, val << shift);
	if (ret)
		return ret;

	/* Configure lane mapping. */
	if (num_hw_data_lanes == 4) {
		mask = 0xff;
		shift = 0;
	} else {
		mask = 0xf;
		shift = 4 * (index % 2);
	}

	reg = 0x8a3 + index / 2;

	val = 0;
	for (i = 0; i < num_hw_data_lanes ; i++) {
		unsigned int map;

		if (i < num_data_lanes)
			map = phy->mipi.data_lanes[i] - 1;
		else
			map = ffz(used_data_lanes);

		val |= (map << (i * 2));
		used_data_lanes |= BIT(map);
	}

	ret = max96724_update_bits(priv, reg, mask << shift, val << shift);
	if (ret)
		return ret;

	/* Configure lane polarity. */
	if (num_hw_data_lanes == 4) {
		mask = 0x3f;
		clk_bit = 5;
		shift = 0;
	} else {
		mask = 0x7;
		clk_bit = 2;
		shift = 4 * (index % 2);
	}

	reg = 0x8a5 + index / 2;

	val = 0;
	for (i = 0; i < num_data_lanes + 1; i++)
		if (phy->mipi.lane_polarities[i])
			val |= BIT(i == 0 ? clk_bit : i < 3 ? i - 1 : i);
	ret = max96724_update_bits(priv, reg, mask << shift, val << shift);
	if (ret)
		return ret;

	if (dpll_freq > 1500000000ull) {
		/* Enable initial deskew with 2 x 32k UI. */
		ret = max96724_write(priv, 0x903 + 0x40 * index, 0x81);
		if (ret)
			return ret;

		/* Enable periodic deskew with 2 x 1k UI.. */
		ret = max96724_write(priv, 0x904 + 0x40 * index, 0x81);
		if (ret)
			return ret;
	} else {
		/* Disable initial deskew. */
		ret = max96724_write(priv, 0x903 + 0x40 * index, 0x07);
		if (ret)
			return ret;

		/* Disable periodic deskew. */
		ret = max96724_write(priv, 0x904 + 0x40 * index, 0x01);
		if (ret)
			return ret;
	}

	/* Put DPLL block into reset. */
	ret = max96724_update_bits(priv, 0x1c00 + 0x100 * index, BIT(0), 0x00);
	if (ret)
		return ret;

	/* Set DPLL frequency. */
	reg = 0x415 + 0x3 * index;
	ret = max96724_update_bits(priv, reg, GENMASK(4, 0),
				div_u64(dpll_freq, 100000000));
	if (ret)
		return ret;

	/* Enable DPLL frequency. */
	ret = max96724_update_bits(priv, reg, BIT(5), BIT(5));
	if (ret)
		return ret;

	/* Pull DPLL block out of reset. */
	reg = 0x1c00 + 0x100 * index;
	ret = max96724_update_bits(priv, reg, BIT(0), 0x01);
	if (ret)
		return ret;

	/* Set alternate memory map modes. */
	val  = phy->alt_mem_map12 ? BIT(0) : 0;
	val |= phy->alt_mem_map8 ? BIT(1) : 0;
	val |= phy->alt_mem_map10 ? BIT(2) : 0;
	val |= phy->alt2_mem_map8 ? BIT(4) : 0;
	reg = 0x933 + 0x40 * index;
	ret = max96724_update_bits(priv, reg, GENMASK(2, 0), val);
	if (ret)
		return ret;

	/* Enable PHY. */
	shift = 4;
	if (num_hw_data_lanes == 4)
		/* PHY 1 -> bits [1:0] */
		/* PHY 2 -> bits [3:2] */
		mask = 0x3 << ((index / 2) * 2 + shift);
	else
		mask = 0x1 << (index + shift);

	ret = max96724_update_bits(priv, 0x8a2, mask, mask);
	if (ret)
		return ret;

	return 0;
}

static int max96724_init_pipe_remap(struct max96724_priv *priv,
					struct max_des_pipe *pipe,
					struct max_des_dt_vc_remap *remap,
					unsigned int i)
{
	unsigned int index = pipe->index;
	unsigned int reg, val, shift, mask;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);
	/* Set source Data Type and Virtual Channel. */
	/* TODO: implement extended Virtual Channel. */
	reg = 0x90d + 0x40 * index + i * 2;
	ret = max96724_write(priv, reg,
				MAX_DES_DT_VC(remap->from_dt, remap->from_vc));
	if (ret)
		return ret;

	/* Set destination Data Type and Virtual Channel. */
	/* TODO: implement extended Virtual Channel. */
	reg = 0x90e + 0x40 * index + i * 2;
	ret = max96724_write(priv, reg,
				MAX_DES_DT_VC(remap->to_dt, remap->to_vc));
	if (ret)
		return ret;

	/* Set destination PHY. */
	reg = 0x92d + 0x40 * index + i / 4;
	shift = (i % 4) * 2;
	mask = 0x3 << shift;
	val = (remap->phy & 0x3) << shift;
	ret = max96724_update_bits(priv, reg, mask, val);
	if (ret)
		return ret;

	/* Enable remap. */
	reg = 0x90b + 0x40 * index + i / 8;
	val = BIT(i % 8);
	ret = max96724_update_bits(priv, reg, val, val);
	if (ret)
		return ret;

	return 0;
}

static int max96724_init_pipe(struct max_des_priv *des_priv,
				struct max_des_pipe *pipe)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	unsigned int index = pipe->index;
	unsigned int reg, shift, mask;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);
	/* Set destination PHY. */
	shift = index * 2;
	ret = max96724_update_bits(priv, 0x8ca, GENMASK(1, 0) << shift,
				pipe->phy_id << shift);
	if (ret)
		return ret;

	shift = 4;
	if (des_priv->links[pipe->link_id].tunnel_mode) {
		reg = 0x936 + 0x40 * index;
		ret = max96724_update_bits(priv, reg, 0x01, 0x01);
		if (ret)
			return ret;

		reg = 0x939 + 0x40 * index;
		ret = max96724_update_bits(priv, reg, 0x40, 0x40);
		if (ret)
			return ret;
	}
	reg = 0x939 + 0x40 * index;
	ret = max96724_update_bits(priv, reg, GENMASK(1, 0) << shift,
				pipe->phy_id << shift);
	if (ret)
		return ret;

	/* Enable pipe. */
	ret = max96724_update_bits(priv, 0xf4, BIT(index), BIT(index));
	if (ret)
		return ret;

	if (!des_priv->pipe_stream_autoselect) {
		/* Set source stream. */
		reg = 0xf0 + index / 2;
		shift = 4 * (index % 2);
		ret = max96724_update_bits(priv, reg, GENMASK(1, 0) << shift,
					pipe->stream_id << shift);
		if (ret)
			return ret;
	}

	/* Set source link. */
	shift += 2;
	ret = max96724_update_bits(priv, reg, GENMASK(1, 0) << shift,
				pipe->link_id << shift);
	if (ret)
		return ret;

	/* Set 8bit double mode. */
	mask = BIT(index) << 4;
	ret = max96724_update_bits(priv, 0x414, mask, pipe->dbl8 ? mask : 0);
	if (ret)
		return ret;

	mask = BIT(index) << 4;
	ret = max96724_update_bits(priv, 0x417, mask, pipe->dbl8mode ? mask : 0);
	if (ret)
		return ret;

	/* Set 10bit double mode. */
	if (index == 3) {
		reg = 0x41d;
		mask = BIT(4);
	} else if (index == 2) {
		reg = 0x41e;
		mask = BIT(6);
	} else if (index == 1) {
		reg = 0x41f;
		mask = BIT(6);
	} else {
		reg = 0x41f;
		mask = BIT(4);
	}

	ret = max96724_update_bits(priv, reg,
				mask | (mask << 1),
				(pipe->dbl10 ? mask : 0) |
				(pipe->dbl10mode ? (mask << 1) : 0));
	if (ret)
		return ret;

	/* Set 12bit double mode. */
	mask = BIT(index);
	ret = max96724_update_bits(priv, 0x41f, mask, pipe->dbl12 ? mask : 0);
	if (ret)
		return ret;

	return 0;
}

static int max96724_init_fsync(struct max_des_priv *des_priv,
				  struct max_des_fsync *fsync)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	int ret = 0;

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (fsync->internal || fsync->internal_output) {
		ret = max96724_write(priv, 0x4b1, 0x00);
		ret += max96724_write(priv, 0x4a2, 0x01);
		ret += max96724_write(priv, 0x4a7, (fsync->freq >> 16) & 0xff);
		ret += max96724_write(priv, 0x4a6, (fsync->freq >> 8) & 0xff);
		ret += max96724_write(priv, 0x4a5, (fsync->freq >> 0) & 0xff);
		ret += max96724_write(priv, 0x4af, 0xcf);
		ret += max96724_write(priv, 0x4a0, fsync->internal_output << 2);
	}
	else if (fsync->external) {
		ret = max96724_write(priv, 0x4a0, fsync->external << 3);
	}

	return ret;
}

static int max96724_update_pipe_remaps(struct max_des_priv *des_priv,
					struct max_des_pipe *pipe)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	for (i = 0; i < pipe->num_remaps; i++) {
		struct max_des_dt_vc_remap *remap = &pipe->remaps[i];

		ret = max96724_init_pipe_remap(priv, pipe, remap, i);
		if (ret)
			return ret;
	}

	return 0;
}

static int max96724_select_links(struct max_des_priv *des_priv,
				unsigned int mask)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96724_update_bits(priv, 0x6, GENMASK(3, 0), mask);

	msleep(60);

	return 0;
}

static int max96724_post_init(struct max_des_priv *des_priv)
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

static const struct max_des_ops max96724_ops = {
	.num_phys = 4,
	.num_pipes = 4,
	.num_links = 4,
	.supports_pipe_link_remap = true,
	.supports_pipe_stream_autoselect = true,
	.supports_tunnel_mode = true,
	.log_pipe_status = max96724_log_pipe_status,
	.log_phy_status = max96724_log_phy_status,
	.mipi_enable = max96724_mipi_enable,
	.init = max96724_init,
	.init_phy = max96724_init_phy,
	.init_pipe = max96724_init_pipe,
	.init_fsync = max96724_init_fsync,
	.update_pipe_remaps = max96724_update_pipe_remaps,
	.select_links = max96724_select_links,
	.post_init = max96724_post_init,
};

#define MAX96724_PIN(n) PINCTRL_PIN(n, "mfp" __stringify(n))

static const struct pinctrl_pin_desc max96724_pins[] = {
	MAX96724_PIN(0), MAX96724_PIN(1), MAX96724_PIN(2),
	MAX96724_PIN(3), MAX96724_PIN(4), MAX96724_PIN(5),
	MAX96724_PIN(6), MAX96724_PIN(7), MAX96724_PIN(8),
};

#define MAX96724_GROUP_PINS(name, ...)                                         \
	static const unsigned int name##_pins[] = { __VA_ARGS__ }

MAX96724_GROUP_PINS(mfp0, 0);
MAX96724_GROUP_PINS(mfp1, 1);
MAX96724_GROUP_PINS(mfp2, 2);
MAX96724_GROUP_PINS(mfp3, 3);
MAX96724_GROUP_PINS(mfp4, 4);
MAX96724_GROUP_PINS(mfp5, 5);
MAX96724_GROUP_PINS(mfp6, 6);
MAX96724_GROUP_PINS(mfp7, 7);
MAX96724_GROUP_PINS(mfp8, 8);

#define MAX96724_GROUP(name)                                                   \
	PINCTRL_PINGROUP(__stringify(name), name##_pins,                       \
			ARRAY_SIZE(name##_pins))

static const struct pingroup max96724_ctrl_groups[] = {
	MAX96724_GROUP(mfp0), MAX96724_GROUP(mfp1), MAX96724_GROUP(mfp2),
	MAX96724_GROUP(mfp3), MAX96724_GROUP(mfp4), MAX96724_GROUP(mfp5),
	MAX96724_GROUP(mfp6), MAX96724_GROUP(mfp7), MAX96724_GROUP(mfp8),
};

#define MAX96724_FUNC_GROUPS(name, ...)                                        \
	static const char *const name##_groups[] = { __VA_ARGS__ }

MAX96724_FUNC_GROUPS(gpio, "mfp0", "mfp1", "mfp2", "mfp3", "mfp4", "mfp5",
			"mfp6", "mfp7", "mfp8");

enum max96724_func {
	max96724_func_gpio,
};

#define MAX96724_FUNC(name)                                                    \
	[max96724_func_##name] = PINCTRL_PINFUNCTION(                          \
		__stringify(name), name##_groups, ARRAY_SIZE(name##_groups))

static const struct pinfunction max96724_functions[] = {
	MAX96724_FUNC(gpio),
};

enum max96724_pinctrl_params {
	MAX96724_PINCTRL_PULL_STRENGTH_WEAK = PIN_CONFIG_END + 1,
	MAX96724_PINCTRL_JITTER_COMPENSATION_EN,
	MAX96724_PINCTRL_GMSL_TX_EN_A,
	MAX96724_PINCTRL_GMSL_RX_EN_A,
	MAX96724_PINCTRL_GMSL_TX_ID_A,
	MAX96724_PINCTRL_GMSL_RX_ID_A,
	MAX96724_PINCTRL_GMSL_TX_EN_B,
	MAX96724_PINCTRL_GMSL_RX_EN_B,
	MAX96724_PINCTRL_GMSL_TX_ID_B,
	MAX96724_PINCTRL_GMSL_RX_ID_B,
	MAX96724_PINCTRL_GMSL_TX_EN_C,
	MAX96724_PINCTRL_GMSL_RX_EN_C,
	MAX96724_PINCTRL_GMSL_TX_ID_C,
	MAX96724_PINCTRL_GMSL_RX_ID_C,
	MAX96724_PINCTRL_GMSL_TX_EN_D,
	MAX96724_PINCTRL_GMSL_RX_EN_D,
	MAX96724_PINCTRL_GMSL_TX_ID_D,
	MAX96724_PINCTRL_GMSL_RX_ID_D,
	MAX96724_PINCTRL_INPUT_VALUE,
};

static const struct pinconf_generic_params max96724_cfg_params[] = {
	{ "maxim,pull-strength-weak", MAX96724_PINCTRL_PULL_STRENGTH_WEAK, 0 },
	{ "maxim,jitter-compensation", MAX96724_PINCTRL_JITTER_COMPENSATION_EN, 0 },
	{ "maxim,gmsl-tx-a", MAX96724_PINCTRL_GMSL_TX_EN_A, 0 },
	{ "maxim,gmsl-rx-a", MAX96724_PINCTRL_GMSL_RX_EN_A, 0 },
	{ "maxim,gmsl-tx-id-a", MAX96724_PINCTRL_GMSL_TX_ID_A, 0 },
	{ "maxim,gmsl-rx-id-a", MAX96724_PINCTRL_GMSL_RX_ID_A, 0 },
	{ "maxim,gmsl-tx-b", MAX96724_PINCTRL_GMSL_TX_EN_B, 0 },
	{ "maxim,gmsl-rx-b", MAX96724_PINCTRL_GMSL_RX_EN_B, 0 },
	{ "maxim,gmsl-tx-id-b", MAX96724_PINCTRL_GMSL_TX_ID_B, 0 },
	{ "maxim,gmsl-rx-id-b", MAX96724_PINCTRL_GMSL_RX_ID_B, 0 },
	{ "maxim,gmsl-tx-c", MAX96724_PINCTRL_GMSL_TX_EN_C, 0 },
	{ "maxim,gmsl-rx-c", MAX96724_PINCTRL_GMSL_RX_EN_C, 0 },
	{ "maxim,gmsl-tx-id-c", MAX96724_PINCTRL_GMSL_TX_ID_C, 0 },
	{ "maxim,gmsl-rx-id-c", MAX96724_PINCTRL_GMSL_RX_ID_C, 0 },
	{ "maxim,gmsl-tx-d", MAX96724_PINCTRL_GMSL_TX_EN_D, 0 },
	{ "maxim,gmsl-rx-d", MAX96724_PINCTRL_GMSL_RX_EN_D, 0 },
	{ "maxim,gmsl-tx-id-d", MAX96724_PINCTRL_GMSL_TX_ID_D, 0 },
	{ "maxim,gmsl-rx-id-d", MAX96724_PINCTRL_GMSL_RX_ID_D, 0 },
};

static int max96724_ctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(max96724_ctrl_groups);
}

static const char *max96724_ctrl_get_group_name(struct pinctrl_dev *pctldev,
						unsigned selector)
{
	return max96724_ctrl_groups[selector].name;
}

static int max96724_ctrl_get_group_pins(struct pinctrl_dev *pctldev,
					unsigned selector,
					const unsigned **pins,
					unsigned *num_pins)
{
	*pins = (unsigned *)max96724_ctrl_groups[selector].pins;
	*num_pins = max96724_ctrl_groups[selector].npins;

	return 0;
}

static int max96724_get_pin_config_reg(unsigned int offset, u32 param,
					unsigned int *reg, unsigned int *mask,
					unsigned int *val)
{
	*reg = 0x300 + offset * 0x3;

	switch (param) {
	case PIN_CONFIG_OUTPUT_ENABLE:
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = BIT(0);
		*val = 0b0;
		return 0;
	case PIN_CONFIG_INPUT_ENABLE:
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = BIT(0);
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_GMSL_TX_EN_A:
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = BIT(1);
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_EN_A:
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = BIT(2);
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_INPUT_VALUE:
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = BIT(3);
		*val = 0b1;
		return 0;
	case PIN_CONFIG_OUTPUT:
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = BIT(4);
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_JITTER_COMPENSATION_EN:
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = BIT(5);
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_PULL_STRENGTH_WEAK:
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = BIT(7);
		*val = 0b0;
		return 0;
	case MAX96724_PINCTRL_GMSL_TX_ID_A:
		*reg += 1;
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = GENMASK(4, 0);
		return 0;
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		*reg += 1;
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = BIT(5);
		*val = 0b0;
		return 0;
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		*reg += 1;
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = BIT(5);
		*val = 0b1;
		return 0;
	case PIN_CONFIG_BIAS_DISABLE:
		*reg += 1;
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = GENMASK(7, 6);
		*val = 0b00;
		return 0;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		*reg += 1;
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = GENMASK(7, 6);
		*val = 0b10;
		return 0;
	case PIN_CONFIG_BIAS_PULL_UP:
		*reg += 1;
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = GENMASK(7, 6);
		*val = 0b01;
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_ID_A:
		*reg += 2;
		if (offset > 4)
			*reg += 1;
		if (offset > 10)
			*reg += 1;
		*mask = GENMASK(4, 0);
		return 0;
	case MAX96724_PINCTRL_GMSL_TX_ID_B:
		*reg += 0x37;
		if (offset > 2)
			*reg += 1;
		if (offset > 7)
			*reg += 1;
		*mask = GENMASK(4, 0);
		return 0;
	case MAX96724_PINCTRL_GMSL_TX_EN_B:
		*reg += 0x37;
		if (offset > 2)
			*reg += 1;
		if (offset > 7)
			*reg += 1;
		*mask = BIT(5);
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_ID_B:
		*reg += 0x38;
		if (offset > 2)
			*reg += 1;
		if (offset > 7)
			*reg += 1;
		*mask = GENMASK(4, 0);
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_EN_B:
		*reg += 0x38;
		if (offset > 2)
			*reg += 1;
		if (offset > 7)
			*reg += 1;
		*mask = BIT(5);
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_GMSL_TX_ID_C:
		*reg += 0x6d;
		if (offset > 0)
			*reg += 1;
		if (offset > 5)
			*reg += 1;
		*mask = GENMASK(4, 0);
		return 0;
	case MAX96724_PINCTRL_GMSL_TX_EN_C:
		*reg += 0x6d;
		if (offset > 0)
			*reg += 1;
		if (offset > 5)
			*reg += 1;
		*mask = BIT(5);
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_ID_C:
		*reg += 0x6e;
		if (offset > 0)
			*reg += 1;
		if (offset > 5)
			*reg += 1;
		*mask = GENMASK(4, 0);
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_EN_C:
		*reg += 0x6e;
		if (offset > 0)
			*reg += 1;
		if (offset > 5)
			*reg += 1;
		*mask = BIT(5);
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_GMSL_TX_ID_D:
		*reg += 0xa4;
		if (offset > 3)
			*reg += 1;
		if (offset > 8)
			*reg += 1;
		*mask = GENMASK(4, 0);
		return 0;
	case MAX96724_PINCTRL_GMSL_TX_EN_D:
		*reg += 0xa4;
		if (offset > 3)
			*reg += 1;
		if (offset > 8)
			*reg += 1;
		*mask = BIT(5);
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_ID_D:
		*reg += 0xa5;
		if (offset > 3)
			*reg += 1;
		if (offset > 8)
			*reg += 1;
		*mask = GENMASK(4, 0);
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_EN_D:
		*reg += 0xa5;
		if (offset > 3)
			*reg += 1;
		if (offset > 8)
			*reg += 1;
		*mask = BIT(5);
		*val = 0b1;
		return 0;
	default:
		return -ENOTSUPP;
	}
}

static int max96724_conf_pin_config_get(struct pinctrl_dev *pctldev,
					unsigned int offset,
					unsigned long *config)
{
	struct max96724_priv *priv = pinctrl_dev_get_drvdata(pctldev);
	u32 param = pinconf_to_config_param(*config);
	unsigned int reg, mask, val;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96724_get_pin_config_reg(offset, param, &reg, &mask, &val);
	if (ret)
		return ret;

	switch (param) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
	case PIN_CONFIG_DRIVE_PUSH_PULL:
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		ret = max96724_read(priv, reg);
		if (ret < 0)
			return ret;

		val = max96724_field_get(ret, mask) == val;
		if (!val)
			return -EINVAL;

		break;
	case MAX96724_PINCTRL_JITTER_COMPENSATION_EN:
	case MAX96724_PINCTRL_PULL_STRENGTH_WEAK:
	case MAX96724_PINCTRL_GMSL_TX_EN_A:
	case MAX96724_PINCTRL_GMSL_TX_EN_B:
	case MAX96724_PINCTRL_GMSL_TX_EN_C:
	case MAX96724_PINCTRL_GMSL_TX_EN_D:
	case MAX96724_PINCTRL_GMSL_RX_EN_A:
	case MAX96724_PINCTRL_GMSL_RX_EN_B:
	case MAX96724_PINCTRL_GMSL_RX_EN_C:
	case MAX96724_PINCTRL_GMSL_RX_EN_D:
	case MAX96724_PINCTRL_INPUT_VALUE:
	case PIN_CONFIG_OUTPUT_ENABLE:
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT:
		ret = max96724_read(priv, reg);
		if (ret < 0)
			return ret;

		val = max96724_field_get(ret, mask) == val;
		break;
	case MAX96724_PINCTRL_GMSL_TX_ID_A:
	case MAX96724_PINCTRL_GMSL_TX_ID_B:
	case MAX96724_PINCTRL_GMSL_TX_ID_C:
	case MAX96724_PINCTRL_GMSL_TX_ID_D:
	case MAX96724_PINCTRL_GMSL_RX_ID_A:
	case MAX96724_PINCTRL_GMSL_RX_ID_B:
	case MAX96724_PINCTRL_GMSL_RX_ID_C:
	case MAX96724_PINCTRL_GMSL_RX_ID_D:
		ret = max96724_read(priv, reg);
		if (ret < 0)
			return ret;

		val = max96724_field_get(val, mask);
		break;
	default:
		return -ENOTSUPP;
	}

	*config = pinconf_to_config_packed(param, val);

	return 0;
}

static int max96724_conf_pin_config_set_one(struct max96724_priv *priv,
						unsigned int offset,
						unsigned long config)
{
	u32 param = pinconf_to_config_param(config);
	u32 arg = pinconf_to_config_argument(config);
	unsigned int reg, mask, val;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96724_get_pin_config_reg(offset, param, &reg, &mask, &val);
	if (ret)
		return ret;

	switch (param) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
	case PIN_CONFIG_DRIVE_PUSH_PULL:
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		val = max96724_field_prep(val, mask);

		ret = max96724_update_bits(priv, reg, mask, val);
		break;
	case MAX96724_PINCTRL_JITTER_COMPENSATION_EN:
	case MAX96724_PINCTRL_PULL_STRENGTH_WEAK:
	case MAX96724_PINCTRL_GMSL_TX_EN_A:
	case MAX96724_PINCTRL_GMSL_TX_EN_B:
	case MAX96724_PINCTRL_GMSL_TX_EN_C:
	case MAX96724_PINCTRL_GMSL_TX_EN_D:
	case MAX96724_PINCTRL_GMSL_RX_EN_A:
	case MAX96724_PINCTRL_GMSL_RX_EN_B:
	case MAX96724_PINCTRL_GMSL_RX_EN_C:
	case MAX96724_PINCTRL_GMSL_RX_EN_D:
	case PIN_CONFIG_OUTPUT_ENABLE:
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT:
		val = max96724_field_prep(arg ? val : ~val, mask);

		ret = max96724_update_bits(priv, reg, mask, val);
		break;
	case MAX96724_PINCTRL_GMSL_TX_ID_A:
	case MAX96724_PINCTRL_GMSL_TX_ID_B:
	case MAX96724_PINCTRL_GMSL_TX_ID_C:
	case MAX96724_PINCTRL_GMSL_TX_ID_D:
	case MAX96724_PINCTRL_GMSL_RX_ID_A:
	case MAX96724_PINCTRL_GMSL_RX_ID_B:
	case MAX96724_PINCTRL_GMSL_RX_ID_C:
	case MAX96724_PINCTRL_GMSL_RX_ID_D:
		val = max96724_field_prep(arg, mask);

		ret = max96724_update_bits(priv, reg, mask, val);
		break;
	default:
		return -ENOTSUPP;
	}

	if (param == PIN_CONFIG_OUTPUT) {
		config = pinconf_to_config_packed(PIN_CONFIG_OUTPUT_ENABLE, 1);
		ret = max96724_conf_pin_config_set_one(priv, offset, config);
		if (ret)
			return ret;
	}

	/* Enable for all links if jitter compensation is enabled*/
	if (param == MAX96724_PINCTRL_JITTER_COMPENSATION_EN) {
		max96724_get_pin_config_reg(offset,
						MAX96724_PINCTRL_GMSL_TX_ID_B, &reg,
						&mask, &val);
		max96724_update_bits(priv, reg, BIT(6), BIT(6));
		max96724_get_pin_config_reg(offset,
						MAX96724_PINCTRL_GMSL_TX_ID_C, &reg,
						&mask, &val);
		max96724_update_bits(priv, reg, BIT(6), BIT(6));
		max96724_get_pin_config_reg(offset,
						MAX96724_PINCTRL_GMSL_TX_ID_D, &reg,
						&mask, &val);
		max96724_update_bits(priv, reg, BIT(6), BIT(6));
	}

	return ret;
}

static int max96724_conf_pin_config_set(struct pinctrl_dev *pctldev,
					unsigned int offset,
					unsigned long *configs,
					unsigned int num_configs)
{
	struct max96724_priv *priv = pinctrl_dev_get_drvdata(pctldev);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	while (num_configs--) {
		unsigned long config = *configs;

		ret = max96724_conf_pin_config_set_one(priv, offset, config);
		if (ret)
			return ret;

		configs++;
	}

	return 0;
}

static int max96724_mux_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(max96724_functions);
}

static const char *max96724_mux_get_function_name(struct pinctrl_dev *pctldev,
						unsigned selector)
{
	return max96724_functions[selector].name;
}

static int max96724_mux_get_groups(struct pinctrl_dev *pctldev,
				unsigned selector,
				const char *const **groups,
				unsigned *const num_groups)
{
	*groups = max96724_functions[selector].groups;
	*num_groups = max96724_functions[selector].ngroups;

	return 0;
}

static int max96724_mux_set(struct pinctrl_dev *pctldev, unsigned selector,
				unsigned group)
{
	return 0;
}

static int max96724_gpio_get_direction(struct gpio_chip *gc,
					unsigned int offset)
{
	unsigned long config =
		pinconf_to_config_packed(PIN_CONFIG_OUTPUT_ENABLE, 0);
	struct max96724_priv *priv = gpiochip_get_data(gc);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96724_conf_pin_config_get(priv->pctldev, offset, &config);
	if (ret)
		return ret;

	return pinconf_to_config_argument(config) ? GPIO_LINE_DIRECTION_OUT :
							GPIO_LINE_DIRECTION_IN;
}

static int max96724_gpio_direction_input(struct gpio_chip *gc,
					unsigned int offset)
{
	unsigned long config =
		pinconf_to_config_packed(PIN_CONFIG_INPUT_ENABLE, 1);
	struct max96724_priv *priv = gpiochip_get_data(gc);

	dev_dbg(priv->dev, "%s()\n", __func__);

	return max96724_conf_pin_config_set_one(priv, offset, config);
}

static int max96724_gpio_direction_output(struct gpio_chip *gc,
					unsigned int offset, int value)
{
	unsigned long config =
		pinconf_to_config_packed(PIN_CONFIG_OUTPUT, value);
	struct max96724_priv *priv = gpiochip_get_data(gc);

	dev_dbg(priv->dev, "%s()\n", __func__);

	return max96724_conf_pin_config_set_one(priv, offset, config);
}

static int max96724_gpio_get(struct gpio_chip *gc, unsigned int offset)
{
	unsigned long config =
		pinconf_to_config_packed(MAX96724_PINCTRL_INPUT_VALUE, 0);
	struct max96724_priv *priv = gpiochip_get_data(gc);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96724_conf_pin_config_get(priv->pctldev, offset, &config);
	if (ret)
		return ret;

	return pinconf_to_config_argument(config);
}

static void max96724_gpio_set(struct gpio_chip *gc, unsigned int offset,
				int value)
{
	unsigned long config =
		pinconf_to_config_packed(PIN_CONFIG_OUTPUT, value);
	struct max96724_priv *priv = gpiochip_get_data(gc);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96724_conf_pin_config_set_one(priv, offset, config);
	if (ret)
		dev_err(priv->dev,
			"Failed to set GPIO %u output value, err: %d\n", offset,
			ret);
}

static struct pinctrl_ops max96724_ctrl_ops = {
	.get_groups_count = max96724_ctrl_get_groups_count,
	.get_group_name = max96724_ctrl_get_group_name,
	.get_group_pins = max96724_ctrl_get_group_pins,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_pin,
	.dt_free_map = pinconf_generic_dt_free_map,
};

static const struct pinconf_ops max96724_conf_ops = {
	.pin_config_get = max96724_conf_pin_config_get,
	.pin_config_set = max96724_conf_pin_config_set,
	.is_generic = true,
};

static const struct pinmux_ops max96724_mux_ops = {
	.get_functions_count = max96724_mux_get_functions_count,
	.get_function_name = max96724_mux_get_function_name,
	.get_function_groups = max96724_mux_get_groups,
	.set_mux = max96724_mux_set,
	.strict = true,
};

static int max96724_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct max96724_priv *priv;
	int ret;

	dev_info(dev, "%s() device node: %s\n", __func__,
		 client->dev.of_node->full_name);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	priv->client = client;
	i2c_set_clientdata(client, priv);

	priv->regmap = devm_regmap_init_i2c(client, &max_des_i2c_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	priv->des_priv.dev = dev;
	priv->des_priv.client = client;
	priv->des_priv.regmap = priv->regmap;
	priv->des_priv.ops = &max96724_ops;

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
	msleep(60);

	priv->i2c_addr_change = false;
	ret = of_property_read_u32(dev->of_node, "phy-reg", &priv->i2c_addr);
	if (!ret) {
		if (priv->i2c_addr != priv->client->addr) {
			priv->i2c_addr_change = true;
			ret = max96724_change_address(priv);
			if (ret)
				return ret;
		}
	}

	ret = max96724_reset(priv);
	if (ret)
		return ret;

	priv->pctldesc = (struct pinctrl_desc){
		.owner = THIS_MODULE,
		.name = MAX96724_NAME,
		.pins = max96724_pins,
		.npins = ARRAY_SIZE(max96724_pins),
		.pctlops = &max96724_ctrl_ops,
		.confops = &max96724_conf_ops,
		.pmxops = &max96724_mux_ops,
		.custom_params = max96724_cfg_params,
		.num_custom_params = ARRAY_SIZE(max96724_cfg_params),
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
		.label = MAX96724_NAME,
		.base = -1,
		.ngpio = MAX96724_GPIO_NUM,
		.parent = dev,
		.can_sleep = true,
		.request = gpiochip_generic_request,
		.free = gpiochip_generic_free,
		.set_config = gpiochip_generic_config,
		.get_direction = max96724_gpio_get_direction,
		.direction_input = max96724_gpio_direction_input,
		.direction_output = max96724_gpio_direction_output,
		.get = max96724_gpio_get,
		.set = max96724_gpio_set,
	};

	ret = devm_gpiochip_add_data(dev, &priv->gc, priv);
	if (ret)
		return ret;

	return max_des_probe(&priv->des_priv);
}

static int max96724_remove(struct i2c_client *client)
{
	struct max96724_priv *priv = i2c_get_clientdata(client);

	return max_des_remove(&priv->des_priv);
}

static const struct of_device_id max96724_of_table[] = {
	{ .compatible = "maxim,max96724" },
	{ },
};
MODULE_DEVICE_TABLE(of, max96724_of_table);

static struct i2c_driver max96724_i2c_driver = {
	.driver	= {
		.name = MAX96724_NAME,
		.of_match_table	= of_match_ptr(max96724_of_table),
	},
	.probe_new = max96724_probe,
	.remove = max96724_remove,
};

module_i2c_driver(max96724_i2c_driver);

MODULE_DESCRIPTION("Maxim MAX96724 Quad GMSL2 Deserializer Driver");
MODULE_AUTHOR("TechNexion Inc.");
MODULE_LICENSE("GPL");