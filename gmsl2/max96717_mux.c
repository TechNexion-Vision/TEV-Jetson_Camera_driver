// SPDX-License-Identifier: GPL-2.0
/*
* Maxim MAX96717 GMSL2 Serializer Driver
*
*/

#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include <linux/regmap.h>
#include <linux/gpio/driver.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>

#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

#include "max_ser.h"
#include "max_serdes.h"

#define MAX96717_NAME				"max96717"
#define MAX96717_PINCTRL_NAME		MAX96717_NAME "-pinctrl"
#define MAX96717_GPIOCHIP_NAME		MAX96717_NAME "-gpiochip"
#define MAX96717_GPIO_NUM			(11)
#define MAX96717_PIPES_NUM			(4)
#define MAX96717_PHYS_NUM			(2)
#define MAX96717_LANE_CONFIGS_NUM	(4)

struct max96717_priv {
	struct max_ser_priv ser_priv;
	const struct max96717_chip_info *info;

	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;

	struct pinctrl_dev *pctldev;
	struct pinctrl_desc pctldesc;
	struct gpio_chip gc;
};

struct max96717_chip_info {
	bool supports_tunnel_mode;
	bool supports_noncontinuous_clock;
	bool supports_pkt_cnt;
	unsigned int num_pipes;
	unsigned int num_dts_per_pipe;
	unsigned int pipe_hw_ids[MAX96717_PIPES_NUM];
	unsigned int num_phys;
	unsigned int phy_hw_ids[MAX96717_PHYS_NUM];
	unsigned int num_lane_configs;
	unsigned int lane_configs[MAX96717_LANE_CONFIGS_NUM][MAX96717_PHYS_NUM];
	unsigned int phy_configs[MAX96717_LANE_CONFIGS_NUM];
};

#define ser_to_priv(ser) \
	container_of(ser, struct max96717_priv, ser_priv)

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

static int max_ser_update_pipe_active(struct max_ser_priv *priv,
					struct max_ser_pipe *pipe)
{
	struct max_ser_subdev_priv *sd_priv;
	bool enable = 0;

	dev_dbg(priv->dev, "%s()\n", __func__);

	for_each_subdev(priv, sd_priv) {
		if (sd_priv->pipe_id == pipe->index && sd_priv->active) {
			enable = 1;
			break;
		}
	}

	if (enable == pipe->active)
		return 0;

	pipe->active = enable;

	return priv->ops->set_pipe_enable(priv, pipe, enable);
}

static int max_ser_ch_enable(struct max_ser_subdev_priv *sd_priv, bool enable)
{
	struct max_ser_priv *priv = sd_priv->priv;
	struct max_ser_pipe *pipe = max_ser_ch_pipe(sd_priv);
	int ret = 0;

	dev_dbg(priv->dev, "%s()\n", __func__);

	mutex_lock(&priv->lock);

	if (sd_priv->active == enable)
		goto exit;

	sd_priv->active = enable;

	ret = max_ser_update_pipe_active(priv, pipe);

exit:
	mutex_unlock(&priv->lock);

	return ret;
}

static int max_ser_i2c_mux_select(struct i2c_mux_core *muxc, u32 chan)
{
	return 0;
}

