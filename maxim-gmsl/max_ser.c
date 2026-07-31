// SPDX-License-Identifier: GPL-2.0
/*
 * Maxim GMSL2 Serializer Driver
 *
 */

#include "max_ser.h"

#include <linux/delay.h>
#include <linux/module.h>

#include "max_ser.h"
#include "max_serdes.h"

const struct regmap_config max_ser_i2c_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0x1f00,
};
EXPORT_SYMBOL_GPL(max_ser_i2c_regmap);

static LIST_HEAD(max_ser_registry);
static DEFINE_MUTEX(max_ser_registry_lock);

static struct max_ser_subdev_priv *next_subdev(struct max_ser_priv *priv,
						struct max_ser_subdev_priv *sd_priv)
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

static inline struct max_ser_subdev_priv *sd_to_max_ser(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max_ser_subdev_priv, sd);
}

static int max_ser_allocate(struct max_ser_priv *priv)
{
	unsigned int i;

	priv->phys = devm_kcalloc(priv->dev, priv->ops->num_phys,
				  sizeof(*priv->phys), GFP_KERNEL);
	if (!priv->phys)
		return -ENOMEM;

	priv->pipes = devm_kcalloc(priv->dev, priv->ops->num_pipes,
				   sizeof(*priv->pipes), GFP_KERNEL);
	if (!priv->pipes)
		return -ENOMEM;

	priv->i2c_xlates = devm_kcalloc(priv->dev, priv->ops->num_i2c_xlates,
					sizeof(*priv->i2c_xlates), GFP_KERNEL);
	if (!priv->i2c_xlates)
		return -ENOMEM;

	for (i = 0; i < priv->ops->num_pipes; i++) {
		struct max_ser_pipe *pipe = &priv->pipes[i];

		pipe->dts = devm_kcalloc(priv->dev, priv->ops->num_dts_per_pipe,
					 sizeof(*pipe->dts), GFP_KERNEL);
	}

	return 0;
}

