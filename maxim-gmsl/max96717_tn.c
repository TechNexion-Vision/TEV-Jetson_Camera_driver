// SPDX-License-Identifier: GPL-2.0
/*
 * Maxim MAX96717 GMSL2 Serializer Driver
 *
 */

#include <linux/gpio/driver.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>

#include "max_ser.h"
#include "max_serdes.h"

#define MAX96717_REG0								0x0

#define MAX96717_REG2								0x2
#define MAX96717_REG2_VID_TX_EN_P(p)				BIT(4 + (p))

#define MAX96717_REG3								0x3
#define MAX96717_REG3_RCLKSEL						GENMASK(1, 0)
#define MAX96717_REG3_RCLKSEL_REFERENCE_PLL			0b11

#define MAX96717_REG6								0x6
#define MAX96717_REG6_RCLKEN						BIT(5)

#define MAX96717_I2C_2(x)							(0x42 + (x) * 0x2)
#define MAX96717_I2C_2_SRC							GENMASK(7, 1)

#define MAX96717_I2C_3(x)							(0x43 + (x) * 0x2)
#define MAX96717_I2C_3_DST							GENMASK(7, 1)

#define MAX96717_TX3(p)								(0x53 + (p) * 0x4)
#define MAX96717_TX3_TX_STR_SEL						GENMASK(1, 0)

#define MAX96717_VIDEO_TX0(p)						(0x100 + (p) * 0x8)
#define MAX96717_VIDEO_TX0_AUTO_BPP					BIT(3)

#define MAX96717_VIDEO_TX1(p)						(0x101 + (p) * 0x8)
#define MAX96717_VIDEO_TX1_BPP						GENMASK(5, 0)

#define MAX96717_VIDEO_TX2(p)						(0x102 + (p) * 0x8)
#define MAX96717_VIDEO_TX2_PCLKDET					BIT(7)
#define MAX96717_VIDEO_TX2_DRIFT_DET_EN				BIT(1)

#define MAX96717_GPIO_A(x)							(0x2be + (x) * 0x3)
#define MAX96717_GPIO_A_GPIO_OUT_DIS				BIT(0)
#define MAX96717_GPIO_A_GPIO_TX_EN					BIT(1)
#define MAX96717_GPIO_A_GPIO_RX_EN					BIT(2)
#define MAX96717_GPIO_A_GPIO_IN						BIT(3)
#define MAX96717_GPIO_A_GPIO_OUT					BIT(4)
#define MAX96717_GPIO_A_TX_COMP_EN					BIT(5)
#define MAX96717_GPIO_A_RES_CFG						BIT(7)

#define MAX96717_GPIO_B(x)							(0x2bf + (x) * 0x3)
#define MAX96717_GPIO_B_GPIO_TX_ID					GENMASK(4, 0)
#define MAX96717_GPIO_B_OUT_TYPE					BIT(5)
#define MAX96717_GPIO_B_PULL_UPDN_SEL				GENMASK(7, 6)
#define MAX96717_GPIO_B_PULL_UPDN_SEL_NONE			0b00
#define MAX96717_GPIO_B_PULL_UPDN_SEL_PU			0b01
#define MAX96717_GPIO_B_PULL_UPDN_SEL_PD			0b10

#define MAX96717_GPIO_C(x)							(0x2c0 + (x) * 0x3)
#define MAX96717_GPIO_C_GPIO_RX_ID					GENMASK(4, 0)

#define MAX96717_CMU2								0x302
#define MAX96717_CMU2_PFDDIV_RSHORT					GENMASK(6, 4)
#define MAX96717_CMU2_PFDDIV_RSHORT_1_1V			0b001

#define MAX96717_FRONTTOP_0							0x308
#define MAX96717_FRONTTOP_0_CLK_SEL_P(x)			BIT(x)
#define MAX96717_FRONTTOP_0_START_PORT(x)			BIT((x) + 4)

#define MAX96717_FRONTTOP_1(p)						(0x309 + (p) * 0x2)
#define MAX96717_FRONTTOP_2(p)						(0x30a + (p) * 0x2)