static int max_ser_i2c_mux_init(struct max_ser_priv *priv)
{
	struct device_node *i2c_mux;
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	i2c_mux = of_find_node_by_name(priv->dev->of_node, "i2c-mux");
	if (!i2c_mux) {
		dev_dbg(priv->dev, "find not property of i2c-mux\n");
		return 0;
	}

	if (!i2c_check_functionality(priv->client->adapter, I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
		return -ENODEV;

	priv->mux = i2c_mux_alloc(priv->client->adapter, &priv->client->dev,
				priv->ops->num_pipes, 0, I2C_MUX_LOCKED,
				max_ser_i2c_mux_select, NULL);
	if (!priv->mux)
		return -ENOMEM;

	priv->mux->priv = priv;

	for (i = 0; i < priv->ops->num_pipes; i++) {
		struct max_ser_pipe *pipe = &priv->pipes[i];

		if (!pipe->enabled)
			continue;

		ret = i2c_mux_add_adapter(priv->mux, 0, pipe->index, 0);
		if (ret)
			goto err_add_adapters;
	}

	return 0;

err_add_adapters:
	i2c_mux_del_adapters(priv->mux);
	return ret;
}

static int max_ser_update_pipe_dts(struct max_ser_priv *priv,
				struct max_ser_pipe *pipe)
{
	struct max_ser_subdev_priv *sd_priv;

	dev_dbg(priv->dev, "%s()\n", __func__);

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

static int max_ser_update_pipe_vcs(struct max_ser_priv *priv,
				struct max_ser_pipe *pipe)
{
	struct max_ser_subdev_priv *sd_priv;

	dev_dbg(priv->dev, "%s()\n", __func__);

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

	dev_dbg(priv->dev, "%s()\n", __func__);

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

static int max_ser_parse_xlate(struct max_ser_priv *priv)
{
	struct property *local;
	struct property *remote;
	int ret;
	u32 local_addr;
	u32 remote_addr;
	struct max_i2c_xlate *xlate;

	dev_dbg(priv->dev, "%s()\n", __func__);

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

	ret = 0;
	while(ret == 0 && priv->num_i2c_xlates < priv->ops->num_i2c_xlates) {
		ret = of_property_read_u32_index(priv->dev->of_node,
						"i2c-addr-alias-map-local",
						priv->num_i2c_xlates,
						&local_addr);
		if (ret != 0 || local_addr > 0x7f)
			break;

		ret = of_property_read_u32_index(priv->dev->of_node,
						"i2c-addr-alias-map-remote",
						priv->num_i2c_xlates,
						&remote_addr);
		if (ret != 0 || remote_addr > 0x7f)
			break;

		xlate = &priv->i2c_xlates[priv->num_i2c_xlates++];
		xlate->src = (u8)(local_addr & 0x7f);
		xlate->dst = (u8)(remote_addr & 0x7f);

		dev_info(priv->dev, "i2c address alias "
			"index: %d local: 0x%x remote: 0x%x\n",
			priv->num_i2c_xlates, xlate->dst, xlate->src);

		ret = priv->ops->init_i2c_xlate(priv);

		if (ret != 0)
			break;
	}

	return 0;
}

static int max_ser_parse_i2c_dt(struct max_ser_priv *priv)
{
	struct device *dev = &priv->client->dev;
	struct device_node *i2c_mux;
	struct device_node *node = NULL;

	dev_dbg(priv->dev, "%s()\n", __func__);

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
	of_node_put(dev->of_node);

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

	dev_dbg(priv->dev, "%s()\n", __func__);

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

	return 0;
}

static int max_ser_parse_dt(struct max_ser_priv *priv)
{
	const char *channel_node_name = "channel";
	const char *pipe_node_name = "pipe";
	struct device_node *node;
	struct max_ser_subdev_priv *sd_priv;
	struct max_ser_pipe *pipe;
	struct max_ser_phy *phy;
	unsigned int i;
	u32 index;
	u32 val;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	val = of_property_read_bool(priv->dev->of_node, "maxim,tunnel-mode");
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

	return 0;
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

static int max_ser_probe(struct max_ser_priv *priv)
{
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	mutex_init(&priv->lock);

	ret = max_ser_allocate(priv);
	if (ret)
		return ret;

	ret = max_ser_parse_dt(priv);
	if (ret)
		return ret;

	ret = max_ser_parse_i2c_dt(priv);
	if (ret)
		return ret;

	ret = max_ser_parse_xlate(priv);
	if (ret)
		return ret;

	ret = max_ser_init(priv);
	if (ret)
		return ret;

	ret = max_ser_i2c_mux_init(priv);
	if (ret)
		return ret;

	dev_info(priv->dev, "probe successful\n");
	return 0;
}

static int max_ser_remove(struct max_ser_priv *priv)
{
	i2c_mux_del_adapters(priv->mux);
	return 0;
}

static int max96717_read(struct max96717_priv *priv, int reg)
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

static int max96717_write(struct max96717_priv *priv, unsigned int reg, u8 val)
{
	int ret;

	ret = regmap_write(priv->regmap, reg, val);
	dev_dbg(priv->dev, "write %d 0x%x = 0x%02x\n", ret, reg, val);

	if (ret)
		dev_err(priv->dev, "write 0x%04x failed\n", reg);

	return ret;
}

static int max96717_update_bits(struct max96717_priv *priv, unsigned int reg,
				u8 mask, u8 val)
{
	int ret;

	ret = regmap_update_bits(priv->regmap, reg, mask, val);
	dev_dbg(priv->dev, "update %d 0x%x 0x%02x = 0x%02x\n", ret, reg, mask, val);

	if (ret)
		dev_err(priv->dev, "update 0x%04x failed\n", reg);

	return ret;
}

static unsigned int max96717_field_get(unsigned int val, unsigned int mask)
{
	return (val & mask) >> __ffs(mask);
}

static unsigned int max96717_field_prep(unsigned int val, unsigned int mask)
{
	return (val << __ffs(mask)) & mask;
}

static int max96717_wait_for_device(struct max96717_priv *priv)
{
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	for (i = 0; i < 10; i++) {
		ret = max96717_read(priv, 0x0);
		if (ret >= 0)
			return 0;

		msleep(100);

		dev_dbg(priv->dev, "Retry %u waiting for serializer: %d\n", i, ret);
	}

	return ret;
}

static unsigned int max96717_pipe_id(struct max96717_priv *priv,
					struct max_ser_pipe *pipe)
{
	return priv->info->pipe_hw_ids[pipe->index];
}

static unsigned int max96717_phy_id(struct max96717_priv *priv,
					struct max_ser_phy *phy)
{
	return priv->info->phy_hw_ids[phy->index];
}

static int max96717_log_status(struct max_ser_priv *ser_priv, const char *name)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	int ret;

	if (!priv->info->supports_tunnel_mode)
		return 0;

	ret = max96717_read(priv, 0x38f);
	if (ret < 0)
		return ret;

	dev_info(priv->dev, "%s: \t\ttun_pkt_cnt: %u\n", name, ret);

	return 0;
}

static int max96717_log_pipe_status(struct max_ser_priv *ser_priv,
					struct max_ser_pipe *pipe,
					const char *name)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	unsigned int index = max96717_pipe_id(priv, pipe);
	int ret;

	ret = max96717_read(priv, 0x102 + 0x8 * index);
	if (ret < 0)
		return ret;

	dev_info(priv->dev, "%s: \tpclkdet: %u\n", name, !!(ret & BIT(7)));

	return 0;
}

static int max96717_log_phy_status(struct max_ser_priv *ser_priv,
				struct max_ser_phy *phy,
				const char *name)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	int ret;

	if (!priv->info->supports_pkt_cnt)
		return 0;

	ret = max96717_read(priv, 0x38d);
	if (ret < 0)
		return ret;

	dev_info(priv->dev, "%s: \tphy_pkt_cnt: %u\n", name, ret);

	ret = max96717_read(priv, 0x38e);
	if (ret < 0)
		return ret;

	dev_info(priv->dev, "%s: \tcsi_pkt_cnt: %u\n", name, ret);

	ret = max96717_read(priv, 0x390);
	if (ret < 0)
		return ret;

	dev_info(priv->dev, "%s: \tphy_clk_cnt: %u\n", name, ret);

	return 0;
}

static int max96717_set_pipe_enable(struct max_ser_priv *ser_priv,
					struct max_ser_pipe *pipe, bool enable)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	unsigned int index = max96717_pipe_id(priv, pipe);
	unsigned int mask = BIT(index + 4);

	dev_dbg(priv->dev, "%s() - %d\n", __func__, enable);

	return max96717_update_bits(priv, 0x2, mask, enable ? mask : 0);
}

static int max96717_set_pipe_dt_en(struct max96717_priv *priv,
				struct max_ser_pipe *pipe,
				unsigned int i, bool en)
{
	unsigned int index = max96717_pipe_id(priv, pipe);
	unsigned int reg, mask;

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (i < 2) {
		reg = 0x314 + index * 2 + i;
		mask = BIT(6);
	} else {
		i -= 2;

		reg = 0x3dc + i;
		mask = BIT(6);
	}

	return max96717_update_bits(priv, reg, mask, en ? mask : 0);
}

static int max96717_set_pipe_dt(struct max96717_priv *priv,
				struct max_ser_pipe *pipe,
				unsigned int i)
{
	unsigned int index = max96717_pipe_id(priv, pipe);
	u32 dt = pipe->dts[i];
	unsigned int reg;

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (i < 2) {
		reg = 0x314 + index * 2 + i;
	} else {
		i -= 2;

		reg = 0x3dc + i;
	}

	return max96717_update_bits(priv, reg, GENMASK(5, 0), dt);
}

static int max96717_update_pipe_dts(struct max_ser_priv *ser_priv,
					struct max_ser_pipe *pipe)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	for (i = 0; i < pipe->num_dts; i++) {
		ret = max96717_set_pipe_dt(priv, pipe, i);
		if (ret)
			return ret;

		ret = max96717_set_pipe_dt_en(priv, pipe, i, true);
		if (ret)
			return ret;
	}

	/* Disable unused DTs. */
	for (i = pipe->num_dts; i < ser_priv->ops->num_dts_per_pipe; i++) {
		ret = max96717_set_pipe_dt_en(priv, pipe, i, false);
		if (ret)
			return ret;
	}

	return 0;
}

