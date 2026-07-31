// SPDX-License-Identifier: GPL-2.0
/*
 * Maxim MAX96724 Quad GMSL2 Deserializer Driver
 *
 */

#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/regmap.h>

#include "max_des.h"

#define MAX96724_REG0							0x0

#define MAX96724_REG6							0x6
#define MAX96724_REG6_LINK_EN					GENMASK(3, 0)

#define MAX96724_REG26(x)						(0x10 + (x) / 2)
#define MAX96724_REG26_RX_RATE_PHY(x)			(GENMASK(1, 0) << (4 * ((x) % 2)))
#define MAX96724_REG26_RX_RATE_3Gbps			0b01
#define MAX96724_REG26_RX_RATE_6Gbps			0b10

#define MAX96724_PWR1							0x13
#define MAX96724_PWR1_RESET_ALL					BIT(6)

#define MAX96724_CTRL1							0x18
#define MAX96724_CTRL1_RESET_ONESHOT			GENMASK(3, 0)
#define MAX96724_CTRL1_RESET_LINK				GENMASK(7, 4)

#define MAX96724_CTRL3							0x1a
#define MAX96724_CTRL3_LOCKED_A					BIT(3)

#define MAX96724_CTRL12							0x0a
#define MAX96724_CTRL13							0x0b
#define MAX96724_CTRL14							0x0c

#define MAX96724_VIDEO_PIPE_SEL(p)				(0xf0 + (p) / 2)
#define MAX96724_VIDEO_PIPE_SEL_STREAM(p)		(GENMASK(1, 0) << (4 * ((p) % 2)))
#define MAX96724_VIDEO_PIPE_SEL_LINK(p)			(GENMASK(3, 2) << (4 * ((p) % 2)))

#define MAX96724_VIDEO_PIPE_EN					0xf4
#define MAX96724_VIDEO_PIPE_EN_MASK(p)			BIT(p)
#define MAX96724_VIDEO_PIPE_EN_STREAM_SEL_ALL	BIT(4)

#define MAX96724_VPRBS(p)						(0x1dc + (p) * 0x20)
#define MAX96724_VPRBS_VIDEO_LOCK				BIT(0)

#define MAX96724_GPIO_A(x)						(0x300 + (x) * 3)
#define MAX96724_GPIO_A_GPIO_OUT_DIS			BIT(0)
#define MAX96724_GPIO_A_GPIO_TX_EN				BIT(1)
#define MAX96724_GPIO_A_GPIO_RX_EN				BIT(2)
#define MAX96724_GPIO_A_GPIO_IN					BIT(3)
#define MAX96724_GPIO_A_GPIO_OUT				BIT(4)
#define MAX96724_GPIO_A_TX_COMP_EN				BIT(5)
#define MAX96724_GPIO_A_RES_CFG					BIT(7)

#define MAX96724_GPIO_B(x)						(0x301 + (x) * 3)
#define MAX96724_GPIO_B_GPIO_TX_ID				GENMASK(4, 0)
#define MAX96724_GPIO_B_OUT_TYPE				BIT(5)
#define MAX96724_GPIO_B_PULL_UPDN_SEL			GENMASK(7, 6)
#define MAX96724_GPIO_B_PULL_UPDN_SEL_NONE		0b00
#define MAX96724_GPIO_B_PULL_UPDN_SEL_PU		0b01
#define MAX96724_GPIO_B_PULL_UPDN_SEL_PD		0b10

#define MAX96724_GPIO_C(x)						(0x302 + (x) * 3)
#define MAX96724_GPIO_C_GPIO_RX_ID				GENMASK(4, 0)

#define MAX96724_GPIO_B_B(x)					(0x337 + (x) * 3)
#define MAX96724_GPIO_B_GPIO_TX_ID_B			GENMASK(4, 0)
#define MAX96724_GPIO_B_GPIO_TX_EN_B			BIT(5)
#define MAX96724_GPIO_B_TX_COMP_EN_B			BIT(6)

#define MAX96724_GPIO_C_B(x)					(0x338 + (x) * 3)
#define MAX96724_GPIO_C_GPIO_RX_ID_B			GENMASK(4, 0)
#define MAX96724_GPIO_C_GPIO_RX_EN_B			BIT(5)

#define MAX96724_GPIO_B_C(x)					(0x36d + (x) * 3)
#define MAX96724_GPIO_B_GPIO_TX_ID_C			GENMASK(4, 0)
#define MAX96724_GPIO_B_GPIO_TX_EN_C			BIT(5)
#define MAX96724_GPIO_B_TX_COMP_EN_C			BIT(6)

#define MAX96724_GPIO_C_C(x)					(0x36e + (x) * 3)
#define MAX96724_GPIO_C_GPIO_RX_ID_C			GENMASK(4, 0)
#define MAX96724_GPIO_C_GPIO_RX_EN_C			BIT(5)

#define MAX96724_GPIO_B_D(x)					(0x3a4 + (x) * 3)
#define MAX96724_GPIO_B_GPIO_TX_ID_D			GENMASK(4, 0)
#define MAX96724_GPIO_B_GPIO_TX_EN_D			BIT(5)
#define MAX96724_GPIO_B_TX_COMP_EN_D			BIT(6)

#define MAX96724_GPIO_C_D(x)					(0x3a5 + (x) * 3)
#define MAX96724_GPIO_C_GPIO_RX_ID_D			GENMASK(4, 0)
#define MAX96724_GPIO_C_GPIO_RX_EN_D			BIT(5)

#define MAX96724_BACKTOP12						0x40b
#define MAX96724_BACKTOP12_CSI_OUT_EN			BIT(1)

#define MAX96724_BACKTOP21						0x414
#define MAX96724_BACKTOP21_BPP8DBL(p)			BIT(4 + (p))

#define MAX96724_BACKTOP22(x)					(0x415 + (x) * 0x3)
#define MAX96724_BACKTOP22_PHY_CSI_TX_DPLL		GENMASK(4, 0)
#define MAX96724_BACKTOP22_PHY_CSI_TX_DPLL_EN	BIT(5)

#define MAX96724_BACKTOP24						0x417
#define MAX96724_BACKTOP24_BPP8DBL_MODE(p)		BIT(4 + (p))

#define MAX96724_BACKTOP30						0x41d
#define MAX96724_BACKTOP30_BPP10DBL3			BIT(4)
#define MAX96724_BACKTOP30_BPP10DBL3_MODE		BIT(5)

#define MAX96724_BACKTOP31						0x41e
#define MAX96724_BACKTOP31_BPP10DBL2			BIT(6)
#define MAX96724_BACKTOP31_BPP10DBL2_MODE		BIT(7)

#define MAX96724_BACKTOP32						0x41f
#define MAX96724_BACKTOP32_BPP12(p)				BIT(p)
#define MAX96724_BACKTOP32_BPP10DBL0			BIT(4)
#define MAX96724_BACKTOP32_BPP10DBL0_MODE		BIT(5)
#define MAX96724_BACKTOP32_BPP10DBL1			BIT(6)
#define MAX96724_BACKTOP32_BPP10DBL1_MODE		BIT(7)

