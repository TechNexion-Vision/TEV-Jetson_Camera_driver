// #define DEBUG
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/tegra_v4l2_camera.h>
#include <media/tegracam_core.h>

#include "otp_flash.h"

#define AP1302_BRIGHTNESS						(0x7000)
#define AP1302_BRIGHTNESS_MASK					(0xFFFF)
#define AP1302_CONTRAST							(0x7002)
#define AP1302_CONTRAST_MASK					(0xFFFF)
#define AP1302_SATURATION						(0x7006)
#define AP1302_SATURATION_MASK					(0xFFFF)
#define AP1302_AWB_CTRL_MODE					(0x5100)
#define AP1302_AWB_CTRL_MODE_MASK				(0x00FF)
#define AP1302_AWB_CTRL_MODE_MANUAL_TEMP 		(7U << 0)
#define AP1302_AWB_CTRL_MODE_AUTO				(15U << 0)
#define AP1302_AWB_CTRL_MODE_MANUAL_TEMP_IDX 	(0U << 0)
#define AP1302_AWB_CTRL_MODE_AUTO_IDX			(1U << 0)
#define AP1302_GAMMA							(0x700A)
#define AP1302_GAMMA_MASK						(0xFFFF)
#define AP1302_AE_MANUAL_EXP_TIME				(0x500C)
#define AP1302_AE_MANUAL_EXP_TIME_MASK			(0xFFFFFFFF)
#define AP1302_AE_MANUAL_GAIN					(0x5006)
#define AP1302_AE_MANUAL_GAIN_MASK				(0xFFFF)
#define AP1302_ORIENTATION						(0x100C)
#define AP1302_ORIENTATION_HFLIP				(1U << 0)
#define AP1302_ORIENTATION_VFLIP				(1U << 1)
#define AP1302_AWB_MANUAL_TEMP					(0x510A)
#define AP1302_AWB_MANUAL_TEMP_MASK				(0xFFFF)
#define AP1302_SHARPEN							(0x7010)
#define AP1302_SHARPEN_MASK						(0xFFFF)
#define AP1302_BACKLIGHT_COMPENSATION 			(0x501A)
#define AP1302_BACKLIGHT_COMPENSATION_MASK		(0xFFFF)
#define AP1302_DZ_TGT_FCT						(0x1010)
#define AP1302_DZ_TGT_FCT_MASK					(0xFFFF)
#define AP1302_SFX_MODE							(0x1016)
#define AP1302_SFX_MODE_SFX_MASK				(0x00FF)
#define AP1302_SFX_MODE_SFX_NORMAL				(0U << 0)
#define AP1302_SFX_MODE_SFX_BW					(3U << 0)
#define AP1302_SFX_MODE_SFX_GRAYSCALE			(6U << 0)
#define AP1302_SFX_MODE_SFX_NEGATIVE			(7U << 0)
#define AP1302_SFX_MODE_SFX_SKETCH				(15U << 0)
#define AP1302_SFX_MODE_SFX_NORMAL_IDX			(0U << 0)
#define AP1302_SFX_MODE_SFX_BW_IDX				(1U << 0)
#define AP1302_SFX_MODE_SFX_GRAYSCALE_IDX		(2U << 0)
#define AP1302_SFX_MODE_SFX_NEGATIVE_IDX		(3U << 0)
#define AP1302_SFX_MODE_SFX_SKETCH_IDX			(4U << 0)
#define AP1302_AE_CTRL_MODE						(0x5002)
#define AP1302_AE_CTRL_MODE_MASK				(0x00FF)
#define AP1302_AE_CTRL_MANUAL_EXP_TIME_GAIN		(0U << 0)
#define AP1302_AE_CTRL_FULL_AUTO				(12U << 0)
#define AP1302_AE_CTRL_MANUAL_EXP_TIME_GAIN_IDX	(0U << 0)
#define AP1302_AE_CTRL_FULL_AUTO_IDX			(1U << 0)
#define AP1302_DZ_CT_X							(0x118C)
#define AP1302_DZ_CT_X_MASK						(0xFFFF)
#define AP1302_DZ_CT_Y							(0x118E)
#define AP1302_DZ_CT_Y_MASK						(0xFFFF)
#define AP1302_FLICK_CTRL                       (0x5440)
#define AP1302_FLICK_CTRL_FREQ(n)				((n) << 8)
#define AP1302_FLICK_CTRL_ETC_IHDR_UP			BIT(6)
#define AP1302_FLICK_CTRL_ETC_DIS				BIT(5)
#define AP1302_FLICK_CTRL_FRC_OVERRIDE_MAX_ET	BIT(4)
#define AP1302_FLICK_CTRL_FRC_OVERRIDE_UPPER_ET	BIT(3)
#define AP1302_FLICK_CTRL_FRC_EN				BIT(2)
#define AP1302_FLICK_CTRL_MODE_MASK				(0x03)
#define AP1302_FLICK_CTRL_ETC_IHDR_UP			BIT(6)
#define AP1302_FLICK_CTRL_ETC_DIS				BIT(5)
#define AP1302_FLICK_CTRL_FRC_OVERRIDE_MAX_ET	BIT(4)
#define AP1302_FLICK_CTRL_FRC_OVERRIDE_UPPER_ET	BIT(3)
#define AP1302_FLICK_CTRL_FRC_EN				BIT(2)
#define AP1302_FLICK_CTRL_MODE_DISABLED         (0U << 0)
#define AP1302_FLICK_CTRL_MODE_MANUAL           (1U << 0)
#define AP1302_FLICK_CTRL_MODE_AUTO             (2U << 0)
#define AP1302_FLICK_CTRL_FREQ_MASK			    (0xFF00)
#define AP1302_FLICK_CTRL_MODE_50HZ             (AP1302_FLICK_CTRL_FREQ(50) | AP1302_FLICK_CTRL_MODE_MANUAL)
#define AP1302_FLICK_CTRL_MODE_60HZ             (AP1302_FLICK_CTRL_FREQ(60) | AP1302_FLICK_CTRL_MODE_MANUAL)
#define AP1302_FLICK_MODE_DISABLED_IDX			(0U << 0)
#define AP1302_FLICK_MODE_ENABLED_IDX			(3U << 0)

#define V4L2_CID_SENSOR_FLASH_ID            (V4L2_CID_USER_BASE + 44)

static const struct of_device_id __of_match[] = {
	{ .compatible = "tn,tevi-ar0144", },
	{ },
};
MODULE_DEVICE_TABLE(of, __of_match);

/*
 * WARNING: frmfmt ordering need to match mode definition in
 * device tree!
 */
static const int __30fps = 30;
static const int __15fps = 15;
static const struct camera_common_frmfmt sensor_frmfmt[] = {
	{{1280, 800}, &__30fps, 1, 0, 0 /* number depend on dts mode list */},
	{{1280, 720}, &__30fps, 2, 0, 0 /* number depend on dts mode list */},
};

struct sensor_obj {
	struct v4l2_subdev		*subdev;
	struct camera_common_data	*s_data;
	struct tegracam_device		*tc_dev;
	struct i2c_client         *dser_i2c;
	void				*otp_flash_instance;
	u32				frame_length;
	u32             vc_id;
	u32             gpio_addr;
	u32             ser_alias_id;
	u8              flash_id;

	struct mutex lock; /* Protects formats */
};

static int ar0144_i2c_read_8b(struct i2c_client *client, u8 reg, u8 *val, u8 size)
{
	dev_dbg(&client->dev, "read reg 0x%x, size %d\n", reg, size);

	if (1 != i2c_master_send(client, &reg, 1))
		goto __except;
	if (size != i2c_master_recv(client, val, size))
		goto __except;
	return 0;

__except:
	dev_err(&client->dev, "failed to i2c read. reg 0x%x\n", reg);
	return -EIO;
}

static int ar0144_i2c_read(struct i2c_client *client, u16 reg, u8 *val, u8 size)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret = 0;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = val;
	msg[1].len = size;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0)
	{
		//dev_err(&client->dev, "i2c transfer error.\n");
		dev_err(&client->dev, "i2c[R] transfer error. addr=%x, reg=%x, val=%x, size=%d\n", client->addr, reg, val[0], size);
		return -EIO;
	}

	return ret;
	//return i2c_transfer(client->adapter, msg, 2);
}

