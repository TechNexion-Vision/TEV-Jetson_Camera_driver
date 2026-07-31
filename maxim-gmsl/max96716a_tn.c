// SPDX-License-Identifier: GPL-2.0
/*
 * Maxim MAX96716A Quad GMSL2 Deserializer Driver
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

#define MAX96716A_REG0							0x0

#define MAX96716A_REG1							0x1
#define MAX96716A_REG1_RX_RATE_A				GENMASK(1, 0)
#define MAX96716A_REG1_RX_RATE_3Gbps			0b01
#define MAX96716A_REG1_RX_RATE_6Gbps			0b10
#define MAX96716A_REG1_RX_RATE_12Gbps			0b11
#define MAX96716A_REG1_DIS_REM_CC				BIT(4)

#define MAX96716A_REG2							0x2
#define MAX96716A_REG2_VID_EN(p)				BIT((p) + 4)

#define MAX96716A_REG3							0x3
#define MAX96716A_REG3_DIS_REM_CC_B				BIT(2)
#define MAX96716A_REG3_UART_1_EN				BIT(4)
#define MAX96716A_REG3_UART_2_EN				BIT(5)

#define MAX96716A_REG4							0x4
#define MAX96716A_REG4_RX_RATE_B				GENMASK(1, 0)

#define MAX96716A_REG5							0x5
#define MAX96716A_REG5_ERRB_EN					BIT(6)
#define MAX96716A_REG5_LOCK_EN					BIT(7)

#define MAX96716A_CTRL0							0x10
#define MAX96716A_CTRL0_LINK_CFG				GENMASK(1, 0)
#define MAX96716A_CTRL0_AUTO_LINK				BIT(4)
#define MAX96716A_CTRL0_RESET_ONESHOT			BIT(5)
#define MAX96716A_CTRL0_RESET_LINK				BIT(6)
#define MAX96716A_CTRL0_RESET_ALL				BIT(7)

#define MAX96716A_CTRL2							0x12
#define MAX96716A_CTRL2_RESET_ONESHOT_B			BIT(5)

#define MAX96716A_CTRL3							0x13
#define MAX96716A_CTRL3_LOCKED					BIT(3)
#define MAX96716A_CTRL3_LINK_MODE(x)			BIT((x) + 4)

#define MAX96716A_MIPI_TX0(x)					(0x28 + (x) * 0x5000)
#define MAX96716A_MIPI_TX0_RX_FEC_EN			BIT(1)

#define MAX96716A_RX50(p)						(0x50 + (p))
#define MAX96716A_RX50_STR_SEL					GENMASK(1, 0)

#define MAX96716A_VIDEO_PIPE_EN					0x160
#define MAX96716A_VIDEO_PIPE_EN_MASK(p)			BIT(p)

#define MAX96716A_VIDEO_PIPE_SEL				0x161
#define MAX96716A_VIDEO_PIPE_SEL_STREAM(p)		(GENMASK(1, 0) << ((p) * 3))
#define MAX96716A_VIDEO_PIPE_SEL_LINK(x)		BIT((x) * 3 + 2)

#define MAX96716A_VPRBS(p)						(0x1dc + (p) * 0x20)
#define MAX96716A_VPRBS_VIDEO_LOCK				BIT(0)

#define MAX96716A_GPIO_A(x)						(0x2b0 + (x) * 3)
#define MAX96716A_GPIO_A_GPIO_OUT_DIS			BIT(0)
#define MAX96716A_GPIO_A_GPIO_TX_EN				BIT(1)
#define MAX96716A_GPIO_A_GPIO_RX_EN				BIT(2)
#define MAX96716A_GPIO_A_GPIO_IN				BIT(3)
#define MAX96716A_GPIO_A_GPIO_OUT				BIT(4)
#define MAX96716A_GPIO_A_TX_COMP_EN				BIT(5)
#define MAX96716A_GPIO_A_RES_CFG				BIT(7)

#define MAX96716A_GPIO_B(x)						(0x2b1 + (x) * 3)
#define MAX96716A_GPIO_B_GPIO_TX_ID				GENMASK(4, 0)
#define MAX96716A_GPIO_B_OUT_TYPE				BIT(5)
#define MAX96716A_GPIO_B_PULL_UPDN_SEL			GENMASK(7, 6)
#define MAX96716A_GPIO_B_PULL_UPDN_SEL_NONE		0b00
#define MAX96716A_GPIO_B_PULL_UPDN_SEL_PU		0b01
#define MAX96716A_GPIO_B_PULL_UPDN_SEL_PD		0b10

#define MAX96716A_GPIO_C(x)						(0x2b2 + (x) * 3)
#define MAX96716A_GPIO_C_GPIO_RX_ID				GENMASK(4, 0)

#define MAX96716A_GPIO_ERRB						4

#define MAX96716A_BACKTOP12						0x313
#define MAX96716A_BACKTOP12_CSI_OUT_EN			BIT(1)

#define MAX96716A_BACKTOP21						0x31c
#define MAX96716A_BACKTOP21_BPP8DBL(p)			BIT(4 + (p))

#define MAX96716A_BACKTOP22(x)					(0x31d + (x) * 0x3)
#define MAX96716A_BACKTOP22_PHY_CSI_TX_DPLL		GENMASK(4, 0)
#define MAX96716A_BACKTOP22_PHY_CSI_TX_DPLL_EN	BIT(5)

#define MAX96716A_BACKTOP24						0x31f
#define MAX96716A_BACKTOP24_BPP8DBL_MODE(p)		BIT(4 + (p))

#define MAX96716A_BACKTOP32						0x327
#define MAX96716A_BACKTOP32_BPP10DBL(p)			BIT(p)
#define MAX96716A_BACKTOP32_BPP10DBL_MODE(p)	BIT(4 + (p))

#define MAX96716A_BACKTOP33						0x328
#define MAX96716A_BACKTOP32_BPP12DBL(p)			BIT(p)

#define MAX96716A_MIPI_PHY0						0x330
#define MAX96716A_MIPI_PHY0_FORCE_CSI_OUT_EN	BIT(7)

#define MAX96716A_MIPI_PHY2						0x332
#define MAX96716A_MIPI_PHY2_PHY_STDBY_N(x)		(GENMASK(5, 4) << ((x) * 2))

#define MAX96716A_MIPI_PHY3(x)					(0x333 + (x))
#define MAX96716A_MIPI_PHY3_PHY_LANE_MAP_4		GENMASK(7, 0)

#define MAX96716A_MIPI_PHY5(x)					(0x335 + (x))
#define MAX96716A_MIPI_PHY5_PHY_POL_MAP_0_1		GENMASK(1, 0)
#define MAX96716A_MIPI_PHY5_PHY_POL_MAP_2_3		GENMASK(4, 3)
#define MAX96716A_MIPI_PHY5_PHY_POL_MAP_CLK(x)	((x) == 0 ? BIT(5) : BIT(2))

#define MAX96716A_MIPI_PHY18					0x342
#define MAX96716A_MIPI_PHY18_CSI2_TX_PKT_CNT(x)	(GENMASK(3, 0) << (4 * (x)))

#define MAX96716A_MIPI_PHY20(x)					(0x344 + (x))

#define MAX96716A_FSYNC_0						0x3e0
#define MAX96716A_FSYNC_0_FSYNC_METH			GENMASK(1, 0)
#define MAX96716A_FSYNC_0_FSYNC_METH_MANUAL		0b00
#define MAX96716A_FSYNC_0_FSYNC_METH_SEMI_AUTO	0b01
#define MAX96716A_FSYNC_0_FSYNC_METH_AUTO		0b10
#define MAX96716A_FSYNC_0_FSYNC_MODE				GENMASK(3, 2)
#define MAX96716A_FSYNC_0_FSYNC_MODE_GEN_ON_IO_OFF	0b00
#define MAX96716A_FSYNC_0_FSYNC_MODE_GEN_ON_IO_ON	0b01
#define MAX96716A_FSYNC_0_FSYNC_MODE_GEN_OFF_IO_ON	0b10
#define MAX96716A_FSYNC_0_FSYNC_MODE_GEN_OFF_IO_OFF	0b11
#define MAX96716A_FSYNC_0_EN_VS_GEN				BIT(4)
#define MAX96716A_FSYNC_0_FSYNC_OUT_PIN			BIT(5)

#define MAX96716A_FSYNC_2						0x3e2
#define MAX96716A_FSYNC_2_K_VAL					GENMASK(3, 0)
#define MAX96716A_FSYNC_2_K_VAL_1P71US			0x1
#define MAX96716A_FSYNC_2_K_VAL_SIGN			BIT(4)
#define MAX96716A_FSYNC_2_MST_LINK_SEL			GENMASK(7, 5)
#define MAX96716A_FSYNC_2_MST_LINK_SEL_VIDEO_X	0b000
#define MAX96716A_FSYNC_2_MST_LINK_SEL_VIDEO_Y	0b001
#define MAX96716A_FSYNC_2_MST_LINK_SEL_AUTO_0	0b100
#define MAX96716A_FSYNC_2_MST_LINK_SEL_AUTO_1	0b101

#define MAX96716A_FSYNC_5_PERIOD_L				0x3e5
#define MAX96716A_FSYNC_6_PERIOD_M				0x3e6
#define MAX96716A_FSYNC_7_PERIOD_H				0x3e7

#define MAX96716A_FSYNC_10						0x3ea
#define MAX96716A_FSYNC_10_OVLP_WIN_L			GENMASK(7, 0)

#define MAX96716A_FSYNC_11						0x3eb
#define MAX96716A_FSYNC_11_OVLP_WIN_H			GENMASK(4, 0)

#define MAX96716A_FSYNC_15						0x3ef
#define MAX96716A_FSYNC_15_FS_LINK				GENMASK(3, 0)
#define MAX96716A_FSYNC_15_AUTO_FS_LINKS			BIT(4)
#define MAX96716A_FSYNC_15_FS_USE_XTAL			BIT(6)
#define MAX96716A_FSYNC_15_FS_GPIO_TPYE			BIT(7)

#define MAX96716A_FSYNC_17						0x3f1
#define MAX96716A_FSYNC_17_FSYNC_TX_ID			GENMASK(7, 3)

#define MAX96716A_MIPI_TX3(x)					(0x403 + (x) * 0x40)
#define MAX96716A_MIPI_TX3_DESKEW_INIT_8X32K	field_prep(GENMASK(2, 0), 0b001)
#define MAX96716A_MIPI_TX3_DESKEW_INIT_AUTO		BIT(7)

#define MAX96716A_MIPI_TX4(x)					(0x404 + (x) * 0x40)
#define MAX96716A_MIPI_TX4_DESKEW_PER_2K		field_prep(GENMASK(2, 0), 0b001)
#define MAX96716A_MIPI_TX4_DESKEW_PER_AUTO		BIT(7)

#define MAX96716A_MIPI_TX10(x)					(0x40a + (x) * 0x40)
#define MAX96716A_MIPI_TX10_CSI2_LANE_CNT		GENMASK(7, 6)
#define MAX96716A_MIPI_TX10_CSI2_CPHY_EN		BIT(5)

#define MAX96716A_MIPI_TX11(p)					(0x40b + (p) * 0x40)
#define MAX96716A_MIPI_TX12(p)					(0x40c + (p) * 0x40)

#define MAX96716A_MIPI_TX13(p, x)				(0x40d + (p) * 0x40 + (x) * 0x2)
#define MAX96716A_MIPI_TX13_MAP_SRC_DT			GENMASK(5, 0)
#define MAX96716A_MIPI_TX13_MAP_SRC_VC			GENMASK(7, 6)

#define MAX96716A_MIPI_TX14(p, x)				(0x40e + (p) * 0x40 + (x) * 0x2)
#define MAX96716A_MIPI_TX14_MAP_DST_DT			GENMASK(5, 0)
#define MAX96716A_MIPI_TX14_MAP_DST_VC			GENMASK(7, 6)

#define MAX96716A_MIPI_TX45(p, x)				(0x42d + (p) * 0x40 + (x) / 4)
#define MAX96716A_MIPI_TX45_MAP_DPHY_DEST(x)	(GENMASK(1, 0) << (2 * ((x) % 4)))

#define MAX96716A_MIPI_TX51(x)					(0x433 + (x) * 0x40)
#define MAX96716A_MIPI_TX51_ALT_MEM_MAP_12		BIT(0)
#define MAX96716A_MIPI_TX51_ALT_MEM_MAP_8		BIT(1)
#define MAX96716A_MIPI_TX51_ALT_MEM_MAP_10		BIT(2)
#define MAX96716A_MIPI_TX51_ALT2_MEM_MAP_8		BIT(4)

#define MAX96716A_MIPI_TX52(x)					(0x434 + (x) * 0x40)
#define MAX96716A_MIPI_TX52_TUN_DEST			BIT(1)
#define MAX96716A_MIPI_TX52_TUN_EN				BIT(0)

#define MAX96716A_GMSL1_EN						0xf00
#define MAX96716A_GMSL1_EN_LINK_EN				GENMASK(1, 0)

#define MAX96716A_RLMS0B(x)						(0x140b + (x) * 0x100)
#define MAX96716A_RLMS0A(x)						(0x140a + (x) * 0x100)
#define MAX96716A_RLMS18(x)						(0x1418 + (x) * 0x100)
#define MAX96716A_RLMS1F(x)						(0x141f + (x) * 0x100)
#define MAX96716A_RLMS21(x)						(0x1421 + (x) * 0x100)
#define MAX96716A_RLMS23(x)						(0x1423 + (x) * 0x100)
#define MAX96716A_RLMS31(x)						(0x1431 + (x) * 0x100)
#define MAX96716A_RLMS3E(x)						(0x143e + (x) * 0x100)
#define MAX96716A_RLMS3F(x)						(0x143f + (x) * 0x100)
#define MAX96716A_RLMS45(x)						(0x1445 + (x) * 0x100)
#define MAX96716A_RLMS46(x)						(0x1446 + (x) * 0x100)
#define MAX96716A_RLMS49(x)						(0x1449 + (x) * 0x100)
#define MAX96716A_RLMS8C(x)						(0x148c + (x) * 0x100)
#define MAX96716A_RLMS98(x)						(0x1498 + (x) * 0x100)
#define MAX96716A_RLMSA5(x)						(0x14a5 + (x) * 0x100)
#define MAX96716A_RLMSAC(x)						(0x14ac + (x) * 0x100)
#define MAX96716A_RLMSAD(x)						(0x14ad + (x) * 0x100)

#define MAX96716A_DPLL_0(x)						(0x1c00 + (x) * 0x100)
#define MAX96716A_DPLL_0_CONFIG_SOFT_RST_N		BIT(0)

#define MAX96716A_CTRL9							0x5009
#define MAX96716A_CTRL9_LOCKED_B				BIT(3)

#define MAX96716A_GPIO_A_B(x)					(0x52b0 + (x) * 3)
#define MAX96716A_GPIO_A_B_GPIO_TX_EN			BIT(1)
#define MAX96716A_GPIO_A_B_GPIO_RX_EN			BIT(2)
#define MAX96716A_GPIO_A_B_TX_COMP_EN			BIT(5)

#define MAX96716A_GPIO_B_B(x)					(0x52b1 + (x) * 3)
#define MAX96716A_GPIO_B_B_GPIO_TX_ID			GENMASK(4, 0)

#define MAX96716A_GPIO_C_B(x)					(0x52b2 + (x) * 3)
#define MAX96716A_GPIO_C_B_GPIO_RX_ID			GENMASK(4, 0)

#define MAX96716A_PIPES_NUM		2
#define MAX96716A_PHYS_NUM		2
#define MAX96716A_NAME			"max96716a"
#define MAX96716A_GPIO_NUM		13

static const struct regmap_config max96716a_i2c_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
};

struct max96716a_priv {
	struct max_des_priv des_priv;
	const struct max96716a_chip_info *info;

	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;

	struct pinctrl_dev *pctldev;
	struct pinctrl_desc pctldesc;
	struct gpio_chip gc;

	unsigned int i2c_addr;
	unsigned int source_id;

	struct gpio_desc *reset_gpio;

	bool errb_enabled;
};

struct max96716a_chip_info {
	unsigned int max_register;
	unsigned int num_pipes;
	unsigned int pipe_hw_ids[MAX96716A_PIPES_NUM];
	unsigned int num_phys;
	unsigned int phy_hw_ids[MAX96716A_PHYS_NUM];
	unsigned int num_links;
	bool phy0_first_lanes_on_master_phy;
	bool polarity_on_physical_lanes;
	bool supports_pipe_link_remap;
	bool supports_tunnel_mode;
	bool supports_phy_log;
	bool fix_tx_ids;
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
	container_of(des, struct max96716a_priv, des_priv)

static int max96716a_read(struct max96716a_priv *priv, int reg)
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

static int max96716a_write(struct max96716a_priv *priv, unsigned int reg, u8 val)
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

static int max96716a_update_bits(struct max96716a_priv *priv, unsigned int reg,
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

#define MAX96716A_PIN(n) PINCTRL_PIN(n, "mfp" __stringify(n))

static const struct pinctrl_pin_desc max96716a_pins[] = {
	MAX96716A_PIN(0), MAX96716A_PIN(1), MAX96716A_PIN(2),
	MAX96716A_PIN(3), MAX96716A_PIN(4), MAX96716A_PIN(5),
	MAX96716A_PIN(6), MAX96716A_PIN(7), MAX96716A_PIN(8),
	MAX96716A_PIN(9), MAX96716A_PIN(10), MAX96716A_PIN(11),
	MAX96716A_PIN(12),
};

#define MAX96716A_GROUP_PINS(name, ...)                                         \
	static const unsigned int name##_pins[] = { __VA_ARGS__ }

MAX96716A_GROUP_PINS(mfp0, 0);
MAX96716A_GROUP_PINS(mfp1, 1);
MAX96716A_GROUP_PINS(mfp2, 2);
MAX96716A_GROUP_PINS(mfp3, 3);
MAX96716A_GROUP_PINS(mfp4, 4);
MAX96716A_GROUP_PINS(mfp5, 5);
MAX96716A_GROUP_PINS(mfp6, 6);
MAX96716A_GROUP_PINS(mfp7, 7);
MAX96716A_GROUP_PINS(mfp8, 8);
MAX96716A_GROUP_PINS(mfp9, 9);
MAX96716A_GROUP_PINS(mfp10, 10);
MAX96716A_GROUP_PINS(mfp11, 11);
MAX96716A_GROUP_PINS(mfp12, 12);

#define MAX96716A_GROUP(name)                                                   \
	PINCTRL_PINGROUP(__stringify(name), name##_pins,                       \
			ARRAY_SIZE(name##_pins))

static const struct pingroup max96716a_ctrl_groups[] = {
	MAX96716A_GROUP(mfp0), MAX96716A_GROUP(mfp1), MAX96716A_GROUP(mfp2),
	MAX96716A_GROUP(mfp3), MAX96716A_GROUP(mfp4), MAX96716A_GROUP(mfp5),
	MAX96716A_GROUP(mfp6), MAX96716A_GROUP(mfp7), MAX96716A_GROUP(mfp8),
	MAX96716A_GROUP(mfp9), MAX96716A_GROUP(mfp10), MAX96716A_GROUP(mfp11),
	MAX96716A_GROUP(mfp12),
};

#define MAX96716A_FUNC_GROUPS(name, ...)                                        \
	static const char *const name##_groups[] = { __VA_ARGS__ }

MAX96716A_FUNC_GROUPS(gpio, "mfp0", "mfp1", "mfp2", "mfp3", "mfp4", "mfp5",
			"mfp6", "mfp7", "mfp8", "mfp9", "mfp10", "mfp11", "mfp12");

enum max96716a_func {
	max96716a_func_gpio,
};

#define MAX96716A_FUNC(name)                                                    \
	[max96716a_func_##name] = PINCTRL_PINFUNCTION(                          \
		__stringify(name), name##_groups, ARRAY_SIZE(name##_groups))

static const struct pinfunction max96716a_functions[] = {
	MAX96716A_FUNC(gpio),
};

enum max96716a_pinctrl_params {
	MAX96716A_PINCTRL_PULL_STRENGTH_WEAK = PIN_CONFIG_END + 1,
	MAX96716A_PINCTRL_JITTER_COMPENSATION_EN,
	MAX96716A_PINCTRL_GMSL_TX_EN_A,
	MAX96716A_PINCTRL_GMSL_RX_EN_A,
	MAX96716A_PINCTRL_GMSL_TX_ID_A,
	MAX96716A_PINCTRL_GMSL_RX_ID_A,
	MAX96716A_PINCTRL_GMSL_TX_EN_B,
	MAX96716A_PINCTRL_GMSL_RX_EN_B,
	MAX96716A_PINCTRL_GMSL_TX_ID_B,
	MAX96716A_PINCTRL_GMSL_RX_ID_B,
	MAX96716A_PINCTRL_INPUT_VALUE,
};

static const struct pinconf_generic_params max96716a_cfg_params[] = {
	{ "maxim,pull-strength-weak", MAX_PINCONF_PARAM(MAX96716A_PINCTRL_PULL_STRENGTH_WEAK), 0 },
	{ "maxim,jitter-compensation", MAX_PINCONF_PARAM(MAX96716A_PINCTRL_JITTER_COMPENSATION_EN), 0 },
	{ "maxim,gmsl-tx-a", MAX_PINCONF_PARAM(MAX96716A_PINCTRL_GMSL_TX_EN_A), 0 },
	{ "maxim,gmsl-rx-a", MAX_PINCONF_PARAM(MAX96716A_PINCTRL_GMSL_RX_EN_A), 0 },
	{ "maxim,gmsl-tx-id-a", MAX_PINCONF_PARAM(MAX96716A_PINCTRL_GMSL_TX_ID_A), 0 },
	{ "maxim,gmsl-rx-id-a", MAX_PINCONF_PARAM(MAX96716A_PINCTRL_GMSL_RX_ID_A), 0 },
	{ "maxim,gmsl-tx-b", MAX_PINCONF_PARAM(MAX96716A_PINCTRL_GMSL_TX_EN_B), 0 },
	{ "maxim,gmsl-rx-b", MAX_PINCONF_PARAM(MAX96716A_PINCTRL_GMSL_RX_EN_B), 0 },
	{ "maxim,gmsl-tx-id-b", MAX_PINCONF_PARAM(MAX96716A_PINCTRL_GMSL_TX_ID_B), 0 },
	{ "maxim,gmsl-rx-id-b", MAX_PINCONF_PARAM(MAX96716A_PINCTRL_GMSL_RX_ID_B), 0 },
};

static int max96716a_ctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(max96716a_ctrl_groups);
}

static const char *max96716a_ctrl_get_group_name(struct pinctrl_dev *pctldev,
						unsigned selector)
{
	return max96716a_ctrl_groups[selector].name;
}

static int max96716a_ctrl_get_group_pins(struct pinctrl_dev *pctldev,
					unsigned selector,
					const unsigned **pins,
					unsigned *num_pins)
{
	*pins = (unsigned *)max96716a_ctrl_groups[selector].pins;
	*num_pins = max96716a_ctrl_groups[selector].npins;

	return 0;
}

static int max96716a_get_pin_config_reg(unsigned int offset, u32 param,
					unsigned int *reg, unsigned int *mask,
					unsigned int *val)
{
	*reg = MAX96716A_GPIO_A(offset);

	switch (param) {
	case PIN_CONFIG_OUTPUT_ENABLE:
		*mask = MAX96716A_GPIO_A_GPIO_OUT_DIS;
		*val = 0b0;
		return 0;
	case PIN_CONFIG_INPUT_ENABLE:
		*mask = MAX96716A_GPIO_A_GPIO_OUT_DIS;
		*val = 0b1;
		return 0;
	case MAX96716A_PINCTRL_GMSL_TX_EN_A:
		*mask = MAX96716A_GPIO_A_GPIO_TX_EN;
		*val = 0b1;
		return 0;
	case MAX96716A_PINCTRL_GMSL_RX_EN_A:
		*mask = MAX96716A_GPIO_A_GPIO_RX_EN;
		*val = 0b1;
		return 0;
	case MAX96716A_PINCTRL_INPUT_VALUE:
		*mask = MAX96716A_GPIO_A_GPIO_IN;
		*val = 0b1;
		return 0;
	case PIN_CONFIG_OUTPUT:
		*mask = MAX96716A_GPIO_A_GPIO_OUT;
		*val = 0b1;
		return 0;
	case MAX96716A_PINCTRL_JITTER_COMPENSATION_EN:
		*mask = MAX96716A_GPIO_A_TX_COMP_EN;
		*val = 0b1;
		return 0;
	case MAX96716A_PINCTRL_PULL_STRENGTH_WEAK:
		*mask = MAX96716A_GPIO_A_RES_CFG;
		*val = 0b0;
		return 0;
	}

	*reg = MAX96716A_GPIO_B(offset);

	switch (param) {
	case MAX96716A_PINCTRL_GMSL_TX_ID_A:
		*mask = MAX96716A_GPIO_B_GPIO_TX_ID;
		return 0;
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		*mask = MAX96716A_GPIO_B_OUT_TYPE;
		*val = 0b0;
		return 0;
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		*mask = MAX96716A_GPIO_B_OUT_TYPE;
		*val = 0b1;
		return 0;
	case PIN_CONFIG_BIAS_DISABLE:
		*mask = MAX96716A_GPIO_B_PULL_UPDN_SEL;
		*val = MAX96716A_GPIO_B_PULL_UPDN_SEL_NONE;
		return 0;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		*mask = MAX96716A_GPIO_B_PULL_UPDN_SEL;
		*val = MAX96716A_GPIO_B_PULL_UPDN_SEL_PD;
		return 0;
	case PIN_CONFIG_BIAS_PULL_UP:
		*mask = MAX96716A_GPIO_B_PULL_UPDN_SEL;
		*val = MAX96716A_GPIO_B_PULL_UPDN_SEL_PU;
		return 0;
	}

	*reg = MAX96716A_GPIO_C(offset);

	switch (param) {
	case MAX96716A_PINCTRL_GMSL_RX_ID_A:
		*mask = MAX96716A_GPIO_C_GPIO_RX_ID;
		return 0;
	}

	*reg = MAX96716A_GPIO_A_B(offset);

	switch (param) {
	case MAX96716A_PINCTRL_GMSL_TX_EN_B:
		*mask = MAX96716A_GPIO_A_B_GPIO_TX_EN;
		*val = 0b1;
		return 0;
	case MAX96716A_PINCTRL_GMSL_RX_EN_B:
		*mask = MAX96716A_GPIO_A_B_GPIO_RX_EN;
		*val = 0b1;
		return 0;
	}

	*reg = MAX96716A_GPIO_B_B(offset);

	switch (param) {
	case MAX96716A_PINCTRL_GMSL_TX_ID_B:
		*mask = MAX96716A_GPIO_B_B_GPIO_TX_ID;
		return 0;
	}

	*reg = MAX96716A_GPIO_C_B(offset);

	switch (param) {
	case MAX96716A_PINCTRL_GMSL_RX_ID_B:
		*mask = MAX96716A_GPIO_C_B_GPIO_RX_ID;
		return 0;
	default:
		return -ENOTSUPP;
	}
}

static int max96716a_conf_pin_config_get(struct pinctrl_dev *pctldev,
					unsigned int offset,
					unsigned long *config)
{
	struct max96716a_priv *priv = pinctrl_dev_get_drvdata(pctldev);
	u32 param = pinconf_to_config_param(*config);
	unsigned int reg, mask, val;
	int ret;

	ret = max96716a_get_pin_config_reg(offset, param, &reg, &mask, &val);
	if (ret)
		return ret;

	switch (param) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
	case PIN_CONFIG_DRIVE_PUSH_PULL:
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		ret = max96716a_read(priv, reg);
		if (ret < 0)
			return ret;

		val = field_get(mask, ret) == val;
		if (!val)
			return -EINVAL;

		break;
	case MAX96716A_PINCTRL_JITTER_COMPENSATION_EN:
	case MAX96716A_PINCTRL_PULL_STRENGTH_WEAK:
	case MAX96716A_PINCTRL_GMSL_TX_EN_A:
	case MAX96716A_PINCTRL_GMSL_RX_EN_A:
	case MAX96716A_PINCTRL_GMSL_TX_EN_B:
	case MAX96716A_PINCTRL_GMSL_RX_EN_B:
	case MAX96716A_PINCTRL_INPUT_VALUE:
	case PIN_CONFIG_OUTPUT_ENABLE:
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT:
		ret = max96716a_read(priv, reg);
		if (ret < 0)
			return ret;

		val = field_get(mask, ret) == val;
		break;
	case MAX96716A_PINCTRL_GMSL_TX_ID_A:
	case MAX96716A_PINCTRL_GMSL_RX_ID_A:
	case MAX96716A_PINCTRL_GMSL_TX_ID_B:
	case MAX96716A_PINCTRL_GMSL_RX_ID_B:
		ret = max96716a_read(priv, reg);
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

static int max96716a_errb_enable(struct max96716a_priv *priv, bool enable)
{
	int ret;

	ret = max96716a_update_bits(priv, MAX96716A_REG5,
				   MAX96716A_REG5_ERRB_EN,
				   field_prep(MAX96716A_REG5_ERRB_EN, enable));
	if (ret)
		return ret;

	dev_info(priv->dev, "ERRB output %s\n", enable ? "enabled" : "disabled");

	return 0;
}


static int max96716a_conf_pin_config_set_one(struct max96716a_priv *priv,
						unsigned int offset,
						unsigned long config)
{
	u32 param = pinconf_to_config_param(config);
	u32 arg = pinconf_to_config_argument(config);
	unsigned int reg, mask, val;
	int ret;

	ret = max96716a_get_pin_config_reg(offset, param, &reg, &mask, &val);
	if (ret)
		return ret;

	switch (param) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
	case PIN_CONFIG_DRIVE_PUSH_PULL:
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		val = field_prep(mask, val);

		ret = max96716a_update_bits(priv, reg, mask, val);
		break;
	case MAX96716A_PINCTRL_JITTER_COMPENSATION_EN:
	case MAX96716A_PINCTRL_PULL_STRENGTH_WEAK:
	case MAX96716A_PINCTRL_GMSL_TX_EN_A:
	case MAX96716A_PINCTRL_GMSL_RX_EN_A:
	case MAX96716A_PINCTRL_GMSL_TX_EN_B:
	case MAX96716A_PINCTRL_GMSL_RX_EN_B:
	case PIN_CONFIG_OUTPUT_ENABLE:
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT:
		val = field_prep(mask, arg ? val : ~val);

		ret = max96716a_update_bits(priv, reg, mask, val);

		if (offset == MAX96716A_GPIO_ERRB) {
			priv->errb_enabled = false;
			ret = max96716a_errb_enable(priv, priv->errb_enabled);
			if (ret)
				return ret;
		}
		break;
	case MAX96716A_PINCTRL_GMSL_TX_ID_A:
	case MAX96716A_PINCTRL_GMSL_RX_ID_A:
	case MAX96716A_PINCTRL_GMSL_TX_ID_B:
	case MAX96716A_PINCTRL_GMSL_RX_ID_B:
		val = field_prep(mask, arg);

		ret = max96716a_update_bits(priv, reg, mask, val);
		break;
	default:
		return -ENOTSUPP;
	}

	if (param == PIN_CONFIG_OUTPUT) {
		config = pinconf_to_config_packed(PIN_CONFIG_OUTPUT_ENABLE, 1);
		ret = max96716a_conf_pin_config_set_one(priv, offset, config);
		if (ret)
			return ret;
	}

	return ret;
}

static int max96716a_conf_pin_config_set(struct pinctrl_dev *pctldev,
					unsigned int offset,
					unsigned long *configs,
					unsigned int num_configs)
{
	struct max96716a_priv *priv = pinctrl_dev_get_drvdata(pctldev);
	int ret;

	while (num_configs--) {
		unsigned long config = *configs;

		ret = max96716a_conf_pin_config_set_one(priv, offset, config);
		if (ret)
			return ret;

		configs++;
	}

	return 0;
}

static int max96716a_mux_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(max96716a_functions);
}

static const char *max96716a_mux_get_function_name(struct pinctrl_dev *pctldev,
						unsigned selector)
{
	return max96716a_functions[selector].name;
}

static int max96716a_mux_get_groups(struct pinctrl_dev *pctldev,
				unsigned selector,
				const char *const **groups,
				unsigned *const num_groups)
{
	*groups = max96716a_functions[selector].groups;
	*num_groups = max96716a_functions[selector].ngroups;

	return 0;
}

static int max96716a_mux_set(struct pinctrl_dev *pctldev, unsigned selector,
				unsigned group)
{
	return 0;
}

static int max96716a_gpio_get_direction(struct gpio_chip *gc,
					unsigned int offset)
{
	unsigned long config =
		pinconf_to_config_packed(PIN_CONFIG_OUTPUT_ENABLE, 0);
	struct max96716a_priv *priv = gpiochip_get_data(gc);
	int ret;

	ret = max96716a_conf_pin_config_get(priv->pctldev, offset, &config);
	if (ret)
		return ret;

	return pinconf_to_config_argument(config) ? GPIO_LINE_DIRECTION_OUT :
							GPIO_LINE_DIRECTION_IN;
}

static int max96716a_gpio_direction_input(struct gpio_chip *gc,
					unsigned int offset)
{
	unsigned long config =
		pinconf_to_config_packed(PIN_CONFIG_INPUT_ENABLE, 1);
	struct max96716a_priv *priv = gpiochip_get_data(gc);

	return max96716a_conf_pin_config_set_one(priv, offset, config);
}

static int max96716a_gpio_direction_output(struct gpio_chip *gc,
					unsigned int offset, int value)
{
	unsigned long config =
		pinconf_to_config_packed(PIN_CONFIG_OUTPUT, value);
	struct max96716a_priv *priv = gpiochip_get_data(gc);

	return max96716a_conf_pin_config_set_one(priv, offset, config);
}

static int max96716a_gpio_get(struct gpio_chip *gc, unsigned int offset)
{
	unsigned long config =
		pinconf_to_config_packed(MAX_PINCONF_PARAM(MAX96716A_PINCTRL_INPUT_VALUE), 0);
	struct max96716a_priv *priv = gpiochip_get_data(gc);
	int ret;

	ret = max96716a_conf_pin_config_get(priv->pctldev, offset, &config);
	if (ret)
		return ret;

	return pinconf_to_config_argument(config);
}

static void max96716a_gpio_set(struct gpio_chip *gc, unsigned int offset,
				int value)
{
	unsigned long config =
		pinconf_to_config_packed(PIN_CONFIG_OUTPUT, value);
	struct max96716a_priv *priv = gpiochip_get_data(gc);
	int ret;

	ret = max96716a_conf_pin_config_set_one(priv, offset, config);
	if (ret)
		dev_err(priv->dev,
			"Failed to set GPIO %u output value, err: %d\n", offset,
			ret);
}

static struct pinctrl_ops max96716a_ctrl_ops = {
	.get_groups_count = max96716a_ctrl_get_groups_count,
	.get_group_name = max96716a_ctrl_get_group_name,
	.get_group_pins = max96716a_ctrl_get_group_pins,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_pin,
	.dt_free_map = pinconf_generic_dt_free_map,
};

static const struct pinconf_ops max96716a_conf_ops = {
	.pin_config_get = max96716a_conf_pin_config_get,
	.pin_config_set = max96716a_conf_pin_config_set,
	.is_generic = true,
};

static const struct pinmux_ops max96716a_mux_ops = {
	.get_functions_count = max96716a_mux_get_functions_count,
	.get_function_name = max96716a_mux_get_function_name,
	.get_function_groups = max96716a_mux_get_groups,
	.set_mux = max96716a_mux_set,
	.strict = true,
};

static unsigned int max96716a_pipe_id(struct max96716a_priv *priv,
				     struct max_des_pipe *pipe)
{
	return priv->info->pipe_hw_ids[pipe->index];
}

static unsigned int max96716a_phy_id(struct max96716a_priv *priv,
				    struct max_des_phy *phy)
{
	return priv->info->phy_hw_ids[phy->index];
}

static int max96716a_log_pipe_status(struct max_des_priv *des_priv,
				    struct max_des_pipe *pipe, const char *name)
{
	struct max96716a_priv *priv = des_to_priv(des_priv);
	unsigned int index = max96716a_pipe_id(priv, pipe);
	unsigned int val;
	int ret;

	ret = regmap_read(priv->regmap, MAX96716A_VPRBS(index), &val);
	if (ret)
		return ret;

	pr_info("%s: \tvideo_lock: %u\n", name,
		!!(val & MAX96716A_VPRBS_VIDEO_LOCK));

	return 0;
}

static int max96716a_log_phy_status(struct max_des_priv *des_priv,
				   struct max_des_phy *phy, const char *name)
{
	struct max96716a_priv *priv = des_to_priv(des_priv);
	unsigned int index = phy->index;
	unsigned int val;
	int ret;

	if (!priv->info->supports_phy_log)
		return 0;

	ret = regmap_read(priv->regmap, MAX96716A_MIPI_PHY18, &val);
	if (ret)
		return ret;

	pr_info("%s: \tcsi2_pkt_cnt: %lu\n", name,
		field_get(MAX96716A_MIPI_PHY18_CSI2_TX_PKT_CNT(index), val));

	ret = regmap_read(priv->regmap, MAX96716A_MIPI_PHY20(index), &val);
	if (ret)
		return ret;

	pr_info("%s: \tphy_pkt_cnt: %u\n", name, val);

	return 0;
}

static int max96716a_mipi_enable(struct max_des_priv *des_priv, bool enable)
{
	struct max96716a_priv *priv = des_to_priv(des_priv);
	int ret;

	if (enable) {
		ret = max96716a_update_bits(priv, MAX96716A_BACKTOP12,
					   MAX96716A_BACKTOP12_CSI_OUT_EN,
					   MAX96716A_BACKTOP12_CSI_OUT_EN);
		if (ret)
			return ret;
	} else {
		ret = max96716a_update_bits(priv, MAX96716A_BACKTOP12,
					   MAX96716A_BACKTOP12_CSI_OUT_EN,
					   0x00);
		if (ret)
			return ret;
	}

	return 0;
}

static int max96716a_set_pipe_enable(struct max_des_priv *des_priv,
				    struct max_des_pipe *pipe, bool enable)
{
	struct max96716a_priv *priv = des_to_priv(des_priv);
	unsigned int index = max96716a_pipe_id(priv, pipe);

	return max96716a_update_bits(priv, MAX96716A_REG2,
				   MAX96716A_REG2_VID_EN(index),
				   field_prep(MAX96716A_REG2_VID_EN(index),
				   		enable));
}

static int max96716a_check_gmsl_links(struct max_des_priv *des_priv)
{
	struct max96716a_priv *priv = des_to_priv(des_priv);
	unsigned int locked_links_mask = 0;
	unsigned int links_mask = des_priv->gmsl_link_mask;
	u16 link_lock_addr[2] = {
		MAX96716A_CTRL3,
		MAX96716A_CTRL9
	};
	unsigned long timeout;

	dev_dbg(priv->dev, "%s()\n", __func__);

	des_priv->ops->select_links(des_priv, links_mask);

	msleep(100);

	timeout = jiffies + msecs_to_jiffies(100);

	while (!time_after(jiffies, timeout)) {
		int current_link = ffs(links_mask) - 1;

		if (current_link == -1)
			break;

		des_priv->links[current_link].enabled = false;
		if ((max96716a_read(priv, link_lock_addr[current_link]) & MAX96716A_CTRL3_LOCKED)) {
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

static int max96716a_init(struct max_des_priv *des_priv)
{
	struct max96716a_priv *priv = des_to_priv(des_priv);
	unsigned int locked_links;
	int retries = 3;
	int ret;

	while (retries--) {
		locked_links = max96716a_check_gmsl_links(des_priv);
		if (locked_links == des_priv->gmsl_link_mask)
			break;

		max96716a_update_bits(priv, MAX96716A_CTRL0,
					   MAX96716A_CTRL0_RESET_LINK,
					   MAX96716A_CTRL0_RESET_LINK);
		usleep_range(2000, 2500);
		max96716a_update_bits(priv, MAX96716A_CTRL0,
					   MAX96716A_CTRL0_RESET_LINK,
					   0x00);
		usleep_range(2000, 2500);
	}

	if (locked_links == 0) {
		dev_err(priv->dev, "No GMSL link has locked after 3 retries. Abort!\n");
		return -ENODEV;
	}

	dev_info(priv->dev, "GMSL link has locked - mask [0x%x]\n", locked_links);

	/* Disable all PHYs. */
	ret = max96716a_update_bits(priv, MAX96716A_MIPI_PHY2,
				   GENMASK(7, 4), 0x00);
	if (ret)
		return ret;

	/* Disable all pipes. */
	ret = max96716a_update_bits(priv, MAX96716A_REG2,
				   GENMASK(7, 4), 0x00);
	if (ret)
		return ret;

	ret = max96716a_update_bits(priv, MAX96716A_VIDEO_PIPE_EN,
					MAX96716A_VIDEO_PIPE_EN_MASK(0), 0x00);
	if (ret)
		return ret;

	ret = max96716a_update_bits(priv, MAX96716A_VIDEO_PIPE_EN,
					MAX96716A_VIDEO_PIPE_EN_MASK(1), 0x00);
	if (ret)
		return ret;


	/* Disable link auto-select. */
	ret = max96716a_update_bits(priv, MAX96716A_CTRL0,
				   MAX96716A_CTRL0_AUTO_LINK, 0);
	if (ret)
		return ret;

	/* Disable all UART channels */
	ret = max96716a_update_bits(priv, MAX96716A_REG3,
				   MAX96716A_REG3_UART_1_EN |
				   MAX96716A_REG3_UART_2_EN,
				   0x00);
	if (ret)
		return ret;

	/* Reset one shot */
	ret = max96716a_update_bits(priv, MAX96716A_CTRL0,
				   MAX96716A_CTRL0_RESET_ONESHOT,
				   MAX96716A_CTRL0_RESET_ONESHOT);
	if (ret)
		return ret;
	ret = max96716a_update_bits(priv, MAX96716A_CTRL2,
				   MAX96716A_CTRL2_RESET_ONESHOT_B,
				   MAX96716A_CTRL2_RESET_ONESHOT_B);
	if (ret)
		return ret;
	msleep(65);

	return 0;
}