static int max96717_update_pipe_vcs(struct max_ser_priv *ser_priv,
					struct max_ser_pipe *pipe)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	unsigned int index = max96717_pipe_id(priv, pipe);
	unsigned int reg = 0x309 + 0x2 * index;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96717_write(priv, reg, (pipe->vcs >> 0) & 0xff);
	if (ret)
		return ret;

	return max96717_write(priv, reg + 0x1, (pipe->vcs >> 8) & 0xff);
}

static int max96717_init_lane_config(struct max96717_priv *priv)
{
	struct max_ser_priv *ser_priv = &priv->ser_priv;
	struct max_ser_phy *phy;
	unsigned int i, j;
	int ret;

	dev_dbg(priv->dev, "%s() - lanes [%d], phys [%d]\n",
		__func__, priv->info->num_lane_configs, priv->info->num_phys);

	for (i = 0; i < priv->info->num_lane_configs; i++) {
		bool matching = true;

		for (j = 0; j < priv->info->num_phys; j++) {
			phy = max_ser_phy_by_id(ser_priv, j);

			if (phy->enabled && phy->mipi.num_data_lanes !=
				priv->info->lane_configs[i][j]) {
				matching = false;
				break;
			}
		}

		if (matching)
			break;
	}

	if (i == priv->info->num_lane_configs) {
		dev_err(priv->dev, "Invalid lane configuration\n");
		return -EINVAL;
	}

	ret = max96717_update_bits(priv, 0x330, GENMASK(2, 0),
				priv->info->phy_configs[i]);
	if (ret)
		return ret;

	return 0;
}

static int max96717_init(struct max_ser_priv *ser_priv)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	unsigned int mask;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);
	/*
	* Set CMU2 PFDDIV to 1.1V for correct functionality of the device,
	* as mentioned in the datasheet, under section MANDATORY REGISTER PROGRAMMING.
	*/
	ret = max96717_update_bits(priv, 0x302, 0x70, 0x10);
	if (ret)
		return ret;

	if (priv->info->supports_tunnel_mode) {
		mask = BIT(7);
		dev_info(priv->dev, "%s() - set tunnel_mode [%d]\n",
			__func__, ser_priv->tunnel_mode);

		ret = max96717_update_bits(priv, 0x383, mask,
					ser_priv->tunnel_mode
					? mask : 0x00);
		if (ret)
			return ret;

		// ret = max96717_update_bits(priv, 0x315, mask,
		// 			ser_priv->tunnel_mode
		// 			? 0x00 : mask);
		// if (ret)
		// 	return ret;
	}

	/* Disable ports. */
	ret = max96717_update_bits(priv, 0x308, 0x20, 0x00);
	if (ret)
		return ret;

	/* Disable pipes. */
	ret = max96717_write(priv, 0x311, 0x00);
	if (ret)
		return ret;

	/* Set I2C to Fast-mode speed (400Kbps) */
	ret = max96717_update_bits(priv, 0x40, GENMASK(5, 4), 0x10);
	ret = max96717_update_bits(priv, 0x41, GENMASK(6, 4), 0x50);

	ret = max96717_init_lane_config(priv);
	if (ret)
		return ret;

	return 0;
}

static int max96717_init_i2c_xlate(struct max_ser_priv *ser_priv)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	unsigned int reg;
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	for (i = 0; i < ser_priv->ops->num_i2c_xlates; i++) {
		u8 src = 0, dst = 0;

		if (i < ser_priv->num_i2c_xlates) {
			src = ser_priv->i2c_xlates[i].src;
			dst = ser_priv->i2c_xlates[i].dst;
		}

		reg = 0x42 + 0x2 * i;

		ret = max96717_write(priv, reg, src << 1);
		if (ret)
			return ret;

		ret = max96717_write(priv, reg + 1, dst << 1);
		if (ret)
			return ret;
	}

	return 0;
}