static int ar0144_i2c_read_16b(struct i2c_client *client, u16 reg, u16 *value)
{
	u8 v[2] = {0,0};
	int ret;

	ret = ar0144_i2c_read(client, reg, v, 2);

	if (unlikely(ret < 0)) {
		dev_err(&client->dev, "i2c[R16] transfer error. addr=%x, reg=%x, val=%x\n", client->addr, reg, *value);
		return ret;
	}

	*value = (v[0] << 8) | v[1];
	dev_dbg(&client->dev, "%s() read reg 0x%x, value 0x%x\n",
		__func__, reg, *value);

	return 0;
}

static int __i2c_write_8b(struct i2c_client *client, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	buf[0] = reg;
	buf[1] = val;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
	{
		//dev_err(&client->dev, "i2c transfer error.\n");
		dev_err(&client->dev, "i2c[W] transfer error. addr=%x, reg=%x, val=%x\n", client->addr, reg, val);
		return -EIO;
	}

	return 0;
}

static int __i2c_write_16b(struct i2c_client *client, u16 reg, u16 val)
{
	struct i2c_msg msg;
	u8 buf[4];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 8;
	buf[3] = val & 0xff;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
	{
		//dev_err(&client->dev, "i2c transfer error.\n");
		dev_err(&client->dev, "i2c[W16] transfer error. addr=%x, reg=%x, val=%x\n", client->addr, reg, val);
		return -EIO;
	}

	return 0;
}

static int __i2c_write_bust(struct i2c_client *client, u8 *buf, size_t len)
{
	struct i2c_msg msg;
	int ret;

	if (len == 0) {
		return 0;
	}

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = len;

	// client->adapter->timeout = 0x7fffffff;
	// client->adapter->retries = 10;
	// dev_info(&client->dev, "%s(): addr=0x%02x%02x, data=0x%02x%02x\n", 
	// 			msg.buf, (msg.buf + 1), (msg.buf + 2), (msg.buf + 3));

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
	{
		dev_err(&client->dev, "i2c transfer error.\n");
		return -EIO;
	}

	return 0;
}

// static int __extend_ub953_setting(struct i2c_client *client, struct sensor_obj *priv)
// {
// 	int ret;
// 	int count = 0;
// 	u8 v = 0;
// 	u8 old_addr = client->addr;

// 	dev_dbg(&client->dev, "%s(): ser_id: 0x%02x\n", __func__, priv->ser_alias_id);
// 	client->addr = priv->ser_alias_id;
// 	ret = __i2c_write_8b(client, 3, 0x5b);
// 	msleep(500);

// 	ar0144_i2c_read_8b(client, 0x3, &v, 1);

// 	while((v & 0x13) != 0x13){
// 		if(count++ > 10){
// 			dev_err(&client->dev, "Set Async Mode Fail!\n");
// 			return -EINVAL;
// 		}
// 		// __i2c_write(client, ser_alias_addr, 3, v | 0x12);
// 		__i2c_write_8b(client, 3, v | 0x13);
// 		msleep(50);
// 		// ar0144_i2c_read(client, ser_alias_addr, 3, &v, 1);
// 		ar0144_i2c_read_8b(client, 0x3, &v, 1);
// 	}

// 	client->addr = old_addr;

// 	return ret;
// }

// static int __extend_gpio_setting(struct i2c_client *client, struct sensor_obj *priv)
// {
// 	int ret;
// 	u8 old_addr = client->addr;

// 	dev_dbg(&client->dev, "%s()\n", __func__);
// 	client->addr = priv->gpio_addr; //TN238 extend gpio chip i2c address
// 	ret = __i2c_write_8b(client, 3, 0xFA);
// 	ret = __i2c_write_8b(client, 1, 1);
// 	client->addr = old_addr;

// 	return ret;
// }

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,
	.cache_type = REGCACHE_RBTREE,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	.use_single_rw = true,
#else
	.use_single_read = true,
	.use_single_write = true,
#endif
};

// static const u32 ctrl_cid_list[] = {
// 	TEGRA_CAMERA_CID_SENSOR_MODE_ID,
// };

// static int sensor_set_group_hold(struct tegracam_device *tc_dev, bool val)
// {
// 	/* TEVI-AP1302 does not support group hold */
// 	return 0;
// }

// static struct tegracam_ctrl_ops sensor_nv_ctrl_ops = {
// 	.numctrls = ARRAY_SIZE(ctrl_cid_list),
// 	.ctrl_cid_list = ctrl_cid_list,
// 	.set_group_hold = sensor_set_group_hold,
// };

struct sensor_obj* _to_sensor_priv(struct v4l2_ctrl *ctrl)
{
	struct tegracam_ctrl_handler *ctrl_hdl = 
			container_of(ctrl->handler, struct tegracam_ctrl_handler, ctrl_handler);

	return (struct sensor_obj*)ctrl_hdl->tc_dev->priv;
}

static int set_standby_mode_rel419(struct i2c_client *client, int enable)
{
	u16 v = 0;
	int timeout;
	dev_dbg(&client->dev, "%s():enable=%d\n", __func__, enable);

	if (enable == 1) {
		__i2c_write_16b(client, 0x601a, 0x8140);
		for (timeout = 0 ; timeout < 50 ; timeout ++) {
			usleep_range(9000, 10000);
			ar0144_i2c_read_16b(client, 0x601a, &v);
			if ((v & 0x200) == 0x200)
				break;
		}
		if (timeout < 50) {
			__i2c_write_16b(client, 0xFFFE, 1);
			msleep(100);
		} else {
			dev_err(&client->dev, "timeout: line[%d]\n", __LINE__);
			return -EINVAL;
		}
	}  else {
		__i2c_write_16b(client, 0xFFFE, 0);
		for (timeout = 0 ; timeout < 500 ; timeout ++) {
			usleep_range(9000, 10000);
			ar0144_i2c_read_16b(client, 0, &v);
			if (v != 0)
				break;
		}
		if (timeout >= 500) {
			dev_err(&client->dev, "timeout: line[%d]v=%x\n", __LINE__, v);
			return -EINVAL;
		}

		for (timeout = 0 ; timeout < 100 ; timeout ++) {
			usleep_range(9000, 10000);
			ar0144_i2c_read_16b(client, 0x601a, &v);
			if ((v & 0x200) == 0x200)
				break;
		}
		if ( (v & 0x200) != 0x200 ) {
			dev_dbg(&client->dev, "stop waking up: camera is working.\n");
			return 0;
		}

		__i2c_write_16b(client, 0x601a, 0x241);
		usleep_range(1000, 2000);
		for (timeout = 0 ; timeout < 100 ; timeout ++) {
			usleep_range(9000, 10000);
			ar0144_i2c_read_16b(client, 0x601a, &v);
			if ((v & 1) == 0)
				break;
		}
		if (timeout >= 100) {
			dev_err(&client->dev, "timeout: line[%d]v=%x\n", __LINE__, v);
			return -EINVAL;
		}

		for (timeout = 0 ; timeout < 100 ; timeout ++) {
			usleep_range(9000, 10000);
			ar0144_i2c_read_16b(client, 0x601a, &v);
			if ((v & 0x8000) == 0x8000)
				break;
		}
		if (timeout >= 100) {
			dev_err(&client->dev, "timeout: line[%d]v=%x\n", __LINE__, v);
			return -EINVAL;
		}

		__i2c_write_16b(client, 0x601a, 0x8250);
		for (timeout = 0 ; timeout < 100 ; timeout ++) {
			usleep_range(9000, 10000);
			ar0144_i2c_read_16b(client, 0x601a, &v);
			if ((v & 0x8040) == 0x8040)
				break;
		}
		if (timeout >= 100) {
			dev_err(&client->dev, "timeout: line[%d]v=%x\n", __LINE__, v);
			return -EINVAL;
		}

		dev_dbg(&client->dev, "sensor wake up\n");
	}

	return 0;
}