#define MAX96724_FSYNC_0						0x4a0
#define MAX96724_FSYNC_0_FSYNC_METH				GENMASK(1, 0)
#define MAX96724_FSYNC_0_FSYNC_METH_MANUAL		0b00
#define MAX96724_FSYNC_0_FSYNC_METH_SEMI_AUTO	0b01
#define MAX96724_FSYNC_0_FSYNC_METH_AUTO		0b10
#define MAX96724_FSYNC_0_FSYNC_MODE				GENMASK(3, 2)
#define MAX96724_FSYNC_0_FSYNC_MODE_GEN_ON_IO_OFF	0b00
#define MAX96724_FSYNC_0_FSYNC_MODE_GEN_ON_IO_ON	0b01
#define MAX96724_FSYNC_0_FSYNC_MODE_GEN_OFF_IO_ON	0b10
#define MAX96724_FSYNC_0_FSYNC_MODE_GEN_OFF_IO_OFF	0b11
#define MAX96724_FSYNC_0_EN_VS_GEN				BIT(4)
#define MAX96724_FSYNC_0_FSYNC_OUT_PIN			BIT(5)

#define MAX96724_FSYNC_2						0x4a2
#define MAX96724_FSYNC_2_K_VAL					GENMASK(3, 0)
#define MAX96724_FSYNC_2_K_VAL_1P71US			0x1
#define MAX96724_FSYNC_2_K_VAL_SIGN				BIT(4)
#define MAX96724_FSYNC_2_MST_LINK_SEL			GENMASK(7, 5)
#define MAX96724_FSYNC_2_MST_LINK_SEL_AUTO		0b100

#define MAX96724_FSYNC_5_PERIOD_L				0x4a5
#define MAX96724_FSYNC_6_PERIOD_M				0x4a6
#define MAX96724_FSYNC_7_PERIOD_H				0x4a7

#define MAX96724_FSYNC_15						0x4af
#define MAX96724_FSYNC_15_FS_LINK				GENMASK(3, 0)
#define MAX96724_FSYNC_15_AUTO_FS_LINKS			BIT(4)
#define MAX96724_FSYNC_15_FS_USE_XTAL			BIT(6)
#define MAX96724_FSYNC_15_FS_GPIO_TPYE			BIT(7)

#define MAX96724_FSYNC_17						0x4b1
#define MAX96724_FSYNC_17_FSYNC_TX_ID			GENMASK(7, 3)

#define MAX96724_FSYNC_23						0x4b7
#define MAX96724_FSYNC_23_FSYNC_RST_MODE		BIT(5)

#define MAX96724_MIPI_PHY0						0x8a0
#define MAX96724_MIPI_PHY0_PHY_CONFIG			GENMASK(4, 0)
#define MAX96724_MIPI_PHY0_PHY_4X2				BIT(0)
#define MAX96724_MIPI_PHY0_PHY_2X4				BIT(2)
#define MAX96724_MIPI_PHY0_PHY_1X4A_2X2			BIT(3)
#define MAX96724_MIPI_PHY0_PHY_1X4B_2X2			BIT(4)
#define MAX96724_MIPI_PHY0_FORCE_CSI_OUT_EN		BIT(7)

#define MAX96724_MIPI_PHY2						0x8a2
#define MAX96724_MIPI_PHY2_PHY_STDB_N_4(x)		(GENMASK(5, 4) << ((x) / 2 * 2))
#define MAX96724_MIPI_PHY2_PHY_STDB_N_2(x)		(BIT(4 + (x)))

#define MAX96724_MIPI_PHY3(x)					(0x8a3 + (x) / 2)
#define MAX96724_MIPI_PHY3_PHY_LANE_MAP_4		GENMASK(7, 0)
#define MAX96724_MIPI_PHY3_PHY_LANE_MAP_2(x)	(GENMASK(3, 0) << (4 * ((x) % 2)))

#define MAX96724_MIPI_PHY5(x)					(0x8a5 + (x) / 2)
#define MAX96724_MIPI_PHY5_PHY_POL_MAP_4_0_1	GENMASK(1, 0)
#define MAX96724_MIPI_PHY5_PHY_POL_MAP_4_2_3	GENMASK(4, 3)
#define MAX96724_MIPI_PHY5_PHY_POL_MAP_4_CLK	BIT(5)
#define MAX96724_MIPI_PHY5_PHY_POL_MAP_2(x)		(GENMASK(1, 0) << (3 * ((x) % 2)))
#define MAX96724_MIPI_PHY5_PHY_POL_MAP_2_CLK(x)	BIT(2 + 3 * ((x) % 2))

#define MAX96724_MIPI_PHY13						0x8ad
#define MAX96724_MIPI_PHY13_T_T3_PREBEGIN		GENMASK(5, 0)
#define MAX96724_MIPI_PHY13_T_T3_PREBEGIN_64X7	field_prep(MAX96724_MIPI_PHY13_T_T3_PREBEGIN, 63)

#define MAX96724_MIPI_PHY14						0x8ae
#define MAX96724_MIPI_PHY14_T_T3_PREP			GENMASK(1, 0)
#define MAX96724_MIPI_PHY14_T_T3_PREP_55NS		field_prep(MAX96724_MIPI_PHY14_T_T3_PREP, 0b01)
#define MAX96724_MIPI_PHY14_T_T3_POST			GENMASK(6, 2)
#define MAX96724_MIPI_PHY14_T_T3_POST_32X7		field_prep(MAX96724_MIPI_PHY14_T_T3_POST, 31)

#define MAX96724_MIPI_CTRL_SEL					0x8ca
#define MAX96724_MIPI_CTRL_SEL_MASK(p)			(GENMASK(1, 0) << ((p) * 2))

#define MAX96724_MIPI_PHY25(x)					(0x8d0 + (x) / 2)
#define MAX96724_MIPI_PHY25_CSI2_TX_PKT_CNT(x)	(GENMASK(3, 0) << (4 * ((x) % 2)))

#define MAX96724_MIPI_PHY27(x)					(0x8d2 + (x) / 2)
#define MAX96724_MIPI_PHY27_PHY_PKT_CNT(x)		(GENMASK(3, 0) << (4 * ((x) % 2)))

#define MAX96724_MIPI_TX3(x)					(0x903 + (x) * 0x40)
#define MAX96724_MIPI_TX3_DESKEW_INIT_8X32K		field_prep(GENMASK(2, 0), 0b001)
#define MAX96724_MIPI_TX3_DESKEW_INIT_AUTO		BIT(7)

#define MAX96724_MIPI_TX4(x)					(0x904 + (x) * 0x40)
#define MAX96724_MIPI_TX4_DESKEW_PER_2K			field_prep(GENMASK(2, 0), 0b001)
#define MAX96724_MIPI_TX4_DESKEW_PER_AUTO		BIT(7)