static int max96716a_init_phy(struct max_des_priv *des_priv,
			     struct max_des_phy *phy)
{
	struct max96716a_priv *priv = des_to_priv(des_priv);
	unsigned int num_data_lanes = phy->mipi.num_data_lanes;
	unsigned int dpll_freq = phy->link_frequency * 2;
	unsigned int index = phy->index;
	unsigned int hw_index = max96716a_phy_id(priv, phy);
	unsigned int master_phy, slave_phy;
	unsigned int master_shift, slave_shift;
	unsigned int val, mask;
	unsigned int clk_bit, lane_0_bit, lane_2_bit;
	unsigned int used_data_lanes = 0;
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	/*
	 * MAX96716A has four PHYs, but does not support single-PHY configurations,
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
	 */
	if (index == 0) {
		master_phy = 1;
		slave_phy = 0;
	} else if (index == 1) {
		master_phy = 2;
		slave_phy = 3;
	} else {
		return -EINVAL;
	}

	/* Configure a lane count. */
	/* TODO: Add support CPHY mode. */
	ret = max96716a_update_bits(priv, MAX96716A_MIPI_TX10(hw_index),
				   MAX96716A_MIPI_TX10_CSI2_LANE_CNT,
				   field_prep(MAX96716A_MIPI_TX10_CSI2_LANE_CNT,
						num_data_lanes - 1));
	if (ret)
		return ret;