static int sensor_standby(struct i2c_client *client, int enable)
{
	u16 v = 0;
	int timeout;
	u16 checksum = 0;
	dev_dbg(&client->dev, "%s():enable=%d\n", __func__, enable);

	for (timeout = 0 ; timeout < 50 ; timeout ++) {
		usleep_range(9000, 10000);
		ar0144_i2c_read_16b(client, 0x6134, &checksum);
		if (checksum == 0xFFFF)
			break;
	}

	if(checksum != 0xFFFF){
		return set_standby_mode_rel419(client, enable); // standby for rel419
	}

	if (enable == 1) {
		__i2c_write_16b(client, 0x601a, 0x0180);
		for (timeout = 0 ; timeout < 50 ; timeout ++) {
			usleep_range(9000, 10000);
			ar0144_i2c_read_16b(client, 0x601a, &v);
			if ((v & 0x200) == 0x200)
				break;
		}
	}  else {
		__i2c_write_16b(client, 0x601a, 0x0380);
		usleep_range(1000, 2000);
		for (timeout = 0 ; timeout < 100 ; timeout ++) {
			usleep_range(9000, 10000);
			ar0144_i2c_read_16b(client, 0x601a, &v);
			if ((v & 0x200) == 0)
				break;
		}
		if (timeout >= 100) {
			dev_err(&client->dev, "timeout: line[%d]v=%x\n", __LINE__, v);
			return -EINVAL;
		}

		dev_dbg(&client->dev, "sensor wake up\n");
	}

	return 0;
}

static int sensor_power_on(struct camera_common_data *s_data)
{
	dev_info(s_data->dev, "%s() power on\n", __func__);
	s_data->power->state = SWITCH_ON;
	return 0;
/*
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	if (pdata && pdata->power_on) {
		err = pdata->power_on(pw);
		if (err)
			dev_err(dev, "%s() failed.\n", __func__);
		else
			pw->state = SWITCH_ON;
		return err;
	}

	if (pw->reset_gpio && pw->pwdn_gpio) {
		if (gpio_cansleep(pw->reset_gpio) && gpio_cansleep(pw->pwdn_gpio)) {
			gpio_set_value_cansleep(pw->pwdn_gpio, 0);
			gpio_set_value_cansleep(pw->reset_gpio, 0);
		}
		else {
			gpio_set_value(pw->pwdn_gpio, 0);
			gpio_set_value(pw->reset_gpio, 0);
		}
	}

	if (unlikely(!(pw->avdd || pw->iovdd || pw->dvdd)))
		goto skip_power_seqn;

	usleep_range(10, 20);

	if (pw->avdd) {
		err = regulator_enable(pw->avdd);
		if (err)
			goto sensor_avdd_fail;
	}

	if (pw->iovdd) {
		err = regulator_enable(pw->iovdd);
		if (err)
			goto sensor_iovdd_fail;
	}

	if (pw->dvdd) {
		err = regulator_enable(pw->dvdd);
		if (err)
			goto sensor_dvdd_fail;
	}

	usleep_range(10, 20);

skip_power_seqn:
	if (pw->pwdn_gpio) {
		if (gpio_cansleep(pw->pwdn_gpio))
			gpio_set_value_cansleep(pw->pwdn_gpio, 1);
		else
			gpio_set_value(pw->pwdn_gpio, 1);
	}

	msleep(20);

	if (pw->reset_gpio) {
		if (gpio_cansleep(pw->reset_gpio))
			gpio_set_value_cansleep(pw->reset_gpio, 1);
		else
			gpio_set_value(pw->reset_gpio, 1);
	}

	msleep(250);

	pw->state = SWITCH_ON;

	return 0;

sensor_dvdd_fail:
	regulator_disable(pw->iovdd);

sensor_iovdd_fail:
	regulator_disable(pw->avdd);

sensor_avdd_fail:
	dev_err(dev, "%s() failed.\n", __func__);

	return -ENODEV;
*/
}

static int sensor_power_off(struct camera_common_data *s_data)
{
	dev_info(s_data->dev, "%s() power off\n", __func__);
	s_data->power->state = SWITCH_OFF;
	return 0;
/*
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	if (pdata && pdata->power_off) {
		err = pdata->power_off(pw);
		if (err) {
			dev_err(dev, "%s() failed.\n", __func__);
			return err;
		}
	} else {
		if (pw->reset_gpio && pw->pwdn_gpio) {
			if (gpio_cansleep(pw->reset_gpio) && gpio_cansleep(pw->pwdn_gpio)) {
				gpio_set_value_cansleep(pw->pwdn_gpio, 0);
				gpio_set_value_cansleep(pw->reset_gpio, 0);
			}
			else {
				gpio_set_value(pw->pwdn_gpio, 0);
				gpio_set_value(pw->reset_gpio, 0);
			}
		}

		usleep_range(10, 10);

		if (pw->dvdd)
			regulator_disable(pw->dvdd);
		if (pw->iovdd)
			regulator_disable(pw->iovdd);
		if (pw->avdd)
			regulator_disable(pw->avdd);
	}

	pw->state = SWITCH_OFF;

	return 0;
*/
}

static int sensor_power_put(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;
	dev_info(tc_dev->dev, "%s()\n", __func__);

	if (unlikely(!pw))
		return -EFAULT;

	if (likely(pw->dvdd))
		devm_regulator_put(pw->dvdd);

	if (likely(pw->avdd))
		devm_regulator_put(pw->avdd);

	if (likely(pw->iovdd))
		devm_regulator_put(pw->iovdd);

	pw->dvdd = NULL;
	pw->avdd = NULL;
	pw->iovdd = NULL;

	if (likely(pw->pwdn_gpio))
		gpio_free(pw->pwdn_gpio);

	if (likely(pw->reset_gpio))
		gpio_free(pw->reset_gpio);

	return 0;
}

static int sensor_power_get(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct clk *parent;
	int err = 0;
	dev_dbg(tc_dev->dev, "%s()\n", __func__);

	if (!pdata) {
		dev_err(dev, "pdata missing\n");
		return -EFAULT;
	}

	/* Sensor MCLK (aka. INCK) */
	if (pdata->mclk_name) {
		pw->mclk = devm_clk_get(dev, pdata->mclk_name);
		if (IS_ERR(pw->mclk)) {
			dev_err(dev, "unable to get clock %s\n",
				pdata->mclk_name);
			return PTR_ERR(pw->mclk);
		}

		if (pdata->parentclk_name) {
			parent = devm_clk_get(dev, pdata->parentclk_name);
			if (IS_ERR(parent)) {
				dev_err(dev, "unable to get parent clock %s",
					pdata->parentclk_name);
			} else
				clk_set_parent(pw->mclk, parent);
		}
	}

	/* analog 2.8v */
	if (pdata->regulators.avdd)
		err |= camera_common_regulator_get(dev,
				&pw->avdd, pdata->regulators.avdd);
	/* IO 1.8v */
	if (pdata->regulators.iovdd)
		err |= camera_common_regulator_get(dev,
				&pw->iovdd, pdata->regulators.iovdd);
	/* dig 1.2v */
	if (pdata->regulators.dvdd)
		err |= camera_common_regulator_get(dev,
				&pw->dvdd, pdata->regulators.dvdd);
	if (err) {
		dev_err(dev, "%s() unable to get regulator(s)\n", __func__);
		goto done;
	}

	/* Reset or ENABLE GPIO */
	pw->pwdn_gpio = pdata->pwdn_gpio;
	err = gpio_request(pw->pwdn_gpio, "cam_power_gpio");
	if (err < 0) {
		dev_err(dev, "%s() unable to request power_gpio (%d)\n",
			__func__, err);
		err = 0;  // Pass Error
		goto done;
	}

	pw->reset_gpio = pdata->reset_gpio;
	err = gpio_request(pw->reset_gpio, "cam_reset_gpio");
	if (err < 0) {
		dev_err(dev, "%s() unable to request reset_gpio (%d)\n",
			__func__, err);
		err = 0;  // Pass Error
		goto done;
	}

done:
	pw->state = SWITCH_OFF;
	return err;
}