static int max96717_init_phy(struct max_ser_priv *ser_priv,
				struct max_ser_phy *phy)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	unsigned int num_data_lanes = phy->mipi.num_data_lanes;
	unsigned int index = max96717_phy_id(priv, phy);
	unsigned int val, shift, mask;
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (num_data_lanes == 4)
		val = 0x3;
	else
		val = 0x1;

	shift = index == 1 ? 4 : 0;
	mask = GENMASK(1, 0);

	/* Configure a lane count. */
	/* TODO: Add support for 1-lane configurations. */
	ret = max96717_update_bits(priv, 0x331, mask << shift, val << shift);
	if (ret)
		return ret;

	/* Configure lane mapping. */
	/* TODO: Add support for lane swapping. */
	/* TODO: Handle PHY A. */
	ret = max96717_update_bits(priv, 0x332, 0xf0, 0xe0);
	if (ret)
		return ret;

	ret = max96717_update_bits(priv, 0x333, 0x0f, 0x04);
	if (ret)
		return ret;

	/* Configure lane polarity. */
	/* Lower two lanes. */
	/* TODO: Handle PHY A. */
	val = 0;
	for (i = 0; i < 3 && i < num_data_lanes + 1; i++)
		if (phy->mipi.lane_polarities[i])
			val |= BIT(i == 0 ? 2 : i - 1);
	ret = max96717_update_bits(priv, 0x335, 0x7, val);
	if (ret)
		return ret;

	/* Upper two lanes. */
	val = 0;
	shift = 4;
	mask = GENMASK(2, 0);
	for (i = 3; i < num_data_lanes + 1; i++)
		if (phy->mipi.lane_polarities[i])
			val |= BIT(i - 3);
	ret = max96717_update_bits(priv, 0x334, mask << shift, val << shift);
	if (ret)
		return ret;

	if (priv->info->supports_noncontinuous_clock) {
		mask = BIT(6);

		ret = max96717_update_bits(priv, 0x330, mask,
					phy->mipi.flags &
					V4L2_MBUS_CSI2_NONCONTINUOUS_CLOCK
					? mask : 0x00);
		if (ret)
			return ret;
	}

	/* Enable PHY. */
	shift = 4;
	mask = BIT(index) << shift;
	ret = max96717_update_bits(priv, 0x308, mask, mask);
	if (ret)
		return ret;

	return 0;
}

static int max96717_init_pipe_stream_id(struct max96717_priv *priv,
					struct max_ser_pipe *pipe)
{
	unsigned int index = max96717_pipe_id(priv, pipe);

	return max96717_write(priv, 0x53 + 0x4 * index, pipe->stream_id);
}

static int max96717_init_pipe(struct max_ser_priv *ser_priv,
				struct max_ser_pipe *pipe)
{
	struct max_ser_phy *phy = max_ser_pipe_phy(ser_priv, pipe);
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	unsigned int index = max96717_pipe_id(priv, pipe);
	unsigned int phy_id = max96717_phy_id(priv, phy);
	unsigned int reg, val, shift, mask;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96717_set_pipe_enable(ser_priv, pipe, false);
	if (ret)
		return ret;

	/* Map pipe to PHY. */
	mask = BIT(index);
	val = phy_id == 1 ? mask : 0;
	ret = max96717_update_bits(priv, 0x308, mask, val);
	if (ret)
		return ret;

	/* Enable pipe output to PHY. */
	shift = phy_id == 1 ? 4 : 0;
	mask = BIT(index) << shift;
	ret = max96717_update_bits(priv, 0x311, mask, mask);
	if (ret)
		return ret;

	/* Set 8bit double mode. */
	mask = BIT(index);
	ret = max96717_update_bits(priv, 0x312, mask, pipe->dbl8 ? mask : 0);
	if (ret)
		return ret;

	/* Set 10bit double mode. */
	mask = BIT(index);
	ret = max96717_update_bits(priv, 0x313, mask, pipe->dbl10 ? mask : 0);
	if (ret)
		return ret;

	/* Set 12bit double mode. */
	mask = BIT(index) << 4;
	ret = max96717_update_bits(priv, 0x313, mask, pipe->dbl12 ? mask : 0);
	if (ret)
		return ret;

	/* Set override soft BPP. */
	reg = 0x31c + index;
	mask = BIT(5);
	ret = max96717_update_bits(priv, reg, mask, pipe->soft_bpp ? mask : 0);
	if (ret)
		return ret;

	/* Set soft BPP. */
	ret = max96717_update_bits(priv, reg, GENMASK(4, 0), pipe->soft_bpp);
	if (ret)
		return ret;

	/* Set override BPP. */
	reg = 0x100 + 0x8 * index;
	mask = BIT(3);
	ret = max96717_update_bits(priv, reg, mask, pipe->bpp ? 0 : mask);
	if (ret)
		return ret;

	/* Set BPP. */
	ret = max96717_update_bits(priv, reg + 1, GENMASK(5, 0), pipe->bpp);
	if (ret)
		return ret;

	ret = max96717_init_pipe_stream_id(priv, pipe);
	if (ret)
		return ret;

	return 0;
}

static int max96717_init_pipes_stream_ids(struct max96717_priv *priv)
{
	struct max_ser_priv *ser_priv = &priv->ser_priv;
	unsigned int used_stream_ids = 0;
	struct max_ser_pipe *pipe;
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	for (i = 0; i < ser_priv->ops->num_pipes; i++) {
		pipe = max_ser_pipe_by_id(ser_priv, i);

		if (!pipe->enabled)
			continue;

		if (used_stream_ids & BIT(pipe->stream_id)) {
			dev_err(priv->dev, "Duplicate stream %u\n", pipe->index);
			return -EINVAL;
		}

		used_stream_ids |= BIT(pipe->stream_id);
	}

	for (i = 0; i < ser_priv->ops->num_pipes; i++) {
		pipe = max_ser_pipe_by_id(ser_priv, i);

		if (pipe->enabled)
			continue;

		/* Stream ID already used, find a free one. */
		/* TODO: check whether there is no unused stream ID? */
		if (used_stream_ids & BIT(pipe->stream_id))
			pipe->stream_id = ffz(used_stream_ids);

		ret = max96717_init_pipe_stream_id(priv, pipe);
		if (ret)
			return ret;
	}

	return 0;
}