	/* Configure lane mapping. */
	/*
	 * The lane of each PHY can be mapped to physical lanes 0, 1, 2,
	 * and 3. This mapping is exclusive, multiple lanes, even if unused
	 * cannot be mapped to the same physical lane.
	 * Each lane mapping is represented as two bits.
	 */
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

	ret = max96716a_update_bits(priv, MAX96716A_MIPI_PHY3(index),
				   MAX96716A_MIPI_PHY3_PHY_LANE_MAP_4, val);
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
	 * On MAX96716A, each bit represents the lane polarity of logical lanes.
	 * Each of these lanes can be mapped to any physical lane.
	 * 0th bit is for lane 0.
	 * 1st bit is for lane 1.
	 * 2nd bit is for clock lane.
	 */
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

	ret = max96716a_update_bits(priv, MAX96716A_MIPI_PHY5(index),
				   MAX96716A_MIPI_PHY5_PHY_POL_MAP_0_1 |
				   MAX96716A_MIPI_PHY5_PHY_POL_MAP_CLK(1) |
				   MAX96716A_MIPI_PHY5_PHY_POL_MAP_2_3 |
				   MAX96716A_MIPI_PHY5_PHY_POL_MAP_CLK(0),
				   val);
	if (ret)
		return ret;

	/* Put DPLL block into reset. */
	ret = max96716a_update_bits(priv, MAX96716A_DPLL_0(hw_index),
				   MAX96716A_DPLL_0_CONFIG_SOFT_RST_N, 0x00);
	if (ret)
		return ret;