static struct camera_common_pdata *sensor_parse_dt(
	struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct device_node *np = dev->of_node;
	struct camera_common_pdata *board_priv_pdata;
	struct camera_common_pdata *ret = NULL;
	struct sensor_obj * priv = (struct sensor_obj*)tc_dev->priv;
	int err = 0;
	int gpio;

	struct device_node *dser_node;
	struct i2c_client *dser_i2c;
	u32 port_idx = 4;
	u32 ser_alias_id = 0;
	u32 gpio_addr = 0x25;
	u8 dser_i2c_val = 0x7F;
	u32 dser_time = 0;
	u32 alias_local_addr;

	if (!np)
		return NULL;

	board_priv_pdata = devm_kzalloc(dev,
		sizeof(*board_priv_pdata), GFP_KERNEL);
	if (!board_priv_pdata)
		return NULL;

	/*=====================================================*/
	dev_info(dev, "read fpdlink-dser-device from node...\n");
	dser_node = of_parse_phandle(np, "nvidia,fpdlink-dser-device", 0);
	if (dser_node == NULL) {
		dev_err(dev,
			"missing %s handle\n",
				"nvidia,fpdlink-dser-device");
		goto error;
	}

	dev_info(dev, "find i2c device by node...\n");
	dser_i2c = of_find_i2c_device_by_node(dser_node);
	of_node_put(dser_node);

	if (dser_i2c == NULL) {
		dev_err(dev, "missing deserializer dev handle\n");
		ret = ERR_PTR(-EPROBE_DEFER);
		goto error;
	}
	if (dser_i2c->dev.driver == NULL) {
		dev_err(dev, "missing deserializer driver\n");
		goto error;
	}
	dev_info(dev, "dser_i2c at addr 0x%0x\n", dser_i2c->addr);
	priv->dser_i2c = dser_i2c;

	err = of_property_read_u32(dser_node, "ser_alias_id", &ser_alias_id);
	if (err) {
		dev_err(dev, "invalid ser_alias_id on %pOF\n", dser_node);
		goto error;
	}
	dev_info(dev, "Read ser_alias_id %d\n", ser_alias_id);
	priv->ser_alias_id = ser_alias_id;

	err = of_property_read_u32_index(dser_node,
						 "i2c_addr_alias_map_local",
						 1,
						 &alias_local_addr);
	if (err) {
		dev_err(dev, "invalid ser_alias_id on %pOF\n", dser_node);
		goto error;
	}
	dev_info(dev, "Read alias_local_addr %x\n", alias_local_addr);
	priv->flash_id = alias_local_addr;

	err = of_property_read_u32(dser_node, "port", &port_idx);
	if (err) {
		dev_err(dev, "invalid port on %pOF\n", dser_node);
		goto error;
	}
	dev_info(dev, "Read port %d\n", port_idx);

	if (port_idx > 3){
		dev_err(dev, "invalid port %d\n", port_idx);
		goto error;
	}

	while(dser_time < 10) {
		dev_info(dev, "Select port %d...\n", port_idx);
		priv->vc_id = port_idx;
		__i2c_write_8b(dser_i2c, 0x4c,
						(port_idx << 4) | 
						(1 << port_idx));
		msleep(500);
		ar0144_i2c_read_8b(dser_i2c, 0x4d, &dser_i2c_val, 1);
		dev_info(dev, "Read 0x4D=0x%x...\n", dser_i2c_val);
		msleep(100);
		if((dser_i2c_val & 0x7) == 3){
			ar0144_i2c_read_8b(dser_i2c, 0x5b, &dser_i2c_val, 1);
			__i2c_write_8b(dser_i2c, 0x5c,
				    ser_alias_id << 1);
			msleep(100);
			break;
		}
		dser_time++;
	}

	err = of_property_read_u32_index(dser_node, 
						"i2c_addr_alias_map_local", 0,
						 &gpio_addr);
	if (err) {
		dev_err(dev, "cannot found gpio_addr on %pOF from idx 0\n", dser_node);
		goto error;
	}
	priv->gpio_addr = gpio_addr;
	/*=====================================================*/
	dev_info(dev, "%s():np_name=%s\n", __func__, np->name);
	gpio = of_get_named_gpio(np, "power-gpios", 0);
	if (gpio < 0) {
		if (gpio == -EPROBE_DEFER)
			ret = ERR_PTR(-EPROBE_DEFER);
		dev_err(dev, "power-gpios not found\n");
		// goto error;
	}
	board_priv_pdata->pwdn_gpio = (unsigned int)gpio;

	gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (gpio < 0) {
		if (gpio == -EPROBE_DEFER)
			ret = ERR_PTR(-EPROBE_DEFER);
		dev_err(dev, "reset-gpios not found\n");
		// goto error;
	}
	board_priv_pdata->reset_gpio = (unsigned int)gpio;
	gpio_direction_output(board_priv_pdata->reset_gpio, 1);

	err = of_property_read_string(np, "mclk", &board_priv_pdata->mclk_name);
	if (err)
		dev_dbg(dev, "mclk name not present, "
			"assume sensor driven externally\n");

	err = of_property_read_string(np, "avdd-reg",
		&board_priv_pdata->regulators.avdd);
	err |= of_property_read_string(np, "iovdd-reg",
		&board_priv_pdata->regulators.iovdd);
	err |= of_property_read_string(np, "dvdd-reg",
		&board_priv_pdata->regulators.dvdd);
	if (err)
		dev_dbg(dev, "avdd, iovdd and/or dvdd reglrs. not present, "
			"assume sensor powered independently\n");

	board_priv_pdata->has_eeprom =
		of_property_read_bool(np, "has-eeprom");

	return board_priv_pdata;

error:
	devm_kfree(dev, board_priv_pdata);

	return ret;
}

static int sensor_set_mode(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;

	dev_dbg(tc_dev->dev,
		"%s() , {%d}\n",
		__func__,
		s_data->mode);

	return 0;
}

static int sensor_start_streaming(struct tegracam_device *tc_dev)
{
	int ret = 0;
	dev_info(tc_dev->dev, "%s()\n", __func__);

	ret = sensor_standby(tc_dev->client, 0);

	if (ret == 0) {
		int fps = 15;
		// VIDEO_MAX_FPS
		__i2c_write_16b(tc_dev->client, 0x2020, fps << 8);
	}

	return ret;
}

static int sensor_stop_streaming(struct tegracam_device *tc_dev)
{
	dev_dbg(tc_dev->dev, "%s()\n", __func__);

	return sensor_standby(tc_dev->client, 1);
}

static struct camera_common_sensor_ops sensor_common_ops = {
	.numfrmfmts = ARRAY_SIZE(sensor_frmfmt),
	.frmfmt_table = sensor_frmfmt,
	.power_on = sensor_power_on,
	.power_off = sensor_power_off,
//	.write_reg = sensor_write_reg,
//	.read_reg = sensor_read_reg,
	.parse_dt = sensor_parse_dt,
	.power_get = sensor_power_get,
	.power_put = sensor_power_put,
	.set_mode = sensor_set_mode,
	.start_streaming = sensor_start_streaming,
	.stop_streaming = sensor_stop_streaming,
};

static int sensor_load_bootdata(struct sensor_obj *priv)
{
	struct device *dev = priv->tc_dev->dev;
	int index = 0;

	u16 otp_data;
	u16 *bootdata_temp_area;
	u16 checksum;
	int i;
	const int len_each_time = 1024;
	size_t len = len_each_time;

	bootdata_temp_area = devm_kzalloc(dev,
					  len_each_time * 4 + 2,
					  GFP_KERNEL);
	if (bootdata_temp_area == NULL) {
		dev_err(dev, "allocate memory failed\n");
		return -EINVAL;
	}

	checksum = tevi_ar0144_otp_flash_get_checksum(priv->otp_flash_instance);

	while(len != 0) {
		while(i < BOOT_DATA_WRITE_LEN) {
			bootdata_temp_area[0] =
				cpu_to_be16(BOOT_DATA_START_REG + i);
			len = tevi_ar0144_otp_flash_read(priv->otp_flash_instance,
					     (u8 *)(&bootdata_temp_area[1]),
					     index, len_each_time);
			if (len == 0) {
				dev_dbg(dev, "length get zero\n");
				break;
			}

			dev_dbg(dev,
				"load ronaming data of length [%zu] into register [%x]\n",
				len,
				BOOT_DATA_START_REG + i);
			__i2c_write_bust(priv->tc_dev->client,
					 (u8 *)bootdata_temp_area,
					 len + 2);
			index += len;
			i += len_each_time;
		}

		i = 0;
	}

	__i2c_write_16b(priv->tc_dev->client, 0x6002, 0xffff);
	devm_kfree(dev, bootdata_temp_area);

	msleep(500);

	index = 0;
	otp_data = 0;
	while(otp_data != checksum && index < 20) {
		msleep(10);
		ar0144_i2c_read_16b(priv->tc_dev->client, 0x6134, &otp_data);
		index ++;
	}
	if (unlikely(index == 20)) {
		if (likely(otp_data == 0))
			dev_err(dev, "failed try to read checksum\n");
		else
			dev_err(dev, "bootdata checksum missmatch\n");

		return -EINVAL;
	}

	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 Controls
 */

static int sensor_set_brightness(struct sensor_obj *priv, s32 value)
{
	// Format is u3.12
	return __i2c_write_16b(priv->tc_dev->client, AP1302_BRIGHTNESS, value & AP1302_BRIGHTNESS_MASK);
}

static int sensor_get_brightness(struct sensor_obj *priv, s32 *value)
{
	u16 val;
	int ret;
	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_BRIGHTNESS, &val);
	if (ret)
		return ret;

	*value = val & AP1302_BRIGHTNESS_MASK;
	return 0;
}