static int max96717_post_init(struct max_ser_priv *ser_priv)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	struct max_ser_subdev_priv *sd_priv;
	struct max_ser_pipe *pipe;
	const struct max_format *fmt;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96717_init_pipes_stream_ids(priv);
	if (ret)
		return ret;

	msleep(100);

	/* fix format to UYVY8_1X16 */
	fmt = max_format_by_code(MEDIA_BUS_FMT_UYVY8_1X16);
	if (!fmt)
		return -EINVAL;

	for_each_subdev(ser_priv, sd_priv) {
		pipe = &ser_priv->pipes[sd_priv->pipe_id];
		sd_priv->fmt = fmt;

		mutex_lock(&ser_priv->lock);
		ret = max_ser_update_pipe_dts(ser_priv, pipe);
		mutex_unlock(&ser_priv->lock);
		if (ret)
			return -EINVAL;

		max_ser_ch_enable(sd_priv, true);
		if (ret)
			return -EINVAL;
	}

	max96717_update_bits(priv, 0x330, 0x08, 0x08);
	max96717_update_bits(priv, 0x330, 0x08, 0x00);

	return 0;
}

static const struct max_ser_ops max96717_ops = {
	.num_i2c_xlates = 2,
	.log_status = max96717_log_status,
	.log_pipe_status = max96717_log_pipe_status,
	.log_phy_status = max96717_log_phy_status,
	.set_pipe_enable = max96717_set_pipe_enable,
	.update_pipe_dts = max96717_update_pipe_dts,
	.update_pipe_vcs = max96717_update_pipe_vcs,
	.init = max96717_init,
	.init_i2c_xlate = max96717_init_i2c_xlate,
	.init_phy = max96717_init_phy,
	.init_pipe = max96717_init_pipe,
	.post_init = max96717_post_init,
};

#define MAX96717_PIN(n) \
	PINCTRL_PIN(n, "mfp" __stringify(n))

static const struct pinctrl_pin_desc max96717_pins[] = {
	MAX96717_PIN(0),
	MAX96717_PIN(1),
	MAX96717_PIN(2),
	MAX96717_PIN(3),
	MAX96717_PIN(4),
	MAX96717_PIN(5),
	MAX96717_PIN(6),
	MAX96717_PIN(7),
	MAX96717_PIN(8),
	MAX96717_PIN(9),
	MAX96717_PIN(10),
};

#define MAX96717_GROUP_PINS(name, ...) \
	static const unsigned int name ## _pins[] = { __VA_ARGS__ }

MAX96717_GROUP_PINS(mfp0, 0);
MAX96717_GROUP_PINS(mfp1, 1);
MAX96717_GROUP_PINS(mfp2, 2);
MAX96717_GROUP_PINS(mfp3, 3);
MAX96717_GROUP_PINS(mfp4, 4);
MAX96717_GROUP_PINS(mfp5, 5);
MAX96717_GROUP_PINS(mfp6, 6);
MAX96717_GROUP_PINS(mfp7, 7);
MAX96717_GROUP_PINS(mfp8, 8);
MAX96717_GROUP_PINS(mfp9, 9);
MAX96717_GROUP_PINS(mfp10, 10);

