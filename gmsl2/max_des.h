// SPDX-License-Identifier: GPL-2.0
/*
*
*/

#include <linux/i2c-mux.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

#include "max_serdes.h"

#ifndef MAX_DES_H
#define MAX_DES_H

/* TODO: remove */
#define MAX_DES_REMAPS_NUM		16

#define MAX_DES_SOURCE_PAD		0
#define MAX_DES_SINK_PAD		1
#define MAX_DES_PAD_NUM			2

const struct regmap_config max_des_i2c_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0x1f00,
};

struct max_des_asd {
	struct v4l2_async_subdev base;
	struct max_des_subdev_priv *sd_priv;
};

#define MAX_DES_DT_VC(dt, vc) (((vc) & 0x3) << 6 | ((dt) & 0x3f))

struct max_des_dt_vc_remap {
	u8 from_dt;
	u8 from_vc;
	u8 to_dt;
	u8 to_vc;
	u8 phy;
};

struct max_des_subdev_priv {
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *pixel_rate_ctrl;

	unsigned int index;
	struct device_node *node;

	struct max_des_priv *priv;
	const struct max_format *fmt;
	struct v4l2_mbus_framefmt format;
	const char *label;

	struct media_pad pads[MAX_DES_PAD_NUM];

	bool active;
	unsigned int pipe_id;
	unsigned int phy_id;
	unsigned int src_vc_id;
	unsigned int dst_vc_id;
};

struct max_des_link {
	unsigned int index;
	bool enabled;
	struct max_i2c_xlate ser_xlate;
	bool ser_xlate_enabled;
	bool tunnel_mode;
};

struct max_des_pipe {
	unsigned int index;
	unsigned int phy_id;
	unsigned int stream_id;
	unsigned int link_id;
	struct max_des_dt_vc_remap remaps[MAX_DES_REMAPS_NUM];
	unsigned int num_remaps;
	bool dbl8;
	bool dbl10;
	bool dbl12;
	bool dbl8mode;
	bool dbl10mode;
	bool enabled;
};

struct max_des_phy {
	unsigned int index;
	s64 link_frequency;
	struct v4l2_mbus_config_mipi_csi2 mipi;
	bool alt_mem_map8;
	bool alt2_mem_map8;
	bool alt_mem_map10;
	bool alt_mem_map12;
	bool bus_config_parsed;
	bool enabled;
};

struct max_des_fsync {
	unsigned int freq;
	bool internal_output;
	bool internal;
	bool external;
};

struct max_des_ops {
	unsigned int num_phys;
	unsigned int num_pipes;
	unsigned int num_links;
	bool fix_tx_ids;
	bool supports_pipe_link_remap;
	bool supports_pipe_stream_autoselect;
	bool supports_tunnel_mode;

	int (*log_status)(struct max_des_priv *priv, const char *name);
	int (*log_pipe_status)(struct max_des_priv *priv, struct max_des_pipe *pipe,
				const char *name);
	int (*log_phy_status)(struct max_des_priv *priv, struct max_des_phy *phy,
				const char *name);
	int (*mipi_enable)(struct max_des_priv *priv, bool enable);
	int (*init)(struct max_des_priv *priv);
	int (*init_phy)(struct max_des_priv *priv, struct max_des_phy *phy);
	int (*init_pipe)(struct max_des_priv *priv, struct max_des_pipe *pipe);
	int (*init_link)(struct max_des_priv *priv, struct max_des_link *link);
	int (*init_fsync)(struct max_des_priv *priv, struct max_des_fsync *fsync);
	int (*update_pipe_remaps)(struct max_des_priv *priv, struct max_des_pipe *pipe);
	int (*select_links)(struct max_des_priv *priv, unsigned int mask);
	int (*post_init)(struct max_des_priv *priv);
};

struct max_des_priv {
	const struct max_des_ops *ops;

	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;

	struct i2c_mux_core *mux;
	int mux_chan;

	unsigned int gmsl_link_mask;
	unsigned int gmsl_links_used;

	unsigned int num_subdevs;
	struct mutex lock;
	bool active;

	bool pipe_stream_autoselect;
	struct max_des_phy *phys;
	struct max_des_pipe *pipes;
	struct max_des_link *links;
	struct max_des_fsync *fsync;
	struct max_des_subdev_priv *sd_privs;
};

static inline struct max_des_phy *max_des_phy_by_id(struct max_des_priv *priv,
							unsigned int index)
{
	return &priv->phys[index];
}

#endif // MAX_DES_H