#define MAX96717_FRONTTOP_9							0x311
#define MAX96717_FRONTTOP_9_START_PORT(p, x)		BIT((p) + (x) * 4)

#define MAX96717_FRONTTOP_10						0x312
#define MAX96717_FRONTTOP_10_BPP8DBL(p)				BIT(p)

#define MAX96717_FRONTTOP_11						0x313
#define MAX96717_FRONTTOP_11_BPP10DBL(p)			BIT(p)
#define MAX96717_FRONTTOP_11_BPP12DBL(p)			BIT((p) + 4)

#define MAX96717_FRONTTOP_12(p, x)					(0x314 + (p) * 0x2 + (x))
#define MAX96717_MEM_DT_SEL							GENMASK(5, 0)
#define MAX96717_MEM_DT_EN							BIT(6)

#define MAX96717_FRONTTOP_20(p)						(0x31c + (p) * 0x1)
#define MAX96717_FRONTTOP_20_SOFT_BPP_EN			BIT(5)
#define MAX96717_FRONTTOP_20_SOFT_BPP				GENMASK(4, 0)

#define MAX96717_MIPI_RX0							0x330
#define MAX96717_MIPI_RX0_NONCONTCLK_EN				BIT(6)
#define MAX96717_MIPI_RX0_MIPI_RX_RESET				BIT(3)

#define MAX96717_MIPI_RX1							0x331
#define MAX96717_MIPI_RX1_CTRL_NUM_LANES			GENMASK(5, 4)

#define MAX96717_MIPI_RX2							0x332
#define MAX96717_MIPI_RX2_PHY1_LANE_MAP				GENMASK(7, 4)

#define MAX96717_MIPI_RX3							0x333
#define MAX96717_MIPI_RX3_PHY2_LANE_MAP				GENMASK(3, 0)

#define MAX96717_MIPI_RX4							0x334
#define MAX96717_MIPI_RX4_PHY1_POL_MAP				GENMASK(5, 4)

#define MAX96717_MIPI_RX5							0x335
#define MAX96717_MIPI_RX5_PHY2_POL_MAP				GENMASK(1, 0)
#define MAX96717_MIPI_RX5_PHY2_POL_MAP_CLK			BIT(2)

#define MAX96717_EXT11								0x383
#define MAX96717_EXT11_TUN_MODE						BIT(7)

#define MAX96717_EXT21								0x38d
#define MAX96717_EXT22								0x38e
#define MAX96717_EXT23								0x38f
#define MAX96717_EXT24								0x390

#define MAX96717_EXTA(x)							(0x3dc + (x))

#define MAX96717_REF_VTG0							0x3f0
#define MAX96717_REF_VTG0_REFGEN_EN					BIT(0)
#define MAX96717_REF_VTG0_REFGEN_RST				BIT(1)
#define MAX96717_REF_VTG0_REFGEN_PREDEF_FREQ_ALT	BIT(3)
#define MAX96717_REF_VTG0_REFGEN_PREDEF_FREQ		GENMASK(5, 4)
#define MAX96717_REF_VTG0_REFGEN_PREDEF_EN			BIT(6)

#define MAX96717_REF_VTG1							0x3f1
#define MAX96717_REF_VTG1_PCLKEN					BIT(0)
#define MAX96717_REF_VTG1_PCLK_GPIO					GENMASK(5, 1)
#define MAX96717_REF_VTG1_RCLKEN_Y					BIT(7)

#define MAX96717_PIO_SLEW_0							0x56f
#define MAX96717_PIO_SLEW_0_PIO00_SLEW				GENMASK(1, 0)
#define MAX96717_PIO_SLEW_0_PIO01_SLEW				GENMASK(3, 2)
#define MAX96717_PIO_SLEW_0_PIO02_SLEW				GENMASK(5, 4)

