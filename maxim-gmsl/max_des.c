// SPDX-License-Identifier: GPL-2.0
/*
 * Maxim GMSL2 Deserializer Driver
 *
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/regmap.h>

#include "max_des.h"
#include "max_ser.h"
#include "max_serdes.h"

#define MAX_DES_PCLK						25000000ull

#define MAX_DES_LINK_FREQUENCY_MIN			100000000ull
#define MAX_DES_LINK_FREQUENCY_DEFAULT		750000000ull
#define MAX_DES_LINK_FREQUENCY_MAX			1250000000ull

static LIST_HEAD(max_des_registry);
static DEFINE_MUTEX(max_des_registry_lock);

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
	int ret;

	for_each_subdev(priv, sd_priv) {
		if (sd_priv->active) {
			enable = 1;
			break;
		}
	}

	if (enable == priv->active)
		return 0;

	ret = priv->ops->mipi_enable(priv, enable);
	if (ret)
		return ret;

	priv->active = enable;
	return 0;
}

int max_des_ch_enable(struct max_des_subdev_priv *sd_priv, bool enable)
{
	struct max_des_priv *priv = sd_priv->priv;
	struct max_des_pipe *pipe = &priv->pipes[sd_priv->pipe_id];
	int ret = 0;

	mutex_lock(&priv->lock);

	if (sd_priv->active == enable)
		goto exit;

	if (enable) {
		if (priv->ops->set_pipe_enable) {
			ret = priv->ops->set_pipe_enable(priv, pipe, true);
			if (ret)
				goto exit;
		}
		sd_priv->active = true;
		ret = __max_des_mipi_update(priv);
		if (ret) {
			sd_priv->active = false;
			if (priv->ops->set_pipe_enable)
				priv->ops->set_pipe_enable(priv, pipe, false);
		}
	} else {
		sd_priv->active = false;
		ret = __max_des_mipi_update(priv);
		if (ret) {
			sd_priv->active = true;
			goto exit;
		}

		if (priv->ops->set_pipe_enable) {
			ret = priv->ops->set_pipe_enable(priv, pipe, false);
			if (ret) {
				sd_priv->active = true;
				__max_des_mipi_update(priv);
			}
		}
	}

exit:
	mutex_unlock(&priv->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(max_des_ch_enable);

int max_des_ch_enable_by_node(struct device_node *des_np,
			      unsigned int channel, bool enable)
{
	struct max_des_subdev_priv *sd_priv;
	struct max_des_priv *priv;
	int ret = -EPROBE_DEFER;

	if (!des_np)
		return -EINVAL;

	mutex_lock(&max_des_registry_lock);
	list_for_each_entry(priv, &max_des_registry, registry_node) {
		if (priv->dev->of_node != des_np)
			continue;

		ret = -EINVAL;
		for_each_subdev(priv, sd_priv) {
			if (sd_priv->index != channel)
				continue;

			ret = max_des_ch_enable(sd_priv, enable);
			break;
		}
		break;
	}
	mutex_unlock(&max_des_registry_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(max_des_ch_enable_by_node);

int max_des_is_ready_by_node(struct device_node *des_np)
{
	struct max_des_priv *priv;
	int ret = -EPROBE_DEFER;

	if (!des_np)
		return -EINVAL;

	mutex_lock(&max_des_registry_lock);
	list_for_each_entry(priv, &max_des_registry, registry_node) {
		if (priv->dev->of_node != des_np)
			continue;

		ret = 0;
		break;
	}
	mutex_unlock(&max_des_registry_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(max_des_is_ready_by_node);

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

static int max_des_parse_fsync(struct max_des_priv *priv)
{
	struct device *dev = &priv->client->dev;
	char const *fsync_mode;
	u32 fsync_freq = 0;
	int ret;

	if (of_property_read_string(dev->of_node, "fsync-mode", &fsync_mode)) {
		return 0;
	}

	dev_dbg(priv->dev, "%s(): mode: %s\n", __func__, fsync_mode);

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
					   u8 power_up_addr, u8 new_addr,
					   u8 source_id)
{
	u8 addrs[] = { power_up_addr, new_addr };
	struct i2c_client *client;
	struct regmap *regmap;
	int ret;

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
	if (ret) {
		dev_err(priv->dev,
			"Failed to set links mask: %d\n", ret);
		goto err_regmap_exit;
	}

	ret = max_ser_wait_for_multiple(client, regmap, addrs, ARRAY_SIZE(addrs));
	if (ret) {
		dev_err(priv->dev,
			"Failed waiting for serializer with new or old address: %d\n", ret);
		goto err_regmap_exit;
	}

	ret = max_ser_reset(regmap);
	if (ret) {
		dev_err(priv->dev, "Failed to reset serializer: %d\n", ret);
		goto err_regmap_exit;
	}

	ret = max_ser_wait(client, regmap, power_up_addr);
	if (ret) {
		dev_err(priv->dev,
			"Failed waiting for serializer with new address: %d\n", ret);
		goto err_regmap_exit;
	}

	ret = max_ser_change_address(client, regmap, new_addr);
	if (ret) {
		dev_err(priv->dev, "Failed to change serializer address: %d\n", ret);
		goto err_regmap_exit;
	}

	ret = max_ser_wait(client, regmap, new_addr);
	if (ret) {
		dev_err(priv->dev,
			"Failed waiting for serializer with new address: %d\n", ret);
		goto err_regmap_exit;
	}

	if (priv->ops->fix_tx_ids) {
		ret = max_ser_fix_tx_ids(regmap, source_id);
		if (ret) {
			dev_err(priv->dev,
				"Failed to set tx id: %d\n", ret);
			goto err_regmap_exit;
		}
	}

err_regmap_exit:
	regmap_exit(regmap);

err_unregister_client:
	client->addr = power_up_addr;
	i2c_unregister_device(client);

	return ret;
}

static int max_des_parse_i2c_dt(struct max_des_priv *priv)
{
	struct device *dev = &priv->client->dev;
	struct device_node *i2c_mux;
	struct device_node *node = NULL;
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

	i2c_mux = of_find_node_by_name(dev->of_node, "i2c-mux");
	if (!i2c_mux) {
		dev_err(priv->dev, "Failed to find i2c-mux node\n");
		return -EINVAL;
	}

	/* Identify which i2c-mux channels are enabled */
	for_each_child_of_node(i2c_mux, node) {
		u32 id = 0;

		of_property_read_u32(node, "reg", &id);
		if (id >= priv->ops->num_links)
			continue;

		if (!of_device_is_available(node)) {
			dev_dbg(priv->dev, "Skipping disabled I2C bus port %u\n", id);
			continue;
		}
	}
	of_node_put(node);
	of_node_put(i2c_mux);


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
		dev_dbg(priv->dev, "%s(): find not property of alias map\n", __func__);
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

		priv->gmsl_link_mask |= BIT(i);
		priv->gmsl_links_used++;
	}

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

	pipe->code_name = "UYVY8_1X16";
	of_property_read_string(node, "dt", &pipe->code_name);
	dev_dbg(priv->dev, "%s(): format = %s\n", __func__, pipe->code_name);

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
	struct max_des_subdev_priv *sd_priv;
	struct device_node *node;
	struct max_des_link *link;
	struct max_des_pipe *pipe;
	struct max_des_phy *phy;
	unsigned int i;
	u32 index;
	u32 val;
	int ret;

	val = device_property_read_bool(priv->dev, "maxim,pipe-stream-autoselect");
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

	ret = max_des_parse_i2c_dt(priv);
	if (ret)
		return ret;

	return max_des_parse_fsync(priv);
}