	/* Set DPLL frequency. */
	ret = max96716a_update_bits(priv, MAX96716A_BACKTOP22(hw_index),
				   MAX96716A_BACKTOP22_PHY_CSI_TX_DPLL,
				   div_u64(dpll_freq, 100000000));
	if (ret)
		return ret;

	/* Enable DPLL frequency. */
	ret = max96716a_update_bits(priv, MAX96716A_BACKTOP22(hw_index),
				   MAX96716A_BACKTOP22_PHY_CSI_TX_DPLL_EN,
				   MAX96716A_BACKTOP22_PHY_CSI_TX_DPLL_EN);
	if (ret)
		return ret;

	/* Pull DPLL block out of reset. */
	ret = max96716a_update_bits(priv, MAX96716A_DPLL_0(hw_index),
				   MAX96716A_DPLL_0_CONFIG_SOFT_RST_N,
				   MAX96716A_DPLL_0_CONFIG_SOFT_RST_N);
	if (ret)
		return ret;


	if (dpll_freq > 1500000000ull) {
		/* Enable initial deskew with 8 x 32k UI. */
		ret = max96716a_write(priv, MAX96716A_MIPI_TX3(hw_index),
					   MAX96716A_MIPI_TX3_DESKEW_INIT_AUTO |
					   MAX96716A_MIPI_TX3_DESKEW_INIT_8X32K);
		if (ret)
			return ret;

		/* Enable periodic deskew with 2 x 1k UI.. */
		ret = max96716a_write(priv, MAX96716A_MIPI_TX4(hw_index),
					   MAX96716A_MIPI_TX4_DESKEW_PER_AUTO |
					   MAX96716A_MIPI_TX4_DESKEW_PER_2K);
		if (ret)
			return ret;
	} else {
		/* Disable initial deskew. */
		ret = max96716a_write(priv, MAX96716A_MIPI_TX3(hw_index), 0x07);
		if (ret)
			return ret;

		/* Disable periodic deskew. */
		ret = max96716a_write(priv, MAX96716A_MIPI_TX4(hw_index), 0x01);
		if (ret)
			return ret;
	}

