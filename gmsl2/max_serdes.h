// SPDX-License-Identifier: GPL-2.0
/*
*
*/

#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

#ifndef MAX_SERDES_H
#define MAX_SERDES_H

#define v4l2_async_nf_init v4l2_async_notifier_init
#define v4l2_async_nf_unregister v4l2_async_notifier_unregister
#define v4l2_async_nf_cleanup v4l2_async_notifier_cleanup
#define v4l2_async_subdev_nf_register v4l2_async_subdev_notifier_register
#define v4l2_async_nf_add_fwnode(notifier, fwnode, type)		\
	((type *)v4l2_async_notifier_add_fwnode_subdev(notifier, fwnode, sizeof(type)))

#define v4l2_mbus_config_mipi_csi2 v4l2_fwnode_bus_mipi_csi2

struct pingroup {
	const char *name;
	const unsigned int *pins;
	size_t npins;
};

#define PINCTRL_PINGROUP(_name, _pins, _npins)	\
(struct pingroup) {				\
	.name = _name,				\
	.pins = _pins,				\
	.npins = _npins,			\
}

struct pinfunction {
	const char *name;
	const char * const *groups;
	size_t ngroups;
};

#define PINCTRL_PINFUNCTION(_name, _groups, _ngroups)	\
(struct pinfunction) {					\
		.name = (_name),			\
		.groups = (_groups),			\
		.ngroups = (_ngroups),			\
	}

#define MAX_SERDES_STREAMS_NUM     4
#define MAX_SERDES_VC_ID_NUM	   4

struct max_i2c_xlate {
	u8 src;
	u8 dst;
	u8 id;
};

struct max_format {
	const char *name;
	u32 code;
	u8 dt;
	u8 bpp;
	bool dbl;
};

#define MAX_DT_FS			0x00
#define MAX_DT_FE			0x01
#define MAX_DT_EMB8			0x12
#define MAX_DT_YUV422_8B		0x1e
#define MAX_DT_YUV422_10B		0x1f
#define MAX_DT_RGB565			0x22
#define MAX_DT_RGB666			0x23
#define MAX_DT_RGB888			0x24
#define MAX_DT_RAW8			0x2a
#define MAX_DT_RAW10			0x2b
#define MAX_DT_RAW12			0x2c
#define MAX_DT_RAW14			0x2d
#define MAX_DT_RAW16			0x2e
#define MAX_DT_RAW20			0x2f

#define MAX_FMT(_code, _dt, _bpp, _dbl) 	\
{						\
	.name = __stringify(_code),		\
	.code = MEDIA_BUS_FMT_ ## _code,	\
	.dt = (_dt),				\
	.bpp = (_bpp),				\
	.dbl = (_dbl),				\
}

static const struct max_format max_formats[] = {
	MAX_FMT(FIXED, MAX_DT_EMB8, 8, 1),
	MAX_FMT(YUYV8_1X16, MAX_DT_YUV422_8B, 16, 0),
	MAX_FMT(UYVY8_1X16, MAX_DT_YUV422_8B, 16, 0),
	MAX_FMT(YUYV10_1X20, MAX_DT_YUV422_10B, 20, 0),
	MAX_FMT(RGB565_1X16, MAX_DT_RGB565, 16, 0),
	MAX_FMT(RGB666_1X18, MAX_DT_RGB666, 18, 0),
	MAX_FMT(RGB888_1X24, MAX_DT_RGB888, 24, 0),
	MAX_FMT(SBGGR8_1X8, MAX_DT_RAW8, 8, 1),
	MAX_FMT(SGBRG8_1X8, MAX_DT_RAW8, 8, 1),
	MAX_FMT(SGRBG8_1X8, MAX_DT_RAW8, 8, 1),
	MAX_FMT(SRGGB8_1X8, MAX_DT_RAW8, 8, 1),
	MAX_FMT(SBGGR10_1X10, MAX_DT_RAW10, 10, 1),
	MAX_FMT(SGBRG10_1X10, MAX_DT_RAW10, 10, 1),
	MAX_FMT(SGRBG10_1X10, MAX_DT_RAW10, 10, 1),
	MAX_FMT(SRGGB10_1X10, MAX_DT_RAW10, 10, 1),
	MAX_FMT(SBGGR12_1X12, MAX_DT_RAW12, 12, 1),
	MAX_FMT(SGBRG12_1X12, MAX_DT_RAW12, 12, 1),
	MAX_FMT(SGRBG12_1X12, MAX_DT_RAW12, 12, 1),
	MAX_FMT(SRGGB12_1X12, MAX_DT_RAW12, 12, 1),
	MAX_FMT(SBGGR14_1X14, MAX_DT_RAW14, 14, 0),
	MAX_FMT(SGBRG14_1X14, MAX_DT_RAW14, 14, 0),
	MAX_FMT(SGRBG14_1X14, MAX_DT_RAW14, 14, 0),
	MAX_FMT(SRGGB14_1X14, MAX_DT_RAW14, 14, 0),
	MAX_FMT(SBGGR16_1X16, MAX_DT_RAW16, 16, 0),
	MAX_FMT(SGBRG16_1X16, MAX_DT_RAW16, 16, 0),
	MAX_FMT(SGRBG16_1X16, MAX_DT_RAW16, 16, 0),
	MAX_FMT(SRGGB16_1X16, MAX_DT_RAW16, 16, 0),
};

static const struct max_format *max_format_by_code(u32 code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(max_formats); i++)
		if (max_formats[i].code == code)
			return &max_formats[i];

	return NULL;
}

static const struct max_format *max_format_by_dt(u8 dt)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(max_formats); i++)
		if (max_formats[i].dt == dt)
			return &max_formats[i];

	return NULL;
}

#endif // MAX_SERDES_H