#define MAX96717_PIO_SLEW_1							0x570
#define MAX96717_PIO_SLEW_1_PIO05_SLEW				GENMASK(3, 2)
#define MAX96717_PIO_SLEW_1_PIO06_SLEW				GENMASK(5, 4)

#define MAX96717_PIO_SLEW_2							0x571
#define MAX96717_PIO_SLEW_2_PIO010_SLEW				GENMASK(5, 4)
#define MAX96717_PIO_SLEW_2_PIO011_SLEW				GENMASK(7, 6)

#define MAX96717_RLMSCE								0x14ce
#define MAX96717_RLMSCE_ENFFE						BIT(0)
#define MAX96717_RLMSCE_ENMINUS_MAN					BIT(3)
#define MAX96717_RLMSCE_ENMINUS_REG					BIT(4)

#define MAX96717_NAME					"max96717"
#define MAX96717_PINCTRL_NAME			MAX96717_NAME "-pinctrl"
#define MAX96717_GPIOCHIP_NAME			MAX96717_NAME "-gpiochip"
#define MAX96717_GPIO_NUM				11
#define MAX96717_PIPES_NUM				4
#define MAX96717_PHYS_NUM				2
#define MAX96717_LANE_CONFIGS_NUM		4

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

#define ser_to_priv(ser) \
	container_of(ser, struct max96717_priv, ser_priv)