#define MAX96724_MIPI_TX10(x)					(0x90a + (x) * 0x40)
#define MAX96724_MIPI_TX10_CSI2_CPHY_EN			BIT(5)
#define MAX96724_MIPI_TX10_CSI2_LANE_CNT		GENMASK(7, 6)

#define MAX96724_MIPI_TX11(p)					(0x90b + (p) * 0x40)
#define MAX96724_MIPI_TX12(p)					(0x90c + (p) * 0x40)

#define MAX96724_MIPI_TX13(p, x)				(0x90d + (p) * 0x40 + (x) * 0x2)
#define MAX96724_MIPI_TX13_MAP_SRC_DT			GENMASK(5, 0)
#define MAX96724_MIPI_TX13_MAP_SRC_VC			GENMASK(7, 6)

#define MAX96724_MIPI_TX14(p, x)				(0x90e + (p) * 0x40 + (x) * 0x2)
#define MAX96724_MIPI_TX14_MAP_DST_DT			GENMASK(5, 0)
#define MAX96724_MIPI_TX14_MAP_DST_VC			GENMASK(7, 6)

#define MAX96724_MIPI_TX45(p, x)				(0x92d + (p) * 0x40 + (x) / 4)
#define MAX96724_MIPI_TX45_MAP_DPHY_DEST(x)		(GENMASK(1, 0) << (2 * ((x) % 4)))

#define MAX96724_MIPI_TX51(x)					(0x933 + (x) * 0x40)
#define MAX96724_MIPI_TX51_ALT_MEM_MAP_12		BIT(0)
#define MAX96724_MIPI_TX51_ALT_MEM_MAP_8		BIT(1)
#define MAX96724_MIPI_TX51_ALT_MEM_MAP_10		BIT(2)
#define MAX96724_MIPI_TX51_ALT2_MEM_MAP_8		BIT(4)

#define MAX96724_MIPI_TX54(x)					(0x936 + (x) * 0x40)
#define MAX96724_MIPI_TX54_TUN_EN				BIT(0)

#define MAX96724_MIPI_TX57(x)					(0x939 + (x) * 0x40)
#define MAX96724_MIPI_TX57_TUN_DEST				GENMASK(5, 4)
#define MAX96724_MIPI_TX57_DIS_AUTO_TUN_DET		BIT(6)

#define MAX96724_DE_DET							0x11f0
#define MAX96724_HS_DET							0x11f1
#define MAX96724_VS_DET							0x11f2
#define MAX96724_HS_POL							0x11f3
#define MAX96724_VS_POL							0x11f4
#define MAX96724_DET(p)							BIT(p)

#define MAX96724_RLMS49(x)						(0x1449 + (x) * 0x100)

#define MAX96724_DPLL_0(x)						(0x1c00 + (x) * 0x100)
#define MAX96724_DPLL_0_CONFIG_SOFT_RST_N		BIT(0)

#define MAX96724_PHYS_NUM			4
#define MAX96724_PHY1_ALT_CLOCK		5
#define MAX96724_NAME 				"max96724"
#define MAX96724_GPIO_NUM 			9

static const struct regmap_config max96724_i2c_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0x1f00,
};

struct max96724_priv {
	struct max_des_priv des_priv;

	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;

	struct pinctrl_dev *pctldev;
	struct pinctrl_desc pctldesc;
	struct gpio_chip gc;

	unsigned int i2c_addr;
	unsigned int source_id;

	struct gpio_desc *reset_gpio;
};

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

#define des_to_priv(des) \
	container_of(des, struct max96724_priv, des_priv)

static int max96724_read(struct max96724_priv *priv, int reg)
{
	int ret, val;
	int cnt = 3;

	while (cnt--) {
		ret = regmap_read(priv->regmap, reg, &val);
		dev_dbg(priv->dev, "%s(): read %d 0x%x = 0x%02x\n", __func__, ret, reg, val);
		if (!ret) {
			break;
		}
		dev_dbg(priv->dev, "%s(): retry %d\n", __func__, cnt);
	}

	if (ret) {
		dev_err(priv->dev, "read 0x%04x failed\n", reg);
		return ret;
	}

	return val;
}

static int max96724_write(struct max96724_priv *priv, unsigned int reg, u8 val)
{
	int ret;
	int cnt = 3;

	while (cnt--) {
		ret = regmap_write(priv->regmap, reg, val);
		dev_dbg(priv->dev, "%s(): write %d 0x%x = 0x%02x\n", __func__, ret, reg, val);
		if (!ret) {
			break;
		}
		dev_dbg(priv->dev, "%s(): retry %d\n", __func__, cnt);
	}

	if (ret)
		dev_err(priv->dev, "write 0x%04x failed\n", reg);

	return ret;
}