static int sensor_set_contrast(struct sensor_obj *priv, s32 value)
{
	// Format is u3.12
	return __i2c_write_16b(priv->tc_dev->client, AP1302_CONTRAST, value & AP1302_CONTRAST_MASK);
}

static int sensor_get_contrast(struct sensor_obj *priv, s32 *value)
{
	u16 val;
	int ret;
	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_CONTRAST, &val);
	if (ret)
		return ret;

	*value = val & AP1302_CONTRAST_MASK;
	return 0;
}

static int sensor_set_saturation(struct sensor_obj *priv, s32 value)
{
	// Format is u3.12
	return __i2c_write_16b(priv->tc_dev->client, AP1302_SATURATION, value & AP1302_SATURATION_MASK);
}

static int sensor_get_saturation(struct sensor_obj *priv, s32 *value)
{
	u16 val;
	int ret;
	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_SATURATION, &val);
	if (ret)
		return ret;

	*value = val & AP1302_SATURATION_MASK;
	return 0;
}

static const char * const awb_mode_strings[] = {
	"Manual Temp Mode", // AP1302_AWB_CTRL_MODE_MANUAL_TEMP
	"Auto Mode", // AP1302_AWB_CTRL_MODE_AUTO
	NULL,
};

static int sensor_set_awb_mode(struct sensor_obj *priv, s32 mode)
{
	u16 val = mode & AP1302_AWB_CTRL_MODE_MASK;

	switch(val)
	{
	case 0:
		val = AP1302_AWB_CTRL_MODE_MANUAL_TEMP;
		break;
	case 1:
		val = AP1302_AWB_CTRL_MODE_AUTO;
		break;
	default:
		val = AP1302_AWB_CTRL_MODE_AUTO;
		break;
	}

	return __i2c_write_16b(priv->tc_dev->client, AP1302_AWB_CTRL_MODE, val);
}

static int sensor_get_awb_mode(struct sensor_obj *priv, s32 *mode)
{
	u16 val;
	int ret;

	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_AWB_CTRL_MODE, &val);
	if (ret)
		return ret;

	switch (val & AP1302_AWB_CTRL_MODE_MASK)
	{
	case AP1302_AWB_CTRL_MODE_MANUAL_TEMP:
		*mode = 0;
		break;
	case AP1302_AWB_CTRL_MODE_AUTO:
		*mode = 1;
		break;
	default:
		*mode = 1;
		break;
	}

	return 0;
}

static int sensor_set_gamma(struct sensor_obj *priv, s32 value)
{
	// Format is u3.12
	return __i2c_write_16b(priv->tc_dev->client, AP1302_GAMMA, value & AP1302_GAMMA_MASK);
}

static int sensor_get_gamma(struct sensor_obj *priv, s32 *value)
{
	u16 val;
	int ret;
	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_GAMMA, &val);
	if (ret)
		return ret;

	*value = val & AP1302_GAMMA_MASK;
	return 0;
}

static int sensor_set_exposure(struct sensor_obj *priv, s32 value)
{
	int ret;

	ret = __i2c_write_16b(priv->tc_dev->client, AP1302_AE_MANUAL_EXP_TIME, (value >> 16) & 0xFFFF);
	if (ret)
		return ret;
	usleep_range(9000, 10000);
	ret = __i2c_write_16b(priv->tc_dev->client, AP1302_AE_MANUAL_EXP_TIME + 2, value & 0xFFFF);
	if (ret)
		return ret;

	return 0;
}

static int sensor_get_exposure(struct sensor_obj *priv, s32 *value)
{
	u16 val_msb, val_lsb;
	int ret;

	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_AE_MANUAL_EXP_TIME, &val_msb);
	if (ret)
		return ret;
	usleep_range(9000, 10000);
	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_AE_MANUAL_EXP_TIME + 2, &val_lsb);
	if (ret)
		return ret;

	*value = ((u32)(val_msb) << 16) + val_lsb;
	return 0;
}

static int sensor_set_gain(struct sensor_obj *priv, s32 value)
{
	// Format is u8
	return __i2c_write_16b(priv->tc_dev->client, AP1302_AE_MANUAL_GAIN, value & AP1302_AE_MANUAL_GAIN_MASK);
}

static int sensor_get_gain(struct sensor_obj *priv, s32 *value)
{
	u16 val;
	int ret;
	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_AE_MANUAL_GAIN, &val);
	if (ret)
		return ret;

	*value = val & AP1302_AE_MANUAL_GAIN_MASK;
	return 0;
}

static int sensor_set_hflip(struct sensor_obj *priv, s32 flip)
{
	u16 val;
	int ret;

	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_ORIENTATION, &val);
	if (ret)
		return ret;

	val &= ~AP1302_ORIENTATION_HFLIP;
	val |= flip ? AP1302_ORIENTATION_HFLIP : 0;

	return __i2c_write_16b(priv->tc_dev->client, AP1302_ORIENTATION, val);
}

static int sensor_get_hflip(struct sensor_obj *priv, s32 *flip)
{
	u16 val;
	int ret;

	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_ORIENTATION, &val);
	if (ret)
		return ret;

	*flip = !!(val & AP1302_ORIENTATION_HFLIP);
	return 0;
}

static int sensor_set_vflip(struct sensor_obj *priv, s32 flip)
{
	u16 val;
	int ret;

	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_ORIENTATION, &val);
	if (ret)
		return ret;

	val &= ~AP1302_ORIENTATION_VFLIP;
	val |= flip ? AP1302_ORIENTATION_VFLIP : 0;

	return __i2c_write_16b(priv->tc_dev->client, AP1302_ORIENTATION, val);
}

static int sensor_get_vflip(struct sensor_obj *priv, s32 *flip)
{
	u16 val;
	int ret;

	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_ORIENTATION, &val);
	if (ret)
		return ret;

	*flip = !!(val & AP1302_ORIENTATION_VFLIP);
	return 0;
}

static int sensor_set_awb_temp(struct sensor_obj *priv, s32 value)
{
	return __i2c_write_16b(priv->tc_dev->client, AP1302_AWB_MANUAL_TEMP, value & AP1302_AWB_MANUAL_TEMP_MASK);
}

static int sensor_get_awb_temp(struct sensor_obj *priv, s32 *value)
{
	u16 val;
	int ret;
	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_AWB_MANUAL_TEMP, &val);
	if (ret)
		return ret;

	*value = val & AP1302_AWB_MANUAL_TEMP_MASK;
	return 0;
}

static int sensor_set_sharpen(struct sensor_obj *priv, s32 value)
{
	// Format is u3.12
	return __i2c_write_16b(priv->tc_dev->client, AP1302_SHARPEN, value & AP1302_SHARPEN_MASK);
}

static int sensor_get_sharpen(struct sensor_obj *priv, s32 *value)
{
	u16 val;
	int ret;
	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_SHARPEN, &val);
	if (ret)
		return ret;

	*value = val & AP1302_SHARPEN_MASK;
	return 0;
}

static int sensor_set_backlight_compensation(struct sensor_obj *priv, s32 value)
{
	// Format is u3.12
	return __i2c_write_16b(priv->tc_dev->client, AP1302_BACKLIGHT_COMPENSATION, value & AP1302_BACKLIGHT_COMPENSATION_MASK);
}