static int max96717_read(struct max96717_priv *priv, int reg)
{
	int ret, val;
	int cnt = 3;

	while (cnt--) {
		ret = regmap_read(priv->regmap, reg, &val);
		dev_dbg(priv->dev, "read %d 0x%x = 0x%02x\n", ret, reg, val);
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

static int max96717_write(struct max96717_priv *priv, unsigned int reg, u8 val)
{
	int ret;
	int cnt = 3;

	while (cnt--) {
		ret = regmap_write(priv->regmap, reg, val);
		dev_dbg(priv->dev, "write %d 0x%x = 0x%02x\n", ret, reg, val);
		if (!ret) {
			break;
		}
		dev_dbg(priv->dev, "%s(): retry %d\n", __func__, cnt);
	}

	if (ret)
		dev_err(priv->dev, "write 0x%04x failed\n", reg);

	return ret;
}

static int max96717_update_bits(struct max96717_priv *priv, unsigned int reg,
				u8 mask, u8 val)
{
	int ret;
	int cnt = 3;

	while (cnt--) {
		ret = regmap_update_bits(priv->regmap, reg, mask, val);
		dev_dbg(priv->dev, "update %d 0x%x 0x%02x = 0x%02x\n", ret, reg, mask, val);
		if (!ret) {
			break;
		}
		dev_dbg(priv->dev, "%s(): retry %d\n", __func__, cnt);
	}

	if (ret)
		dev_err(priv->dev, "update 0x%04x failed\n", reg);

	return ret;
}

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
	*reg = MAX96717_GPIO_A(offset);

	switch (param) {
	case PIN_CONFIG_OUTPUT_ENABLE:
		*mask = MAX96717_GPIO_A_GPIO_OUT_DIS;
		*val = 0b0;
		return 0;
	case PIN_CONFIG_INPUT_ENABLE:
		*mask = MAX96717_GPIO_A_GPIO_OUT_DIS;
		*val = 0b1;
		return 0;
	case MAX96717_PINCTRL_GMSL_TX_EN:
		*mask = MAX96717_GPIO_A_GPIO_TX_EN;
		*val = 0b1;
		return 0;
	case MAX96717_PINCTRL_GMSL_RX_EN:
		*mask = MAX96717_GPIO_A_GPIO_RX_EN;
		*val = 0b1;
		return 0;
	case MAX96717_PINCTRL_INPUT_VALUE:
		*mask = MAX96717_GPIO_A_GPIO_IN;
		*val = 0b1;
		return 0;
	case PIN_CONFIG_OUTPUT:
		*mask = MAX96717_GPIO_A_GPIO_OUT;
		*val = 0b1;
		return 0;
	case MAX96717_PINCTRL_JITTER_COMPENSATION_EN:
		*mask = MAX96717_GPIO_A_TX_COMP_EN;
		*val = 0b1;
		return 0;
	case MAX96717_PINCTRL_PULL_STRENGTH_WEAK:
		*mask = MAX96717_GPIO_A_RES_CFG;
		*val = 0b0;
		return 0;
	}

	*reg = MAX96717_GPIO_B(offset);

	switch (param) {
	case MAX96717_PINCTRL_GMSL_TX_ID:
		*mask = MAX96717_GPIO_B_GPIO_TX_ID;
		return 0;
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		*mask = MAX96717_GPIO_B_OUT_TYPE;
		*val = 0b0;
		return 0;
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		*mask = MAX96717_GPIO_B_OUT_TYPE;
		*val = 0b1;
		return 0;
	case PIN_CONFIG_BIAS_DISABLE:
		*mask = MAX96717_GPIO_B_PULL_UPDN_SEL;
		*val = MAX96717_GPIO_B_PULL_UPDN_SEL_NONE;
		return 0;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		*mask = MAX96717_GPIO_B_PULL_UPDN_SEL;
		*val = MAX96717_GPIO_B_PULL_UPDN_SEL_PD;
		return 0;
	case PIN_CONFIG_BIAS_PULL_UP:
		*mask = MAX96717_GPIO_B_PULL_UPDN_SEL;
		*val = MAX96717_GPIO_B_PULL_UPDN_SEL_PU;
		return 0;
	}

	switch (param) {
	case PIN_CONFIG_SLEW_RATE:
		if (offset < 3) {
			*reg = MAX96717_PIO_SLEW_0;
			if (offset == 0)
				*mask = MAX96717_PIO_SLEW_0_PIO00_SLEW;
			else if (offset == 1)
				*mask = MAX96717_PIO_SLEW_0_PIO01_SLEW;
			else
				*mask = MAX96717_PIO_SLEW_0_PIO02_SLEW;
		} else if (offset < 5) {
			*reg = MAX96717_PIO_SLEW_1;
			if (offset == 3)
				*mask = MAX96717_PIO_SLEW_1_PIO05_SLEW;
			else
				*mask = MAX96717_PIO_SLEW_1_PIO06_SLEW;
		} else if (offset < 7) {
			return -EINVAL;
		} else if (offset < 9) {
			*reg  = MAX96717_PIO_SLEW_2;
			if (offset == 7)
				*mask = MAX96717_PIO_SLEW_2_PIO010_SLEW;
			else
				*mask = MAX96717_PIO_SLEW_2_PIO011_SLEW;
		} else {
			return -EINVAL;
		}
		return 0;
	case MAX96717_PINCTRL_GMSL_RX_ID:
		*reg = MAX96717_GPIO_C(offset);
		*mask = MAX96717_GPIO_C_GPIO_RX_ID;
		return 0;
	case MAX96717_PINCTRL_RCLKOUT_CLK:
		if (offset != 2 && offset != 4)
			return -EINVAL;

		*reg = MAX96717_REG3;
		*mask = MAX96717_REG3_RCLKSEL;
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

		val = field_get(mask, ret) == val;
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

		val = field_get(mask, ret) == val;
		break;
	case MAX96717_PINCTRL_GMSL_TX_ID:
	case MAX96717_PINCTRL_GMSL_RX_ID:
	case PIN_CONFIG_SLEW_RATE:
	case MAX96717_PINCTRL_RCLKOUT_CLK:
		ret = max96717_read(priv, reg);
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

static int max96717_conf_pin_config_set_one(struct max96717_priv *priv,
					    unsigned int offset,
					    unsigned long config)
{
	u32 param = pinconf_to_config_param(config);
	u32 arg = pinconf_to_config_argument(config);
	unsigned int reg, mask, val;
	int ret;

	ret = max96717_get_pin_config_reg(offset, param, &reg, &mask, &val);
	if (ret)
		return ret;

	switch (param) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
	case PIN_CONFIG_DRIVE_PUSH_PULL:
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		val = field_prep(mask, val);

		ret = max96717_update_bits(priv, reg, mask, val);
		break;
	case MAX96717_PINCTRL_JITTER_COMPENSATION_EN:
	case MAX96717_PINCTRL_PULL_STRENGTH_WEAK:
	case MAX96717_PINCTRL_GMSL_TX_EN:
	case MAX96717_PINCTRL_GMSL_RX_EN:
	case PIN_CONFIG_OUTPUT_ENABLE:
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT:
		val = field_prep(mask, arg ? val : ~val);

		ret = max96717_update_bits(priv, reg, mask, val);
		break;
	case MAX96717_PINCTRL_GMSL_TX_ID:
	case MAX96717_PINCTRL_GMSL_RX_ID:
	case PIN_CONFIG_SLEW_RATE:
	case MAX96717_PINCTRL_RCLKOUT_CLK:
		val = field_prep(mask, arg);

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

	ret = max96717_update_bits(priv, MAX96717_REF_VTG1,
				   MAX96717_REF_VTG1_PCLKEN,
				   MAX96717_REF_VTG1_PCLKEN);
	if (ret)
		return ret;

	ret = max96717_update_bits(priv, MAX96717_REF_VTG1,
				   MAX96717_REF_VTG1_PCLK_GPIO,
				   field_prep(MAX96717_REF_VTG1_PCLK_GPIO, group));
	if (ret)
		return ret;

	return 0;
}

static int max96717_mux_set_rclkout(struct max96717_priv *priv, unsigned int group)
{
	int ret;

	/* Enable RCLK. */
	ret = max96717_update_bits(priv, MAX96717_REG6,
				   MAX96717_REG6_RCLKEN,
				   MAX96717_REG6_RCLKEN);
	if (ret)
		return ret;

	/* Enable RCLK output on PCLK. */
	ret = max96717_update_bits(priv, MAX96717_REF_VTG1,
				   MAX96717_REF_VTG1_RCLKEN_Y,
				   MAX96717_REF_VTG1_RCLKEN_Y);
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

	ret = max96717_read(priv, MAX96717_EXT23);
	if (ret < 0)
		return ret;

	pr_info("%s: \t\ttun_pkt_cnt: %u\n", name, ret);

	return 0;
}

static int max96717_log_pipe_status(struct max_ser_priv *ser_priv,
				    struct max_ser_pipe *pipe,
				    const char *name)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	unsigned int index = max96717_pipe_id(priv, pipe);
	int ret;

	ret = max96717_read(priv, MAX96717_VIDEO_TX2(index));
	if (ret < 0)
		return ret;

	pr_info("%s: \tpclkdet: %u\n", name, !!(ret & MAX96717_VIDEO_TX2_PCLKDET));

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

	ret = max96717_read(priv, MAX96717_EXT21);
	if (ret < 0)
		return ret;

	pr_info("%s: \tphy_pkt_cnt: %u\n", name, ret);

	ret = max96717_read(priv, MAX96717_EXT22);
	if (ret < 0)
		return ret;

	pr_info("%s: \tcsi_pkt_cnt: %u\n", name, ret);

	ret = max96717_read(priv, MAX96717_EXT24);
	if (ret < 0)
		return ret;

	pr_info("%s: \tphy_clk_cnt: %u\n", name, ret);

	return 0;
}

static int max96717_set_pipe_enable(struct max_ser_priv *ser_priv,
				    struct max_ser_pipe *pipe, bool enable)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	unsigned int index = max96717_pipe_id(priv, pipe);
	unsigned int mask = MAX96717_REG2_VID_TX_EN_P(index);

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (enable) {
		max96717_update_bits(priv, MAX96717_MIPI_RX0,
					MAX96717_MIPI_RX0_MIPI_RX_RESET,
					MAX96717_MIPI_RX0_MIPI_RX_RESET);
		max96717_update_bits(priv, MAX96717_MIPI_RX0,
					MAX96717_MIPI_RX0_MIPI_RX_RESET,
					0x00);
	}

	return max96717_update_bits(priv, MAX96717_REG2, mask, enable ? mask : 0);
}

static int max96717_set_pipe_dt_en(struct max96717_priv *priv,
				   struct max_ser_pipe *pipe,
				   unsigned int i, bool en)
{
	unsigned int index = max96717_pipe_id(priv, pipe);
	unsigned int reg;

	if (i < 2) {
		reg = MAX96717_FRONTTOP_12(index, i);
	} else {
		reg = MAX96717_EXTA(i - 2);
	}

	return max96717_update_bits(priv, reg, MAX96717_MEM_DT_EN,
				   field_prep(MAX96717_MEM_DT_EN, en));
}

static int max96717_set_pipe_dt(struct max96717_priv *priv,
				struct max_ser_pipe *pipe,
				unsigned int i)
{
	unsigned int index = max96717_pipe_id(priv, pipe);
	u32 dt = pipe->dts[i];
	unsigned int reg;

	if (i < 2)
		reg = MAX96717_FRONTTOP_12(index,  i);
	else
		reg = MAX96717_EXTA(i - 2);

	return max96717_update_bits(priv, reg, MAX96717_MEM_DT_SEL,
				   field_prep(MAX96717_MEM_DT_SEL, dt));
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
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96717_write(priv, MAX96717_FRONTTOP_1(index),
				   (pipe->vcs >> 0) & 0xff);
	if (ret)
		return ret;

	return max96717_write(priv, MAX96717_FRONTTOP_2(index),
				   (pipe->vcs >> 8) & 0xff);
}

static int max96717_init_lane_config(struct max96717_priv *priv)
{
	struct max_ser_priv *ser_priv = &priv->ser_priv;
	struct max_ser_phy *phy;
	unsigned int i, j;

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

	return 0;
}

static int max96717_init(struct max_ser_priv *ser_priv)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	/*
	 * Set CMU2 PFDDIV to 1.1V for correct functionality of the device,
	 * as mentioned in the datasheet, under section MANDATORY REGISTER PROGRAMMING.
	 */
	ret = max96717_update_bits(priv, MAX96717_CMU2,
				   MAX96717_CMU2_PFDDIV_RSHORT,
				   field_prep(MAX96717_CMU2_PFDDIV_RSHORT,
					    MAX96717_CMU2_PFDDIV_RSHORT_1_1V));
	if (ret)
		return ret;

	/* Enable GMSL negative output in coax mode for optimal performance */
	ret = max96717_update_bits(priv, MAX96717_RLMSCE,
				   MAX96717_RLMSCE_ENMINUS_MAN |
				   MAX96717_RLMSCE_ENMINUS_REG,
				   field_prep(MAX96717_RLMSCE_ENMINUS_MAN,
						MAX96717_RLMSCE_ENMINUS_MAN) |
				   field_prep(MAX96717_RLMSCE_ENMINUS_REG,
						MAX96717_RLMSCE_ENMINUS_REG));
	if (ret)
		return ret;

	if (priv->info->supports_tunnel_mode) {

		ret = max96717_update_bits(priv, MAX96717_EXT11,
					   MAX96717_EXT11_TUN_MODE,
				   	   field_prep(MAX96717_EXT11_TUN_MODE,
					    	ser_priv->tunnel_mode));
		if (ret)
			return ret;
	}

	/* Disable ports. */
	ret = max96717_update_bits(priv, MAX96717_FRONTTOP_0, GENMASK(5, 4), 0x00);
	if (ret)
		return ret;

	/* Reset pipe to ports mapping. */
	ret = max96717_update_bits(priv, MAX96717_FRONTTOP_0, GENMASK(3, 0), 0x00);
	if (ret)
		return ret;

	/* Disable pipes. */
	ret = max96717_write(priv, MAX96717_FRONTTOP_9, 0x00);
	if (ret)
		return ret;

	ret = max96717_init_lane_config(priv);
	if (ret)
		return ret;

	return 0;
}

