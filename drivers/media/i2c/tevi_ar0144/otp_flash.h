#ifndef __OTP_FLASH_H__
#define __OTP_FLASH_H__

#include <linux/i2c.h>

#define BOOT_DATA_START_REG (0x8000)
#define BOOT_DATA_END_REG (0x9FFF)
#define BOOT_DATA_WRITE_LEN (BOOT_DATA_END_REG - BOOT_DATA_START_REG + 1)

struct otp_flash;
struct otp_flash *tevi_ar0144_otp_flash_init(struct i2c_client *client);
u16 tevi_ar0144_otp_flash_get_checksum(struct otp_flash *instance);
size_t tevi_ar0144_otp_flash_read(struct otp_flash *instance, u8 *data, int addr, size_t len);
size_t tevi_ar0144_otp_flash_get_pll_length(struct otp_flash *instance);
size_t tevi_ar0144_otp_flash_get_pll_section(struct otp_flash*instance, u8 *data);

#endif