	/* Set alternate memory map modes. */
	val  = phy->alt_mem_map12 ? MAX96716A_MIPI_TX51_ALT_MEM_MAP_12 : 0;
	val |= phy->alt_mem_map8 ? MAX96716A_MIPI_TX51_ALT_MEM_MAP_8 : 0;
	val |= phy->alt_mem_map10 ? MAX96716A_MIPI_TX51_ALT_MEM_MAP_10 : 0;
	val |= phy->alt2_mem_map8 ? MAX96716A_MIPI_TX51_ALT2_MEM_MAP_8 : 0;
	ret = max96716a_update_bits(priv, MAX96716A_MIPI_TX51(hw_index),
				   GENMASK(4, 0), val);
	if (ret)
		return ret;

	/* Enable PHY. */
	mask = (BIT(master_phy) | BIT(slave_phy)) << 4;
	ret = max96716a_update_bits(priv, MAX96716A_MIPI_PHY2, mask, mask);
	if (ret)
		return ret;

	return 0;
}

static int max96716a_init_pipe(struct max_des_priv *des_priv,
			      struct max_des_pipe *pipe)
{
	struct max96716a_priv *priv = des_to_priv(des_priv);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	/* Enable pipe. */
	ret = max96716a_set_pipe_enable(des_priv, pipe, false);
	if (ret)
		return ret;