static int max96717_init_i2c_xlate(struct max_ser_priv *ser_priv, unsigned int i)
{
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96717_update_bits(priv, MAX96717_I2C_2(i),
					MAX96717_I2C_2_SRC,
					field_prep(MAX96717_I2C_2_SRC,
						ser_priv->i2c_xlates[i].src));
	if (ret)
		return ret;

	ret = max96717_update_bits(priv, MAX96717_I2C_3(i),
					MAX96717_I2C_3_DST,
					field_prep(MAX96717_I2C_3_DST,
						ser_priv->i2c_xlates[i].dst));
	if (ret)
		return ret;

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

	/* Configure a lane count. */
	/* TODO: Add support for 1-lane configurations. */
	ret = max96717_update_bits(priv, MAX96717_MIPI_RX1,
				   MAX96717_MIPI_RX1_CTRL_NUM_LANES,
				   field_prep(MAX96717_MIPI_RX1_CTRL_NUM_LANES,
					    num_data_lanes - 1));
	if (ret)
		return ret;

	/* Configure lane mapping. */
	/* TODO: Add support for lane swapping. */
	/* TODO: Handle PHY A. */
	ret = max96717_update_bits(priv, MAX96717_MIPI_RX2,
				   MAX96717_MIPI_RX2_PHY1_LANE_MAP, 0xe0);
	if (ret)
		return ret;

	ret = max96717_update_bits(priv, MAX96717_MIPI_RX3,
				   MAX96717_MIPI_RX3_PHY2_LANE_MAP, 0x04);
	if (ret)
		return ret;

	/* Configure lane polarity. */
	/* Lower two lanes. */
	/* TODO: Handle PHY A. */
	val = 0;
	for (i = 0; i < 3 && i < num_data_lanes + 1; i++)
		if (phy->mipi.lane_polarities[i])
			val |= BIT(i == 0 ? 2 : i - 1);
	ret = max96717_update_bits(priv, MAX96717_MIPI_RX5, GENMASK(2, 0), val);
	if (ret)
		return ret;

	/* Upper two lanes. */
	val = 0;
	shift = 4;
	mask = GENMASK(2, 0);
	for (i = 3; i < num_data_lanes + 1; i++)
		if (phy->mipi.lane_polarities[i])
			val |= BIT(i - 3);
	ret = max96717_update_bits(priv, MAX96717_MIPI_RX4, mask << shift, val << shift);
	if (ret)
		return ret;

	if (priv->info->supports_noncontinuous_clock) {
		ret = max96717_update_bits(priv, MAX96717_MIPI_RX0,
					   MAX96717_MIPI_RX0_NONCONTCLK_EN,
					   phy->mipi.flags &
					   V4L2_MBUS_CSI2_NONCONTINUOUS_CLOCK
					   ? MAX96717_MIPI_RX0_NONCONTCLK_EN : 0x00);
		if (ret)
			return ret;
	}

	/* Enable PHY. */
	ret = max96717_update_bits(priv, MAX96717_FRONTTOP_0,
				   MAX96717_FRONTTOP_0_START_PORT(index),
				   MAX96717_FRONTTOP_0_START_PORT(index));
	if (ret)
		return ret;

	return 0;
}