static int sensor_get_backlight_compensation(struct sensor_obj *priv, s32 *value)
{
	u16 val;
	int ret;
	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_BACKLIGHT_COMPENSATION, &val);
	if (ret)
		return ret;

	*value = val & AP1302_BACKLIGHT_COMPENSATION_MASK;
	return 0;
}

static int sensor_set_zoom_target(struct sensor_obj *priv, s32 value)
{
	// Format u7.8
	return __i2c_write_16b(priv->tc_dev->client, AP1302_DZ_TGT_FCT, value & AP1302_DZ_TGT_FCT_MASK);
}

static int sensor_get_zoom_target(struct sensor_obj *priv, s32 *value)
{
	u16 val;
	int ret;
	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_DZ_TGT_FCT, &val);
	if (ret)
		return ret;

	*value = val & AP1302_DZ_TGT_FCT_MASK;
	return 0;
}

static const char * const sfx_mode_strings[] = {
	"Normal Mode", // AP1302_SFX_MODE_SFX_NORMAL
	"Black and White Mode", // AP1302_SFX_MODE_SFX_BW
	"Grayscale Mode", // AP1302_SFX_MODE_SFX_GRAYSCALE
	"Negative Mode", // AP1302_SFX_MODE_SFX_NEGATIVE
	"Sketch Mode", // AP1302_SFX_MODE_SFX_SKETCH
	NULL,
};

static int sensor_set_special_effect(struct sensor_obj *priv, s32 mode)
{
	u16 val = mode & AP1302_SFX_MODE_SFX_MASK;

	switch(val)
	{
	case 0:
		val = AP1302_SFX_MODE_SFX_NORMAL;
		break;
	case 1:
		val = AP1302_SFX_MODE_SFX_BW;
		break;
	case 2:
		val = AP1302_SFX_MODE_SFX_GRAYSCALE;
		break;
	case 3:
		val = AP1302_SFX_MODE_SFX_NEGATIVE;
		break;
	case 4:
		val = AP1302_SFX_MODE_SFX_SKETCH;
		break;
	default:
		val = AP1302_SFX_MODE_SFX_NORMAL;
		break;
	}

	return __i2c_write_16b(priv->tc_dev->client, AP1302_SFX_MODE, val);
}

static int sensor_get_special_effect(struct sensor_obj *priv, s32 *mode)
{
	u16 val;
	int ret;

	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_SFX_MODE, &val);
	if (ret)
		return ret;

	switch (val & AP1302_SFX_MODE_SFX_MASK)
	{
	case AP1302_SFX_MODE_SFX_NORMAL:
		*mode = 0;
		break;
	case AP1302_SFX_MODE_SFX_BW:
		*mode = 1;
		break;
	case AP1302_SFX_MODE_SFX_GRAYSCALE:
		*mode = 2;
		break;
	case AP1302_SFX_MODE_SFX_NEGATIVE:
		*mode = 3;
		break;
	case AP1302_SFX_MODE_SFX_SKETCH:
		*mode = 4;
		break;
	default:
		*mode = 0;
		break;
	}

	return 0;
}

static const char * const ae_mode_strings[] = {
	"Manual Mode", // AP1302_AE_CTRL_MANUAL_EXP_TIME_GAIN
	"Auto Mode", // AP1302_AE_CTRL_FULL_AUTO
	NULL,
};

static int sensor_set_ae_mode(struct sensor_obj *priv, s32 mode)
{
	u16 val = mode & AP1302_AE_CTRL_MODE_MASK;

	switch(val)
	{
	case 0:
		val |= AP1302_AE_CTRL_MANUAL_EXP_TIME_GAIN;
		break;
	case 1:
		val |= AP1302_AE_CTRL_FULL_AUTO;
		break;
	default:
		val |= AP1302_AE_CTRL_FULL_AUTO;
		break;
	}

	return __i2c_write_16b(priv->tc_dev->client, AP1302_AE_CTRL_MODE, val);
}

static int sensor_get_ae_mode(struct sensor_obj *priv, s32 *mode)
{
	u16 val;
	int ret;

	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_AE_CTRL_MODE, &val);
	if (ret)
		return ret;

	switch (val & AP1302_AE_CTRL_MODE_MASK)
	{
	case AP1302_AE_CTRL_MANUAL_EXP_TIME_GAIN:
		*mode = 0;
		break;
	case AP1302_AE_CTRL_FULL_AUTO:
		*mode = 1;
		break;
	default:
		*mode = 1;
		break;
	}
	return 0;
}

static const char * const flick_mode_strings[] = {
	"Disabled", 
	"50 Hz",
	"60 Hz",
	"Auto",
	NULL,
};

static int sensor_set_flick_mode(struct sensor_obj *priv, s32 mode)
{
	u16 val = 0;
	switch(mode)
	{
	case 0:
		val = AP1302_FLICK_CTRL_MODE_DISABLED;
		break;
	case 1:
		val = AP1302_FLICK_CTRL_MODE_50HZ;
		break;
	case 2:
		val = AP1302_FLICK_CTRL_MODE_60HZ;
		break;
	case 3:
		val = AP1302_FLICK_CTRL_MODE_AUTO | 
				AP1302_FLICK_CTRL_FRC_OVERRIDE_UPPER_ET | 
				AP1302_FLICK_CTRL_FRC_EN;
		break;
	default:
		val = AP1302_FLICK_CTRL_MODE_DISABLED;
		break;
	}

	return __i2c_write_16b(priv->tc_dev->client, AP1302_FLICK_CTRL, val);
}

static int sensor_get_flick_mode(struct sensor_obj *priv, s32 *mode)
{
	u16 val;
	int ret;

	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_FLICK_CTRL, &val);
	if (ret)
		return ret;

	switch (val & AP1302_FLICK_CTRL_MODE_MASK)
	{
	case AP1302_FLICK_CTRL_MODE_DISABLED:
		*mode = 0;
		break;
	case AP1302_FLICK_CTRL_MODE_MANUAL:
		if((val & AP1302_FLICK_CTRL_FREQ_MASK) == AP1302_FLICK_CTRL_FREQ(50))
			*mode = 1;
		else if((val & AP1302_FLICK_CTRL_FREQ_MASK)  == AP1302_FLICK_CTRL_FREQ(50))
			*mode = 2;
		break;
	case AP1302_FLICK_CTRL_MODE_AUTO:
		*mode = 3;
		break;
	default:
		*mode = 0;
		break;
	}
	return 0;
}

static int sensor_set_pan_target(struct sensor_obj *priv, s32 value)
{
	// Format u7.8
	return __i2c_write_16b(priv->tc_dev->client, AP1302_DZ_CT_X, value & AP1302_DZ_CT_X_MASK);
}

static int sensor_get_pan_target(struct sensor_obj *priv, s32 *value)
{
	u16 val;
	int ret;
	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_DZ_CT_X, &val);
	if (ret)
		return ret;

	*value = val & AP1302_DZ_CT_X_MASK;
	return 0;
}

static int sensor_set_tilt_target(struct sensor_obj *priv, s32 value)
{
	// Format u7.8
	return __i2c_write_16b(priv->tc_dev->client, AP1302_DZ_CT_Y, value & AP1302_DZ_CT_Y_MASK);
}

static int sensor_get_tilt_target(struct sensor_obj *priv, s32 *value)
{
	u16 val;
	int ret;
	ret = ar0144_i2c_read_16b(priv->tc_dev->client, AP1302_DZ_CT_Y, &val);
	if (ret)
		return ret;

	*value = val & AP1302_DZ_CT_Y_MASK;
	return 0;
}