static int max96724_update_bits(struct max96724_priv *priv, unsigned int reg,
			        u8 mask, u8 val)
{
	int ret;
	int cnt = 3;

	while (cnt--) {
		ret = regmap_update_bits(priv->regmap, reg, mask, val);
		dev_dbg(priv->dev, "%s(): update %d 0x%x 0x%02x = 0x%02x\n", __func__, ret, reg, mask, val);
		if (!ret) {
			break;
		}
		dev_dbg(priv->dev, "%s(): retry %d\n", __func__, cnt);
	}

	if (ret)
		dev_err(priv->dev, "update 0x%04x failed\n", reg);

	return ret;
}

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
	{ "maxim,pull-strength-weak", MAX_PINCONF_PARAM(MAX96724_PINCTRL_PULL_STRENGTH_WEAK), 0 },
	{ "maxim,jitter-compensation", MAX_PINCONF_PARAM(MAX96724_PINCTRL_JITTER_COMPENSATION_EN), 0 },
	{ "maxim,gmsl-tx-a", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_TX_EN_A), 0 },
	{ "maxim,gmsl-rx-a", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_RX_EN_A), 0 },
	{ "maxim,gmsl-tx-id-a", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_TX_ID_A), 0 },
	{ "maxim,gmsl-rx-id-a", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_RX_ID_A), 0 },
	{ "maxim,gmsl-tx-b", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_TX_EN_B), 0 },
	{ "maxim,gmsl-rx-b", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_RX_EN_B), 0 },
	{ "maxim,gmsl-tx-id-b", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_TX_ID_B), 0 },
	{ "maxim,gmsl-rx-id-b", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_RX_ID_B), 0 },
	{ "maxim,gmsl-tx-c", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_TX_EN_C), 0 },
	{ "maxim,gmsl-rx-c", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_RX_EN_C), 0 },
	{ "maxim,gmsl-tx-id-c", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_TX_ID_C), 0 },
	{ "maxim,gmsl-rx-id-c", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_RX_ID_C), 0 },
	{ "maxim,gmsl-tx-d", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_TX_EN_D), 0 },
	{ "maxim,gmsl-rx-d", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_RX_EN_D), 0 },
	{ "maxim,gmsl-tx-id-d", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_TX_ID_D), 0 },
	{ "maxim,gmsl-rx-id-d", MAX_PINCONF_PARAM(MAX96724_PINCTRL_GMSL_RX_ID_D), 0 },
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
	*reg = MAX96724_GPIO_A(offset);

	switch (param) {
	case PIN_CONFIG_OUTPUT_ENABLE:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*mask = MAX96724_GPIO_A_GPIO_OUT_DIS;
		*val = 0b0;
		return 0;
	case PIN_CONFIG_INPUT_ENABLE:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*mask = MAX96724_GPIO_A_GPIO_OUT_DIS;
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_GMSL_TX_EN_A:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*mask = MAX96724_GPIO_A_GPIO_TX_EN;
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_EN_A:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*mask = MAX96724_GPIO_A_GPIO_RX_EN;
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_INPUT_VALUE:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*mask = MAX96724_GPIO_A_GPIO_IN;
		*val = 0b1;
		return 0;
	case PIN_CONFIG_OUTPUT:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*mask = MAX96724_GPIO_A_GPIO_OUT;
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_JITTER_COMPENSATION_EN:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*mask = MAX96724_GPIO_A_TX_COMP_EN;
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_PULL_STRENGTH_WEAK:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*mask = MAX96724_GPIO_A_RES_CFG;
		*val = 0b0;
		return 0;
	case MAX96724_PINCTRL_GMSL_TX_ID_A:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*reg += 1;
		*mask = MAX96724_GPIO_B_GPIO_TX_ID;
		return 0;
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*reg += 1;
		*mask = MAX96724_GPIO_B_OUT_TYPE;
		*val = 0b0;
		return 0;
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*reg += 1;
		*mask = MAX96724_GPIO_B_OUT_TYPE;
		*val = 0b1;
		return 0;
	case PIN_CONFIG_BIAS_DISABLE:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*reg += 1;
		*mask = MAX96724_GPIO_B_PULL_UPDN_SEL;
		*val = MAX96724_GPIO_B_PULL_UPDN_SEL_NONE;
		return 0;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*reg += 1;
		*mask = MAX96724_GPIO_B_PULL_UPDN_SEL;
		*val = MAX96724_GPIO_B_PULL_UPDN_SEL_PD;
		return 0;
	case PIN_CONFIG_BIAS_PULL_UP:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*reg += 1;
		*mask = MAX96724_GPIO_B_PULL_UPDN_SEL;
		*val = MAX96724_GPIO_B_PULL_UPDN_SEL_PU;
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_ID_A:
		if (offset > 4)
			*reg += 1;
		if (offset > 9)
			*reg += 1;
		*reg += 2;
		*mask = MAX96724_GPIO_C_GPIO_RX_ID;
		return 0;
	}

	*reg = MAX96724_GPIO_B_B(offset);

	switch (param) {
	case MAX96724_PINCTRL_GMSL_TX_ID_B:
		if (offset > 2)
			*reg += 1;
		if (offset > 7)
			*reg += 1;
		*mask = MAX96724_GPIO_B_GPIO_TX_ID_B;
		return 0;
	case MAX96724_PINCTRL_GMSL_TX_EN_B:
		if (offset > 2)
			*reg+=1;
		if (offset > 7)
			*reg+=1;
		*mask = MAX96724_GPIO_B_GPIO_TX_EN_B;
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_ID_B:
		if (offset > 2)
			*reg+=1;
		if (offset > 7)
			*reg+=1;
		*reg += 1;
		*mask = MAX96724_GPIO_C_GPIO_RX_ID_B;
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_EN_B:
		if (offset > 2)
			*reg+=1;
		if (offset > 7)
			*reg+=1;
		*reg += 1;
		*mask = MAX96724_GPIO_C_GPIO_RX_EN_B;
		*val = 0b1;
		return 0;
	}

	*reg = MAX96724_GPIO_B_C(offset);

	switch (param) {
	case MAX96724_PINCTRL_GMSL_TX_ID_C:
		if (offset > 0)
			*reg+=1;
		if (offset > 5)
			*reg+=1;
		*mask = MAX96724_GPIO_B_GPIO_TX_ID_C;
		return 0;
	case MAX96724_PINCTRL_GMSL_TX_EN_C:
		if (offset > 0)
			*reg+=1;
		if (offset > 5)
			*reg+=1;
		*mask = MAX96724_GPIO_B_GPIO_TX_EN_C;
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_ID_C:
		if (offset > 0)
			*reg+=1;
		if (offset > 5)
			*reg+=1;
		*reg += 1;
		*mask = MAX96724_GPIO_C_GPIO_RX_ID_C;
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_EN_C:
		if (offset > 0)
			*reg+=1;
		if (offset > 5)
			*reg+=1;
		*reg += 1;
		*mask = MAX96724_GPIO_C_GPIO_RX_EN_C;
		*val = 0b1;
		return 0;
	}

	*reg = MAX96724_GPIO_B_D(offset);

	switch (param) {
	case MAX96724_PINCTRL_GMSL_TX_ID_D:
		if (offset > 3)
			*reg += 1;
		if (offset > 8)
			*reg += 1;
		*mask = MAX96724_GPIO_B_GPIO_TX_ID_D;
		return 0;
	case MAX96724_PINCTRL_GMSL_TX_EN_D:
		if (offset > 3)
			*reg+=1;
		if (offset > 8)
			*reg += 1;
		*mask = MAX96724_GPIO_B_GPIO_TX_EN_D;
		*val = 0b1;
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_ID_D:
		if (offset > 3)
			*reg+=1;
		if (offset > 8)
			*reg += 1;
		*reg += 1;
		*mask = MAX96724_GPIO_C_GPIO_RX_ID_D;
		return 0;
	case MAX96724_PINCTRL_GMSL_RX_EN_D:
		if (offset > 3)
			*reg+=1;
		if (offset > 8)
			*reg += 1;
		*reg += 1;
		*mask = MAX96724_GPIO_C_GPIO_RX_EN_D;
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

		val = field_get(mask, ret) == val;
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

		val = field_get(mask, ret) == val;
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

		val = field_get(mask, val);
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

	ret = max96724_get_pin_config_reg(offset, param, &reg, &mask, &val);
	if (ret)
		return ret;

	switch (param) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
	case PIN_CONFIG_DRIVE_PUSH_PULL:
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		val = field_prep(mask, val);

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
		val = field_prep(mask, arg ? val : ~val);

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
		val = field_prep(mask, arg);

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

	return max96724_conf_pin_config_set_one(priv, offset, config);
}

static int max96724_gpio_direction_output(struct gpio_chip *gc,
					  unsigned int offset, int value)
{
	unsigned long config =
		pinconf_to_config_packed(PIN_CONFIG_OUTPUT, value);
	struct max96724_priv *priv = gpiochip_get_data(gc);

	return max96724_conf_pin_config_set_one(priv, offset, config);
}