static int max96717_init_pipe_stream_id(struct max96717_priv *priv,
					struct max_ser_pipe *pipe)
{
	unsigned int index = max96717_pipe_id(priv, pipe);

	return max96717_write(priv, MAX96717_TX3(index), pipe->stream_id);
}

static int max96717_init_pipe(struct max_ser_priv *ser_priv,
			      struct max_ser_pipe *pipe)
{
	struct max_ser_phy *phy = max_ser_pipe_phy(ser_priv, pipe);
	struct max96717_priv *priv = ser_to_priv(ser_priv);
	unsigned int index = max96717_pipe_id(priv, pipe);
	unsigned int phy_id = max96717_phy_id(priv, phy);
	int ret;

	dev_dbg(priv->dev, "%s()\n", __func__);

	ret = max96717_set_pipe_enable(ser_priv, pipe, false);
	if (ret)
		return ret;

	/* Map pipe to PHY. */
	ret = max96717_update_bits(priv, MAX96717_FRONTTOP_0,
				   MAX96717_FRONTTOP_0_CLK_SEL_P(index),
				   field_prep(MAX96717_FRONTTOP_0_CLK_SEL_P(index), phy_id));
	if (ret)
		return ret;

	/* Enable pipe output to PHY. */
	ret = max96717_update_bits(priv, MAX96717_FRONTTOP_9,
				   MAX96717_FRONTTOP_9_START_PORT(index, phy_id),
				   MAX96717_FRONTTOP_9_START_PORT(index, phy_id));
	if (ret)
		return ret;