static int sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_obj *priv = _to_sensor_priv(ctrl);

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return sensor_set_brightness(priv, ctrl->val);

	case V4L2_CID_CONTRAST:
		return sensor_set_contrast(priv, ctrl->val);

	case V4L2_CID_SATURATION:
		return sensor_set_saturation(priv, ctrl->val);

	case V4L2_CID_AUTO_WHITE_BALANCE:
		return sensor_set_awb_mode(priv, ctrl->val);

	case V4L2_CID_GAMMA:
		return sensor_set_gamma(priv, ctrl->val);

	case V4L2_CID_EXPOSURE:
		return sensor_set_exposure(priv, ctrl->val);

	case V4L2_CID_GAIN:
		return sensor_set_gain(priv, ctrl->val);

	case V4L2_CID_HFLIP:
		return sensor_set_hflip(priv, ctrl->val);

	case V4L2_CID_VFLIP:
		return sensor_set_vflip(priv, ctrl->val);

	case V4L2_CID_POWER_LINE_FREQUENCY:
		return sensor_set_flick_mode(priv, ctrl->val);

	case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
		return sensor_set_awb_temp(priv, ctrl->val);

	case V4L2_CID_SHARPNESS:
		return sensor_set_sharpen(priv, ctrl->val);

	case V4L2_CID_BACKLIGHT_COMPENSATION:
		return sensor_set_backlight_compensation(priv, ctrl->val);

	case V4L2_CID_COLORFX:
		return sensor_set_special_effect(priv, ctrl->val);

	case V4L2_CID_EXPOSURE_AUTO:
		return sensor_set_ae_mode(priv, ctrl->val);

	case V4L2_CID_PAN_ABSOLUTE:
		return sensor_set_pan_target(priv, ctrl->val);

	case V4L2_CID_TILT_ABSOLUTE:
		return sensor_set_tilt_target(priv, ctrl->val);

	case V4L2_CID_ZOOM_ABSOLUTE:
		return sensor_set_zoom_target(priv, ctrl->val);

	case V4L2_CID_SENSOR_FLASH_ID:
		return 0;

	default:
		dev_dbg(priv->tc_dev->dev, "Unknown control 0x%x\n",
			ctrl->id);
		return -EINVAL;
	}
}

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_obj *priv = _to_sensor_priv(ctrl);

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return sensor_get_brightness(priv, &ctrl->val);

	case V4L2_CID_CONTRAST:
		return sensor_get_contrast(priv, &ctrl->val);

	case V4L2_CID_SATURATION:
		return sensor_get_saturation(priv, &ctrl->val);

	case V4L2_CID_AUTO_WHITE_BALANCE:
		return sensor_get_awb_mode(priv, &ctrl->val);

	case V4L2_CID_GAMMA:
		return sensor_get_gamma(priv, &ctrl->val);

	case V4L2_CID_EXPOSURE:
		return sensor_get_exposure(priv, &ctrl->val);

	case V4L2_CID_GAIN:
		return sensor_get_gain(priv, &ctrl->val);

	case V4L2_CID_HFLIP:
		return sensor_get_hflip(priv, &ctrl->val);

	case V4L2_CID_VFLIP:
		return sensor_get_vflip(priv, &ctrl->val);

	case V4L2_CID_POWER_LINE_FREQUENCY:
		return sensor_get_flick_mode(priv, &ctrl->val);

	case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
		return sensor_get_awb_temp(priv, &ctrl->val);

	case V4L2_CID_SHARPNESS:
		return sensor_get_sharpen(priv, &ctrl->val);

	case V4L2_CID_BACKLIGHT_COMPENSATION:
		return sensor_get_backlight_compensation(priv, &ctrl->val);

	case V4L2_CID_COLORFX:
		return sensor_get_special_effect(priv, &ctrl->val);

	case V4L2_CID_EXPOSURE_AUTO:
		return sensor_get_ae_mode(priv, &ctrl->val);

	case V4L2_CID_PAN_ABSOLUTE:
		return sensor_get_pan_target(priv, &ctrl->val);

	case V4L2_CID_TILT_ABSOLUTE:
		return sensor_get_tilt_target(priv, &ctrl->val);

	case V4L2_CID_ZOOM_ABSOLUTE:
		return sensor_get_zoom_target(priv, &ctrl->val);

	case V4L2_CID_SENSOR_FLASH_ID:
		ctrl->val = priv->flash_id;
		return 0;

	default:
		dev_dbg(priv->tc_dev->dev, "Unknown control 0x%x\n",
			ctrl->id);
		return -EINVAL;
	}
}

static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
	.s_ctrl = sensor_s_ctrl,
};

static const struct v4l2_ctrl_config sensor_ctrls[] = {
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_BRIGHTNESS,
		.name = "Brightness",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_CONTRAST,
		.name = "Contrast",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_SATURATION,
		.name = "Saturation",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_AUTO_WHITE_BALANCE,
		.name = "White_Balance_Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.max = AP1302_AWB_CTRL_MODE_AUTO_IDX,
		.def = AP1302_AWB_CTRL_MODE_AUTO_IDX,
		.qmenu = awb_mode_strings,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_GAMMA,
		.name = "Gamma",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x2333, // 2.2
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_EXPOSURE,
		.name = "Exposure",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xF4240,
		.step = 1,
		.def = 0x8235, // 33333 us
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_GAIN,
		.name = "Gain",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x100,
		.def = 0x100,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_HFLIP,
		.name = "HFlip",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_VFLIP,
		.name = "VFlip",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_POWER_LINE_FREQUENCY,
		.name = "Power_Line_Frequency",
		.type = V4L2_CTRL_TYPE_MENU,
		.max = AP1302_FLICK_MODE_ENABLED_IDX,
		.def = AP1302_FLICK_MODE_DISABLED_IDX,
		.qmenu = flick_mode_strings,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.name = "White_Balance_Temperature",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x8FC,
		.max = 0x3A98,
		.step = 0x1,
		.def = 0x1388,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_SHARPNESS,
		.name = "Sharpness",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_BACKLIGHT_COMPENSATION,
		.name = "Backlight_Compensation",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_COLORFX,
		.name = "Special_Effect",
		.type = V4L2_CTRL_TYPE_MENU,
		.max = AP1302_SFX_MODE_SFX_SKETCH_IDX,
		.def = AP1302_SFX_MODE_SFX_NORMAL_IDX,
		.qmenu = sfx_mode_strings,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_EXPOSURE_AUTO,
		.name = "Exposure_Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.max = AP1302_AE_CTRL_FULL_AUTO_IDX,
		.def = AP1302_AE_CTRL_FULL_AUTO_IDX,
		.qmenu = ae_mode_strings,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_PAN_ABSOLUTE,
		.name = "Pan_Target",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_TILT_ABSOLUTE,
		.name = "Tilt_Target",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_ZOOM_ABSOLUTE,
		.name = "Zoom_Target",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xFFFF,
		.step = 0x1,
		.def = 0x0,
	},
	{
		.ops = &sensor_ctrl_ops,
		.id = V4L2_CID_SENSOR_FLASH_ID,
		.name = "Sensor_Flash_ID",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0x7F,
		.step = 0x1,
		.def = 0x54,
	},
};