	ret = max96716a_update_bits(priv, MAX96716A_VIDEO_PIPE_EN,
					MAX96716A_VIDEO_PIPE_EN_MASK(pipe->index),
					MAX96716A_VIDEO_PIPE_EN_MASK(pipe->index));
	if (ret)
		return ret;

	/* Set source stream. */
	ret = max96716a_update_bits(priv, MAX96716A_VIDEO_PIPE_SEL,
				   MAX96716A_VIDEO_PIPE_SEL_STREAM(pipe->index) |
				   MAX96716A_VIDEO_PIPE_SEL_LINK(pipe->index),
				   field_prep(MAX96716A_VIDEO_PIPE_SEL_STREAM(pipe->index),
				   		pipe->stream_id) |
				   field_prep(MAX96716A_VIDEO_PIPE_SEL_LINK(pipe->index),
				   		pipe->link_id));
	if (ret)
		return ret;

	return 0;
}

static int max96716a_init_link(struct max_des_priv *des_priv,
			      struct max_des_link *link)
{
	struct max96716a_priv *priv = des_to_priv(des_priv);
	unsigned int index = link->index;
	unsigned int i, pipe_id;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	/* RLMS register setting for robust 6Gbps GMSL2 rate */
	ret = max96716a_write(priv, MAX96716A_RLMS3F(index), 0x3d);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMS3E(index), 0xfd);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMSAD(index), 0x68);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMSAC(index), 0xa8);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMS18(index), 0x07);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMS1F(index), 0xc2);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMS8C(index), 0x20);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMS98(index), 0xc0);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMS46(index), 0x01);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMS45(index), 0x81);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMS0B(index), 0x44);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMS0A(index), 0x08);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMS31(index), 0x18);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMS21(index), 0x08);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMSA5(index), 0x70);
	if (ret)
		return ret;

	/* RLMS register setting for long 6Gbps GMSL2 rate */
	ret = max96716a_write(priv, MAX96716A_RLMS1F(index), 0x8c);
	if (ret)
		return ret;

	ret = max96716a_write(priv, MAX96716A_RLMS23(index), 0x58);
	if (ret)
		return ret;

	/* RLMS register setting for robust 6Gbps operation */
	ret = max96716a_write(priv, MAX96716A_RLMS49(index), 0xf5);
	if (ret)
		return ret;

	if (index == 0)
		ret = max96716a_update_bits(priv, MAX96716A_CTRL0,
					   MAX96716A_CTRL0_RESET_ONESHOT,
					   MAX96716A_CTRL0_RESET_ONESHOT);
	else if (index == 1)
		ret = max96716a_update_bits(priv, MAX96716A_CTRL2,
					   MAX96716A_CTRL2_RESET_ONESHOT_B,
					   MAX96716A_CTRL2_RESET_ONESHOT_B);
	if (ret)
		return ret;
	msleep(65);

	if (priv->info->supports_tunnel_mode) {
		for (i = 0; i < priv->info->num_pipes; i++) {
			if (des_priv->pipes[i].link_id == index)
				break;
		}
		pipe_id = max96716a_pipe_id(priv, &des_priv->pipes[i]);
		ret = max96716a_update_bits(priv, MAX96716A_MIPI_TX52(pipe_id),
					   MAX96716A_MIPI_TX52_TUN_EN,
					   field_prep(MAX96716A_MIPI_TX52_TUN_EN,
							link->tunnel_mode));
		if (ret)
			return ret;
		ret = max96716a_update_bits(priv, MAX96716A_MIPI_TX52(pipe_id),
					   MAX96716A_MIPI_TX52_TUN_DEST,
					   field_prep(MAX96716A_MIPI_TX52_TUN_DEST,
							des_priv->pipes[i].phy_id));
		if (ret)
			return ret;
	}

	return 0;
}