	/* Set 8bit double mode. */
	ret = max96717_update_bits(priv, MAX96717_FRONTTOP_10,
				   MAX96717_FRONTTOP_10_BPP8DBL(index),
				   field_prep(MAX96717_FRONTTOP_10_BPP8DBL(index),
				   		pipe->dbl8));
	if (ret)
		return ret;

	/* Set 10bit double mode. */
	ret = max96717_update_bits(priv, MAX96717_FRONTTOP_11,
				   MAX96717_FRONTTOP_11_BPP10DBL(index),
				   field_prep(MAX96717_FRONTTOP_11_BPP10DBL(index),
				   		pipe->dbl10));
	if (ret)
		return ret;

	/* Set 12bit double mode. */
	ret = max96717_update_bits(priv, MAX96717_FRONTTOP_11,
				   MAX96717_FRONTTOP_11_BPP12DBL(index),
				   field_prep(MAX96717_FRONTTOP_11_BPP12DBL(index),
				   		pipe->dbl12));
	if (ret)
		return ret;

	/* Set override soft BPP. */
	ret = max96717_update_bits(priv, MAX96717_FRONTTOP_20(index),
				   MAX96717_FRONTTOP_20_SOFT_BPP_EN |
				   MAX96717_FRONTTOP_20_SOFT_BPP,
				   field_prep(MAX96717_FRONTTOP_20_SOFT_BPP_EN,
				   		!!pipe->soft_bpp) |
				   field_prep(MAX96717_FRONTTOP_20_SOFT_BPP,
				   		pipe->soft_bpp));
	if (ret)
		return ret;

