//#define DEBUG
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
static const struct camera_common_frmfmt sensor_frmfmt[] = {
	{{1280, 800}, &__30fps, 1, 0, 0 /* number depend on dts mode list */},
};

static const u32 ctrl_cid_list[] = {
};

struct sensor_obj {
	struct v4l2_subdev		*subdev;
	struct camera_common_data	*s_data;
	struct tegracam_device		*tc_dev;
	void				*otp_flash_instance;
	u32				frame_length;
};

static int __i2c_read_8b(struct i2c_client *client, u8 reg, u8 *val, u8 size)
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

static int __i2c_read(struct i2c_client *client, u16 reg, u8 *val, u8 size)
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

static int __i2c_read_16b(struct i2c_client *client, u16 reg, u16 *value)
{
	u8 v[2] = {0,0};
	int ret;

	ret = __i2c_read(client, reg, v, 2);

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

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
	{
		dev_err(&client->dev, "i2c transfer error.\n");
		return -EIO;
	}

	return 0;
}

// static int __extend_gpio_setting(struct i2c_client *client)
// {
// 	int ret;
// 	u8 old_addr = client->addr;

// 	dev_dbg(&client->dev, "%s()\n", __func__);
// 	client->addr = 0x21; //TN238 extend gpio chip i2c address
// 	ret = __i2c_write_8b(client, 3, 0xFA);
// 	ret = __i2c_write_8b(client, 1, 1);
// 	client->addr = old_addr;

// 	return ret;
// }

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.use_single_rw = true,
};

// static struct tegracam_ctrl_ops sensor_ctrl_ops = {
// 	.numctrls = ARRAY_SIZE(ctrl_cid_list),
// 	.ctrl_cid_list = ctrl_cid_list,
// };

static int sensor_standby(struct i2c_client *client, int enable)
{
	u16 v = 0;
	int timeout;
	dev_info(&client->dev, "%s()\n", __func__);

	if (enable == 1) {
		__i2c_write_16b(client, 0x601a, 0x140);
		for (timeout = 0 ; timeout < 50 ; timeout ++) {
			usleep_range(9000, 10000);
			__i2c_read_16b(client, 0x601a, &v);
			if ((v & 0x200) == 0x200)
				break;
		}
		if (timeout < 50) {
			__i2c_write_16b(client, 0xFFFE, 1);
			msleep(1);
		} else {
			dev_err(&client->dev, "timeout: line[%d]\n", __LINE__);
			return -EINVAL;
		}
	} else {
		__i2c_write_16b(client, 0xFFFE, 0);
		for (timeout = 0 ; timeout < 50 ; timeout ++) {
			usleep_range(9000, 10000);
			__i2c_read_16b(client, 0, &v);
			if (v != 0)
				break;
		}
		if (timeout >= 50) {
			dev_err(&client->dev, "timeout: line[%d]\n", __LINE__);
			return -EINVAL;
		}

		__i2c_write_16b(client, 0x601a, 0x241);
		usleep_range(1000, 2000);
		for (timeout = 0 ; timeout < 10 ; timeout ++) {
			usleep_range(9000, 10000);
			__i2c_read_16b(client, 0x601a, &v);
			if ((v & 1) == 0)
				break;
		}
		if (timeout >= 10) {
			dev_err(&client->dev, "timeout: line[%d]\n", __LINE__);
			return -EINVAL;
		}

		for (timeout = 0 ; timeout < 10 ; timeout ++) {
			usleep_range(9000, 10000);
			__i2c_read_16b(client, 0x601a, &v);
			if ((v & 0x8000) == 0x8000)
				break;
		}
		if (timeout >= 10) {
			dev_err(&client->dev, "timeout: line[%d]\n", __LINE__);
			return -EINVAL;
		}

		__i2c_write_16b(client, 0x601a, v | 0x10);
		for (timeout = 0 ; timeout < 10 ; timeout ++) {
			usleep_range(9000, 10000);
			__i2c_read_16b(client, 0x601a, &v);
			if (v == 0x8040)
				break;
		}
		if (timeout >= 10) {
			dev_err(&client->dev, "timeout: line[%d]\n", __LINE__);
			return -EINVAL;
		}
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
	int err = 0;
	int gpio;

	struct device_node *dser_node;
	struct i2c_client *dser_i2c = NULL;
	u32 port_idx = 4;
	u32 ser_alias_id = 0;
	u8 dser_i2c_val = 0x7F;
	u32 dser_time = 0;

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
		goto error;
	}
	if (dser_i2c->dev.driver == NULL) {
		dev_err(dev, "missing deserializer driver\n");
		goto error;
	}
	dev_info(dev, "dser_i2c at addr 0x%0x\n", dser_i2c->addr);

	err = of_property_read_u32(dser_node, "ser_alias_id", &ser_alias_id);
	if (err) {
		dev_err(dev, "invalid ser_alias_id on %pOF\n", dser_node);
		goto error;
	}
	dev_info(dev, "Read ser_alias_id %d\n", ser_alias_id);

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
		__i2c_write_8b(dser_i2c, 0x4c,
						(port_idx << 4) | 
						(1 << port_idx));
		msleep(500);
		__i2c_read_8b(dser_i2c, 0x4d, &dser_i2c_val, 1);
		dev_info(dev, "Read 0x4D=0x%x...\n", dser_i2c_val);
		msleep(100);
		if((dser_i2c_val & 0x7) == 3){
			__i2c_read_8b(dser_i2c, 0x5b, &dser_i2c_val, 1);
			__i2c_write_8b(dser_i2c, 0x5c,
				    ser_alias_id << 1);
			msleep(100);
			break;
		}
		dser_time++;
	}
	/*=====================================================*/
	dev_info(dev, "%s():np_name=%s\n", __func__, np->name);
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

	dev_dbg(tc_dev->dev,
		"%s() , {%d}\n",
		__func__,
		s_data->mode);

	return 0;
}