#define MAX96717_GROUP(name) \
	PINCTRL_PINGROUP(__stringify(name), name ## _pins, ARRAY_SIZE(name ## _pins))

static const struct pingroup max96717_ctrl_groups[] = {
	MAX96717_GROUP(mfp0),
	MAX96717_GROUP(mfp1),
	MAX96717_GROUP(mfp2),
	MAX96717_GROUP(mfp3),
	MAX96717_GROUP(mfp4),
	MAX96717_GROUP(mfp5),
	MAX96717_GROUP(mfp6),
	MAX96717_GROUP(mfp7),
	MAX96717_GROUP(mfp8),
	MAX96717_GROUP(mfp9),
	MAX96717_GROUP(mfp10),
};

#define MAX96717_FUNC_GROUPS(name, ...) \
	static const char * const name ## _groups[] = { __VA_ARGS__ }

MAX96717_FUNC_GROUPS(gpio, "mfp0", "mfp1", "mfp2", "mfp3", "mfp4", "mfp5",
			"mfp6", "mfp7", "mfp8", "mfp9", "mfp10");
MAX96717_FUNC_GROUPS(pclk, "mfp0", "mfp1", "mfp2", "mfp3", "mfp4", "mfp7", "mfp8");
MAX96717_FUNC_GROUPS(rclkout, "mfp4", "mfp2");

enum max96717_func {
	max96717_func_gpio,
	max96717_func_pclk,
	max96717_func_rclkout,
};

#define MAX96717_FUNC(name)						\
	[max96717_func_ ## name] =					\
		PINCTRL_PINFUNCTION(__stringify(name), name ## _groups,	\
					ARRAY_SIZE(name ## _groups))

static const struct pinfunction max96717_functions[] = {
	MAX96717_FUNC(gpio),
	MAX96717_FUNC(pclk),
	MAX96717_FUNC(rclkout),
};

enum max96717_pinctrl_params {
	MAX96717_PINCTRL_PULL_STRENGTH_WEAK = PIN_CONFIG_END + 1,
	MAX96717_PINCTRL_JITTER_COMPENSATION_EN,
	MAX96717_PINCTRL_GMSL_TX_EN,
	MAX96717_PINCTRL_GMSL_RX_EN,
	MAX96717_PINCTRL_GMSL_TX_ID,
	MAX96717_PINCTRL_GMSL_RX_ID,
	MAX96717_PINCTRL_RCLKOUT_CLK,
	MAX96717_PINCTRL_INPUT_VALUE,
};

static const struct pinconf_generic_params max96717_cfg_params[] = {
	{ "maxim,pull-strength-weak", MAX96717_PINCTRL_PULL_STRENGTH_WEAK, 0 },
	{ "maxim,jitter-compensation", MAX96717_PINCTRL_JITTER_COMPENSATION_EN, 0 },
	{ "maxim,gmsl-tx", MAX96717_PINCTRL_GMSL_TX_EN, 0 },
	{ "maxim,gmsl-rx", MAX96717_PINCTRL_GMSL_RX_EN, 0 },
	{ "maxim,gmsl-tx-id", MAX96717_PINCTRL_GMSL_TX_ID, 0 },
	{ "maxim,gmsl-rx-id", MAX96717_PINCTRL_GMSL_RX_ID, 0 },
	{ "maxim,rclkout-clock", MAX96717_PINCTRL_RCLKOUT_CLK, 0 },
};

static int max96717_ctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(max96717_ctrl_groups);
}

static const char *max96717_ctrl_get_group_name(struct pinctrl_dev *pctldev,
						unsigned selector)
{
	return max96717_ctrl_groups[selector].name;
}

static int max96717_ctrl_get_group_pins(struct pinctrl_dev *pctldev, unsigned selector,
					const unsigned **pins, unsigned *num_pins)
{
	*pins = (unsigned *) max96717_ctrl_groups[selector].pins;
	*num_pins = max96717_ctrl_groups[selector].npins;

	return 0;
}

static int max96717_get_pin_config_reg(unsigned int offset, u32 param,
					unsigned int *reg, unsigned int *mask,
					unsigned int *val)
{
	*reg = 0x2be + offset * 0x3;

	switch (param) {
	case PIN_CONFIG_OUTPUT_ENABLE:
		*mask = BIT(0);
		*val = 0b0;
		return 0;
	case PIN_CONFIG_INPUT_ENABLE:
		*mask = BIT(0);
		*val = 0b1;
		return 0;
	case MAX96717_PINCTRL_GMSL_TX_EN:
		*mask = BIT(1);
		*val = 0b1;
		return 0;
	case MAX96717_PINCTRL_GMSL_RX_EN:
		*mask = BIT(2);
		*val = 0b1;
		return 0;
	case MAX96717_PINCTRL_INPUT_VALUE:
		*mask = BIT(3);
		*val = 0b1;
		return 0;
	case PIN_CONFIG_OUTPUT:
		*mask = BIT(4);
		*val = 0b1;
		return 0;
	case MAX96717_PINCTRL_JITTER_COMPENSATION_EN:
		*mask = BIT(5);
		*val = 0b1;
		return 0;
	case MAX96717_PINCTRL_PULL_STRENGTH_WEAK:
		*mask = BIT(7);
		*val = 0b0;
		return 0;
	case MAX96717_PINCTRL_GMSL_TX_ID:
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
	case PIN_CONFIG_SLEW_RATE:
		*reg = 0x56f;

		if (offset <= 2) {
			*mask = GENMASK(1, 0) << (offset - 0) * 2;
		} else if (offset <= 4) {
			*reg += 1;
			*mask = GENMASK(3, 2) << (offset - 3) * 2;
		} else if (offset <= 6) {
			return -EINVAL;
		} else if (offset <= 8) {
			*reg += 2;
			*mask = GENMASK(5, 4) << (offset - 7) * 2;
		} else {
			return -EINVAL;
		}
		return 0;
	case MAX96717_PINCTRL_GMSL_RX_ID:
		*reg += 2;
		*mask = GENMASK(4, 0);
		return 0;
	case MAX96717_PINCTRL_RCLKOUT_CLK:
		*reg = 0x3;
		*mask = GENMASK(1, 0);
		return 0;
	default:
		return -ENOTSUPP;
	}
}

static int max96717_conf_pin_config_get(struct pinctrl_dev *pctldev,
					unsigned int offset,
					unsigned long *config)
{
	struct max96717_priv *priv = pinctrl_dev_get_drvdata(pctldev);
	u32 param = pinconf_to_config_param(*config);
	unsigned int reg, mask, val;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96717_get_pin_config_reg(offset, param, &reg, &mask, &val);
	if (ret)
		return ret;

	switch (param) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
	case PIN_CONFIG_DRIVE_PUSH_PULL:
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		ret = max96717_read(priv, reg);
		if (ret < 0)
			return ret;

		val = max96717_field_get(ret, mask) == val;
		if (!val)
			return -EINVAL;

		break;
	case MAX96717_PINCTRL_JITTER_COMPENSATION_EN:
	case MAX96717_PINCTRL_PULL_STRENGTH_WEAK:
	case MAX96717_PINCTRL_GMSL_TX_EN:
	case MAX96717_PINCTRL_GMSL_RX_EN:
	case MAX96717_PINCTRL_INPUT_VALUE:
	case PIN_CONFIG_OUTPUT_ENABLE:
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT:
		ret = max96717_read(priv, reg);
		if (ret < 0)
			return ret;

		val = max96717_field_get(ret, mask) == val;
		break;
	case MAX96717_PINCTRL_GMSL_TX_ID:
	case MAX96717_PINCTRL_GMSL_RX_ID:
	case PIN_CONFIG_SLEW_RATE:
	case MAX96717_PINCTRL_RCLKOUT_CLK:
		ret = max96717_read(priv, reg);
		if (ret < 0)
			return ret;

		val = max96717_field_get(val, mask);
		break;
	default:
		return -ENOTSUPP;
	}

	*config = pinconf_to_config_packed(param, val);

	return 0;
}

static int max96717_conf_pin_config_set_one(struct max96717_priv *priv,
						unsigned int offset,
						unsigned long config)
{
	u32 param = pinconf_to_config_param(config);
	u32 arg = pinconf_to_config_argument(config);
	unsigned int reg, mask, val;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96717_get_pin_config_reg(offset, param, &reg, &mask, &val);
	if (ret)
		return ret;

	switch (param) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
	case PIN_CONFIG_DRIVE_PUSH_PULL:
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		val = max96717_field_prep(val, mask);

		ret = max96717_update_bits(priv, reg, mask, val);
		break;
	case MAX96717_PINCTRL_JITTER_COMPENSATION_EN:
	case MAX96717_PINCTRL_PULL_STRENGTH_WEAK:
	case MAX96717_PINCTRL_GMSL_TX_EN:
	case MAX96717_PINCTRL_GMSL_RX_EN:
	case PIN_CONFIG_OUTPUT_ENABLE:
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT:
		val = max96717_field_prep(arg ? val : ~val, mask);

		ret = max96717_update_bits(priv, reg, mask, val);
		break;
	case MAX96717_PINCTRL_GMSL_TX_ID:
	case MAX96717_PINCTRL_GMSL_RX_ID:
	case PIN_CONFIG_SLEW_RATE:
	case MAX96717_PINCTRL_RCLKOUT_CLK:
		val = max96717_field_prep(arg, mask);

		ret = max96717_update_bits(priv, reg, mask, val);
		break;
	default:
		return -ENOTSUPP;
	}

	if (ret)
		return ret;

	switch (param) {
	case PIN_CONFIG_OUTPUT:
		config = pinconf_to_config_packed(PIN_CONFIG_OUTPUT_ENABLE, 1);
		return max96717_conf_pin_config_set_one(priv, offset, config);
		break;
	case PIN_CONFIG_OUTPUT_ENABLE:
		config = pinconf_to_config_packed(MAX96717_PINCTRL_GMSL_RX_EN, 0);
		return max96717_conf_pin_config_set_one(priv, offset, config);
	default:
		break;
	}

	return 0;
}

static int max96717_conf_pin_config_set(struct pinctrl_dev *pctldev,
					unsigned int offset,
					unsigned long *configs,
					unsigned int num_configs)
{
	struct max96717_priv *priv = pinctrl_dev_get_drvdata(pctldev);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	while (num_configs--) {
		unsigned long config = *configs;

		ret = max96717_conf_pin_config_set_one(priv, offset, config);
		if (ret)
			return ret;

		configs++;
	}

	return 0;
}

static int max96717_mux_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(max96717_functions);
}