static int max_ser_parse_i2c_dt(struct max_ser_priv *priv)
{
	struct device *dev = &priv->client->dev;
	struct device_node *i2c_mux;
	struct device_node *node = NULL;
	struct property *local;
	struct property *remote;
	int ret, i;
	u32 local_addr;
	u32 remote_addr;
	struct max_i2c_xlate *xlate;

	i2c_mux = of_find_node_by_name(dev->of_node, "i2c-mux");
	if (!i2c_mux) {
		dev_dbg(priv->dev, "find not property of i2c-mux\n");
		return 0;
	}

	/* Identify which i2c-mux channels are enabled */
	for_each_child_of_node(i2c_mux, node) {
		u32 id = 0;

		of_property_read_u32(node, "reg", &id);
		if (id >= priv->ops->num_pipes)
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

	if (local == NULL || remote == NULL) {
		dev_dbg(priv->dev, "find not property of alias map\n");
		return 0;
	}

	for (i = 0; i < priv->ops->num_i2c_xlates; i++) {
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

		xlate = &priv->i2c_xlates[i];
		xlate->src = (u8)(local_addr & 0x7f);
		xlate->dst = (u8)(remote_addr & 0x7f);

		dev_info(priv->dev, "i2c address alias "
			"index: %d local: 0x%x remote: 0x%x\n",
			i, xlate->dst, xlate->src);

		ret = priv->ops->init_i2c_xlate(priv, i);
		if (ret != 0)
			break;
	}

	return 0;
}

static int max_ser_parse_ch_dt(struct max_ser_subdev_priv *sd_priv,
			       struct device_node *node)
{
	struct max_ser_priv *priv = sd_priv->priv;
	struct max_ser_pipe *pipe;
	struct max_ser_phy *phy;
	u32 val;
	struct v4l2_fwnode_endpoint v4l2_ep = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	struct v4l2_fwnode_bus_mipi_csi2 *mipi = &v4l2_ep.bus.mipi_csi2;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	of_property_read_string(node, "label", &sd_priv->label);

	val = sd_priv->pipe_id;
	of_property_read_u32(node, "maxim,pipe-id", &val);
	if (val >= priv->ops->num_pipes) {
		dev_err(priv->dev, "Invalid pipe %u\n", val);
		return -EINVAL;
	}
	sd_priv->pipe_id = val;

	val = 0;
	of_property_read_u32(node, "maxim,vc-id", &val);
	if (val >= MAX_SERDES_VC_ID_NUM) {
		dev_err(priv->dev, "Invalid virtual channel %u\n", val);
		return -EINVAL;
	}
	sd_priv->vc_id = val;

	if (of_property_read_bool(node, "maxim,embedded-data"))
		sd_priv->fmt = max_format_by_dt(MAX_DT_EMB8);

	pipe = &priv->pipes[val];
	pipe->enabled = true;

	phy = &priv->phys[pipe->phy_id];
	phy->enabled = true;

	if (v4l2_fwnode_endpoint_parse(of_fwnode_handle(node), &v4l2_ep)) {
		dev_err(priv->dev, "Could not parse v4l2 endpoint\n");
		return ret;
	}

	if (mipi->flags & V4L2_MBUS_CSI2_NONCONTINUOUS_CLOCK &&
		!priv->ops->supports_noncontinuous_clock) {
		dev_err(priv->dev, "Clock non-continuous mode is not supported\n");
		return -EINVAL;
	}

	dev_info(priv->dev, "num_data_lanes [%d], clock lane [%d]\n",
		mipi->num_data_lanes, mipi->clock_lane);

	if (!phy->bus_config_parsed) {
		phy->mipi = v4l2_ep.bus.mipi_csi2;
		phy->bus_config_parsed = true;

		return 0;
	}

	v4l2_fwnode_endpoint_free(&v4l2_ep);

	if (phy->mipi.num_data_lanes != mipi->num_data_lanes) {
		dev_err(priv->dev, "PHY configured with differing number of data lanes\n");
		return -EINVAL;
	}

	if ((phy->mipi.flags & V4L2_MBUS_CSI2_NONCONTINUOUS_CLOCK) !=
		(mipi->flags & V4L2_MBUS_CSI2_NONCONTINUOUS_CLOCK)) {
		dev_err(priv->dev, "PHY configured with differing clock continuity\n");
		return -EINVAL;
	}

	return 0;
}

static int max_ser_parse_pipe_dt(struct max_ser_priv *priv,
				 struct max_ser_pipe *pipe,
				 struct device_node *node)
{
	unsigned int val;

	val = pipe->phy_id;
	of_property_read_u32(node, "maxim,phy-id", &val);
	if (val >= priv->ops->num_phys) {
		dev_err(priv->dev, "Invalid PHY %u\n", val);
		return -EINVAL;
	}
	pipe->phy_id = val;

	val = pipe->stream_id;
	of_property_read_u32(node, "maxim,stream-id", &val);
	if (val >= MAX_SERDES_STREAMS_NUM) {
		dev_err(priv->dev, "Invalid stream %u\n", val);
		return -EINVAL;
	}
	pipe->stream_id = val;

	val = 0;
	of_property_read_u32(node, "maxim,soft-bpp", &val);
	if (val > 24) {
		dev_err(priv->dev, "Invalid soft bpp %u\n", val);
		return -EINVAL;
	}
	pipe->soft_bpp = val;

	val = 0;
	of_property_read_u32(node, "maxim,bpp", &val);
	if (val > 24) {
		dev_err(priv->dev, "Invalid bpp %u\n", val);
		return -EINVAL;
	}
	pipe->bpp = val;

	pipe->dbl8 = of_property_read_bool(node, "maxim,dbl8");
	pipe->dbl10 = of_property_read_bool(node, "maxim,dbl10");
	pipe->dbl12 = of_property_read_bool(node, "maxim,dbl12");

	pipe->code_name = "UYVY8_1X16";
	of_property_read_string(node, "dt", &pipe->code_name);
	dev_dbg(priv->dev, "format = %s\n", pipe->code_name);

	return 0;
}

static int max_ser_parse_dt(struct max_ser_priv *priv)
{
	const char *channel_node_name = "channel";
	const char *pipe_node_name = "pipe";
	struct max_ser_subdev_priv *sd_priv;
	struct device_node *node;
	struct max_ser_pipe *pipe;
	struct max_ser_phy *phy;
	unsigned int i;
	u32 index;
	u32 val;
	int ret;

	val = device_property_read_bool(priv->dev, "maxim,tunnel-mode");
	if (val && !priv->ops->supports_tunnel_mode) {
		dev_err(priv->dev, "Tunnel mode is not supported\n");
		return -EINVAL;
	}
	priv->tunnel_mode = val;

	for (i = 0; i < priv->ops->num_phys; i++) {
		phy = &priv->phys[i];
		phy->index = i;
	}

	for (i = 0; i < priv->ops->num_pipes; i++) {
		pipe = &priv->pipes[i];
		pipe->index = i;
		pipe->phy_id = i % priv->ops->num_phys;
		pipe->stream_id = i % MAX_SERDES_STREAMS_NUM;
	}

	for_each_child_of_node(priv->dev->of_node, node) {
		if (of_node_name_eq(node, pipe_node_name)) {
			ret = of_property_read_u32(node, "reg", &index);
			if (ret) {
				dev_err(priv->dev, "Failed to read reg: %d\n", ret);
				continue;
			}

			if (index >= priv->ops->num_pipes) {
				dev_err(priv->dev, "Invalid pipe number %u\n", index);
				of_node_put(node);
				return -EINVAL;
			}

			pipe = &priv->pipes[index];

			ret = max_ser_parse_pipe_dt(priv, pipe, node);
			if (ret) {
				of_node_put(node);
				return ret;
			}
		}

		if (of_node_name_eq(node, channel_node_name)) {
			ret = of_property_read_u32(node, "reg", &index);
			if (ret) {
				dev_err(priv->dev, "Failed to read reg: %d\n", ret);
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
				dev_err(priv->dev, "Failed to read reg: %d\n", ret);
				continue;
			}

			sd_priv = &priv->sd_privs[i++];
			sd_priv->node = node;
			sd_priv->priv = priv;
			sd_priv->index = index;
			sd_priv->pipe_id = index % priv->ops->num_pipes;

			ret = max_ser_parse_ch_dt(sd_priv, node);
			if (ret) {
				of_node_put(node);
				return ret;
			}
		}
	}

	return max_ser_parse_i2c_dt(priv);
}

int max_ser_update_pipe_dts(struct max_ser_priv *priv,
				   struct max_ser_pipe *pipe)
{
	struct max_ser_subdev_priv *sd_priv;

	pipe->num_dts = 0;

	if (priv->tunnel_mode)
		return 0;

	for_each_subdev(priv, sd_priv) {
		if (sd_priv->pipe_id != pipe->index)
			continue;

		if (pipe->num_dts == priv->ops->num_dts_per_pipe) {
			dev_err(priv->dev, "Too many data types per pipe\n");
			return -EINVAL;
		}

		if (!sd_priv->fmt)
			continue;

		/* TODO: optimize by checking for existing filters. */
		pipe->dts[pipe->num_dts++] = sd_priv->fmt->dt;
	}

	return priv->ops->update_pipe_dts(priv, pipe);
}
EXPORT_SYMBOL_GPL(max_ser_update_pipe_dts);

static int max_ser_update_pipe_vcs(struct max_ser_priv *priv,
				   struct max_ser_pipe *pipe)
{
	struct max_ser_subdev_priv *sd_priv;

	pipe->vcs = 0;

	if (priv->tunnel_mode)
		return 0;

	for_each_subdev(priv, sd_priv) {
		if (sd_priv->pipe_id != pipe->index)
			continue;

		pipe->vcs |= BIT(sd_priv->vc_id);
	}

	return priv->ops->update_pipe_vcs(priv, pipe);
}

static int max_ser_init(struct max_ser_priv *priv)
{
	unsigned int i;
	int ret;

	ret = priv->ops->init(priv);
	if (ret)
		return ret;

	for (i = 0; i < priv->ops->num_phys; i++) {
		struct max_ser_phy *phy = &priv->phys[i];

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
		struct max_ser_pipe *pipe = &priv->pipes[i];

		if (!pipe->enabled)
			continue;

		ret = priv->ops->init_pipe(priv, pipe);
		if (ret)
			return ret;

		ret = max_ser_update_pipe_vcs(priv, pipe);
		if (ret)
			return ret;

		ret = max_ser_update_pipe_dts(priv, pipe);
		if (ret)
			return ret;
	}

	ret = priv->ops->post_init(priv);
	if (ret)
		return ret;

	return 0;
}

static int max_ser_i2c_mux_select(struct i2c_mux_core *mux, u32 chan)
{
	return 0;
}

static int max_ser_i2c_mux_init(struct max_ser_priv *priv)
{
	priv->mux = i2c_mux_alloc(priv->client->adapter, &priv->client->dev,
				1, 0, I2C_MUX_LOCKED,
				max_ser_i2c_mux_select, NULL);
	if (!priv->mux)
		return -ENOMEM;

	priv->mux->priv = priv;

	return i2c_mux_add_adapter(priv->mux, 0, 0, 0);
}

static void max_ser_i2c_mux_deinit(struct max_ser_priv *priv)
{
	if (!priv->mux)
		return;

	i2c_mux_del_adapters(priv->mux);
	priv->mux = NULL;
}

static int max_ser_i2c_adapter_init(struct max_ser_priv *priv)
{
	return max_ser_i2c_mux_init(priv);
}

static void max_ser_i2c_adapter_deinit(struct max_ser_priv *priv)
{
	return max_ser_i2c_mux_deinit(priv);
}

static int max_ser_update_pipe_active(struct max_ser_priv *priv,
				      struct max_ser_pipe *pipe)
{
	struct max_ser_subdev_priv *sd_priv;
	bool enable = 0;
	int ret;

	for_each_subdev(priv, sd_priv) {
		if (sd_priv->pipe_id == pipe->index && sd_priv->active) {
			enable = 1;
			break;
		}
	}

	if (enable == pipe->active)
		return 0;

	ret = priv->ops->set_pipe_enable(priv, pipe, enable);
	if (ret)
		return ret;

	pipe->active = enable;

	return 0;
}

int max_ser_ch_enable(struct max_ser_subdev_priv *sd_priv, bool enable)
{
	struct max_ser_priv *priv = sd_priv->priv;
	struct max_ser_pipe *pipe = max_ser_ch_pipe(sd_priv);
	int ret = 0;

	mutex_lock(&priv->lock);

	if (sd_priv->active == enable)
		goto exit;

	sd_priv->active = enable;

	ret = max_ser_update_pipe_active(priv, pipe);
	if (ret)
		sd_priv->active = !enable;

exit:
	mutex_unlock(&priv->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(max_ser_ch_enable);

int max_ser_ch_enable_by_node(struct device_node *ser_np,
			      unsigned int channel, bool enable)
{
	struct max_ser_subdev_priv *sd_priv;
	struct max_ser_priv *priv;
	int ret = -EPROBE_DEFER;

	if (!ser_np)
		return -EINVAL;

	mutex_lock(&max_ser_registry_lock);
	list_for_each_entry(priv, &max_ser_registry, registry_node) {
		if (priv->dev->of_node != ser_np)
			continue;

		ret = -EINVAL;
		for_each_subdev(priv, sd_priv) {
			if (sd_priv->index != channel)
				continue;

			ret = max_ser_ch_enable(sd_priv, enable);
			break;
		}
		break;
	}
	mutex_unlock(&max_ser_registry_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(max_ser_ch_enable_by_node);

int max_ser_is_ready_by_node(struct device_node *ser_np)
{
	struct max_ser_priv *priv;
	int ret = -EPROBE_DEFER;

	if (!ser_np)
		return -EINVAL;

	mutex_lock(&max_ser_registry_lock);
	list_for_each_entry(priv, &max_ser_registry, registry_node) {
		if (priv->dev->of_node != ser_np)
			continue;

		ret = 0;
		break;
	}
	mutex_unlock(&max_ser_registry_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(max_ser_is_ready_by_node);

int max_ser_probe(struct max_ser_priv *priv)
{
	int ret;

	mutex_init(&priv->lock);
	INIT_LIST_HEAD(&priv->registry_node);

	ret = max_ser_allocate(priv);
	if (ret)
		return ret;

	ret = max_ser_parse_dt(priv);
	if (ret)
		return ret;

	ret = max_ser_init(priv);
	if (ret)
		return ret;

	ret = max_ser_i2c_adapter_init(priv);
	if (ret)
		return ret;

	mutex_lock(&max_ser_registry_lock);
	list_add_tail(&priv->registry_node, &max_ser_registry);
	mutex_unlock(&max_ser_registry_lock);

	dev_info(priv->dev, "probe successful\n");
	return 0;
}
EXPORT_SYMBOL_GPL(max_ser_probe);

int max_ser_remove(struct max_ser_priv *priv)
{
	mutex_lock(&max_ser_registry_lock);
	if (!list_empty(&priv->registry_node))
		list_del_init(&priv->registry_node);
	mutex_unlock(&max_ser_registry_lock);

	max_ser_i2c_adapter_deinit(priv);

	return 0;
}
EXPORT_SYMBOL_GPL(max_ser_remove);

int max_ser_reset(struct regmap *regmap)
{
	int ret;

	ret = regmap_write(regmap, MAX_SER_CTRL0, MAX_SER_CTRL0_RESET_ALL);
	if (ret)
		return ret;

	msleep(50);

	return 0;
}
EXPORT_SYMBOL_GPL(max_ser_reset);

int max_ser_wait_for_multiple(struct i2c_client *client, struct regmap *regmap,
				   u8 *addrs, unsigned int num_addrs)
{
	unsigned int i, j, val;
	int ret;

	for (i = 0; i < 10; i++) {
		for (j = 0; j < num_addrs; j++) {
			client->addr = addrs[j];

			ret = regmap_read(regmap, MAX_SER_REG0, &val);
			if (ret >= 0)
				return 0;
		}

		msleep(100);

		dev_err(&client->dev, "Retry %u waiting for serializer: %d\n", i, ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(max_ser_wait_for_multiple);

int max_ser_wait(struct i2c_client *client, struct regmap *regmap, u8 addr)
{
	return max_ser_wait_for_multiple(client, regmap, &addr, 1);
}
EXPORT_SYMBOL_GPL(max_ser_wait);

int max_ser_change_address(struct i2c_client *client, struct regmap *regmap, u8 addr)
{
	int ret;

	ret = regmap_write(regmap, MAX_SER_REG0,
				   FIELD_PREP(MAX_SER_REG0_DEV_ADDR, addr));
	if (ret)
		return ret;

	client->addr = addr;

	return 0;
}
EXPORT_SYMBOL_GPL(max_ser_change_address);

int max_ser_fix_tx_ids(struct regmap *regmap, u8 addr)
{
	unsigned int addr_regs[] = {
		MAX_SER_CFGI_INFOFR_TR3,
		MAX_SER_CFGL_SPI_TR3,
		MAX_SER_CFGL_GPIO_TR3,
		MAX_SER_CFGL_IIC_X_TR3,
		MAX_SER_CFGL_IIC_Y_TR3,
	};
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(addr_regs); i++) {
		ret = regmap_update_bits(regmap, addr_regs[i],
					   MAX_SER_CFGI_TX_SRC_ID, addr);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(max_ser_fix_tx_ids);

MODULE_LICENSE("GPL");