static int max96724_gpio_get(struct gpio_chip *gc, unsigned int offset)
{
	unsigned long config =
		pinconf_to_config_packed(MAX_PINCONF_PARAM(MAX96724_PINCTRL_INPUT_VALUE), 0);
	struct max96724_priv *priv = gpiochip_get_data(gc);
	int ret;

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

static int max96724_log_pipe_status(struct max_des_priv *des_priv,
				    struct max_des_pipe *pipe, const char *name)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	unsigned int index = pipe->index;
	int ret;

	ret = max96724_read(priv, MAX96724_VPRBS(index));
	if (ret < 0)
		return ret;

	ret = ret & MAX96724_VPRBS_VIDEO_LOCK;
	pr_info("%s: \tvideo_lock: %u\n", name, ret);

	return 0;
}

static int max96724_log_phy_status(struct max_des_priv *des_priv,
				   struct max_des_phy *phy, const char *name)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	unsigned int index = phy->index;
	int ret;

	ret = max96724_read(priv, MAX96724_MIPI_PHY25(index));
	if (ret < 0)
		return ret;

	pr_info("%s: \tcsi2_pkt_cnt: %lu\n", name,
			field_get(MAX96724_MIPI_PHY25_CSI2_TX_PKT_CNT(index), ret));

	ret = max96724_read(priv, MAX96724_MIPI_PHY27(index));
	if (ret < 0)
		return ret;

	pr_info("%s: \tphy_pkt_cnt: %lu\n", name,
			field_get(MAX96724_MIPI_PHY27_PHY_PKT_CNT(index), ret));

	return 0;
}

static int max96724_mipi_enable(struct max_des_priv *des_priv, bool enable)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	int ret;

	if (enable) {
		ret = max96724_update_bits(priv, MAX96724_BACKTOP12,
					   MAX96724_BACKTOP12_CSI_OUT_EN,
					   MAX96724_BACKTOP12_CSI_OUT_EN);
		if (ret)
			return ret;

		if (des_priv->fsync->internal ||
			des_priv->fsync->internal_output) {
			ret = max96724_update_bits(priv, MAX96724_FSYNC_23,
						   MAX96724_FSYNC_23_FSYNC_RST_MODE,
						   MAX96724_FSYNC_23_FSYNC_RST_MODE);
			if (ret)
				return ret;
		}
	} else {
		if (des_priv->fsync->internal ||
			des_priv->fsync->internal_output) {
			ret = max96724_update_bits(priv, MAX96724_FSYNC_23,
						   MAX96724_FSYNC_23_FSYNC_RST_MODE,
						   0x00);
			if (ret)
				return ret;
		}

		ret = max96724_update_bits(priv, MAX96724_BACKTOP12,
					   MAX96724_BACKTOP12_CSI_OUT_EN,
					   0x00);
		if (ret)
			return ret;
	}

	return 0;
}

static int max96724_set_pipe_enable(struct max_des_priv *des_priv,
				    struct max_des_pipe *pipe, bool enable)
{
	struct max96724_priv *priv = des_to_priv(des_priv);

	return max96724_update_bits(priv, MAX96724_VIDEO_PIPE_EN,
				   MAX96724_VIDEO_PIPE_EN_MASK(pipe->index),
				   field_prep(MAX96724_VIDEO_PIPE_EN_MASK(pipe->index),
				   		enable));
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

	ret = max96724_update_bits(priv, MAX96724_MIPI_PHY0,
				   MAX96724_MIPI_PHY0_PHY_CONFIG,
				   max96724_lane_configs[i].bit);
	if (ret)
		return ret;

	return 0;
}