static const char *max96717_mux_get_function_name(struct pinctrl_dev *pctldev,
						unsigned selector)
{
	return max96717_functions[selector].name;
}

static int max96717_mux_get_groups(struct pinctrl_dev *pctldev,
				unsigned selector,
				const char * const **groups,
				unsigned * const num_groups)
{
	*groups = max96717_functions[selector].groups;
	*num_groups = max96717_functions[selector].ngroups;

	return 0;
}

static int max96717_mux_set_pclk(struct max96717_priv *priv, unsigned int group)
{
	int ret;

	ret = max96717_update_bits(priv, 0x3f1, BIT(0), BIT(0));
	if (ret)
		return ret;

	ret = max96717_update_bits(priv, 0x3f1, GENMASK(5, 1),
				FIELD_PREP(GENMASK(5, 1), group));
	if (ret)
		return ret;

	return 0;
}

static int max96717_mux_set_rclkout(struct max96717_priv *priv, unsigned int group)
{
	int ret;

	/* Enable RCLK. */
	ret = max96717_update_bits(priv, 0x6, BIT(5), BIT(5));
	if (ret)
		return ret;

	/* Enable RCLK output on PCLK. */
	ret = max96717_update_bits(priv, 0x3f1, BIT(7), BIT(7));
	if (ret)
		return ret;

	return 0;
}

static int max96717_mux_set(struct pinctrl_dev *pctldev, unsigned selector,
				unsigned group)
{
	struct max96717_priv *priv = pinctrl_dev_get_drvdata(pctldev);
	int ret;

	switch (selector) {
	case max96717_func_pclk:
		return max96717_mux_set_pclk(priv, group);
	case max96717_func_rclkout:
		ret = max96717_mux_set_pclk(priv, group);
		if (ret)
			return ret;

		return max96717_mux_set_rclkout(priv, group);
	}

	return 0;
}

static int max96717_gpio_get_direction(struct gpio_chip *gc, unsigned int offset)
{
	unsigned long config = pinconf_to_config_packed(PIN_CONFIG_OUTPUT_ENABLE, 0);
	struct max96717_priv *priv = gpiochip_get_data(gc);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96717_conf_pin_config_get(priv->pctldev, offset, &config);
	if (ret)
		return ret;

	return pinconf_to_config_argument(config) ? GPIO_LINE_DIRECTION_OUT
						: GPIO_LINE_DIRECTION_IN;
}

static int max96717_gpio_direction_input(struct gpio_chip *gc, unsigned int offset)
{
	unsigned long config = pinconf_to_config_packed(PIN_CONFIG_INPUT_ENABLE, 1);
	struct max96717_priv *priv = gpiochip_get_data(gc);

	return max96717_conf_pin_config_set_one(priv, offset, config);
}

static int max96717_gpio_direction_output(struct gpio_chip *gc, unsigned int offset,
					int value)
{
	unsigned long config = pinconf_to_config_packed(PIN_CONFIG_OUTPUT, value);
	struct max96717_priv *priv = gpiochip_get_data(gc);

	return max96717_conf_pin_config_set_one(priv, offset, config);
}

static int max96717_gpio_get(struct gpio_chip *gc, unsigned int offset)
{
	unsigned long config = pinconf_to_config_packed(MAX96717_PINCTRL_INPUT_VALUE, 0);
	struct max96717_priv *priv = gpiochip_get_data(gc);
	int ret;

	ret = max96717_conf_pin_config_get(priv->pctldev, offset, &config);
	if (ret)
		return ret;

	return pinconf_to_config_argument(config);
}