int max_des_update_pipe_remaps(struct max_des_priv *priv,
				      struct max_des_pipe *pipe)
{
	struct max_des_link *link = &priv->links[pipe->link_id];
	struct max_des_subdev_priv *sd_priv;
	unsigned int i;

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
EXPORT_SYMBOL_GPL(max_des_update_pipe_remaps);

static int max_des_init(struct max_des_priv *priv)
{
	unsigned int i;
	int ret;

	ret = __max_des_mipi_update(priv);
	if (ret)
		return ret;

	ret = priv->ops->init(priv);
	if (ret)
		return ret;

	for (i = 0; i < priv->ops->num_phys; i++) {
		struct max_des_phy *phy = &priv->phys[i];

		dev_dbg(priv->dev, "%s(): phy [%d]: %s\n",
			__func__, phy->index,
			phy->enabled ? "enabled" : "disabled");

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
		struct max_des_link *link = &priv->links[pipe->link_id];

		dev_dbg(priv->dev, "%s(): pipe [%d]: %s\n",
			__func__, pipe->index,
			pipe->enabled ? "enabled" : "disabled");

		if (!pipe->enabled)
			continue;

		dev_dbg(priv->dev, "%s(): pipe link [%d]: %s\n",
		__func__, pipe->link_id,
		link->enabled ? "enabled" : "disabled");

		if (!link->enabled) {
			continue;
		}

		ret = priv->ops->init_pipe(priv, pipe);
		if (ret)
			return ret;

		ret = max_des_update_pipe_remaps(priv, pipe);
		if (ret)
			return ret;

		dev_info(priv->dev, "pipe [%d] -> link [%d]: %s mode",
			pipe->index,
			pipe->link_id,
			priv->links[pipe->link_id].tunnel_mode ? "tunnel" : "pixel");
	}

	if (priv->ops->init_link) {
		for (i = 0; i < priv->ops->num_links; i++) {
			struct max_des_link *link = &priv->links[i];

			dev_dbg(priv->dev, "%s(): link [%d]: %s\n",
				__func__, link->index,
				link->enabled ? "enabled" : "disabled");

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

static void max_des_i2c_mux_deinit(struct max_des_priv *priv)
{
	if (!priv->mux)
		return;

	i2c_mux_del_adapters(priv->mux);
	priv->mux = NULL;
}

static int max_des_i2c_mux_select(struct i2c_mux_core *muxc, u32 chan)
{
	return 0;
}

static int max_des_i2c_mux_init(struct max_des_priv *priv)
{
	struct max_des_subdev_priv *sd_priv;
	int ret;

	if (!i2c_check_functionality(priv->client->adapter, I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
		return -ENODEV;

	priv->mux = i2c_mux_alloc(priv->client->adapter, &priv->client->dev,
				  priv->gmsl_links_used, 0, I2C_MUX_LOCKED,
				  max_des_i2c_mux_select, NULL);
	if (!priv->mux)
		return -ENOMEM;

	priv->mux->priv = priv;

	for_each_subdev(priv, sd_priv) {
		struct max_des_pipe *pipe = &priv->pipes[sd_priv->pipe_id];
		struct max_des_link *link = &priv->links[pipe->link_id];

		if (!link->enabled)
			continue;

		ret = i2c_mux_add_adapter(priv->mux, 0, link->index, 0);
		if (ret)
			goto err_add_adapters;
	}

	return 0;

err_add_adapters:
	max_des_i2c_mux_deinit(priv);

	return ret;
}

static int max_des_i2c_adapter_init(struct max_des_priv *priv)
{
	return max_des_i2c_mux_init(priv);
}

static void max_des_i2c_adapter_deinit(struct max_des_priv *priv)
{
	return max_des_i2c_mux_deinit(priv);
}

static int max_des_post_init(struct max_des_priv *priv)
{
	unsigned int i, mask = 0;
	int ret;

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

int max_des_probe(struct max_des_priv *priv)
{
	int ret;

	mutex_init(&priv->lock);
	INIT_LIST_HEAD(&priv->registry_node);

	ret = max_des_allocate(priv);
	if (ret)
		return ret;

	ret = max_des_parse_dt(priv);
	if (ret)
		return ret;

	ret = max_des_init(priv);
	if (ret)
		return ret;

	ret = max_des_i2c_adapter_init(priv);
	if (ret)
		return ret;

	ret = max_des_post_init(priv);
	if (ret)
		goto err_i2c_adapter_deinit;

	mutex_lock(&max_des_registry_lock);
	list_add_tail(&priv->registry_node, &max_des_registry);
	mutex_unlock(&max_des_registry_lock);

	dev_info(priv->dev, "probe successful\n");
	return 0;

err_i2c_adapter_deinit:
	max_des_i2c_adapter_deinit(priv);

	return ret;
}
EXPORT_SYMBOL_GPL(max_des_probe);

int max_des_remove(struct max_des_priv *priv)
{
	mutex_lock(&max_des_registry_lock);
	if (!list_empty(&priv->registry_node))
		list_del_init(&priv->registry_node);
	mutex_unlock(&max_des_registry_lock);

	max_des_i2c_adapter_deinit(priv);

	return 0;
}
EXPORT_SYMBOL_GPL(max_des_remove);

MODULE_LICENSE("GPL");