static int sensor_start_streaming(struct tegracam_device *tc_dev)
{
	dev_dbg(tc_dev->dev, "%s()\n", __func__);

	return sensor_standby(tc_dev->client, 0);
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
	size_t len = 0;
	size_t pll_len = 0;
	u16 otp_data;
	u16 *bootdata_temp_area;
	u16 checksum;
	int i, ret;
	int count = 0;
	const int len_each_time = 1024;

	bootdata_temp_area = devm_kzalloc(dev,
					  len_each_time + 2,
					  GFP_KERNEL);
	if (bootdata_temp_area == NULL) {
		dev_err(dev, "allocate memory failed\n");
		return -EINVAL;
	}

	checksum = tevi_ar0144_otp_flash_get_checksum(priv->otp_flash_instance);

	//load pll
	bootdata_temp_area[0] = cpu_to_be16(BOOT_DATA_START_REG);
	pll_len = tevi_ar0144_otp_flash_get_pll_section(priv->otp_flash_instance,
					    (u8 *)(&bootdata_temp_area[1]));
	dev_dbg(dev, "load pll data of length [%zu] into register [%x]\n",
		pll_len, BOOT_DATA_START_REG);
	do{
		count++;
		ret = __i2c_write_bust(priv->tc_dev->client, (u8 *)bootdata_temp_area, pll_len + 2);
		if(ret == 0) { count = 0; break;}
	} while(count <= 10);
	__i2c_write_16b(priv->tc_dev->client, 0x6002, 2);
	msleep(1);

	//load bootdata part1
	bootdata_temp_area[0] = cpu_to_be16(BOOT_DATA_START_REG + pll_len);
	len = tevi_ar0144_otp_flash_read(priv->otp_flash_instance,
			     (u8 *)(&bootdata_temp_area[1]),
			     pll_len, len_each_time - pll_len);
	dev_dbg(dev, "load data of length [%zu] into register [%zx]\n",
		len, BOOT_DATA_START_REG + pll_len);
	
	do{
		count++;
		ret = __i2c_write_bust(priv->tc_dev->client, (u8 *)bootdata_temp_area, len + 2);
		if(ret == 0) { count = 0; break;}
	} while(count <= 10);
	i = index = pll_len + len;

	//load bootdata ronaming
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
			
			do{
				count++;
				ret = __i2c_write_bust(priv->tc_dev->client,
					 (u8 *)bootdata_temp_area,
					 len + 2);
				if(ret == 0) { count = 0; break;}
			} while(count <= 10);
			index += len;
			i += len_each_time;
		}

		i = 0;
	}

	__i2c_write_16b(priv->tc_dev->client, 0x6002, 0xffff);
	devm_kfree(dev, bootdata_temp_area);

	index = 0;
	otp_data = 0;
	while(otp_data != checksum && index < 20) {
		msleep(10);
		__i2c_read_16b(priv->tc_dev->client, 0x6134, &otp_data);
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
		goto pass;
	}

	gpio_set_value_cansleep(pw->reset_gpio, 0);
	gpio_set_value_cansleep(pw->pwdn_gpio, 1);
	msleep(10);

	//To make tevi-ar0144 work on jetson nano platform,
	//it must be equipped with TN238 adapter.
	//TN238 adapter include one extend gpio chip.
	//Some gpio controll pin of tevi-ar0144 assign to this chip
	// err = __extend_gpio_setting(priv->tc_dev->client);
	// if (err) {
	// 	dev_err(dev, "%s() error initial extend gpio chip\n",
	// 		__func__);
	// 	goto err_reg_probe;
	// }
	gpio_set_value_cansleep(pw->reset_gpio, 1);