static int sensor_ctrls_init(struct sensor_obj *priv)
{
	struct tegracam_ctrl_handler *ctrl_hdl;
	unsigned int i;
	int ret;

	ctrl_hdl = priv->s_data->tegracam_ctrl_hdl;

	if(ctrl_hdl == NULL){
		dev_info(&priv->tc_dev->client->dev,"init control handler...\n");
		ret = v4l2_ctrl_handler_init(&ctrl_hdl->ctrl_handler, ARRAY_SIZE(sensor_ctrls));
		if (ret) {
			dev_err(&priv->tc_dev->client->dev,"init handler fail\n");
			return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(sensor_ctrls); i++)
	{
		struct v4l2_ctrl * ctrl = v4l2_ctrl_new_custom(&ctrl_hdl->ctrl_handler,
								&sensor_ctrls[i], NULL);
		ret = sensor_g_ctrl(ctrl);
		if (!ret && ctrl->default_value != ctrl->val) {
			// Updating default value based on firmware values
			dev_dbg(&priv->tc_dev->client->dev,"Ctrl '%s' default value updated from %lld to %d\n",
					ctrl->name, ctrl->default_value, ctrl->val);
			ctrl->default_value = ctrl->val;
			ctrl->cur.val = ctrl->val;
		}
	}

	if (ctrl_hdl->ctrl_handler.error) {
		dev_err(&priv->tc_dev->client->dev, "ctrls error\n");
		ret = ctrl_hdl->ctrl_handler.error;
		v4l2_ctrl_handler_free(&ctrl_hdl->ctrl_handler);
		return ret;
	}

	/* Use same lock for controls as for everything else. */
	ctrl_hdl->ctrl_handler.lock = &priv->lock;
	priv->subdev->ctrl_handler = &ctrl_hdl->ctrl_handler;

	return 0;
}

static int sensor_board_setup(struct sensor_obj *priv)
{
	struct camera_common_data *s_data = priv->s_data;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct camera_common_power_rail *pw = s_data->power;
	struct device *dev = s_data->dev;
	u16 chipid = 0;
	u32  port_idx = priv->vc_id;
	int err = 0;

	if (pdata->mclk_name) {
		err = camera_common_mclk_enable(s_data);
		if (err) {
			dev_err(dev, "error turning on mclk (%d)\n", err);
			goto done;
		}
	}

	if (!(pw->reset_gpio && pw->pwdn_gpio)) {
		dev_err(dev, "error the power and reset gpio define\n");
		err = -EIO;
		goto pass;
	}

	gpio_set_value_cansleep(pw->reset_gpio, 0);
	// gpio_set_value_cansleep(pw->pwdn_gpio, 1);
	msleep(500);

	//To make tevi-ar0144 work on jetson nano platform,
	//it must be equipped with TN238 adapter.
	//TN238 adapter include one extend gpio chip.
	//Some gpio controll pin of tevi-ar0144 assign to this chip
	// err = __extend_gpio_setting(priv->tc_dev->client, priv);
	// if (err) {
	// 	dev_err(dev, "%s() error initial extend gpio chip\n",
	// 		__func__);
	// 	goto err_reg_probe;
	// }
	gpio_set_value_cansleep(pw->reset_gpio, 1);
pass:
	msleep(500);

	err = ar0144_i2c_read_16b(priv->tc_dev->client, 0, &chipid);
	if (err) {
		dev_err(dev, "%s() error during i2c read probe (%d)\n",
			__func__, err);
		goto err_reg_probe;
	}

	if (chipid != 0x265) {
		err = -EIO;
		dev_err(dev, "%s() invalid sensor model id: %x\n",
			__func__, chipid);
		goto err_reg_probe;
	}

	priv->otp_flash_instance = tevi_ar0144_otp_flash_init(priv->tc_dev->client);
	if(IS_ERR(priv->otp_flash_instance)) {
		err = -EINVAL;
		dev_err(dev, "otp flash init failed\n");
		goto err_reg_probe;
	}

	if(sensor_load_bootdata(priv) != 0) {
		err = -EINVAL;
		dev_err(dev, "load bootdata failed\n");
		goto err_reg_probe;
	}

	// // switch to non-sync mode
	// msleep(50);
	// __i2c_write_8b(priv->dser_i2c, 0x58, 0x5A);
	// msleep(500);
	// __extend_ub953_setting(priv->dser_i2c, priv);

	////set something reference from DevX tool register log
	//cntx select 'Video'
	__i2c_write_16b(priv->tc_dev->client, 0x1184, 1); //ATOMIC
	__i2c_write_16b(priv->tc_dev->client, 0x1000, 0); //CTRL
	__i2c_write_16b(priv->tc_dev->client, 0x1184, 0xb); //ATOMIC
	msleep(1);
	__i2c_write_16b(priv->tc_dev->client, 0x1184, 1); //ATOMIC
	//Video output 1280x800,YUV422
	__i2c_write_16b(priv->tc_dev->client, 0x2000, 1280); //VIDEO_WIDTH
	__i2c_write_16b(priv->tc_dev->client, 0x2002, 800); //VIDEO_HEIGHT
	//__i2c_write_16b(priv->tc_dev->client, 0x4012, 0x50); //VIDEO_OUT_FMT
	//Video max fps 30
	__i2c_write_16b(priv->tc_dev->client, 0x2020, 0x0F00); //VIDEO_MAX_FPS
	//non-continuous clock,2 lane
	dev_info(dev, "%s():set ISP vc_id: %d", __func__, port_idx);
	// __i2c_write_16b(priv->tc_dev->client, 0x2016, 0x303F | (port_idx << 6)); //VIDEO_HINF_CTRL
	__i2c_write_16b(priv->tc_dev->client, 0x2030, 0x1a); //VIDEO_HINF_CTRL
	//__i2c_write_16b(priv->tc_dev->client, 0x4014, 0); //VIDEO_SENSOR_MODE
	__i2c_write_16b(priv->tc_dev->client, 0x100C, 0x03); //VIDEO_HINF_CTRL
	// __i2c_write_16b(priv->tc_dev->client, 0x6132, 0x3E00); //THROUGHPUT_LIMIT
	__i2c_write_16b(priv->tc_dev->client, 0x1184, 0xb); //ATOMIC
	////let ap1302 go to standby mode
	err = sensor_standby(priv->tc_dev->client, 1);

	// switch to non-sync mode
	// msleep(50);
	// __i2c_write_8b(priv->dser_i2c, 0x58, 0x5A);
	// msleep(100);
	// __extend_ub953_setting(priv->dser_i2c, priv);

	return err;

err_reg_probe:
	gpio_set_value_cansleep(pw->reset_gpio, 0);
	gpio_set_value_cansleep(pw->pwdn_gpio, 0);

	if (pdata->mclk_name)
		camera_common_mclk_disable(s_data);
done:
	return err;
}

static int sensor_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	dev_info(&client->dev, "%s()\n", __func__);

	return 0;
}

static const struct v4l2_subdev_internal_ops sensor_subdev_internal_ops = {
	.open = sensor_open,
};

static int __probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct tegracam_device *tc_dev;
	struct sensor_obj *priv;
	int err;

	dev_info(dev, "probe tevi-ar0144 START...\n");
	dev_info(dev, "probing v4l2 sensor at addr 0x%0x\n", client->addr);

	if (!IS_ENABLED(CONFIG_OF) || !client->dev.of_node)
		return -EINVAL;

	priv = devm_kzalloc(dev,
			sizeof(struct sensor_obj), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	tc_dev = devm_kzalloc(dev,
			sizeof(struct tegracam_device), GFP_KERNEL);
	if (!tc_dev)
		return -ENOMEM;

	tc_dev->client = client;
	tc_dev->dev = dev;
	tc_dev->priv = (void*)priv;
	tc_dev->dev_regmap_config = &sensor_regmap_config;
	tc_dev->sensor_ops = &sensor_common_ops;
	tc_dev->v4l2sd_internal_ops = &sensor_subdev_internal_ops;
	// tc_dev->tcctrl_ops = &sensor_nv_ctrl_ops;

	err = tegracam_device_register(tc_dev);
	if (err) {
		dev_err(dev, "tegra camera driver registration failed\n");
		return err;
	}
	dev_info(dev, "tegracam_device_register done\n");
	priv->tc_dev = tc_dev;
	priv->s_data = tc_dev->s_data;
	priv->subdev = &tc_dev->s_data->subdev;
	tegracam_set_privdata(tc_dev, (void *)priv);

	err = sensor_board_setup(priv);
	if (err) {
		dev_err(dev, "board setup failed\n");
		return err;
	}

	err = tegracam_v4l2subdev_register(tc_dev, true);
	if (err) {
		dev_err(dev, "tegra camera subdev registration failed\n");
		return err;
	}

	err = sensor_ctrls_init(priv);
	if (err) {
		dev_err(&client->dev, "failed to init controls: %d", err);
		goto error_probe;
	}

	dev_info(dev, "detected tevi-ar0144 camera sensor\n");

error_probe:
	mutex_destroy(&priv->lock);

	return 0;
}

static int __remove(struct i2c_client *client)
{
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct sensor_obj *priv = (struct sensor_obj *)s_data->priv;

	tegracam_v4l2subdev_unregister(priv->tc_dev);
	tegracam_device_unregister(priv->tc_dev);

	return 0;
}

static const struct i2c_device_id __id[] = {
	{ "tevi-ar0144", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, __id);

static struct i2c_driver __i2c_driver = {
	.driver = {
		.name = "tevi-ar0144",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(__of_match),
	},
	.probe = __probe,
	.remove = __remove,
	.id_table = __id,
};
module_i2c_driver(__i2c_driver);

MODULE_DESCRIPTION("Media Controller driver for TEVI AR0144");
MODULE_AUTHOR("TechNexion");
MODULE_LICENSE("GPL v2");