static int max96716a_init_fsync(struct max_des_priv *des_priv,
				  struct max_des_fsync *fsync)
{
	struct max96716a_priv *priv = des_to_priv(des_priv);
	int ret = 0;

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (fsync->internal || fsync->internal_output) {
		ret = max96716a_update_bits(priv, MAX96716A_FSYNC_17,
					   MAX96716A_FSYNC_17_FSYNC_TX_ID,
					   field_prep(MAX96716A_FSYNC_17_FSYNC_TX_ID, 0x00));
		if (ret)
			return ret;
		ret = max96716a_update_bits(priv, MAX96716A_FSYNC_2,
					   MAX96716A_FSYNC_2_K_VAL |
					   MAX96716A_FSYNC_2_MST_LINK_SEL,
					   field_prep(MAX96716A_FSYNC_2_K_VAL,
							MAX96716A_FSYNC_2_K_VAL_1P71US) |
					   		field_prep(MAX96716A_FSYNC_2_MST_LINK_SEL,
							MAX96716A_FSYNC_2_MST_LINK_SEL_VIDEO_Y));
		if (ret)
			return ret;
		ret = max96716a_update_bits(priv, MAX96716A_FSYNC_10,
					   MAX96716A_FSYNC_10_OVLP_WIN_L,
					   0x00);
		if (ret)
			return ret;
		ret = max96716a_update_bits(priv, MAX96716A_FSYNC_11,
					   MAX96716A_FSYNC_11_OVLP_WIN_H,
					   field_prep(MAX96716A_FSYNC_11_OVLP_WIN_H, 0x00));
		if (ret)
			return ret;
		ret = max96716a_write(priv, MAX96716A_FSYNC_7_PERIOD_H,
					   (fsync->freq >> 16) & 0xff);
		if (ret)
			return ret;
		ret = max96716a_write(priv, MAX96716A_FSYNC_6_PERIOD_M,
					   (fsync->freq >> 8) & 0xff);
		if (ret)
			return ret;
		ret = max96716a_write(priv, MAX96716A_FSYNC_5_PERIOD_L,
					   (fsync->freq >> 0) & 0xff);
		if (ret)
			return ret;
		ret = max96716a_update_bits(priv, MAX96716A_FSYNC_15,
					   MAX96716A_FSYNC_15_FS_LINK,
					   0x06);
		if (ret)
			return ret;
		ret = max96716a_update_bits(priv, MAX96716A_FSYNC_15,
					   MAX96716A_FSYNC_15_AUTO_FS_LINKS,
					   0);
		if (ret)
			return ret;
		ret = max96716a_update_bits(priv, MAX96716A_FSYNC_15,
					   MAX96716A_FSYNC_15_FS_USE_XTAL,
					   MAX96716A_FSYNC_15_FS_USE_XTAL);
		if (ret)
			return ret;
		ret = max96716a_update_bits(priv, MAX96716A_FSYNC_15,
					   MAX96716A_FSYNC_15_FS_GPIO_TPYE,
					   MAX96716A_FSYNC_15_FS_GPIO_TPYE);
		if (ret)
			return ret;
		ret = max96716a_update_bits(priv, MAX96716A_FSYNC_0,
					   MAX96716A_FSYNC_0_FSYNC_METH |
					   MAX96716A_FSYNC_0_FSYNC_MODE,
					   field_prep(MAX96716A_FSYNC_0_FSYNC_METH,
							MAX96716A_FSYNC_0_FSYNC_METH_MANUAL) |
					   field_prep(MAX96716A_FSYNC_0_FSYNC_MODE,
							fsync->internal ?
							MAX96716A_FSYNC_0_FSYNC_MODE_GEN_ON_IO_OFF :
							MAX96716A_FSYNC_0_FSYNC_MODE_GEN_ON_IO_ON));
		if (ret)
			return ret;
	}
	else if (fsync->external) {
		ret = max96716a_update_bits(priv, MAX96716A_FSYNC_0,
					   MAX96716A_FSYNC_0_FSYNC_METH |
					   MAX96716A_FSYNC_0_FSYNC_MODE,
					   field_prep(MAX96716A_FSYNC_0_FSYNC_METH,
							MAX96716A_FSYNC_0_FSYNC_METH_MANUAL) |
					   field_prep(MAX96716A_FSYNC_0_FSYNC_MODE,
							MAX96716A_FSYNC_0_FSYNC_MODE_GEN_OFF_IO_ON));
		if (ret)
			return ret;
	}

	return ret;
}

static int max96716a_init_pipe_remap(struct max96716a_priv *priv,
				    struct max_des_pipe *pipe,
				    struct max_des_dt_vc_remap *remap,
				    unsigned int i)
{
	unsigned int index = max96716a_pipe_id(priv, pipe);
	unsigned int phy_id = max96716a_phy_id(priv,
							 &priv->des_priv.phys[remap->phy]);
	int ret;

	/* Set source Data Type and Virtual Channel. */
	/* TODO: implement extended Virtual Channel. */
	ret = max96716a_write(priv, MAX96716A_MIPI_TX13(index, i),
			     MAX_DES_DT_VC(remap->from_dt, remap->from_vc));
	if (ret)
		return ret;

	/* Set destination Data Type and Virtual Channel. */
	/* TODO: implement extended Virtual Channel. */
	ret = max96716a_write(priv, MAX96716A_MIPI_TX14(index, i),
			     MAX_DES_DT_VC(remap->to_dt, remap->to_vc));
	if (ret)
		return ret;

	/* Set destination PHY. */
	ret = max96716a_update_bits(priv, MAX96716A_MIPI_TX45(index, i),
				   MAX96716A_MIPI_TX45_MAP_DPHY_DEST(i),
				   field_prep(MAX96716A_MIPI_TX45_MAP_DPHY_DEST(i),
				   		phy_id));
	if (ret)
		return ret;

	/* Enable remap. */
	ret = max96716a_update_bits(priv, MAX96716A_MIPI_TX11(index) + i / 8,
				   BIT(i % 8), BIT(i % 8));
	if (ret)
		return ret;

	return 0;
}

static int max96716a_update_pipe_remaps(struct max_des_priv *des_priv,
				       struct max_des_pipe *pipe)
{
	struct max96716a_priv *priv = des_to_priv(des_priv);
	unsigned int i;
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	for (i = 0; i < pipe->num_remaps; i++) {
		struct max_des_dt_vc_remap *remap = &pipe->remaps[i];

		ret = max96716a_init_pipe_remap(priv, pipe, remap, i);
		if (ret)
			return ret;
	}

	return 0;
}

static int max96716a_select_links(struct max_des_priv *des_priv,
				 unsigned int mask)
{
	struct max96716a_priv *priv = des_to_priv(des_priv);
	int ret;

	dev_dbg(priv->dev, "%s(): mask %d\n", __func__, mask);

	if (!mask) {
		dev_err(priv->dev, "Disable all links unsupported\n");
		return -EINVAL;
	}

	ret = max96716a_update_bits(priv, MAX96716A_GMSL1_EN,
				   MAX96716A_GMSL1_EN_LINK_EN,
				   field_prep(MAX96716A_GMSL1_EN_LINK_EN, mask));
	if (ret)
		return ret;

	ret = max96716a_update_bits(priv, MAX96716A_CTRL0,
				   MAX96716A_CTRL0_LINK_CFG |
				   MAX96716A_CTRL0_AUTO_LINK |
				   MAX96716A_CTRL0_RESET_ONESHOT,
				   field_prep(MAX96716A_CTRL0_LINK_CFG, mask) |
				   MAX96716A_CTRL0_RESET_ONESHOT);
	if (ret)
		return ret;
	max96716a_update_bits(priv, MAX96716A_CTRL2,
				   MAX96716A_CTRL2_RESET_ONESHOT_B,
				   MAX96716A_CTRL2_RESET_ONESHOT_B);
	if (ret)
		return ret;

	msleep(65);

	return 0;
}

