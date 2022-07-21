#define DEBUG           /* Enable dev_dbg */
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/tegra-v4l2-camera.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/camera_common.h>
#include "tevi_ap1302_mode_tbls.h"
#include "otp_flash.h"

static const struct of_device_id sensor_of_match[] = {
	{ .compatible = "tn,tevi-ap1302", },
	{ },
};
MODULE_DEVICE_TABLE(of, sensor_of_match);

static const u32 ctrl_cid_list[] = {
	TEGRA_CAMERA_CID_FRAME_RATE,
	TEGRA_CAMERA_CID_SENSOR_MODE_ID,
};

struct sensor_obj {
	struct v4l2_subdev		*subdev;
	struct camera_common_data	*s_data;
	struct tegracam_device		*tc_dev;
	struct otp_flash		*otp_flash_instance;
	struct header_ver2 *header;
	int exp_gpio;
	int info_gpio;
	u8 selected_mode;
	u8 selected_sensor;
	char *sensor_name;
};

static int sensor_i2c_read(struct i2c_client *client, u16 reg, u8 *val, u8 size)
{
	struct i2c_msg msg[2];
	u8 buf[2];

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

	return i2c_transfer(client->adapter, msg, 2);
}

static int sensor_i2c_read_16b(struct i2c_client *client, u16 reg, u16 *value)
{
	u8 v[2] = {0,0};
	int ret;

	ret = sensor_i2c_read(client, reg, v, 2);

	if (unlikely(ret < 0)) {
		dev_err(&client->dev, "i2c transfer error. addr=%x, reg=%x, val=%x\n", client->addr, reg, *value);
		return ret;
	}

	*value = (v[0] << 8) | v[1];
	dev_dbg(&client->dev, "%s() read reg 0x%x, value 0x%x\n",
		__func__, reg, *value);

	return 0;
}

static int sensor_i2c_write_16b(struct i2c_client *client, u16 reg, u16 val)
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
		dev_err(&client->dev, "i2c transfer error. addr=%x, reg=%x, val=%x\n", client->addr, reg, val);
		return -EIO;
	}

	return 0;
}

static int sensor_i2c_write_bust(struct i2c_client *client, u8 *buf, size_t len)
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

	dev_err(&client->dev, "bust i2c transfer : addr=0x%x, buf=0x%x, len=%zu\n", client->addr, *buf, len);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
	{
		dev_err(&client->dev, "bust i2c transfer error. addr=0x%x, buf=0x%x, len=%zu\n", client->addr, *buf, len);
		return -EIO;
	}

	return 0;
}

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.use_single_rw = true,
};

static int sensor_set_frame_rate(struct tegracam_device *tc_dev, s64 val)
{
	return 0;
}

static int sensor_set_group_hold(struct tegracam_device *tc_dev, bool val)
{
	/* TEVI-AP1302 does not support group hold */
	return 0;
}

static struct tegracam_ctrl_ops sensor_ctrl_ops = {
	.numctrls = ARRAY_SIZE(ctrl_cid_list),
	.ctrl_cid_list = ctrl_cid_list,
	.set_frame_rate = sensor_set_frame_rate,
	.set_group_hold = sensor_set_group_hold,
};