pass:
	msleep(300);

	err = __i2c_read_16b(priv->tc_dev->client, 0, &chipid);
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

	////set something reference from DevX tool register log
	//cntx select 'Video'
	__i2c_write_16b(priv->tc_dev->client, 0x1184, 1); //ATOMIC
	__i2c_write_16b(priv->tc_dev->client, 0x1000, 2); //CTRL
	__i2c_write_16b(priv->tc_dev->client, 0x1184, 0xb); //ATOMIC
	msleep(1);
	__i2c_write_16b(priv->tc_dev->client, 0x1184, 1); //ATOMIC
	//Video output 1280x800,YUV422
	__i2c_write_16b(priv->tc_dev->client, 0x4000, 1280); //VIDEO_WIDTH
	__i2c_write_16b(priv->tc_dev->client, 0x4002, 800); //VIDEO_HEIGHT
	//__i2c_write_16b(priv->tc_dev->client, 0x4012, 0x50); //VIDEO_OUT_FMT
	//Video max fps 30
	__i2c_write_16b(priv->tc_dev->client, 0x4020, 0x1E00); //VIDEO_MAX_FPS
	//non-continuous clock,2 lane
	__i2c_write_16b(priv->tc_dev->client, 0x4030, 0x12); //VIDEO_HINF_CTRL
	//__i2c_write_16b(priv->tc_dev->client, 0x4014, 0); //VIDEO_SENSOR_MODE
	__i2c_write_16b(priv->tc_dev->client, 0x100C, 0x03); //VIDEO_HINF_CTRL
	__i2c_write_16b(priv->tc_dev->client, 0x1184, 0xb); //ATOMIC
	////let ap1302 go to standby mode
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
	tc_dev->dev_regmap_config = &sensor_regmap_config;
	tc_dev->sensor_ops = &sensor_common_ops;
	tc_dev->v4l2sd_internal_ops = &sensor_subdev_internal_ops;
	// tc_dev->tcctrl_ops = &sensor_ctrl_ops;

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

	dev_info(dev, "detected tevi-ar0144 camera sensor\n");

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