static int max96724_check_gmsl_links(struct max_des_priv *des_priv)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	unsigned int locked_links_mask = 0;
	unsigned int links_mask = des_priv->gmsl_link_mask;
	u16 link_lock_addr[4] = {
		MAX96724_CTRL3,
		MAX96724_CTRL12,
		MAX96724_CTRL13,
		MAX96724_CTRL14
	};
	unsigned long timeout;

	dev_dbg(priv->dev, "%s()\n", __func__);

	max96724_update_bits(priv, MAX96724_REG6,
				   MAX96724_REG6_LINK_EN,
				   des_priv->gmsl_link_mask);

	timeout = jiffies + msecs_to_jiffies(100);

	while (!time_after(jiffies, timeout)) {
		int current_link = ffs(links_mask) - 1;

		if (current_link == -1)
			break;

		des_priv->links[current_link].enabled = false;
		if ((max96724_read(priv, link_lock_addr[current_link]) & MAX96724_CTRL3_LOCKED_A)) {
			locked_links_mask |= BIT(current_link);
			des_priv->links[current_link].enabled = true;
		}

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

	while (retries--) {
		locked_links = max96724_check_gmsl_links(des_priv);
		if (locked_links == des_priv->gmsl_link_mask)
			break;

		max96724_update_bits(priv, MAX96724_CTRL1,
					   MAX96724_CTRL1_RESET_LINK,
					   MAX96724_CTRL1_RESET_LINK);
		usleep_range(2000, 2500);
		max96724_update_bits(priv, MAX96724_CTRL1,
					   MAX96724_CTRL1_RESET_LINK,
					   0);
		usleep_range(2000, 2500);
	}

	if (locked_links == 0) {
		dev_err(priv->dev, "No GMSL link has locked after 3 retries. Abort!\n");
		return -ENODEV;
	}

	dev_info(priv->dev, "GMSL link has locked - mask [0x%x]\n", locked_links);

	/* Disable all PHYs. */
	ret = max96724_update_bits(priv, MAX96724_MIPI_PHY2,
				   GENMASK(7, 4), 0x00);
	if (ret)
		return ret;

	ret = max96724_update_bits(priv, MAX96724_VIDEO_PIPE_EN,
				   MAX96724_VIDEO_PIPE_EN_STREAM_SEL_ALL,
				   des_priv->pipe_stream_autoselect
				   ? MAX96724_VIDEO_PIPE_EN_STREAM_SEL_ALL
				   : 0x00);
	if (ret)
		return ret;

	/* Disable all pipes. */
	ret = max96724_update_bits(priv, MAX96724_VIDEO_PIPE_EN,
				   GENMASK(3, 0), 0x00);
	if (ret)
		return ret;

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
	unsigned int val, shift, mask, clk_bit;
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

	ret = max96724_update_bits(priv, MAX96724_MIPI_TX10(index),
				   MAX96724_MIPI_TX10_CSI2_LANE_CNT,
				   field_prep(MAX96724_MIPI_TX10_CSI2_LANE_CNT,
					    num_data_lanes - 1));
	if (ret)
		return ret;

	/* Configure lane mapping. */
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

	if (num_hw_data_lanes == 4) {
		mask = MAX96724_MIPI_PHY3_PHY_LANE_MAP_4;
	} else {
		mask = MAX96724_MIPI_PHY3_PHY_LANE_MAP_2(index);
	}

	ret = max96724_update_bits(priv, MAX96724_MIPI_PHY3(index), mask,
				   field_prep(mask, val));
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

	val = 0;
	for (i = 0; i < num_data_lanes + 1; i++)
		if (phy->mipi.lane_polarities[i])
			val |= BIT(i == 0 ? clk_bit : i < 3 ? i - 1 : i);
	ret = max96724_update_bits(priv, MAX96724_MIPI_PHY5(index), mask << shift, val << shift);
	if (ret)
		return ret;

	if (dpll_freq > 1500000000ull) {
		/* Enable initial deskew with 8 x 32k UI. */
		ret = max96724_write(priv, MAX96724_MIPI_TX3(index),
				   MAX96724_MIPI_TX3_DESKEW_INIT_AUTO |
				   MAX96724_MIPI_TX3_DESKEW_INIT_8X32K);
		if (ret)
			return ret;

		/* Enable periodic deskew with 2 x 1k UI.. */
		ret = max96724_write(priv, MAX96724_MIPI_TX4(index),
				   MAX96724_MIPI_TX4_DESKEW_PER_AUTO |
				   MAX96724_MIPI_TX4_DESKEW_PER_2K);
		if (ret)
			return ret;
	} else {
		/* Disable initial deskew. */
		ret = max96724_write(priv, MAX96724_MIPI_TX3(index), 0x07);
		if (ret)
			return ret;

		/* Disable periodic deskew. */
		ret = max96724_write(priv, MAX96724_MIPI_TX4(index), 0x01);
		if (ret)
			return ret;
	}

	/* Put DPLL block into reset. */
	ret = max96724_update_bits(priv, MAX96724_DPLL_0(index),
				   MAX96724_DPLL_0_CONFIG_SOFT_RST_N, 0);
	if (ret)
		return ret;

	/* Set DPLL frequency. */
	ret = max96724_update_bits(priv, MAX96724_BACKTOP22(index),
				   MAX96724_BACKTOP22_PHY_CSI_TX_DPLL,
				   field_prep(MAX96724_BACKTOP22_PHY_CSI_TX_DPLL,
					    div_u64(dpll_freq, 100000000)));
	if (ret)
		return ret;

	/* Enable DPLL frequency. */
	ret = max96724_update_bits(priv, MAX96724_BACKTOP22(index),
				   MAX96724_BACKTOP22_PHY_CSI_TX_DPLL_EN,
				   MAX96724_BACKTOP22_PHY_CSI_TX_DPLL_EN);
	if (ret)
		return ret;

	/* Pull DPLL block out of reset. */
	ret = max96724_update_bits(priv, MAX96724_DPLL_0(index),
				   MAX96724_DPLL_0_CONFIG_SOFT_RST_N,
				   MAX96724_DPLL_0_CONFIG_SOFT_RST_N);
	if (ret)
		return ret;

	/* Set alternate memory map modes. */
	val  = phy->alt_mem_map12 ? MAX96724_MIPI_TX51_ALT_MEM_MAP_12 : 0;
	val |= phy->alt_mem_map8 ? MAX96724_MIPI_TX51_ALT_MEM_MAP_8 : 0;
	val |= phy->alt_mem_map10 ? MAX96724_MIPI_TX51_ALT_MEM_MAP_10 : 0;
	val |= phy->alt2_mem_map8 ? MAX96724_MIPI_TX51_ALT2_MEM_MAP_8 : 0;
	ret = max96724_update_bits(priv, MAX96724_MIPI_TX51(index),
				   GENMASK(4, 0), val);
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

	ret = max96724_update_bits(priv, MAX96724_MIPI_PHY2, mask, mask);
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
	int ret;

	/* Set source Data Type and Virtual Channel. */
	/* TODO: implement extended Virtual Channel. */
	ret = max96724_write(priv, MAX96724_MIPI_TX13(index, i),
			     MAX_DES_DT_VC(remap->from_dt, remap->from_vc));
	if (ret)
		return ret;

	/* Set destination Data Type and Virtual Channel. */
	/* TODO: implement extended Virtual Channel. */
	ret = max96724_write(priv, MAX96724_MIPI_TX14(index, i),
			     MAX_DES_DT_VC(remap->to_dt, remap->to_vc));
	if (ret)
		return ret;

	/* Set destination PHY. */
	ret = max96724_update_bits(priv, MAX96724_MIPI_TX45(index, i),
				   MAX96724_MIPI_TX45_MAP_DPHY_DEST(i),
				   field_prep(MAX96724_MIPI_TX45_MAP_DPHY_DEST(i),
				   		remap->phy));
	if (ret)
		return ret;

	/* Enable remap. */
	ret = max96724_update_bits(priv, MAX96724_MIPI_TX11(index) + i / 8,
				   BIT(i % 8), BIT(i % 8));
	if (ret)
		return ret;

	return 0;
}

static int max96724_init_pipe(struct max_des_priv *des_priv,
			      struct max_des_pipe *pipe)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	unsigned int index = pipe->index;
	unsigned int reg, mask;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	/* Set destination PHY. */
	ret = max96724_update_bits(priv, MAX96724_MIPI_CTRL_SEL,
				   MAX96724_MIPI_CTRL_SEL_MASK(index),
				   field_prep(MAX96724_MIPI_CTRL_SEL_MASK(index),
				   		pipe->phy_id));
	if (ret)
		return ret;

	ret = max96724_update_bits(priv, MAX96724_MIPI_TX57(index),
				   MAX96724_MIPI_TX57_TUN_DEST,
				   field_prep(MAX96724_MIPI_TX57_TUN_DEST,
						pipe->phy_id));
	if (ret)
		return ret;

	/* Enable pipe. */
	ret = max96724_set_pipe_enable(des_priv, pipe, false);
	if (ret)
		return ret;

	if (!des_priv->pipe_stream_autoselect) {
		/* Set source stream. */
		ret = max96724_update_bits(priv, MAX96724_VIDEO_PIPE_SEL(index),
					   MAX96724_VIDEO_PIPE_SEL_STREAM(index),
					   field_prep(MAX96724_VIDEO_PIPE_SEL_STREAM(index),
					   		pipe->stream_id));
		if (ret)
			return ret;
	}

	/* Set source link. */
	ret = max96724_update_bits(priv, MAX96724_VIDEO_PIPE_SEL(index),
				   MAX96724_VIDEO_PIPE_SEL_LINK(index),
				   field_prep(MAX96724_VIDEO_PIPE_SEL_LINK(index),
						pipe->link_id));
	if (ret)
		return ret;

	/* Set 8bit double mode. */
	ret = max96724_update_bits(priv, MAX96724_BACKTOP21,
				   MAX96724_BACKTOP21_BPP8DBL(index),
				   field_prep(MAX96724_BACKTOP21_BPP8DBL(index),
					   		pipe->dbl8));
	if (ret)
		return ret;

	ret = max96724_update_bits(priv, MAX96724_BACKTOP24,
				   MAX96724_BACKTOP24_BPP8DBL_MODE(index),
				   field_prep(MAX96724_BACKTOP24_BPP8DBL_MODE(index),
					   		pipe->dbl8mode));
	if (ret)
		return ret;

	/* Set 10bit double mode. */
	if (index == 3) {
		reg = MAX96724_BACKTOP30;
		mask = MAX96724_BACKTOP30_BPP10DBL3;
	} else if (index == 2) {
		reg = MAX96724_BACKTOP31;
		mask = MAX96724_BACKTOP31_BPP10DBL2;
	} else if (index == 1) {
		reg = MAX96724_BACKTOP32;
		mask = MAX96724_BACKTOP32_BPP10DBL1;
	} else {
		reg = MAX96724_BACKTOP32;
		mask = MAX96724_BACKTOP32_BPP10DBL0;
	}

	ret = max96724_update_bits(priv, reg,
				   mask | (mask << 1),
				   (pipe->dbl10 ? mask : 0) |
				   (pipe->dbl10mode ? (mask << 1) : 0));
	if (ret)
		return ret;

	/* Set 12bit double mode. */
	ret = max96724_update_bits(priv, MAX96724_BACKTOP32,
				   MAX96724_BACKTOP32_BPP12(index),
				   field_prep(MAX96724_BACKTOP32_BPP12(index),
					   		pipe->dbl12));
	if (ret)
		return ret;

	return 0;
}