static int set_standby_mode_rel419(struct i2c_client *client, int enable)
{
	u16 v = 0;
	int timeout;
	dev_dbg(&client->dev, "%s():enable=%d\n", __func__, enable);

	if (enable == 1) {
		sensor_i2c_write_16b(client, 0x601a, 0x8140);
		for (timeout = 0 ; timeout < 50 ; timeout ++) {
			usleep_range(9000, 10000);
			sensor_i2c_read_16b(client, 0x601a, &v);
			if ((v & 0x200) == 0x200)
				break;
		}
		if (timeout < 50) {
			sensor_i2c_write_16b(client, 0xFFFE, 1);
			msleep(100);
		} else {
			dev_err(&client->dev, "timeout: line[%d]\n", __LINE__);
			return -EINVAL;
		}
	}  else {
		sensor_i2c_write_16b(client, 0xFFFE, 0);
		for (timeout = 0 ; timeout < 500 ; timeout ++) {
			usleep_range(9000, 10000);
			sensor_i2c_read_16b(client, 0, &v);
			if (v != 0)
				break;
		}
		if (timeout >= 500) {
			dev_err(&client->dev, "timeout: line[%d]v=%x\n", __LINE__, v);
			return -EINVAL;
		}

		sensor_i2c_read_16b(client, 0x601a, &v);
		if ((v & 0x200) != 0x200){
			dev_dbg(&client->dev, "stop waking up: camera is working.\n");
			return 0;
		}

		sensor_i2c_write_16b(client, 0x601a, 0x241);
		usleep_range(1000, 2000);
		for (timeout = 0 ; timeout < 100 ; timeout ++) {
			usleep_range(9000, 10000);
			sensor_i2c_read_16b(client, 0x601a, &v);
			if ((v & 1) == 0)
				break;
		}
		if (timeout >= 100) {
			dev_err(&client->dev, "timeout: line[%d]v=%x\n", __LINE__, v);
			return -EINVAL;
		}

		for (timeout = 0 ; timeout < 100 ; timeout ++) {
			usleep_range(9000, 10000);
			sensor_i2c_read_16b(client, 0x601a, &v);
			if ((v & 0x8000) == 0x8000)
				break;
		}
		if (timeout >= 100) {
			dev_err(&client->dev, "timeout: line[%d]v=%x\n", __LINE__, v);
			return -EINVAL;
		}

		sensor_i2c_write_16b(client, 0x601a, 0x8250);
		for (timeout = 0 ; timeout < 100 ; timeout ++) {
			usleep_range(9000, 10000);
			sensor_i2c_read_16b(client, 0x601a, &v);
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

	sensor_i2c_read_16b(client, 0x6134, &checksum);

	if(checksum != 0xFFFF){
		return set_standby_mode_rel419(client, enable); // standby for rel419
	}

	if (enable == 1) {
		sensor_i2c_write_16b(client, 0x601a, 0x0180);
		for (timeout = 0 ; timeout < 50 ; timeout ++) {
			usleep_range(9000, 10000);
			sensor_i2c_read_16b(client, 0x601a, &v);
			if ((v & 0x200) == 0x200)
				break;
		}
	}  else {
		sensor_i2c_write_16b(client, 0x601a, 0x0380);
		usleep_range(1000, 2000);
		for (timeout = 0 ; timeout < 100 ; timeout ++) {
			usleep_range(9000, 10000);
			sensor_i2c_read_16b(client, 0x601a, &v);
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
	dev_dbg(s_data->dev, "%s() power on\n", __func__);
	s_data->power->state = SWITCH_ON;
	return 0;
}

static int sensor_power_off(struct camera_common_data *s_data)
{
	dev_dbg(s_data->dev, "%s() power off\n", __func__);
	s_data->power->state = SWITCH_OFF;
	return 0;
}

static int sensor_power_put(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;

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
		goto done;
	}

	pw->reset_gpio = pdata->reset_gpio;
	err = gpio_request(pw->reset_gpio, "cam_reset_gpio");
	if (err < 0) {
		dev_err(dev, "%s() unable to request reset_gpio (%d)\n",
			__func__, err);
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
	int err = 0;
	int gpio;

	if (!np)
		return NULL;

	board_priv_pdata = devm_kzalloc(dev,
		sizeof(*board_priv_pdata), GFP_KERNEL);
	if (!board_priv_pdata)
		return NULL;

	gpio = of_get_named_gpio(np, "power-gpios", 0);
	if (gpio < 0) {
		if (gpio == -EPROBE_DEFER)
			ret = ERR_PTR(-EPROBE_DEFER);
		dev_err(dev, "power-gpios not found\n");
		goto error;
	}
	board_priv_pdata->pwdn_gpio = (unsigned int)gpio;

	gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (gpio < 0) {
		if (gpio == -EPROBE_DEFER)
			ret = ERR_PTR(-EPROBE_DEFER);
		dev_err(dev, "reset-gpios not found\n");
		goto error;
	}
	board_priv_pdata->reset_gpio = (unsigned int)gpio;

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
	struct sensor_obj *priv = tc_dev->priv;
	dev_info(tc_dev->dev,
		"%s() , {%d}, fmt_width=%d, fmt_height=%d\n",
		__func__,
		s_data->mode, 
		tc_dev->s_data->fmt_width,
		tc_dev->s_data->fmt_height);

	priv->selected_mode = s_data->mode;

	return 0;
}

static int sensor_start_streaming(struct tegracam_device *tc_dev)
{
	struct sensor_obj *priv = tc_dev->priv;
	int ret = 0;
	dev_info(tc_dev->dev, "%s()\n", __func__);

	ret = sensor_standby(tc_dev->client, 0);

	if (ret == 0) {
		int fps = *ap1302_sensor_table[priv->selected_sensor].frmfmt[priv->selected_mode].framerates;
		dev_dbg(tc_dev->dev, "%s() width=%d, height=%d, fps=%d\n", __func__, 
			ap1302_sensor_table[priv->selected_sensor].frmfmt[priv->selected_mode].size.width, 
			ap1302_sensor_table[priv->selected_sensor].frmfmt[priv->selected_mode].size.height, 
			fps);
		sensor_i2c_write_16b(tc_dev->client, 0x1184, 1); //ATOMIC
		//VIDEO_WIDTH
		sensor_i2c_write_16b(tc_dev->client, 0x2000,
				     ap1302_sensor_table[priv->selected_sensor].frmfmt[priv->selected_mode].size.width);
		//VIDEO_HEIGHT
		sensor_i2c_write_16b(tc_dev->client, 0x2002,
				     ap1302_sensor_table[priv->selected_sensor].frmfmt[priv->selected_mode].size.height);
		// VIDEO_MAX_FPS
		sensor_i2c_write_16b(tc_dev->client, 0x2020, fps << 8);
		sensor_i2c_write_16b(tc_dev->client, 0x1184, 0xb); //ATOMIC
	}

	return ret;
}

static int sensor_stop_streaming(struct tegracam_device *tc_dev)
{
	dev_info(tc_dev->dev, "%s()\n", __func__);

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

	checksum = tevi_ap1302_otp_flash_get_checksum(priv->otp_flash_instance);

	while(len != 0) {
		while(i < BOOT_DATA_WRITE_LEN) {
			bootdata_temp_area[0] =
				cpu_to_be16(BOOT_DATA_START_REG + i);
			len = tevi_ap1302_otp_flash_read(priv->otp_flash_instance,
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
			sensor_i2c_write_bust(priv->tc_dev->client,
					 (u8 *)bootdata_temp_area,
					 len + 2);
			index += len;
			i += len_each_time;
		}

		i = 0;
	}

	sensor_i2c_write_16b(priv->tc_dev->client, 0x6002, 0xffff);
	devm_kfree(dev, bootdata_temp_area);

	msleep(500);

	index = 0;
	otp_data = 0;
	while(otp_data != checksum && index < 20) {
		msleep(10);
		sensor_i2c_read_16b(priv->tc_dev->client, 0x6134, &otp_data);
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

static int sensor_board_setup(struct sensor_obj *priv)
{
	struct camera_common_data *s_data = priv->s_data;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct camera_common_power_rail *pw = s_data->power;
	struct device *dev = s_data->dev;
	int data_lanes;
	int continuous_clock;
	bool has_rpi = false;
	int i;
	u16 chipid = 0;
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
		goto done;
	}

	gpio_set_value_cansleep(pw->reset_gpio, 0);
	gpio_set_value_cansleep(pw->pwdn_gpio, 1);
	msleep(200);

	has_rpi = of_property_read_bool(dev->of_node, "has-rpi-adapter");
	if(has_rpi) {
		dev_dbg(dev, "initial extend gpio chip for RPI-Adapter...\n");
		priv->exp_gpio = of_get_named_gpio(dev->of_node, "exp-gpios", 0);
		if (priv->exp_gpio < 0) {
			if (priv->exp_gpio == -EPROBE_DEFER)
				err = -EPROBE_DEFER;
			dev_err(dev, "exp-gpios not found\n");
			goto err_reg_probe;
		}
		priv->info_gpio = of_get_named_gpio(dev->of_node, "info-gpios", 0);
		if (priv->info_gpio < 0) {
			if (priv->info_gpio == -EPROBE_DEFER)
				err = -EPROBE_DEFER;
			dev_err(dev, "info-gpios not found\n");
			goto err_reg_probe;
		}

		gpio_set_value_cansleep(priv->exp_gpio, 1);
		gpio_set_value_cansleep(priv->info_gpio, 1);
	}

	gpio_set_value_cansleep(pw->reset_gpio, 1);
	msleep(300);

	err = sensor_i2c_read_16b(priv->tc_dev->client, 0, &chipid);
	if (err) {
		dev_err(dev, "%s() error during i2c read probe (%d)\n",
			__func__, err);
		goto err_reg_probe;
	}
	if (chipid != 0x265) {
		err = -EIO;
		dev_err(dev, "%s() invalid chip model id: %x\n",
			__func__, chipid);
		goto err_reg_probe;
	}

	data_lanes = 2;
	if (of_property_read_u32(dev->of_node, "data-lanes", &data_lanes) == 0) {
		if ((data_lanes < 1) || (data_lanes > 4)) {
			dev_err(dev, "value of 'data-lanes' property is invaild\n");
			data_lanes = 2;
		}
	}

	continuous_clock = 0;
	if (of_property_read_u32(dev->of_node, "continuous-clock",
				 &continuous_clock) == 0) {
		if (continuous_clock > 1) {
			dev_err(dev, "value of 'continuous-clock' property is invaild\n");
			continuous_clock = 0;
		}
	}

	priv->otp_flash_instance = tevi_ap1302_otp_flash_init(priv->tc_dev->client);
	if(IS_ERR(priv->otp_flash_instance)) {
		err = -EINVAL;
		dev_err(dev, "otp flash init failed\n");
		goto err_reg_probe;
	}

	priv->header = priv->otp_flash_instance->header_data;
	for(i = 0 ; i < ARRAY_SIZE(ap1302_sensor_table); i++)
	{
		if (strcmp((const char*)priv->header->product_name, ap1302_sensor_table[i].sensor_name) == 0)
			break;
	}
	priv->selected_sensor = i;
	dev_info(dev, "selected_sensor:%d, sensor_name:%s\n", i, priv->header->product_name);

	switch(priv->selected_sensor){
	case TEVI_AP1302_AR0144:
		s_data->frmfmt = ar0144_frmfmt;
		s_data->numfmts = ARRAY_SIZE(ar0144_frmfmt);
		break;
	case TEVI_AP1302_AR0234:
		s_data->frmfmt = ar0234_frmfmt;
		s_data->numfmts = ARRAY_SIZE(ar0234_frmfmt);
		break;
	case TEVI_AP1302_AR0521:
	case TEVI_AP1302_AR0522:
		s_data->frmfmt = ar0521_frmfmt;
		s_data->numfmts = ARRAY_SIZE(ar0521_frmfmt);
		break;
	case TEVI_AP1302_AR0821:
		s_data->frmfmt = ar0821_frmfmt;
		s_data->numfmts = ARRAY_SIZE(ar0821_frmfmt);
		break;
	case TEVI_AP1302_AR1335:
		s_data->frmfmt = ar1335_frmfmt;
		s_data->numfmts = ARRAY_SIZE(ar1335_frmfmt);
		break;
	default:
		s_data->frmfmt = sensor_frmfmt;
		s_data->numfmts = ARRAY_SIZE(sensor_frmfmt);
		break;
	}

	if(sensor_load_bootdata(priv) != 0) {
		err = -EINVAL;
		dev_err(dev, "load bootdata failed\n");
		goto err_reg_probe;
	}

	//cntx select 'Video'
	sensor_i2c_write_16b(priv->tc_dev->client, 0x1184, 1); //ATOMIC
	sensor_i2c_write_16b(priv->tc_dev->client, 0x1000, 0); //CTRL
	sensor_i2c_write_16b(priv->tc_dev->client, 0x1184, 0xb); //ATOMIC
	msleep(1);
	sensor_i2c_write_16b(priv->tc_dev->client, 0x1184, 1); //ATOMIC
	//Video output 1920x1080,YUV422
	sensor_i2c_write_16b(priv->tc_dev->client, 0x2000, 1920); //VIDEO_WIDTH
	sensor_i2c_write_16b(priv->tc_dev->client, 0x2002, 1080); //VIDEO_HEIGHT
	sensor_i2c_write_16b(priv->tc_dev->client, 0x2012, 0x50); //VIDEO_OUT_FMT
	//Video max fps 30
	sensor_i2c_write_16b(priv->tc_dev->client, 0x2020, 0x1e00); //VIDEO_MAX_FPS
	//non-continuous clock,2 lane
	sensor_i2c_write_16b(priv->tc_dev->client, 0x2030,
			     0x10 | (continuous_clock << 5) | (data_lanes)); //VIDEO_HINF_CTRL
	sensor_i2c_write_16b(priv->tc_dev->client, 0x1184, 0xb); //ATOMIC

	//let ap1302 go to standby mode
	return sensor_standby(priv->tc_dev->client, 1);

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

	dev_dbg(&client->dev, "%s()\n", __func__);

	return 0;
}

static const struct v4l2_subdev_internal_ops sensor_subdev_internal_ops = {
	.open = sensor_open,
};

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct tegracam_device *tc_dev;
	struct sensor_obj *priv;
	int err;

	dev_info(dev, "probe tevi-ap1302 START...\n");
	dev_dbg(dev, "probing v4l2 sensor at addr 0x%0x\n", client->addr);

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
	tc_dev->dev_regmap_config = &sensor_regmap_config;
	tc_dev->sensor_ops = &sensor_common_ops;
	tc_dev->v4l2sd_internal_ops = &sensor_subdev_internal_ops;
	tc_dev->tcctrl_ops = &sensor_ctrl_ops;

	err = tegracam_device_register(tc_dev);
	if (err) {
		dev_err(dev, "tegra camera driver registration failed\n");
		return err;
	}
	priv->tc_dev = tc_dev;
	priv->s_data = tc_dev->s_data;
	priv->subdev = &tc_dev->s_data->subdev;
	tegracam_set_privdata(tc_dev, (void *)priv);

	err = sensor_board_setup(priv);
	if (err) {
		tegracam_device_unregister(tc_dev);
		dev_err(dev, "board setup failed\n");
		return err;
	}

	err = tegracam_v4l2subdev_register(tc_dev, true);
	if (err) {
		dev_err(dev, "tegra camera subdev registration failed\n");
		return err;
	}

	dev_info(dev, "detected tevi-ap1302 camera sensor\n");

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct sensor_obj *priv = (struct sensor_obj *)s_data->priv;

	tegracam_v4l2subdev_unregister(priv->tc_dev);
	tegracam_device_unregister(priv->tc_dev);

	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{ "tevi-ap1302", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_i2c_driver = {
	.driver = {
		.name = "tevi-ap1302",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sensor_of_match),
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};
module_i2c_driver(sensor_i2c_driver);

MODULE_DESCRIPTION("Media Controller driver for TEVI AP1302");
MODULE_AUTHOR("TechNexion");
MODULE_LICENSE("GPL v2");