static int max96716a_post_init(struct max_des_priv *des_priv)
{
	struct max_des_subdev_priv *sd_priv;
	struct max_des_pipe *pipe;
	const struct max_format *fmt;
	int ret;

	dev_dbg(des_priv->dev, "%s()\n", __func__);

	for_each_subdev(des_priv, sd_priv) {
		pipe = &des_priv->pipes[sd_priv->pipe_id];

		if (!des_priv->links[pipe->link_id].enabled)
			continue;

		fmt = max_format_by_name(pipe->code_name);
		if (!fmt)
			return -EINVAL;
		sd_priv->fmt = fmt;

		dev_dbg(des_priv->dev,
			"%s: pipe_id [%d], phy_id [%d], src_vc_id [%d], dst_vc_id [%d]\n",
			sd_priv->label, sd_priv->pipe_id, sd_priv->phy_id,
			sd_priv->src_vc_id, sd_priv->dst_vc_id);

		mutex_lock(&des_priv->lock);
		ret = max_des_update_pipe_remaps(des_priv, pipe);
		mutex_unlock(&des_priv->lock);
		if (ret)
			return -EINVAL;
	}

	return max96716a_mipi_enable(des_priv, false);
}

static const struct max_des_ops max96716a_ops = {
	.log_pipe_status = max96716a_log_pipe_status,
	.log_phy_status = max96716a_log_phy_status,
	.mipi_enable = max96716a_mipi_enable,
	.set_pipe_enable = max96716a_set_pipe_enable,
	.init = max96716a_init,
	.init_phy = max96716a_init_phy,
	.init_pipe = max96716a_init_pipe,
	.init_link = max96716a_init_link,
	.init_fsync = max96716a_init_fsync,
	.update_pipe_remaps = max96716a_update_pipe_remaps,
	.select_links = max96716a_select_links,
	.post_init = max96716a_post_init,
};

static int max96716a_wait_for_multiple(struct i2c_client *client, struct regmap *regmap,
				u8 *addrs, unsigned int num_addrs)
{
	unsigned int i, j, val;
	int ret;

	for (i = 0; i < 10; i++) {
		for (j = 0; j < num_addrs; j++) {
			client->addr = addrs[j];

			ret = regmap_read(regmap, MAX96716A_REG0, &val);
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

static int max96716a_wait_for_device(struct max96716a_priv *priv)
{
	unsigned int i;
	int ret;

	for (i = 0; i < 10; i++) {
		ret = max96716a_read(priv, MAX96716A_REG0);
		if (ret >= 0)
			return 0;

		msleep(100);

		dev_err(priv->dev, "Retry %u waiting for deserializer: %d\n", i, ret);
	}

	return ret;
}

static int max96716a_reset(struct max96716a_priv *priv)
{
	struct i2c_client *client;
	struct regmap *regmap;
	int ret;
	u8 max96716a_addr[2] = { priv->client->addr, priv->i2c_addr };

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (priv->i2c_addr != priv->client->addr) {
		client = i2c_new_dummy_device(priv->client->adapter, priv->i2c_addr);
		if (IS_ERR(client)) {
			ret = PTR_ERR(client);
			dev_err(priv->dev,
				"Failed to create I2C client: %d\n", ret);
			return ret;
		}

		regmap = regmap_init_i2c(client, &max96716a_i2c_regmap);
		if (IS_ERR(regmap)) {
			ret = PTR_ERR(regmap);
			dev_err(priv->dev,
				"Failed to create I2C regmap: %d\n", ret);
			goto err_unregister_client;
		}

		ret = max96716a_wait_for_multiple(client, regmap, max96716a_addr, ARRAY_SIZE(max96716a_addr));
		if (ret) {
			dev_err(priv->dev,
				"Failed waiting for deserializer with new or old address: %d\n", ret);
			goto err_regmap_exit;
		}

		ret = regmap_write(regmap, MAX96716A_CTRL0, MAX96716A_CTRL0_RESET_ALL);
		if (ret) {
			dev_err(priv->dev, "Failed to soft reset deserializer: %d\n", ret);
			goto err_regmap_exit;
		}
		msleep(50);

		ret = max96716a_wait_for_multiple(client, regmap, max96716a_addr, ARRAY_SIZE(max96716a_addr));
		if (ret) {
			dev_err(priv->dev,
				"Failed waiting for deserializer with new or old address: %d\n", ret);
			goto err_regmap_exit;
		}

		ret = regmap_write(regmap, MAX96716A_REG0, priv->client->addr << 1);
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
		ret = max96716a_wait_for_device(priv);
		if (ret) {
			dev_err(priv->dev, "Failed waiting for MAX96716A, err: %d\n", ret);
			return ret;
		}

		ret = max96716a_update_bits(priv, MAX96716A_CTRL0,
					   MAX96716A_CTRL0_RESET_ALL,
					   MAX96716A_CTRL0_RESET_ALL);
		if (ret)
			return ret;

		msleep(50);

		ret = max96716a_wait_for_device(priv);
		if (ret) {
			dev_err(priv->dev, "Failed waiting for MAX96716A, err: %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static int max96716a_probe(struct i2c_client *client)
{
	struct regmap_config i2c_regmap = max96716a_i2c_regmap;
	struct device *dev = &client->dev;
	struct max96716a_priv *priv;
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

	priv->info = device_get_match_data(dev);
	if (!priv->info) {
		dev_err(dev, "Failed to get match data\n");
		return -ENODEV;
	}

	priv->dev = dev;
	priv->client = client;
	i2c_set_clientdata(client, priv);

	i2c_regmap.max_register = priv->info->max_register;
	priv->regmap = devm_regmap_init_i2c(client, &i2c_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	*ops = max96716a_ops;

	ops->fix_tx_ids = priv->info->fix_tx_ids;
	ops->num_phys = priv->info->num_phys;
	ops->num_pipes = priv->info->num_pipes;
	ops->num_links = priv->info->num_links;
	ops->supports_tunnel_mode = priv->info->supports_tunnel_mode;
	ops->supports_pipe_link_remap = priv->info->supports_pipe_link_remap;

	priv->des_priv.dev = dev;
	priv->des_priv.client = client;
	priv->des_priv.regmap = priv->regmap;
	priv->des_priv.ops = ops;

	priv->errb_enabled = true;
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
	msleep(50);

	ret = max96716a_reset(priv);
	if (ret)
		return ret;

	/* Register pin controller */
	priv->pctldesc = (struct pinctrl_desc){
		.owner = THIS_MODULE,
		.name = MAX96716A_NAME,
		.pins = max96716a_pins,
		.npins = ARRAY_SIZE(max96716a_pins),
		.pctlops = &max96716a_ctrl_ops,
		.confops = &max96716a_conf_ops,
		.pmxops = &max96716a_mux_ops,
		.custom_params = max96716a_cfg_params,
		.num_custom_params = ARRAY_SIZE(max96716a_cfg_params),
	};

	ret = devm_pinctrl_register_and_init(priv->dev, &priv->pctldesc, priv,
						&priv->pctldev);
	if (ret)
		return ret;

	ret = pinctrl_enable(priv->pctldev);
	if (ret)
		return ret;

	priv->gc = (struct gpio_chip){
		.owner = THIS_MODULE,
		.label = MAX96716A_NAME,
		.base = -1,
		.ngpio = MAX96716A_GPIO_NUM,
		.parent = priv->dev,
		.can_sleep = true,
		.request = gpiochip_generic_request,
		.free = gpiochip_generic_free,
		.set_config = gpiochip_generic_config,
		.get_direction = max96716a_gpio_get_direction,
		.direction_input = max96716a_gpio_direction_input,
		.direction_output = max96716a_gpio_direction_output,
		.get = max96716a_gpio_get,
		.set = max96716a_gpio_set,
	};

	ret = devm_gpiochip_add_data(priv->dev, &priv->gc, priv);
	if (ret)
		return ret;

	return max_des_probe(&priv->des_priv);
}

static void max96716a_remove(struct i2c_client *client)
{
	struct max96716a_priv *priv = i2c_get_clientdata(client);

	max_des_remove(&priv->des_priv);
}

static const struct max96716a_chip_info max96716a_info = {
	.max_register = 0x52d6,
	.phy0_first_lanes_on_master_phy = true,
	.supports_pipe_link_remap = true,
	.supports_tunnel_mode = true,
	.supports_phy_log = true,
	.fix_tx_ids = true,
	.num_pipes = 2,
	.pipe_hw_ids = { 1, 2 },
	.num_phys = 2,
	.phy_hw_ids = { 1, 2 },
	.num_links = 2,
};

static const struct of_device_id max96716a_of_table[] = {
	{ .compatible = "maxim,max96716a_tn", .data = &max96716a_info },
	{ },
};
MODULE_DEVICE_TABLE(of, max96716a_of_table);

static struct i2c_driver max96716a_i2c_driver = {
	.driver	= {
		.name = "max96716a",
		.of_match_table	= of_match_ptr(max96716a_of_table),
	},
	.probe = max96716a_probe,
	.remove = max96716a_remove,
};

module_i2c_driver(max96716a_i2c_driver);

MODULE_DESCRIPTION("Maxim MAX96716A Quad GMSL2 Deserializer Driver");
MODULE_AUTHOR("TechNexion Inc.");
MODULE_LICENSE("GPL");