static void max96717_gpio_set(struct gpio_chip *gc, unsigned int offset, int value)
{
	unsigned long config = pinconf_to_config_packed(PIN_CONFIG_OUTPUT, value);
	struct max96717_priv *priv = gpiochip_get_data(gc);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96717_conf_pin_config_set_one(priv, offset, config);
	if (ret)
		dev_err(priv->dev, "Failed to set GPIO %u output value, err: %d\n",
			offset, ret);
}

static struct pinctrl_ops max96717_ctrl_ops = {
	.get_groups_count = max96717_ctrl_get_groups_count,
	.get_group_name = max96717_ctrl_get_group_name,
	.get_group_pins = max96717_ctrl_get_group_pins,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_pin,
	.dt_free_map = pinconf_generic_dt_free_map,
};

static const struct pinconf_ops max96717_conf_ops = {
	.pin_config_get = max96717_conf_pin_config_get,
	.pin_config_set = max96717_conf_pin_config_set,
	.is_generic = true,
};

static const struct pinmux_ops max96717_mux_ops = {
	.get_functions_count = max96717_mux_get_functions_count,
	.get_function_name = max96717_mux_get_function_name,
	.get_function_groups = max96717_mux_get_groups,
	.set_mux = max96717_mux_set,
};

static int max96717_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct max96717_priv *priv;
	struct max_ser_ops *ops;
	int ret;

	dev_info(dev, "%s() device node: %s\n", __func__,
		client->dev.of_node->full_name);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	ops = devm_kzalloc(dev, sizeof(*ops), GFP_KERNEL);
	if (!ops)
		return -ENOMEM;

	priv->info = device_get_match_data(dev);
	if (!priv->info) {
		dev_err(dev, "Failed to get match data\n");
		return -ENODEV;
	}

	priv->dev = dev;
	priv->client = client;
	i2c_set_clientdata(client, priv);

	priv->regmap = devm_regmap_init_i2c(client, &max_ser_i2c_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	*ops = max96717_ops;

	ops->supports_tunnel_mode = priv->info->supports_tunnel_mode;
	ops->supports_noncontinuous_clock = priv->info->supports_noncontinuous_clock;
	ops->num_pipes = priv->info->num_pipes;
	ops->num_dts_per_pipe = priv->info->num_dts_per_pipe;
	ops->num_phys = priv->info->num_phys;

	priv->ser_priv.dev = dev;
	priv->ser_priv.client = client;
	priv->ser_priv.regmap = priv->regmap;
	priv->ser_priv.ops = ops;

	ret = max96717_wait_for_device(priv);
	if (ret) {
		dev_err(dev, "Failed waiting for MAX96717, err: %d\n", ret);
		return ret;
	}

	priv->pctldesc = (struct pinctrl_desc) {
		.owner = THIS_MODULE,
		.name = MAX96717_PINCTRL_NAME,
		.pins = max96717_pins,
		.npins = ARRAY_SIZE(max96717_pins),
		.pctlops = &max96717_ctrl_ops,
		.confops = &max96717_conf_ops,
		.pmxops = &max96717_mux_ops,
		.custom_params = max96717_cfg_params,
		.num_custom_params = ARRAY_SIZE(max96717_cfg_params),
	};

	ret = devm_pinctrl_register_and_init(dev, &priv->pctldesc, priv, &priv->pctldev);
	if (ret)
		return ret;

	ret = pinctrl_enable(priv->pctldev);
	if (ret)
		return ret;

	priv->gc = (struct gpio_chip) {
		.owner = THIS_MODULE,
		.label = MAX96717_GPIOCHIP_NAME,
		.base = -1,
		.ngpio = MAX96717_GPIO_NUM,
		.parent = dev,
		.can_sleep = true,
		.request = gpiochip_generic_request,
		.free = gpiochip_generic_free,
		.set_config = gpiochip_generic_config,
		.get_direction = max96717_gpio_get_direction,
		.direction_input = max96717_gpio_direction_input,
		.direction_output = max96717_gpio_direction_output,
		.get = max96717_gpio_get,
		.set = max96717_gpio_set,
	};

	ret = devm_gpiochip_add_data(dev, &priv->gc, priv);
	if (ret)
		return ret;

	return max_ser_probe(&priv->ser_priv);
}

static int max96717_remove(struct i2c_client *client)
{
	struct max96717_priv *priv = i2c_get_clientdata(client);

	return max_ser_remove(&priv->ser_priv);
}

static const struct max96717_chip_info max96717_info = {
	.supports_pkt_cnt = true,
	.supports_tunnel_mode = true,
	.supports_noncontinuous_clock = true,
	.num_pipes = 1,
	.num_dts_per_pipe = 4,
	.pipe_hw_ids = { 2 },
	.num_phys = 1,
	.phy_hw_ids = { 1 },
	.num_lane_configs = 2,
	.lane_configs = {
		{ 4 },
		{ 2 },
	},
	.phy_configs = {
		0b000,
		0b000,
	},
};

static const struct of_device_id max96717_of_ids[] = {
	{ .compatible = "maxim,max96717", .data = &max96717_info },
	{ }
};
MODULE_DEVICE_TABLE(of, max96717_of_ids);

static struct i2c_driver max96717_i2c_driver = {
	.driver	= {
		.name = MAX96717_NAME,
		.of_match_table = max96717_of_ids,
	},
	.probe_new = max96717_probe,
	.remove = max96717_remove,
};

module_i2c_driver(max96717_i2c_driver);

MODULE_DESCRIPTION("MAX96717 GMSL2 Serializer Driver");
MODULE_AUTHOR("TechNexion Inc.");
MODULE_LICENSE("GPL");