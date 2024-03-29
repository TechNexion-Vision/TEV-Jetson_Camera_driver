#define DEBUG
#include <linux/i2c.h>
#include "otp_flash.h"

struct otp_flash {
	struct i2c_client *client;
	void *header_data;
};

#ifndef __FAKE__
#include "bootdata.h"

struct otp_flash *tevi_ar0144_otp_flash_init(struct i2c_client *client)
{
	return NULL;
}

u16 tevi_ar0144_otp_flash_get_checksum(struct otp_flash *instance)
{
	return BOOTDATA_CHECKSUM;
}

size_t tevi_ar0144_otp_flash_read(struct otp_flash *instance, u8 *data, int addr, size_t len)
{
	size_t l;

	l = len > BOOT_DATA_WRITE_LEN ? BOOT_DATA_WRITE_LEN : len;
	l = (BOOTDATA_TOTAL_SIZE - addr) < l ? BOOTDATA_TOTAL_SIZE - addr : l;

	memmove(data, &((u8*)__bootdata__)[addr], l);
	return l;
}

size_t tevi_ar0144_otp_flash_get_pll_length(struct otp_flash *instance)
{
	return BOOTDATA_PLL_INIT_SIZE;
}

size_t tevi_ar0144_otp_flash_get_pll_section(struct otp_flash *instance, u8 *data)
{
	tevi_ar0144_otp_flash_read(instance, data, 0, BOOTDATA_PLL_INIT_SIZE);
	return BOOTDATA_PLL_INIT_SIZE;
}
#else
/*
header Version 2 : {
	uint8 header_version
	uint16 content_offset //content_offset may not be equal to the size of header
	uint8 product_name[64]
	uint8 product_version
	uint8 lens_name[64]
	uint8 lens_version
	uint8 content_version
	uint32 content_checksum
	uint32 content_len
	uint16 pll_bootdata_len
}
*/
struct header_ver2 {
	u8 header_version;
	u16 content_offset;
	u8 product_name[64];
	u8 product_version;
	u8 lens_name[64];
	u8 lens_version;
	u8 content_version;
	u32 content_checksum;
	u32 content_len;
	u16 pll_bootdata_len;
} __attribute__((packed));

static int tevi_ar0144_otp_read(struct i2c_client *client, u32 reg, u16 size, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[2];

	if (size == 0)
		return 0;

	buf[0] = (reg & 0xffff) >> 8;
	buf[1] = (reg & 0xff);

	msg[0].addr = 0x54 + (reg >> 16);
	msg[0].flags = 0;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = 0x54 + (reg >> 16);
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = val;
	msg[1].len = size;

	return i2c_transfer(client->adapter, msg, 2);
}

struct otp_flash *tevi_ar0144_otp_flash_init(struct i2c_client *client)
{
	struct otp_flash *instance;
	struct device *dev = &client->dev;
	u8 __header_ver;
	struct header_ver2 *header;
	int ret;

	ret = tevi_ar0144_otp_read(client, 0, 1, &__header_ver);
	if (ret < 0)
		return ERR_PTR(ret);

	if (__header_ver == 2) {
		header = devm_kzalloc(dev,
				      sizeof(struct header_ver2),
				      GFP_KERNEL);
		if (header == NULL) {
			dev_err(dev, "allocate memory failed\n");
			return ERR_PTR(-ENOMEM);
		}

		tevi_ar0144_otp_read(client,
			   0,
			   sizeof(struct header_ver2),
			   (u8 *)header);

		dev_info(dev, "Product:%s, Version:%d Lens:%s, Version:%d\n",
			 header->product_name,
			 header->product_version,
			 header->lens_name,
			 header->lens_version);

		dev_dbg(dev, "content ver: %d, content checksum: %x\n",
			header->content_version, header->content_checksum);
		dev_dbg(dev, "content length: %d, pll bootdata length: %d\n",
			header->content_len, header->pll_bootdata_len);

		instance = devm_kzalloc(dev, sizeof(struct otp_flash), GFP_KERNEL);
		if (instance == NULL) {
			dev_err(dev, "allocate memory failed\n");
			return ERR_PTR(-ENOMEM);
		}

		instance->client = client;
		instance->header_data = header;
		return instance;

	} else {
		dev_err(dev, "can't recognize header version number '0x%X'\n",
			__header_ver);
	}

	return ERR_PTR(-EINVAL);
}

u16 tevi_ar0144_otp_flash_get_checksum(struct otp_flash *instance)
{
	struct header_ver2 *header;

	if( ((u8*)instance->header_data)[0] == 2 ) {
		header = (struct header_ver2 *)instance->header_data;
		return header->content_checksum;
	}

	return 0xffff;
}

size_t tevi_ar0144_otp_flash_read(struct otp_flash *instance, u8 *data, int addr, size_t len)
{
	u8 *temp;
	struct header_ver2 *header;
	size_t l;

	temp = (u8*)instance->header_data;
	if(temp[0] == 2) {
		header = (struct header_ver2 *)instance->header_data;
		l = len > BOOT_DATA_WRITE_LEN ? BOOT_DATA_WRITE_LEN : len;
		l = (header->content_len - addr) < l ? header->content_len - addr : l;

		tevi_ar0144_otp_read(instance->client, addr + header->content_offset, l, data);
		return l;
	}

	return 0;
}

size_t tevi_ar0144_otp_flash_get_pll_length(struct otp_flash *instance)
{
	u8 *temp;
	struct header_ver2 *header;

	temp = (u8*)instance->header_data;
	if(temp[0] == 2) {
		header = (struct header_ver2 *)instance->header_data;
		return header->pll_bootdata_len;
	}

	return 0;
}

size_t tevi_ar0144_otp_flash_get_pll_section(struct otp_flash *instance, u8 *data)
{
	u8 *temp;
	struct header_ver2 *header;

	temp = (u8*)instance->header_data;
	if(temp[0] == 2) {
		header = (struct header_ver2 *)instance->header_data;
		if (header->pll_bootdata_len != 0) {
			tevi_ar0144_otp_flash_read(instance, data, 0,
				       header->pll_bootdata_len);
		}
		return header->pll_bootdata_len;
	}

	return 0;
}

#endif