static int max96724_init_link(struct max_des_priv *des_priv,
				  struct max_des_link *link)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	unsigned int index = link->index;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	/* RLMS register setting for robust 6Gbps GMSL2 rate */
	ret = max96724_write(priv, MAX96724_RLMS49(index), 0x75);
	if (ret)
		return ret;

	ret = max96724_update_bits(priv, MAX96724_CTRL1,
				   MAX96724_CTRL1_RESET_ONESHOT,
				   field_prep(MAX96724_CTRL1_RESET_ONESHOT, index));
	if (ret)
		return ret;
	msleep(60);

	return 0;
}

static int max96724_init_fsync(struct max_des_priv *des_priv,
				  struct max_des_fsync *fsync)
{
	struct max96724_priv *priv = des_to_priv(des_priv);
	int ret = 0;

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (fsync->internal || fsync->internal_output) {
		ret = max96724_update_bits(priv, MAX96724_FSYNC_17,
					   MAX96724_FSYNC_17_FSYNC_TX_ID,
					   field_prep(MAX96724_FSYNC_17_FSYNC_TX_ID, 0x00));
		if (ret)
			return ret;
		ret = max96724_update_bits(priv, MAX96724_FSYNC_2,
					   MAX96724_FSYNC_2_K_VAL |
					   MAX96724_FSYNC_2_MST_LINK_SEL,
					   field_prep(MAX96724_FSYNC_2_K_VAL,
							MAX96724_FSYNC_2_K_VAL_1P71US) |
					   		field_prep(MAX96724_FSYNC_2_MST_LINK_SEL,
							MAX96724_FSYNC_2_MST_LINK_SEL_AUTO));
		if (ret)
			return ret;
		ret = max96724_write(priv, MAX96724_FSYNC_7_PERIOD_H,
					   (fsync->freq >> 16) & 0xff);
		if (ret)
			return ret;
		ret = max96724_write(priv, MAX96724_FSYNC_6_PERIOD_M,
					   (fsync->freq >> 8) & 0xff);
		if (ret)
			return ret;
		ret = max96724_write(priv, MAX96724_FSYNC_5_PERIOD_L,
					   (fsync->freq >> 0) & 0xff);
		if (ret)
			return ret;
		ret = max96724_update_bits(priv, MAX96724_FSYNC_15,
					   MAX96724_FSYNC_15_FS_LINK,
					   MAX96724_FSYNC_15_FS_LINK);
		if (ret)
			return ret;
		ret = max96724_update_bits(priv, MAX96724_FSYNC_15,
					   MAX96724_FSYNC_15_AUTO_FS_LINKS,
					   0);
		if (ret)
			return ret;
		ret = max96724_update_bits(priv, MAX96724_FSYNC_15,
					   MAX96724_FSYNC_15_FS_USE_XTAL,
					   MAX96724_FSYNC_15_FS_USE_XTAL);
		if (ret)
			return ret;
		ret = max96724_update_bits(priv, MAX96724_FSYNC_15,
					   MAX96724_FSYNC_15_FS_GPIO_TPYE,
					   MAX96724_FSYNC_15_FS_GPIO_TPYE);
		if (ret)
			return ret;

		ret = max96724_update_bits(priv, MAX96724_FSYNC_0,
					   MAX96724_FSYNC_0_FSYNC_MODE,
					   field_prep(MAX96724_FSYNC_0_FSYNC_MODE,
					   		fsync->internal ?
					   		MAX96724_FSYNC_0_FSYNC_MODE_GEN_ON_IO_OFF :
							MAX96724_FSYNC_0_FSYNC_MODE_GEN_ON_IO_ON));
		if (ret)
			return ret;
	}
	else if (fsync->external) {
		ret = max96724_update_bits(priv, MAX96724_FSYNC_15,
					   MAX96724_FSYNC_15_FS_LINK,
					   MAX96724_FSYNC_15_FS_LINK);
		if (ret)
			return ret;
		ret = max96724_update_bits(priv, MAX96724_FSYNC_15,
					   MAX96724_FSYNC_15_AUTO_FS_LINKS,
					   0);
		if (ret)
			return ret;
		ret = max96724_update_bits(priv, MAX96724_FSYNC_15,
					   MAX96724_FSYNC_15_FS_USE_XTAL,
					   0);
		if (ret)
			return ret;
		ret = max96724_update_bits(priv, MAX96724_FSYNC_15,
					   MAX96724_FSYNC_15_FS_GPIO_TPYE,
					   MAX96724_FSYNC_15_FS_GPIO_TPYE);
		if (ret)
			return ret;
		ret = max96724_update_bits(priv, MAX96724_FSYNC_0,
					   MAX96724_FSYNC_0_FSYNC_MODE,
					   field_prep(MAX96724_FSYNC_0_FSYNC_MODE,
					   		MAX96724_FSYNC_0_FSYNC_MODE_GEN_OFF_IO_ON));
		if (ret)
			return ret;
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

	dev_dbg(priv->dev, "%s(): mask %d\n", __func__, mask);

	ret = max96724_update_bits(priv, MAX96724_REG6,
				   MAX96724_REG6_LINK_EN,
				   field_prep(MAX96724_REG6_LINK_EN, mask));
	if (ret)
		return ret;

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

	for_each_subdev(des_priv, sd_priv) {
		if (!des_priv->links[sd_priv->index].enabled)
			continue;

		pipe = &des_priv->pipes[sd_priv->pipe_id];

		fmt = max_format_by_name(pipe->code_name);
		if (!fmt)
			return -EINVAL;
		sd_priv->fmt = fmt;

		dev_dbg(des_priv->dev,
			"%s(): %s pipe_id [%d], phy_id [%d], src_vc_id [%d], dst_vc_id [%d] fmt [0x%04x]\n",
			__func__, sd_priv->label, sd_priv->pipe_id, sd_priv->phy_id,
			sd_priv->src_vc_id, sd_priv->dst_vc_id,	sd_priv->fmt->code);

		mutex_lock(&des_priv->lock);
		ret = max_des_update_pipe_remaps(des_priv, pipe);
		mutex_unlock(&des_priv->lock);
		if (ret)
			return -EINVAL;
	}

	return max96724_mipi_enable(des_priv, false);
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
	.set_pipe_enable = max96724_set_pipe_enable,
	.init = max96724_init,
	.init_phy = max96724_init_phy,
	.init_pipe = max96724_init_pipe,
	.init_link = max96724_init_link,
	.init_fsync = max96724_init_fsync,
	.update_pipe_remaps = max96724_update_pipe_remaps,
	.select_links = max96724_select_links,
	.post_init = max96724_post_init,
};

static int max96724_wait_for_multiple(struct i2c_client *client, struct regmap *regmap,
				u8 *addrs, unsigned int num_addrs)
{
	unsigned int i, j, val;
	int ret;

	for (i = 0; i < 10; i++) {
		for (j = 0; j < num_addrs; j++) {
			client->addr = addrs[j];

			ret = regmap_read(regmap, MAX96724_REG0, &val);
			if (ret >= 0) {
				dev_dbg(&client->dev, "%s(): Find deserializer addr: 0x%02x\n",
						__func__, client->addr);
				return 0;
			}
		}

		msleep(100);

		dev_dbg(&client->dev, "%s(): Retry %u waiting for deserializer: %d\n",
				__func__, i, ret);
	}

	return ret;
}

static int max96724_wait_for_device(struct max96724_priv *priv)
{
	unsigned int i;
	int ret;

	for (i = 0; i < 10; i++) {
		ret = max96724_read(priv, MAX96724_REG0);
		if (ret >= 0)
			return 0;

		msleep(100);

		dev_err(priv->dev, "Retry %u waiting for deserializer: %d\n", i, ret);
	}

	return ret;
}

static int max96724_reset(struct max96724_priv *priv)
{
	struct i2c_client *client;
	struct regmap *regmap;
	int ret;
	u8 max96724_addr[2] = { priv->client->addr, priv->i2c_addr };

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (priv->i2c_addr != priv->client->addr) {
		client = i2c_new_dummy_device(priv->client->adapter, priv->i2c_addr);
		if (IS_ERR(client)) {
			ret = PTR_ERR(client);
			dev_err(priv->dev,
				"Failed to create I2C client: %d\n", ret);
			return ret;
		}

		regmap = regmap_init_i2c(client, &max96724_i2c_regmap);
		if (IS_ERR(regmap)) {
			ret = PTR_ERR(regmap);
			dev_err(priv->dev,
				"Failed to create I2C regmap: %d\n", ret);
			goto err_unregister_client;
		}

		ret = max96724_wait_for_multiple(client, regmap, max96724_addr, ARRAY_SIZE(max96724_addr));
		if (ret) {
			dev_err(priv->dev,
				"Failed waiting for deserializer with new or old address: %d\n", ret);
			goto err_regmap_exit;
		}

		ret = regmap_write(regmap, MAX96724_PWR1, MAX96724_PWR1_RESET_ALL);
		if (ret) {
			dev_err(priv->dev, "Failed to soft reset deserializer: %d\n", ret);
			goto err_regmap_exit;
		}
		msleep(60);

		ret = max96724_wait_for_multiple(client, regmap, max96724_addr, ARRAY_SIZE(max96724_addr));
		if (ret) {
			dev_err(priv->dev,
				"Failed waiting for deserializer with new or old address: %d\n", ret);
			goto err_regmap_exit;
		}

		ret = regmap_write(regmap, MAX96724_REG0, priv->client->addr << 1);
		if (ret) {
			dev_err(priv->dev, "Failed to change deserializer address: %d\n", ret);
			goto err_regmap_exit;
		}

		dev_info(priv->dev, "change addr to 0x%x\n", client->addr);

err_regmap_exit:
		regmap_exit(regmap);

err_unregister_client:
		i2c_unregister_device(client);
	}
	else {
		ret = max96724_wait_for_device(priv);
		if (ret) {
			dev_err(priv->dev, "Failed waiting for MAX96724, err: %d\n", ret);
			return ret;
		}

		ret = max96724_update_bits(priv, MAX96724_PWR1,
					   MAX96724_PWR1_RESET_ALL,
					   MAX96724_PWR1_RESET_ALL);
		if (ret)
			return ret;

		msleep(60);

		ret = max96724_wait_for_device(priv);
		if (ret) {
			dev_err(priv->dev, "Failed waiting for MAX96724, err: %d\n", ret);
			return ret;
		}
	}

	return ret;
}

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