	/* Set override BPP. */
	ret = max96717_update_bits(priv, MAX96717_VIDEO_TX0(index),
				   MAX96717_VIDEO_TX0_AUTO_BPP,
				   field_prep(MAX96717_VIDEO_TX0_AUTO_BPP,
				   		!pipe->bpp));
	if (ret)
		return ret;

	/* Set BPP. */
	ret = max96717_update_bits(priv, MAX96717_VIDEO_TX1(index),
				   MAX96717_VIDEO_TX1_BPP,
				   field_prep(MAX96717_VIDEO_TX1_BPP,
				   		pipe->bpp));
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

	for_each_subdev(ser_priv, sd_priv) {
		pipe = &ser_priv->pipes[sd_priv->pipe_id];

		fmt = max_format_by_name(pipe->code_name);
		if (!fmt)
			return -EINVAL;
		sd_priv->fmt = fmt;

		mutex_lock(&ser_priv->lock);
		ret = max_ser_update_pipe_dts(ser_priv, pipe);
		mutex_unlock(&ser_priv->lock);
		if (ret)
			return -EINVAL;
	}

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

static int max96717_wait_for_device(struct max96717_priv *priv)
{
	unsigned int i;
	int ret;

	for (i = 0; i < 10; i++) {
		ret = max96717_read(priv, MAX96717_REG0);
		if (ret >= 0)
			return 0;

		msleep(100);

		dev_err(priv->dev, "Retry %u waiting for serializer: %d\n", i, ret);
	}

	return ret;
}

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
	if (ret)
		return ret;

	/* Register pin controller */
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
	{ .compatible = "maxim,max96717_tn", .data = &max96717_info },
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