	priv->regmap = devm_regmap_init_i2c(client, &max96724_i2c_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	priv->des_priv.dev = dev;
	priv->des_priv.client = client;
	priv->des_priv.regmap = priv->regmap;
	priv->des_priv.ops = &max96724_ops;

	priv->i2c_addr = priv->client->addr;
	of_property_read_u32(dev->of_node, "phy-reg", &priv->i2c_addr);
	of_property_read_u32(dev->of_node, "source-id", &priv->source_id);
	if (priv->source_id > 0x7) {
		dev_err(dev, "source-id should be [0 - 7]\n");
		return -EINVAL;
	}

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

	ret = max96724_reset(priv);
	if (ret)
		return ret;

	/* Register pin controller */
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

static void max96724_remove(struct i2c_client *client)
{
	struct max96724_priv *priv = i2c_get_clientdata(client);

	max_des_remove(&priv->des_priv);
}

static const struct of_device_id max96724_of_table[] = {
	{ .compatible = "maxim,max96724_tn" },
	{ },
};
MODULE_DEVICE_TABLE(of, max96724_of_table);

static struct i2c_driver max96724_i2c_driver = {
	.driver	= {
		.name = MAX96724_NAME,
		.of_match_table	= of_match_ptr(max96724_of_table),
	},
	.probe = max96724_probe,
	.remove = max96724_remove,
};

module_i2c_driver(max96724_i2c_driver);

MODULE_DESCRIPTION("Maxim MAX96724 Quad GMSL2 Deserializer Driver");
MODULE_AUTHOR("TechNexion Inc.");
MODULE_LICENSE("GPL");
