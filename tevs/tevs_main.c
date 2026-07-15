#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/tegra-v4l2-camera.h>
#include <media/camera_common.h>
#include <media/v4l2-fwnode.h>
#include "tevs_tbls.h"

#define DRIVER_NAME "tevs"

/* Define host command register of TEVS information page */
#define HOST_COMMAND_TEVS_INFO_VERSION_MSB 						(0x3000)
#define HOST_COMMAND_TEVS_INFO_VERSION_LSB 						(0x3002)
#define HOST_COMMAND_TEVS_BOOT_STATE 							(0x3004)
#define HOST_COMMAND_TEVS_SENSOR_CHIP_ID						(0x3008)
#define HOST_COMMAND_TEVS_MODEL_NUMBER_0						(0x3020)
#define HOST_COMMAND_TEVS_MODEL_NUMBER_1						(0x3022)
#define HOST_COMMAND_TEVS_MODEL_NUMBER_2						(0x3024)

/* Define host command register of ISP control page */
#define HOST_COMMAND_ISP_CTRL_PREVIEW_WIDTH 					(0x3100)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_HEIGHT 					(0x3102)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_FORMAT 					(0x3104)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_SENSOR_MODE 				(0x3106)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_THROUGHPUT 				(0x3108)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_MAX_FPS 					(0x310A)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_UPPER_MSB 		(0x310C)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_UPPER_LSB 		(0x310E)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_MAX_MSB 			(0x3110)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_MAX_LSB 			(0x3112)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_HINF_CTRL 				(0x3114)
#define HOST_COMMAND_ISP_CTRL_AE_MODE 							(0x3116)
#define HOST_COMMAND_ISP_CTRL_EXP_TIME_MSB 						(0x3118)
#define HOST_COMMAND_ISP_CTRL_EXP_TIME_LSB 						(0x311A)
#define HOST_COMMAND_ISP_CTRL_EXP_TIME_MAX_MSB 					(0x311C)
#define HOST_COMMAND_ISP_CTRL_EXP_TIME_MAX_LSB 					(0x311E)
#define HOST_COMMAND_ISP_CTRL_EXP_TIME_MIN_MSB 					(0x3120)
#define HOST_COMMAND_ISP_CTRL_EXP_TIME_MIN_LSB 					(0x3122)
#define HOST_COMMAND_ISP_CTRL_EXP_GAIN						 	(0x3124)
#define HOST_COMMAND_ISP_CTRL_EXP_GAIN_MAX 						(0x3126)
#define HOST_COMMAND_ISP_CTRL_EXP_GAIN_MIN 						(0x3128)
#define HOST_COMMAND_ISP_CTRL_CURRENT_EXP_TIME_MSB 				(0x312A)
#define HOST_COMMAND_ISP_CTRL_CURRENT_EXP_TIME_LSB 				(0x312C)
#define HOST_COMMAND_ISP_CTRL_CURRENT_EXP_GAIN 					(0x312E)
#define HOST_COMMAND_ISP_CTRL_BACKLIGHT_COMPENSATION 			(0x3130)
#define HOST_COMMAND_ISP_CTRL_BACKLIGHT_COMPENSATION_MAX 		(0x3132)
#define HOST_COMMAND_ISP_CTRL_BACKLIGHT_COMPENSATION_MIN 		(0x3134)
#define HOST_COMMAND_ISP_CTRL_AWB_MODE 							(0x3136)
#define HOST_COMMAND_ISP_CTRL_AWB_TEMP 							(0x3138)
#define HOST_COMMAND_ISP_CTRL_AWB_TEMP_MAX 						(0x313A)
#define HOST_COMMAND_ISP_CTRL_AWB_TEMP_MIN 						(0x313C)
#define HOST_COMMAND_ISP_CTRL_BRIGHTNESS 						(0x313E)
#define HOST_COMMAND_ISP_CTRL_BRIGHTNESS_MAX 					(0x3140)
#define HOST_COMMAND_ISP_CTRL_BRIGHTNESS_MIN 					(0x3142)
#define HOST_COMMAND_ISP_CTRL_CONTRAST 							(0x3144)
#define HOST_COMMAND_ISP_CTRL_CONTRAST_MAX 						(0x3146)
#define HOST_COMMAND_ISP_CTRL_CONTRAST_MIN 						(0x3148)
#define HOST_COMMAND_ISP_CTRL_SATURATION 						(0x314A)
#define HOST_COMMAND_ISP_CTRL_SATURATION_MAX 					(0x314C)
#define HOST_COMMAND_ISP_CTRL_SATURATION_MIN 					(0x314E)
#define HOST_COMMAND_ISP_CTRL_GAMMA 							(0x3150)
#define HOST_COMMAND_ISP_CTRL_GAMMA_MAX 						(0x3152)
#define HOST_COMMAND_ISP_CTRL_GAMMA_MIN 						(0x3154)
#define HOST_COMMAND_ISP_CTRL_DENOISE 							(0x3156)
#define HOST_COMMAND_ISP_CTRL_DENOISE_MAX 						(0x3158)
#define HOST_COMMAND_ISP_CTRL_DENOISE_MIN 						(0x315A)
#define HOST_COMMAND_ISP_CTRL_SHARPEN 							(0x315C)
#define HOST_COMMAND_ISP_CTRL_SHARPEN_MAX 						(0x315E)
#define HOST_COMMAND_ISP_CTRL_SHARPEN_MIN 						(0x3160)
#define HOST_COMMAND_ISP_CTRL_FLIP 								(0x3162)
#define HOST_COMMAND_ISP_CTRL_EFFECT 							(0x3164)
#define HOST_COMMAND_ISP_CTRL_ZOOM_TYPE 						(0x3166)
#define HOST_COMMAND_ISP_CTRL_ZOOM_TIMES 						(0x3168)
#define HOST_COMMAND_ISP_CTRL_ZOOM_TIMES_MAX 					(0x316A)
#define HOST_COMMAND_ISP_CTRL_ZOOM_TIMES_MIN 					(0x316C)
#define HOST_COMMAND_ISP_CTRL_CT_X 								(0x316E)
#define HOST_COMMAND_ISP_CTRL_CT_Y 								(0x3170)
#define HOST_COMMAND_ISP_CTRL_CT_MAX 							(0x3172)
#define HOST_COMMAND_ISP_CTRL_CT_MIN 							(0x3174)
#define HOST_COMMAND_ISP_CTRL_SYSTEM_START 						(0x3176)
#define HOST_COMMAND_ISP_CTRL_ISP_RESET 						(0x3178)
#define HOST_COMMAND_ISP_CTRL_TRIGGER_MODE 						(0x317A)
#define HOST_COMMAND_ISP_CTRL_FLICK_CTRL					 	(0x317C)
#define HOST_COMMAND_ISP_CTRL_MIPI_FREQ 						(0x317E)
#define HOST_COMMAND_ISP_CTRL_JPEG_QUAL							(0x3180)
#define HOST_COMMAND_ISP_CTRL_PREVIEW_MIPI_CTRL 				(0x3182)

/* Define host command register of ISP bootdata page */
#define HOST_COMMAND_ISP_BOOTDATA_1								(0x4000)
#define HOST_COMMAND_ISP_BOOTDATA_2								(0x4002)
#define HOST_COMMAND_ISP_BOOTDATA_3								(0x4004)
#define HOST_COMMAND_ISP_BOOTDATA_4								(0x4006)
#define HOST_COMMAND_ISP_BOOTDATA_5								(0x4008)
#define HOST_COMMAND_ISP_BOOTDATA_6								(0x400A)
#define HOST_COMMAND_ISP_BOOTDATA_7								(0x400C)
#define HOST_COMMAND_ISP_BOOTDATA_8								(0x400E)
#define HOST_COMMAND_ISP_BOOTDATA_9								(0x4010)
#define HOST_COMMAND_ISP_BOOTDATA_10							(0x4012)
#define HOST_COMMAND_ISP_BOOTDATA_11							(0x4014)
#define HOST_COMMAND_ISP_BOOTDATA_12							(0x4016)
#define HOST_COMMAND_ISP_BOOTDATA_13							(0x4018)
#define HOST_COMMAND_ISP_BOOTDATA_14							(0x401A)
#define HOST_COMMAND_ISP_BOOTDATA_15							(0x401C)
#define HOST_COMMAND_ISP_BOOTDATA_16							(0x401E)
#define HOST_COMMAND_ISP_BOOTDATA_17							(0x4020)
#define HOST_COMMAND_ISP_BOOTDATA_18							(0x4022)
#define HOST_COMMAND_ISP_BOOTDATA_19							(0x4024)
#define HOST_COMMAND_ISP_BOOTDATA_20							(0x4026)
#define HOST_COMMAND_ISP_BOOTDATA_21							(0x4028)
#define HOST_COMMAND_ISP_BOOTDATA_22							(0x402A)
#define HOST_COMMAND_ISP_BOOTDATA_23							(0x402C)
#define HOST_COMMAND_ISP_BOOTDATA_24							(0x402E)
#define HOST_COMMAND_ISP_BOOTDATA_25							(0x4030)
#define HOST_COMMAND_ISP_BOOTDATA_26							(0x4032)
#define HOST_COMMAND_ISP_BOOTDATA_27							(0x4034)
#define HOST_COMMAND_ISP_BOOTDATA_28							(0x4036)
#define HOST_COMMAND_ISP_BOOTDATA_29							(0x4038)
#define HOST_COMMAND_ISP_BOOTDATA_30							(0x403A)
#define HOST_COMMAND_ISP_BOOTDATA_31							(0x403C)
#define HOST_COMMAND_ISP_BOOTDATA_32							(0x403E)
#define HOST_COMMAND_ISP_BOOTDATA_33							(0x4040)
#define HOST_COMMAND_ISP_BOOTDATA_34							(0x4042)
#define HOST_COMMAND_ISP_BOOTDATA_35							(0x4044)
#define HOST_COMMAND_ISP_BOOTDATA_36							(0x4046)
#define HOST_COMMAND_ISP_BOOTDATA_37							(0x4048)
#define HOST_COMMAND_ISP_BOOTDATA_38							(0x404A)
#define HOST_COMMAND_ISP_BOOTDATA_39							(0x404C)
#define HOST_COMMAND_ISP_BOOTDATA_40							(0x404E)
#define HOST_COMMAND_ISP_BOOTDATA_41							(0x4050)
#define HOST_COMMAND_ISP_BOOTDATA_42							(0x4052)
#define HOST_COMMAND_ISP_BOOTDATA_43							(0x4054)
#define HOST_COMMAND_ISP_BOOTDATA_44							(0x4056)
#define HOST_COMMAND_ISP_BOOTDATA_45							(0x4058)
#define HOST_COMMAND_ISP_BOOTDATA_46							(0x405A)
#define HOST_COMMAND_ISP_BOOTDATA_47							(0x405C)
#define HOST_COMMAND_ISP_BOOTDATA_48							(0x405E)
#define HOST_COMMAND_ISP_BOOTDATA_49							(0x4060)
#define HOST_COMMAND_ISP_BOOTDATA_50							(0x4062)
#define HOST_COMMAND_ISP_BOOTDATA_51							(0x4064)
#define HOST_COMMAND_ISP_BOOTDATA_52							(0x4066)
#define HOST_COMMAND_ISP_BOOTDATA_53							(0x4068)
#define HOST_COMMAND_ISP_BOOTDATA_54							(0x406A)
#define HOST_COMMAND_ISP_BOOTDATA_55							(0x406C)
#define HOST_COMMAND_ISP_BOOTDATA_56							(0x406E)
#define HOST_COMMAND_ISP_BOOTDATA_57							(0x4070)
#define HOST_COMMAND_ISP_BOOTDATA_58							(0x4072)
#define HOST_COMMAND_ISP_BOOTDATA_59							(0x4074)
#define HOST_COMMAND_ISP_BOOTDATA_60							(0x4076)
#define HOST_COMMAND_ISP_BOOTDATA_61							(0x4078)
#define HOST_COMMAND_ISP_BOOTDATA_62							(0x407A)
#define HOST_COMMAND_ISP_BOOTDATA_63							(0x407C)

/* Define special method for controlling ISP with I2C */
#define HOST_COMMAND_ISP_CTRL_I2C_ADDR							(0xF000)
#define HOST_COMMAND_ISP_CTRL_I2C_DATA							(0xF002)

#define TEVS_BRIGHTNESS 						HOST_COMMAND_ISP_CTRL_BRIGHTNESS
#define TEVS_BRIGHTNESS_MAX 					HOST_COMMAND_ISP_CTRL_BRIGHTNESS_MAX
#define TEVS_BRIGHTNESS_MIN 					HOST_COMMAND_ISP_CTRL_BRIGHTNESS_MIN
#define TEVS_BRIGHTNESS_MASK 					(0xFFFF)
#define TEVS_CONTRAST 							HOST_COMMAND_ISP_CTRL_CONTRAST
#define TEVS_CONTRAST_MAX 						HOST_COMMAND_ISP_CTRL_CONTRAST_MAX
#define TEVS_CONTRAST_MIN 						HOST_COMMAND_ISP_CTRL_CONTRAST_MIN
#define TEVS_CONTRAST_MASK 						(0xFFFF)
#define TEVS_SATURATION 						HOST_COMMAND_ISP_CTRL_SATURATION
#define TEVS_SATURATION_MAX 					HOST_COMMAND_ISP_CTRL_SATURATION_MAX
#define TEVS_SATURATION_MIN 					HOST_COMMAND_ISP_CTRL_SATURATION_MIN
#define TEVS_SATURATION_MASK 					(0xFFFF)
#define TEVS_AWB_CTRL_MODE 						HOST_COMMAND_ISP_CTRL_AWB_MODE
#define TEVS_AWB_CTRL_MODE_MASK 				(0x00FF)
#define TEVS_AWB_CTRL_MODE_MANUAL_TEMP 			(7U << 0)
#define TEVS_AWB_CTRL_MODE_AUTO 				(15U << 0)
#define TEVS_AWB_CTRL_MODE_MANUAL_TEMP_IDX 		(0U << 0)
#define TEVS_AWB_CTRL_MODE_AUTO_IDX 			(1U << 0)
#define TEVS_GAMMA 								HOST_COMMAND_ISP_CTRL_GAMMA
#define TEVS_GAMMA_MAX 							HOST_COMMAND_ISP_CTRL_GAMMA_MAX
#define TEVS_GAMMA_MIN 							HOST_COMMAND_ISP_CTRL_GAMMA_MIN
#define TEVS_GAMMA_MASK 						(0xFFFF)
#define TEVS_AE_AUTO_EXP_TIME_UPPER				HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_UPPER_MSB
#define TEVS_AE_AUTO_EXP_TIME_MAX				HOST_COMMAND_ISP_CTRL_PREVIEW_EXP_TIME_MAX_MSB
#define TEVS_AE_AUTO_EXP_TIME_MASK				(0xFFFFFFFF)
#define TEVS_AE_MANUAL_EXP_TIME 				HOST_COMMAND_ISP_CTRL_EXP_TIME_MSB
#define TEVS_AE_MANUAL_EXP_TIME_MAX 			HOST_COMMAND_ISP_CTRL_EXP_TIME_MAX_MSB
#define TEVS_AE_MANUAL_EXP_TIME_MIN 			HOST_COMMAND_ISP_CTRL_EXP_TIME_MIN_MSB
#define TEVS_AE_MANUAL_EXP_TIME_MASK 			(0xFFFFFFFF)
#define TEVS_AE_MANUAL_GAIN 					HOST_COMMAND_ISP_CTRL_EXP_GAIN
#define TEVS_AE_MANUAL_GAIN_MAX 				HOST_COMMAND_ISP_CTRL_EXP_GAIN_MAX
#define TEVS_AE_MANUAL_GAIN_MIN 				HOST_COMMAND_ISP_CTRL_EXP_GAIN_MIN
#define TEVS_AE_MANUAL_GAIN_MASK 				(0x00FF)
#define TEVS_ORIENTATION 						HOST_COMMAND_ISP_CTRL_FLIP
#define TEVS_ORIENTATION_HFLIP_BIT 				(0U)
#define TEVS_ORIENTATION_HFLIP 					BIT(TEVS_ORIENTATION_HFLIP_BIT)
#define TEVS_ORIENTATION_VFLIP_BIT 				(1U)
#define TEVS_ORIENTATION_VFLIP 					BIT(TEVS_ORIENTATION_VFLIP_BIT)
#define TEVS_FLICK_CTRL    						HOST_COMMAND_ISP_CTRL_FLICK_CTRL
#define TEVS_FLICK_CTRL_MASK					(0xFFFF) // TEVS_REG_16BIT(0x5440)
#define TEVS_FLICK_CTRL_FREQ(n)					((n) << 8)
#define TEVS_FLICK_CTRL_ETC_IHDR_UP				BIT(6)
#define TEVS_FLICK_CTRL_ETC_DIS					BIT(5)
#define TEVS_FLICK_CTRL_FRC_OVERRIDE_MAX_ET		BIT(4)
#define TEVS_FLICK_CTRL_FRC_OVERRIDE_UPPER_ET	BIT(3)
#define TEVS_FLICK_CTRL_FRC_EN					BIT(2)
#define TEVS_FLICK_CTRL_MODE_MASK				(3U << 0)
#define TEVS_FLICK_CTRL_MODE_DISABLED			(0U << 0)
#define TEVS_FLICK_CTRL_MODE_MANUAL				(1U << 0)
#define TEVS_FLICK_CTRL_MODE_AUTO				(2U << 0)
#define TEVS_FLICK_CTRL_FREQ_MASK				(0xFF00)
#define TEVS_FLICK_CTRL_MODE_50HZ				(TEVS_FLICK_CTRL_FREQ(50) | TEVS_FLICK_CTRL_MODE_MANUAL)
#define TEVS_FLICK_CTRL_MODE_60HZ				(TEVS_FLICK_CTRL_FREQ(60) | TEVS_FLICK_CTRL_MODE_MANUAL)
#define TEVS_AWB_MANUAL_TEMP 					HOST_COMMAND_ISP_CTRL_AWB_TEMP
#define TEVS_AWB_MANUAL_TEMP_MAX 				HOST_COMMAND_ISP_CTRL_AWB_TEMP_MAX
#define TEVS_AWB_MANUAL_TEMP_MIN 				HOST_COMMAND_ISP_CTRL_AWB_TEMP_MIN
#define TEVS_AWB_MANUAL_TEMP_MASK 				(0xFFFF)
#define TEVS_SHARPEN 							HOST_COMMAND_ISP_CTRL_SHARPEN
#define TEVS_SHARPEN_MAX 						HOST_COMMAND_ISP_CTRL_SHARPEN_MAX
#define TEVS_SHARPEN_MIN 						HOST_COMMAND_ISP_CTRL_SHARPEN_MIN
#define TEVS_SHARPEN_MASK 						(0xFFFF)
#define TEVS_BACKLIGHT_COMPENSATION 			HOST_COMMAND_ISP_CTRL_BACKLIGHT_COMPENSATION
#define TEVS_BACKLIGHT_COMPENSATION_MAX 		HOST_COMMAND_ISP_CTRL_BACKLIGHT_COMPENSATION_MAX
#define TEVS_BACKLIGHT_COMPENSATION_MIN 		HOST_COMMAND_ISP_CTRL_BACKLIGHT_COMPENSATION_MIN
#define TEVS_BACKLIGHT_COMPENSATION_MASK 		(0xFFFF)
#define TEVS_DZ_TGT_FCT 						HOST_COMMAND_ISP_CTRL_ZOOM_TIMES
#define TEVS_DZ_TGT_FCT_MAX 					HOST_COMMAND_ISP_CTRL_ZOOM_TIMES_MAX
#define TEVS_DZ_TGT_FCT_MIN 					HOST_COMMAND_ISP_CTRL_ZOOM_TIMES_MIN
#define TEVS_DZ_TGT_FCT_MASK 					(0xFFFF)
#define TEVS_SFX_MODE 							HOST_COMMAND_ISP_CTRL_EFFECT
#define TEVS_SFX_MODE_SFX_MASK 					(0x00FF)
#define TEVS_SFX_MODE_SFX_NORMAL 				(0U << 0)
#define TEVS_SFX_MODE_SFX_BW 					(3U << 0)
#define TEVS_SFX_MODE_SFX_GRAYSCALE 			(6U << 0)
#define TEVS_SFX_MODE_SFX_NEGATIVE 				(7U << 0)
#define TEVS_SFX_MODE_SFX_SKETCH 				(15U << 0)
#define TEVS_SFX_MODE_SFX_NORMAL_IDX 			(0U << 0)
#define TEVS_SFX_MODE_SFX_BW_IDX 				(1U << 0)
#define TEVS_SFX_MODE_SFX_GRAYSCALE_IDX 		(2U << 0)
#define TEVS_SFX_MODE_SFX_NEGATIVE_IDX 			(3U << 0)
#define TEVS_SFX_MODE_SFX_SKETCH_IDX 			(4U << 0)
#define TEVS_AE_CTRL_MODE 						HOST_COMMAND_ISP_CTRL_AE_MODE
#define TEVS_AE_CTRL_MODE_MASK 					(0x00FF)
#define TEVS_AE_CTRL_MANUAL_EXP_TIME_GAIN 		(0U << 0)
#define TEVS_AE_CTRL_AUTO_GAIN 					(9U << 0)
#define TEVS_AE_CTRL_FULL_AUTO 					(12U << 0)
#define TEVS_AE_CTRL_MANUAL_EXP_TIME_GAIN_IDX 	(0U << 0)
#define TEVS_AE_CTRL_FULL_AUTO_IDX 				(1U << 0)
#define TEVS_AE_CTRL_AUTO_GAIN_IDX				(2U << 0)
#define TEVS_DZ_CT_X 							HOST_COMMAND_ISP_CTRL_CT_X
#define TEVS_DZ_CT_Y 							HOST_COMMAND_ISP_CTRL_CT_Y
#define TEVS_DZ_CT_MASK 						(0xFFFF)
#define TEVS_DZ_CT_MAX 							HOST_COMMAND_ISP_CTRL_CT_MAX
#define TEVS_DZ_CT_MIN 							HOST_COMMAND_ISP_CTRL_CT_MIN
#define TEVS_BSL_MODE_NORMAL_IDX				(0U << 0)
#define TEVS_BSL_MODE_FLASH_IDX 				(1U << 0)
#define TEVS_MAX_FPS							HOST_COMMAND_ISP_CTRL_PREVIEW_MAX_FPS
#define TEVS_MAX_FPS_MASK 						(0x00FF)
#define TEVS_DENOISE							HOST_COMMAND_ISP_CTRL_DENOISE
#define TEVS_DENOISE_MAX 						HOST_COMMAND_ISP_CTRL_DENOISE_MAX
#define TEVS_DENOISE_MIN 						HOST_COMMAND_ISP_CTRL_DENOISE_MIN
#define TEVS_DENOISE_MASK 						(0xFFFF)
#define TEVS_TRIGGER_MODE						HOST_COMMAND_ISP_CTRL_TRIGGER_MODE
#define TEVS_TRIGGER_MODE_MASK		 			(0x0003)
#define TEVS_TRIGGER_MODE_DISABLE				(0U << 0)
#define TEVS_TRIGGER_MODE_SYNC					(1U << 0)
#define TEVS_TRIGGER_MODE_PERIODIC				(2U << 0)
#define TEVS_TRIGGER_MODE_NON_PERIODIC			(3U << 0)
#define TEVS_TRIGGER_MODE_DISABLE_IDX			(0U << 0)
#define TEVS_TRIGGER_MODE_SYNC_IDX				(1U << 0)
#define TEVS_TRIGGER_MODE_PERIODIC_IDX			(2U << 0)
#define TEVS_TRIGGER_MODE_NON_PERIODIC_IDX		(3U << 0)

#define V4L2_CID_USER_TEVS_BASE				(V4L2_CID_USER_BASE + 0x2000)
#define V4L2_CID_TEVS_BSL_MODE				(V4L2_CID_USER_TEVS_BASE + 0)
#define V4L2_CID_TEVS_MAX_FPS				(V4L2_CID_USER_TEVS_BASE + 1)
#define V4L2_CID_TEVS_DENOISE				(V4L2_CID_USER_TEVS_BASE + 2)
#define V4L2_CID_TEVS_AE_EXP_TIME_UPPER		(V4L2_CID_USER_TEVS_BASE + 3)
#define V4L2_CID_TEVS_AE_EXP_TIME_MAX		(V4L2_CID_USER_TEVS_BASE + 4)
#define V4L2_CID_TEVS_TRIGGER_MODE			(V4L2_CID_USER_TEVS_BASE + 5)

#define DEFAULT_HEADER_VERSION 				3
#define TEVS_BOOT_TIME						(250)
#define TOTAL_MICROSEC_PERSEC				(1000000)

#define TEVS_IMG_FORMAT_UYVY				(0x50)
#define TEVS_IMG_FORMAT_Y8				    (0x52)
#define TEVS_IMG_FORMAT_RAW8_BYPASS_ISP		(0x80)
#define TEVS_IMG_FORMAT_RAW8				(0x8A)

#define TEVS_LINK_FREQUENCY_DEFAULT			400000000ull
#define TEVS_PIXEL_RATE_DEFAULT				200000000ull

#define TEVS_CONTINUOUS_CLOCK_DEFAULT 		(0)

struct header_info {
	u8 header_version;
	u16 content_offset;
	u16 sensor_type;
	u8 sensor_fuseid[16];
	u8 product_name[64];
	u8 lens_id[16];
	u16 fix_checksum;
	u8 tn_fw_version[2];
	u16 vendor_fw_version;
	u16 custom_number;
	u8 build_year;
	u8 build_month;
	u8 build_day;
	u8 build_hour;
	u8 build_minute;
	u8 build_second;
	u16 mipi_datarate;
	u32 content_len;
	u16 content_checksum;
	u16 total_checksum;
} __attribute__((packed));

struct tevs {
	struct device *dev;
	struct v4l2_subdev *v4l2_subdev;
	struct camera_common_data	*s_data;
	struct tegracam_device		*tc_dev;
	struct regmap *regmap;
	struct header_info *header_info;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *standby_gpio;

	struct v4l2_subdev_ops subdev_ops;
	struct v4l2_subdev_video_ops video_ops;

	u16 chip_id;
	int data_lanes;
	int continuous_clock;
	int data_frequency;
	u8 selected_mode;
	u8 selected_sensor;
	bool hw_reset_mode;
	int trigger_mode;
	char *sensor_name;
	int vc_id;
	unsigned int fps;

	struct mutex lock; /* Protects formats */
	/* V4L2 Controls */
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *brightness;
	struct v4l2_ctrl *contrast;
	struct v4l2_ctrl *saturation;
	struct v4l2_ctrl *awb;
	struct v4l2_ctrl *gamma;
	struct v4l2_ctrl *exp_time;
	struct v4l2_ctrl *exp_gain;
	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vflip;
	struct v4l2_ctrl *flick;
	struct v4l2_ctrl *wb_temp;
	struct v4l2_ctrl *sharpness;
	struct v4l2_ctrl *backlight_comp;
	struct v4l2_ctrl *colorfx;
	struct v4l2_ctrl *ae;
	struct v4l2_ctrl *pan;
	struct v4l2_ctrl *tilt;
	struct v4l2_ctrl *zoom;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *bsl;
	struct v4l2_ctrl *max_fps;
	struct v4l2_ctrl *denoise;
	struct v4l2_ctrl *ae_exp_upper;
	struct v4l2_ctrl *ae_exp_max;
	struct v4l2_ctrl *trigger;
};

static const struct regmap_config tevs_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

// #define __ENABLE_NV_CTRL
#ifdef __ENABLE_NV_CTRL
static const u32 ctrl_cid_list[] = {
	TEGRA_CAMERA_CID_SENSOR_MODE_ID,
};

static int tevs_set_group_hold(struct tegracam_device *tc_dev, bool val)
{
	/* TEVS does not support group hold */
	return 0;
}

static struct tegracam_ctrl_ops tevs_nv_ctrl_ops = {
	.numctrls = ARRAY_SIZE(ctrl_cid_list),
	.ctrl_cid_list = ctrl_cid_list,
	.set_group_hold = tevs_set_group_hold,
};
#endif

static struct tevs* _to_tevs_priv(struct v4l2_ctrl *ctrl)
{
	struct tegracam_ctrl_handler *ctrl_hdl =
			container_of(ctrl->handler, struct tegracam_ctrl_handler, ctrl_handler);

	return (struct tevs*)ctrl_hdl->tc_dev->priv;
}

static int tevs_i2c_read(struct tevs *tevs, u16 reg, u8 *val, u16 size)
{
	int ret;

	ret = regmap_bulk_read(tevs->regmap, reg, val, size);
	if (ret < 0) {
		dev_err(tevs->dev, "Failed to read from register: ret=%d, reg=0x%x\n", ret, reg);
		return ret;
	}

	return 0;
}

static int tevs_i2c_read_16b(struct tevs *tevs, u16 reg, u16 *value)
{
	u8 v[2] = { 0 };
	int ret;

	ret = tevs_i2c_read(tevs, reg, v, 2);
	if (ret < 0) {
		dev_err(tevs->dev,
			"Failed to read from register: ret=%d, reg=0x%x\n", ret, reg);
		return ret;
	}

	*value = (v[0] << 8) | v[1];
	dev_dbg(tevs->dev,
		"%s() read reg 0x%x, value 0x%x\n",
		__func__, reg, *value);

	return 0;
}

static int tevs_i2c_write(struct tevs *tevs, u16 reg, u8 *val, u16 size)
{
	int ret;

	ret = regmap_bulk_write(tevs->regmap, reg, val, size);
	if (ret < 0) {
		dev_err(tevs->dev, "Failed to write to register: ret=%d reg=0x%x\n", ret, reg);
		return ret;
	}

	return 0;
}

static int tevs_i2c_write_16b(struct tevs *tevs, u16 reg, u16 val)
{
	int ret;
	u8 data[2];
	data[0] = val >> 8;
	data[1] = val & 0xFF;

	ret = regmap_bulk_write(tevs->regmap, reg, data, 2);
	if (ret < 0) {
		dev_err(tevs->dev,
			"Failed to write to register: ret=%d reg=0x%x\n", ret, reg);
		return ret;
	}
	dev_dbg(tevs->dev,
		"%s() write reg 0x%x, value 0x%x\n",
		__func__, reg, val);

	return 0;
}

static int tevs_check_trigger_mode(struct tevs *tevs)
{
	u16 val;
	dev_dbg(tevs->dev, "%s()\n", __func__);

	tevs_i2c_read_16b(tevs, TEVS_TRIGGER_MODE, &val);
	if ((val & TEVS_TRIGGER_MODE_MASK) == TEVS_TRIGGER_MODE_DISABLE)
		return 0;
	else
		return 1;
}

static int tevs_check_version(struct tevs *tevs)
{
	struct device *dev = tevs->dev;
	u8 version[4] = { 0 };
	int ret = 0;

	ret = tevs_i2c_read(tevs, HOST_COMMAND_TEVS_INFO_VERSION_MSB, &version[0], 4);
	if (ret < 0) {
		dev_err(dev, "can't check version\n");
		return ret;
	}
	dev_info(
		dev,
		"Version:%d.%d.%d.%d\n",
		version[0], version[1],
		version[2], version[3]);

	return 0;
}

static int tevs_load_header_info(struct tevs *tevs)
{
	struct device *dev = tevs->dev;
	struct header_info *header = tevs->header_info;
	u8 header_ver;
	int ret = 0;

	ret = tevs_i2c_read(tevs, HOST_COMMAND_ISP_BOOTDATA_1, &header_ver, 1);

	if (ret < 0) {
		dev_err(dev, "can't recognize header info\n");
		return ret;
	}

	if (header_ver == DEFAULT_HEADER_VERSION) {
		tevs_i2c_read(tevs, HOST_COMMAND_ISP_BOOTDATA_1,
				(u8*)header,
				sizeof(struct header_info));

		dev_info(
			dev,
			"Product:%s, HeaderVer:%d, MIPI_Rate:%d\n",
			header->product_name, header->header_version,
			header->mipi_datarate);

		dev_dbg(dev, "content checksum: %x, content length: %d\n",
			header->content_checksum, header->content_len);

		return 0;
	} else {
		dev_err(dev, "can't recognize header version number '0x%X'\n",
			header_ver);
		return -EINVAL;
	}
}

static int tevs_get_chip_id(struct tevs *tevs)
{
	struct device *dev = tevs->dev;
    u16 val;
    int ret = tevs_i2c_read_16b(tevs, HOST_COMMAND_TEVS_SENSOR_CHIP_ID, &val);

    if (ret < 0) {
        dev_err(dev, "Can't get chip ID. ret = %d.\n", ret);
		return ret;
	}

    tevs->chip_id = val;
    dev_info(dev, "Chip ID: 0x%.4X\n", tevs->chip_id);
	return 0;
}

static int tevs_standby(struct tevs *tevs, int enable)
{
	u16 v = 0xFFFF;
	int timeout = 0;
	dev_dbg(tevs->dev, "%s():enable=%d\n", __func__, enable);

	if (enable == 1) {
		tevs_i2c_write_16b(tevs, HOST_COMMAND_ISP_CTRL_SYSTEM_START,
				     0x0000);
		while (timeout < 100) {
			tevs_i2c_read_16b(
				tevs, HOST_COMMAND_ISP_CTRL_SYSTEM_START, &v);
			if ((v & 0xFF00) == 0x0000)
				break;
			if (++timeout >= 100) {
				dev_err(tevs->dev, "timeout: line[%d]v=%x\n",
					__LINE__, v);
				return -EINVAL;
			}
			usleep_range(90000, 100000);
		}
		dev_dbg(tevs->dev, "sensor standby\n");
	} else {
		tevs_i2c_write_16b(tevs, HOST_COMMAND_ISP_CTRL_SYSTEM_START,
				     0x0001);
		while (timeout < 100) {
			tevs_i2c_read_16b(
				tevs, HOST_COMMAND_ISP_CTRL_SYSTEM_START, &v);
			if ((v & 0xFF00) == 0x0100)
				break;
			if (++timeout >= 100) {
				dev_err(tevs->dev, "timeout: line[%d]v=%x\n",
					__LINE__, v);
				return -EINVAL;
			}
			usleep_range(9000, 10000);
		}
		dev_dbg(tevs->dev, "sensor wakeup\n");
	}

	return 0;
}

static int tevs_check_boot_state(struct tevs *tevs)
{
	u16 boot_state;
	u8 timeout = 0;
	int ret = 0;

	while (timeout < 20) {
		tevs_i2c_read_16b(tevs,
				HOST_COMMAND_TEVS_BOOT_STATE, &boot_state);
		if (boot_state == 0x08)
			break;
		dev_dbg(tevs->dev, "tevs bootup state: %d\n", boot_state);
		if (++timeout >= 20) {
			dev_err(tevs->dev, "tevs bootup timeout: state: 0x%02X\n", boot_state);
			ret = -EINVAL;
		}
		msleep(50);
	}

	return ret;
}

/* -----------------------------------------------------------------------------
 * V4L2 Controls
 */

static const char *const awb_mode_strings[] = {
	"Manual Temp Mode", // TEVS_AWB_CTRL_MODE_MANUAL_TEMP
	"Auto Mode", // TEVS_AWB_CTRL_MODE_AUTO
	NULL
};

static const char *const flick_mode_strings[] = {
	"Disabled", "50 Hz", "60 Hz", "Auto", NULL
};

static const char *const sfx_mode_strings[] = {
	"Normal Mode", // TEVS_SFX_MODE_SFX_NORMAL
	"Black and White Mode", // TEVS_SFX_MODE_SFX_BW
	"Grayscale Mode", // TEVS_SFX_MODE_SFX_GRAYSCALE
	"Negative Mode", // TEVS_SFX_MODE_SFX_NEGATIVE
	"Sketch Mode", // TEVS_SFX_MODE_SFX_SKETCH
	NULL
};

static const char *const ae_mode_strings[] = {
	"Manual Mode", // TEVS_AE_CTRL_MANUAL_EXP_TIME_GAIN
	"Auto Mode", // TEVS_AE_CTRL_FULL_AUTO
	"AGC Mode", // TEVS_AE_CTRL_AUTO_GAIN
	NULL
};

static const char *const bsl_mode_strings[] = {
	"Normal Mode",
	"Bootstrap Mode",
	NULL
};

static const char *const trigger_mode_strings[] = {
	"Disabled",
	"Sync to Trigger Mode",
	"Periodic Trigger Mode",
	"Non Periodic Trigger Mode",
	NULL
};

static int tevs_set_brightness(struct tevs *tevs, s32 value)
{
	// Format is u3.12
	return tevs_i2c_write_16b(tevs, TEVS_BRIGHTNESS,
				  value & TEVS_BRIGHTNESS_MASK);
}

static int tevs_set_contrast(struct tevs *tevs, s32 value)
{
	// Format is u3.12
	return tevs_i2c_write_16b(tevs, TEVS_CONTRAST,
				  value & TEVS_CONTRAST_MASK);
}

static int tevs_set_saturation(struct tevs *tevs, s32 value)
{
	// Format is u3.12
	return tevs_i2c_write_16b(tevs, TEVS_SATURATION,
				  value & TEVS_SATURATION_MASK);
}

static int tevs_set_awb_mode(struct tevs *tevs, s32 mode)
{
	u16 val = mode & TEVS_AWB_CTRL_MODE_MASK;

	switch (val) {
	case TEVS_AWB_CTRL_MODE_MANUAL_TEMP_IDX:
		val = TEVS_AWB_CTRL_MODE_MANUAL_TEMP;
		break;
	case TEVS_AWB_CTRL_MODE_AUTO_IDX:
		val = TEVS_AWB_CTRL_MODE_AUTO;
		break;
	default:
		val = TEVS_AWB_CTRL_MODE_AUTO;
		break;
	}

	return tevs_i2c_write_16b(tevs, TEVS_AWB_CTRL_MODE, val);
}

static int tevs_set_gamma(struct tevs *tevs, s32 value)
{
	// Format is u3.12
	return tevs_i2c_write_16b(tevs, TEVS_GAMMA, value & TEVS_GAMMA_MASK);
}

static int tevs_set_exposure(struct tevs *tevs, s32 value)
{
	u8 val[4];
	__be32 temp;
	int ret;

	temp = cpu_to_be32(value);
	memcpy(val, &temp, 4);

	ret = tevs_i2c_write(tevs, TEVS_AE_MANUAL_EXP_TIME, val, 4);
	if (ret)
		return ret;

	return 0;
}

static int tevs_set_gain(struct tevs *tevs, s32 value)
{
	return tevs_i2c_write_16b(tevs, TEVS_AE_MANUAL_GAIN,
				  value & TEVS_AE_MANUAL_GAIN_MASK);
}

static int tevs_set_hflip(struct tevs *tevs, s32 flip)
{
	u16 val;
	int ret;

	ret = tevs_i2c_read_16b(tevs, TEVS_ORIENTATION, &val);
	if (ret)
		return ret;

	val &= ~TEVS_ORIENTATION_HFLIP;
	val |= flip ? TEVS_ORIENTATION_HFLIP : 0;

	return tevs_i2c_write_16b(tevs, TEVS_ORIENTATION, val);
}

static int tevs_set_vflip(struct tevs *tevs, s32 flip)
{
	u16 val;
	int ret;

	ret = tevs_i2c_read_16b(tevs, TEVS_ORIENTATION, &val);
	if (ret)
		return ret;

	val &= ~TEVS_ORIENTATION_VFLIP;
	val |= flip ? TEVS_ORIENTATION_VFLIP : 0;

	return tevs_i2c_write_16b(tevs, TEVS_ORIENTATION, val);
}

static int tevs_set_flick_mode(struct tevs *tevs, s32 mode)
{
	u16 val = 0;
	switch (mode) {
	case V4L2_CID_POWER_LINE_FREQUENCY_DISABLED:
		val = TEVS_FLICK_CTRL_MODE_DISABLED;
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY_50HZ:
		val = TEVS_FLICK_CTRL_MODE_50HZ;
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY_60HZ:
		val = TEVS_FLICK_CTRL_MODE_60HZ;
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY_AUTO:
		val = TEVS_FLICK_CTRL_MODE_AUTO |
		      TEVS_FLICK_CTRL_FRC_OVERRIDE_UPPER_ET |
		      TEVS_FLICK_CTRL_FRC_EN;
		break;
	default:
		val = TEVS_FLICK_CTRL_MODE_DISABLED;
		break;
	}

	return tevs_i2c_write_16b(tevs, TEVS_FLICK_CTRL, val);
}

static int tevs_set_awb_temp(struct tevs *tevs, s32 value)
{
	return tevs_i2c_write_16b(tevs, TEVS_AWB_MANUAL_TEMP,
				  value & TEVS_AWB_MANUAL_TEMP_MASK);
}

static int tevs_set_sharpen(struct tevs *tevs, s32 value)
{
	// Format is u3.12
	return tevs_i2c_write_16b(tevs, TEVS_SHARPEN,
				  value & TEVS_SHARPEN_MASK);
}

static int tevs_set_backlight_compensation(struct tevs *tevs, s32 value)
{
	// Format is u3.12
	return tevs_i2c_write_16b(tevs, TEVS_BACKLIGHT_COMPENSATION,
				  value & TEVS_BACKLIGHT_COMPENSATION_MASK);
}

static int tevs_set_special_effect(struct tevs *tevs, s32 mode)
{
	u16 val = mode & TEVS_SFX_MODE_SFX_MASK;

	switch (val) {
	case TEVS_SFX_MODE_SFX_NORMAL_IDX:
		val = TEVS_SFX_MODE_SFX_NORMAL;
		break;
	case TEVS_SFX_MODE_SFX_BW_IDX:
		val = TEVS_SFX_MODE_SFX_BW;
		break;
	case TEVS_SFX_MODE_SFX_GRAYSCALE_IDX:
		val = TEVS_SFX_MODE_SFX_GRAYSCALE;
		break;
	case TEVS_SFX_MODE_SFX_NEGATIVE_IDX:
		val = TEVS_SFX_MODE_SFX_NEGATIVE;
		break;
	case TEVS_SFX_MODE_SFX_SKETCH_IDX:
		val = TEVS_SFX_MODE_SFX_SKETCH;
		break;
	default:
		val = TEVS_SFX_MODE_SFX_NORMAL;
		break;
	}

	return tevs_i2c_write_16b(tevs, TEVS_SFX_MODE, val);
}

static int tevs_set_ae_mode(struct tevs *tevs, s32 mode)
{
	u16 val = mode & TEVS_AE_CTRL_MODE_MASK;
	u8 exp[4] = { 0 };
	int ret = 0;

	switch (val) {
	case TEVS_AE_CTRL_MANUAL_EXP_TIME_GAIN_IDX:
		val = TEVS_AE_CTRL_MANUAL_EXP_TIME_GAIN;
		break;
	case TEVS_AE_CTRL_FULL_AUTO_IDX:
		val = TEVS_AE_CTRL_FULL_AUTO;
		break;
	case TEVS_AE_CTRL_AUTO_GAIN_IDX:
		val = TEVS_AE_CTRL_AUTO_GAIN;
		break;
	default:
		val = TEVS_AE_CTRL_FULL_AUTO;
		break;
	}

	ret += tevs_i2c_write_16b(tevs, TEVS_AE_CTRL_MODE, val);
	ret += tevs_i2c_read(tevs, TEVS_AE_MANUAL_EXP_TIME, exp, 4);
	tevs->exp_time->cur.val = be32_to_cpup((__be32 *)exp) &
				  TEVS_AE_MANUAL_EXP_TIME_MASK;
	return ret;
}

static int tevs_set_pan_target(struct tevs *tevs, s32 value)
{
	// Format u7.8
	return tevs_i2c_write_16b(tevs, TEVS_DZ_CT_X, value & TEVS_DZ_CT_MASK);
}

static int tevs_set_tilt_target(struct tevs *tevs, s32 value)
{
	// Format u7.8
	return tevs_i2c_write_16b(tevs, TEVS_DZ_CT_Y, value & TEVS_DZ_CT_MASK);
}

static int tevs_set_zoom_target(struct tevs *tevs, s32 value)
{
	// Format u7.8
	return tevs_i2c_write_16b(tevs, TEVS_DZ_TGT_FCT,
				  value & TEVS_DZ_TGT_FCT_MASK);
}

static int tevs_set_bsl_mode(struct tevs *tevs, s32 mode)
{
	u16 val;
	u16 data_freq_tmp;
	dev_dbg(tevs->dev, "%s(): set bls mode: %d", __func__, mode);

	switch (mode) {
	case TEVS_BSL_MODE_NORMAL_IDX:
		gpiod_set_value_cansleep(tevs->reset_gpio, 0);
		usleep_range(9000, 10000);
		gpiod_set_value_cansleep(tevs->reset_gpio, 1);
		usleep_range(9000, 10000);

		msleep(TEVS_BOOT_TIME);

		if (tevs_check_boot_state(tevs) != 0) {
			dev_err(tevs->dev, "check tevs bootup status failed before change data frequency\n");
			return -EINVAL;
		}

		if (tevs->data_frequency != 0) {
			tevs_i2c_read_16b(tevs, HOST_COMMAND_ISP_CTRL_MIPI_FREQ,
					  &data_freq_tmp);
			if (tevs->data_frequency != data_freq_tmp) {
				tevs_i2c_write_16b(
					tevs, HOST_COMMAND_ISP_CTRL_MIPI_FREQ,
					tevs->data_frequency);
				msleep(TEVS_BOOT_TIME);
				if (tevs_check_boot_state(tevs) != 0) {
					dev_err(tevs->dev,
						"check tevs bootup status failed after change data frequency\n");
					return -EINVAL;
				}
			}
		}

		if (tevs->trigger_mode) {
			switch (tevs->trigger_mode) {
			case TEVS_TRIGGER_MODE_DISABLE_IDX:
				val = TEVS_TRIGGER_MODE_DISABLE;
				break;
			case TEVS_TRIGGER_MODE_SYNC_IDX:
				val = TEVS_TRIGGER_MODE_SYNC;
				break;
			case TEVS_TRIGGER_MODE_PERIODIC_IDX:
				val = TEVS_TRIGGER_MODE_PERIODIC;
				break;
			case TEVS_TRIGGER_MODE_NON_PERIODIC_IDX:
				val = TEVS_TRIGGER_MODE_NON_PERIODIC;
				break;
			default:
				val = TEVS_TRIGGER_MODE_DISABLE;
				break;
			}
			val |= 0x380;
			if (tevs_i2c_write_16b(tevs, TEVS_TRIGGER_MODE, val) != 0) {
				dev_err(tevs->dev, "set trigger mode failed\n");
				return -EINVAL;
			}
		}
		tevs_i2c_write_16b(tevs,
				HOST_COMMAND_ISP_CTRL_PREVIEW_MIPI_CTRL,
				tevs->s_data->sensor_props.sensor_modes->
					image_properties.pixel_format ==
					V4L2_PIX_FMT_GREY ?
					(0x2A << 2 | tevs->vc_id) :
					(0x3F << 2 | tevs->vc_id));
		tevs_i2c_write_16b(tevs,
				HOST_COMMAND_ISP_CTRL_PREVIEW_FORMAT,
				tevs->s_data->sensor_props.sensor_modes->
					image_properties.pixel_format == V4L2_PIX_FMT_GREY ?
				TEVS_IMG_FORMAT_Y8 : TEVS_IMG_FORMAT_UYVY);
		tevs_i2c_write_16b(tevs,
				HOST_COMMAND_ISP_CTRL_PREVIEW_HINF_CTRL,
				0x10 | (tevs->continuous_clock << 5) | (tevs->data_lanes));
		break;
	case TEVS_BSL_MODE_FLASH_IDX:
		gpiod_set_value_cansleep(tevs->reset_gpio, 0);
		usleep_range(9000, 10000);
		gpiod_set_value_cansleep(tevs->standby_gpio, 1);
		msleep(100);
		gpiod_set_value_cansleep(tevs->reset_gpio, 1);
		usleep_range(9000, 10000);
		gpiod_set_value_cansleep(tevs->standby_gpio, 0);
		msleep(100);
		break;
	default:
		dev_err(tevs->dev, "%s(): set err bls mode: %d", __func__,
			mode);
		break;
	}

	return 0;
}

static int tevs_set_max_fps(struct tevs *tevs, s32 value)
{
	u8 exp[4] = { 0 };
	int ret = 0;
	ret += tevs_i2c_write_16b(tevs, TEVS_MAX_FPS,
				  value & TEVS_MAX_FPS_MASK);
	ret += tevs_i2c_read(tevs, TEVS_AE_MANUAL_EXP_TIME, exp, 4);
	tevs->exp_time->cur.val = be32_to_cpup((__be32 *)exp) &
				  TEVS_AE_MANUAL_EXP_TIME_MASK;

	if (!ret)
		tevs->fps = value;

	return ret;
}

static int tevs_set_denoise(struct tevs *tevs, s32 value)
{
	// Format is u3.12
	return tevs_i2c_write_16b(tevs, TEVS_DENOISE, value & TEVS_DENOISE_MASK);
}

static int tevs_set_ae_auto_exp_upper(struct tevs *tevs, s32 value)
{
	u8 val[4];
	__be32 temp;

	temp = cpu_to_be32(value);
	memcpy(val, &temp, 4);

	return tevs_i2c_write(tevs, TEVS_AE_AUTO_EXP_TIME_UPPER, val, 4);
}

static int tevs_set_ae_auto_exp_max(struct tevs *tevs, s32 value)
{
	u8 val[4];
	__be32 temp;

	temp = cpu_to_be32(value);
	memcpy(val, &temp, 4);

	return tevs_i2c_write(tevs, TEVS_AE_AUTO_EXP_TIME_MAX, val, 4);
}

static int tevs_set_trigger_mode(struct tevs *tevs, s32 value)
{
	u16 val = value & TEVS_TRIGGER_MODE_MASK;

	switch (val) {
	case TEVS_TRIGGER_MODE_DISABLE_IDX:
		val = TEVS_TRIGGER_MODE_DISABLE;
		break;
	case TEVS_TRIGGER_MODE_SYNC_IDX:
		val = TEVS_TRIGGER_MODE_SYNC;
		break;
	case TEVS_TRIGGER_MODE_PERIODIC_IDX:
		val = TEVS_TRIGGER_MODE_PERIODIC;
		break;
	case TEVS_TRIGGER_MODE_NON_PERIODIC_IDX:
		val = TEVS_TRIGGER_MODE_NON_PERIODIC;
		break;
	default:
		val = TEVS_TRIGGER_MODE_DISABLE;
		break;
	}

	val |= 0x380;
	return tevs_i2c_write_16b(tevs, TEVS_TRIGGER_MODE, val);
}

static int tevs_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct tevs *tevs = _to_tevs_priv(ctrl);

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return tevs_set_brightness(tevs, ctrl->val);

	case V4L2_CID_CONTRAST:
		return tevs_set_contrast(tevs, ctrl->val);

	case V4L2_CID_SATURATION:
		return tevs_set_saturation(tevs, ctrl->val);

	case V4L2_CID_AUTO_WHITE_BALANCE:
		return tevs_set_awb_mode(tevs, ctrl->val);

	case V4L2_CID_GAMMA:
		return tevs_set_gamma(tevs, ctrl->val);

	case V4L2_CID_EXPOSURE:
		return tevs_set_exposure(tevs, ctrl->val);

	case V4L2_CID_GAIN:
		return tevs_set_gain(tevs, ctrl->val);

	case V4L2_CID_HFLIP:
		return tevs_set_hflip(tevs, ctrl->val);

	case V4L2_CID_VFLIP:
		return tevs_set_vflip(tevs, ctrl->val);

	case V4L2_CID_POWER_LINE_FREQUENCY:
		return tevs_set_flick_mode(tevs, ctrl->val);

	case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
		return tevs_set_awb_temp(tevs, ctrl->val);

	case V4L2_CID_SHARPNESS:
		return tevs_set_sharpen(tevs, ctrl->val);

	case V4L2_CID_BACKLIGHT_COMPENSATION:
		return tevs_set_backlight_compensation(tevs, ctrl->val);

	case V4L2_CID_COLORFX:
		return tevs_set_special_effect(tevs, ctrl->val);

	case V4L2_CID_EXPOSURE_AUTO:
		return tevs_set_ae_mode(tevs, ctrl->val);

	case V4L2_CID_PAN_ABSOLUTE:
		return tevs_set_pan_target(tevs, ctrl->val);

	case V4L2_CID_TILT_ABSOLUTE:
		return tevs_set_tilt_target(tevs, ctrl->val);

	case V4L2_CID_ZOOM_ABSOLUTE:
		return tevs_set_zoom_target(tevs, ctrl->val);

	case V4L2_CID_TEVS_BSL_MODE:
		return tevs_set_bsl_mode(tevs, ctrl->val);

	case V4L2_CID_TEVS_MAX_FPS:
		return tevs_set_max_fps(tevs, ctrl->val);

	case V4L2_CID_TEVS_DENOISE:
		return tevs_set_denoise(tevs, ctrl->val);

	case V4L2_CID_TEVS_AE_EXP_TIME_UPPER:
		return tevs_set_ae_auto_exp_upper(tevs, ctrl->val);

	case V4L2_CID_TEVS_AE_EXP_TIME_MAX:
		return tevs_set_ae_auto_exp_max(tevs, ctrl->val);

	case V4L2_CID_TEVS_TRIGGER_MODE:
		return tevs_set_trigger_mode(tevs, ctrl->val);

	default:
		dev_dbg(tevs->dev, "Unknown control 0x%x\n", ctrl->id);
		return -EINVAL;
	}
}

static const struct v4l2_ctrl_ops tevs_ctrl_ops = {
	.s_ctrl = tevs_s_ctrl,
};

static const struct v4l2_ctrl_config tevs_awb_mode = {
	.ops = &tevs_ctrl_ops,
	.id = V4L2_CID_AUTO_WHITE_BALANCE,
	.name = "White_Balance_Mode",
	.type = V4L2_CTRL_TYPE_MENU,
	.max = TEVS_AWB_CTRL_MODE_AUTO_IDX,
	.def = TEVS_AWB_CTRL_MODE_AUTO_IDX,
	.qmenu = awb_mode_strings,
};

static const struct v4l2_ctrl_config tevs_sfx_mode = {
	.ops = &tevs_ctrl_ops,
	.id = V4L2_CID_COLORFX,
	.name = "Special_Effect",
	.type = V4L2_CTRL_TYPE_MENU,
	.max = TEVS_SFX_MODE_SFX_SKETCH_IDX,
	.def = TEVS_SFX_MODE_SFX_NORMAL_IDX,
	.qmenu = sfx_mode_strings,
};

static const struct v4l2_ctrl_config tevs_ae_mode = {
	.ops = &tevs_ctrl_ops,
	.id = V4L2_CID_EXPOSURE_AUTO,
	.name = "Exposure_Mode",
	.type = V4L2_CTRL_TYPE_MENU,
	.max = TEVS_AE_CTRL_AUTO_GAIN_IDX,
	.def = TEVS_AE_CTRL_FULL_AUTO_IDX,
	.qmenu = ae_mode_strings,
};

static const struct v4l2_ctrl_config tevs_bsl_mode = {
	.ops = &tevs_ctrl_ops,
	.id = V4L2_CID_TEVS_BSL_MODE,
	.name = "BSL_Mode",
	.type = V4L2_CTRL_TYPE_MENU,
	.max = TEVS_BSL_MODE_FLASH_IDX,
	.def = TEVS_BSL_MODE_NORMAL_IDX,
	.qmenu = bsl_mode_strings,
};

static const struct v4l2_ctrl_config tevs_max_fps = {
	.ops = &tevs_ctrl_ops,
	.id = V4L2_CID_TEVS_MAX_FPS,
	.name = "Max_FPS",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0x01,
	.max = 0xFF,
	.step = 1,
	.def = 30,
};

static const struct v4l2_ctrl_config tevs_denoise = {
	.ops = &tevs_ctrl_ops,
	.id = V4L2_CID_TEVS_DENOISE,
	.name = "Denoise",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0x0000,
	.max = 0x4000,
	.step = 1,
	.def = 0x2000,
};

static const struct v4l2_ctrl_config tevs_ae_exp_upper = {
	.ops = &tevs_ctrl_ops,
	.id = V4L2_CID_TEVS_AE_EXP_TIME_UPPER,
	.name = "AE_Exposure_Upper",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0x0000,
	.max = 0xFFFFFFFFF,
	.step = 1,
	.def = 0x411A,
};

static const struct v4l2_ctrl_config tevs_ae_exp_max = {
	.ops = &tevs_ctrl_ops,
	.id = V4L2_CID_TEVS_AE_EXP_TIME_MAX,
	.name = "AE_Exposure_Max",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0x0000,
	.max = 0xFFFFFFFFF,
	.step = 1,
	.def = 0x1046A,
};

static const struct v4l2_ctrl_config tevs_trigger_mode = {
	.ops = &tevs_ctrl_ops,
	.id = V4L2_CID_TEVS_TRIGGER_MODE,
	.name = "Trigger_Mode",
	.type = V4L2_CTRL_TYPE_MENU,
	.max = TEVS_TRIGGER_MODE_NON_PERIODIC_IDX,
	.def = TEVS_TRIGGER_MODE_DISABLE_IDX,
	.qmenu = trigger_mode_strings,
};

static int tevs_ctrls_init(struct tevs *tevs)
{
	struct tegracam_ctrl_handler *ctrl_hdl;
	u16 val;
	s64 ctrl_def, ctrl_max, ctrl_min;
	u8 exp[4] = { 0 };
	int ret;
	static s64 link_freq[] = {
		TEVS_LINK_FREQUENCY_DEFAULT,
	};
	static s64 pixel_rate[] = {
		TEVS_PIXEL_RATE_DEFAULT,
	};

	ctrl_hdl = tevs->s_data->tegracam_ctrl_hdl;

	if (ctrl_hdl == NULL) {
		dev_info(&tevs->tc_dev->client->dev,"init control handler...\n");
		ret = v4l2_ctrl_handler_init(&ctrl_hdl->ctrl_handler, 26);
		if (ret) {
			dev_err(&tevs->tc_dev->client->dev,"init handler fail\n");
			return ret;
		}
	}

	ret = tevs_i2c_read_16b(tevs, TEVS_BRIGHTNESS, &val);
	ctrl_def = val & TEVS_BRIGHTNESS_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_BRIGHTNESS_MAX, &val);
	ctrl_max = val & TEVS_BRIGHTNESS_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_BRIGHTNESS_MIN, &val);
	ctrl_min = val & TEVS_BRIGHTNESS_MASK;
	if (ret)
		goto error;
	tevs->brightness = v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
					     V4L2_CID_BRIGHTNESS, ctrl_min,
					     ctrl_max, 1, ctrl_def);

	ret = tevs_i2c_read_16b(tevs, TEVS_CONTRAST, &val);
	ctrl_def = val & TEVS_CONTRAST_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_CONTRAST_MAX, &val);
	ctrl_max = val & TEVS_CONTRAST_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_CONTRAST_MIN, &val);
	ctrl_min = val & TEVS_CONTRAST_MASK;
	if (ret)
		goto error;
	tevs->contrast = v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
					   V4L2_CID_CONTRAST, ctrl_min,
					   ctrl_max, 1, ctrl_def);

	ret = tevs_i2c_read_16b(tevs, TEVS_SATURATION, &val);
	ctrl_def = val & TEVS_SATURATION_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_SATURATION_MAX, &val);
	ctrl_max = val & TEVS_SATURATION_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_SATURATION_MIN, &val);
	ctrl_min = val & TEVS_SATURATION_MASK;
	if (ret)
		goto error;
	tevs->saturation = v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
					     V4L2_CID_SATURATION, ctrl_min,
					     ctrl_max, 1, ctrl_def);

	tevs->awb = v4l2_ctrl_new_custom(&ctrl_hdl->ctrl_handler, &tevs_awb_mode, NULL);
	ret = tevs_i2c_read_16b(tevs, TEVS_AWB_CTRL_MODE, &val);
	if (ret)
		goto error;
	switch (val & TEVS_AWB_CTRL_MODE_MASK) {
	case TEVS_AWB_CTRL_MODE_MANUAL_TEMP:
		tevs->awb->default_value = tevs->awb->cur.val =
			TEVS_AWB_CTRL_MODE_MANUAL_TEMP_IDX;
		break;
	case TEVS_AWB_CTRL_MODE_AUTO:
		tevs->awb->default_value = tevs->awb->cur.val =
			TEVS_AWB_CTRL_MODE_AUTO_IDX;
		break;
	default:
		tevs->awb->default_value = tevs->awb->cur.val =
			TEVS_AWB_CTRL_MODE_AUTO_IDX;
		break;
	}

	ret = tevs_i2c_read_16b(tevs, TEVS_GAMMA, &val);
	ctrl_def = val & TEVS_GAMMA_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_GAMMA_MAX, &val);
	ctrl_max = val & TEVS_GAMMA_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_GAMMA_MIN, &val);
	ctrl_min = val & TEVS_GAMMA_MASK;
	if (ret)
		goto error;
	tevs->gamma = v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
					V4L2_CID_GAMMA, ctrl_min, ctrl_max, 1,
					ctrl_def);

	ret = tevs_i2c_read(tevs, TEVS_AE_MANUAL_EXP_TIME, exp, 4);
	ctrl_def = be32_to_cpup((__be32 *)exp) & TEVS_AE_MANUAL_EXP_TIME_MASK;
	ret += tevs_i2c_read(tevs, TEVS_AE_MANUAL_EXP_TIME_MAX, exp, 4);
	ctrl_max = be32_to_cpup((__be32 *)exp) & TEVS_AE_MANUAL_EXP_TIME_MASK;
	ret += tevs_i2c_read(tevs, TEVS_AE_MANUAL_EXP_TIME_MIN, exp, 4);
	ctrl_min = be32_to_cpup((__be32 *)exp) & TEVS_AE_MANUAL_EXP_TIME_MASK;
	if (ret)
		goto error;
	tevs->exp_time = v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
					   V4L2_CID_EXPOSURE, ctrl_min,
					   ctrl_max, 1, ctrl_def);

	ret = tevs_i2c_read_16b(tevs, TEVS_AE_MANUAL_GAIN, &val);
	ctrl_def = val & TEVS_AE_MANUAL_GAIN_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_AE_MANUAL_GAIN_MAX, &val);
	ctrl_max = val & TEVS_AE_MANUAL_GAIN_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_AE_MANUAL_GAIN_MIN, &val);
	ctrl_min = val & TEVS_AE_MANUAL_GAIN_MASK;
	if (ret)
		goto error;
	tevs->exp_gain = v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
					   V4L2_CID_GAIN, ctrl_min, ctrl_max, 1,
					   ctrl_def);

	ret = tevs_i2c_read_16b(tevs, TEVS_ORIENTATION, &val);
	ctrl_def = val & TEVS_ORIENTATION_HFLIP;
	if (ret)
		goto error;
	tevs->hflip = v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
					V4L2_CID_HFLIP, 0x0, 0x1, 1, ctrl_def);

	ctrl_def = (val & TEVS_ORIENTATION_VFLIP) >> TEVS_ORIENTATION_VFLIP_BIT;
	tevs->vflip = v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
					V4L2_CID_VFLIP, 0x0, 0x1, 1, ctrl_def);

	ret = tevs_i2c_read_16b(tevs, TEVS_FLICK_CTRL, &val);
	if (ret)
		goto error;
	switch (val & TEVS_FLICK_CTRL_MODE_MASK) {
	case TEVS_FLICK_CTRL_MODE_DISABLED:
		ctrl_def = V4L2_CID_POWER_LINE_FREQUENCY_DISABLED;
		break;
	case TEVS_FLICK_CTRL_MODE_MANUAL:
		if ((val & TEVS_FLICK_CTRL_FREQ_MASK) ==
		    TEVS_FLICK_CTRL_FREQ(50))
			ctrl_def = V4L2_CID_POWER_LINE_FREQUENCY_50HZ;
		else if ((val & TEVS_FLICK_CTRL_FREQ_MASK) ==
			 TEVS_FLICK_CTRL_FREQ(60))
			ctrl_def = V4L2_CID_POWER_LINE_FREQUENCY_60HZ;
		break;
	case TEVS_FLICK_CTRL_MODE_AUTO:
		ctrl_def = V4L2_CID_POWER_LINE_FREQUENCY_AUTO;
		break;
	default:
		ctrl_def = V4L2_CID_POWER_LINE_FREQUENCY_DISABLED;
		break;
	}
	tevs->flick = v4l2_ctrl_new_std_menu(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
					V4L2_CID_POWER_LINE_FREQUENCY,
					V4L2_CID_POWER_LINE_FREQUENCY_AUTO,
					0, ctrl_def);

	ret = tevs_i2c_read_16b(tevs, TEVS_AWB_MANUAL_TEMP, &val);
	ctrl_def = val & TEVS_AWB_MANUAL_TEMP_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_AWB_MANUAL_TEMP_MAX, &val);
	ctrl_max = val & TEVS_AWB_MANUAL_TEMP_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_AWB_MANUAL_TEMP_MIN, &val);
	ctrl_min = val & TEVS_AWB_MANUAL_TEMP_MASK;
	if (ret)
		goto error;
	tevs->wb_temp = v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
					  V4L2_CID_WHITE_BALANCE_TEMPERATURE,
					  ctrl_min, ctrl_max, 1, ctrl_def);

	ret = tevs_i2c_read_16b(tevs, TEVS_SHARPEN, &val);
	ctrl_def = val & TEVS_SHARPEN_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_SHARPEN_MAX, &val);
	ctrl_max = val & TEVS_SHARPEN_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_SHARPEN_MIN, &val);
	ctrl_min = val & TEVS_SHARPEN_MASK;
	if (ret)
		goto error;
	tevs->sharpness = v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
					    V4L2_CID_SHARPNESS, ctrl_min,
					    ctrl_max, 1, ctrl_def);

	ret = tevs_i2c_read_16b(tevs, TEVS_BACKLIGHT_COMPENSATION, &val);
	ctrl_def = val & TEVS_BACKLIGHT_COMPENSATION_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_BACKLIGHT_COMPENSATION_MAX, &val);
	ctrl_max = val & TEVS_BACKLIGHT_COMPENSATION_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_BACKLIGHT_COMPENSATION_MIN, &val);
	ctrl_min = val & TEVS_BACKLIGHT_COMPENSATION_MASK;
	if (ret)
		goto error;
	tevs->backlight_comp = v4l2_ctrl_new_std(
		&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops, V4L2_CID_BACKLIGHT_COMPENSATION,
		ctrl_min, ctrl_max, 1, ctrl_def);

	tevs->colorfx = v4l2_ctrl_new_custom(&ctrl_hdl->ctrl_handler, &tevs_sfx_mode, NULL);
	ret = tevs_i2c_read_16b(tevs, TEVS_SFX_MODE, &val);
	if (ret)
		goto error;
	switch (val & TEVS_SFX_MODE_SFX_MASK) {
	case TEVS_SFX_MODE_SFX_NORMAL:
		tevs->colorfx->default_value = tevs->colorfx->cur.val =
			TEVS_SFX_MODE_SFX_NORMAL_IDX;
		break;
	case TEVS_SFX_MODE_SFX_BW:
		tevs->colorfx->default_value = tevs->colorfx->cur.val =
			TEVS_SFX_MODE_SFX_BW_IDX;
		break;
	case TEVS_SFX_MODE_SFX_GRAYSCALE:
		tevs->colorfx->default_value = tevs->colorfx->cur.val =
			TEVS_SFX_MODE_SFX_GRAYSCALE_IDX;
		break;
	case TEVS_SFX_MODE_SFX_NEGATIVE:
		tevs->colorfx->default_value = tevs->colorfx->cur.val =
			TEVS_SFX_MODE_SFX_NEGATIVE_IDX;
		break;
	case TEVS_SFX_MODE_SFX_SKETCH:
		tevs->colorfx->default_value = tevs->colorfx->cur.val =
			TEVS_SFX_MODE_SFX_SKETCH_IDX;
		break;
	default:
		tevs->colorfx->default_value = tevs->colorfx->cur.val =
			TEVS_SFX_MODE_SFX_NORMAL_IDX;
		break;
	}

	tevs->ae = v4l2_ctrl_new_custom(&ctrl_hdl->ctrl_handler, &tevs_ae_mode, NULL);
	ret = tevs_i2c_read_16b(tevs, TEVS_AE_CTRL_MODE, &val);
	if (ret)
		goto error;
	switch (val & TEVS_AE_CTRL_MODE_MASK) {
	case TEVS_AE_CTRL_MANUAL_EXP_TIME_GAIN:
		tevs->ae->default_value = tevs->ae->cur.val =
			TEVS_AE_CTRL_MANUAL_EXP_TIME_GAIN_IDX;
		break;
	case TEVS_AE_CTRL_FULL_AUTO:
		tevs->ae->default_value = tevs->ae->cur.val =
			TEVS_AE_CTRL_FULL_AUTO_IDX;
		break;
	case TEVS_AE_CTRL_AUTO_GAIN: {
		u8 exp[4] = { 0 };
		ret += tevs_i2c_read(tevs, TEVS_AE_MANUAL_EXP_TIME, exp, 4);
		tevs->exp_time->cur.val = be32_to_cpup((__be32 *)exp) &
					  TEVS_AE_MANUAL_EXP_TIME_MASK;
		tevs->ae->default_value = tevs->ae->cur.val =
			TEVS_AE_CTRL_AUTO_GAIN_IDX;
	} break;
	default:
		tevs->ae->default_value = tevs->ae->cur.val =
			TEVS_AE_CTRL_FULL_AUTO_IDX;
			break;
	}

	ret = tevs_i2c_read_16b(tevs, TEVS_DZ_CT_X, &val);
	ctrl_def = val & TEVS_DZ_CT_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_DZ_CT_MAX, &val);
	ctrl_max = val & TEVS_DZ_CT_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_DZ_CT_MIN, &val);
	ctrl_min = val & TEVS_DZ_CT_MASK;
	if (ret)
		goto error;
	tevs->pan = v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
				      V4L2_CID_PAN_ABSOLUTE, ctrl_min, ctrl_max,
				      1, ctrl_def);

	ret = tevs_i2c_read_16b(tevs, TEVS_DZ_CT_Y, &val);
	ctrl_def = val & TEVS_DZ_CT_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_DZ_CT_MAX, &val);
	ctrl_max = val & TEVS_DZ_CT_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_DZ_CT_MIN, &val);
	ctrl_min = val & TEVS_DZ_CT_MASK;
	if (ret)
		goto error;
	tevs->tilt = v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
				       V4L2_CID_TILT_ABSOLUTE, ctrl_min,
				       ctrl_max, 1, ctrl_def);

	ret = tevs_i2c_read_16b(tevs, TEVS_DZ_TGT_FCT, &val);
	ctrl_def = val & TEVS_DZ_TGT_FCT_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_DZ_TGT_FCT_MAX, &val);
	ctrl_max = val & TEVS_DZ_TGT_FCT_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_DZ_TGT_FCT_MIN, &val);
	ctrl_min = val & TEVS_DZ_TGT_FCT_MASK;
	if (ret)
		goto error;
	tevs->zoom = v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
				       V4L2_CID_ZOOM_ABSOLUTE, ctrl_min,
				       ctrl_max, 1, ctrl_def);

	/* By default, link_freq and pixel_rate is read only */
	link_freq[0] = (u64)(tevs->data_frequency >> 1) * 1000000ULL;
	tevs->link_freq = v4l2_ctrl_new_int_menu(
		&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops, V4L2_CID_LINK_FREQ,
		ARRAY_SIZE(link_freq) - 1, 0, link_freq);
	tevs->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* link_freq = (pixel_rate * bpp) / (2 * data_lanes) */
	pixel_rate[0] = link_freq[0] * (2 * tevs->data_lanes) / 16;
	tevs->pixel_rate =
		v4l2_ctrl_new_std(&ctrl_hdl->ctrl_handler, &tevs_ctrl_ops,
				  V4L2_CID_PIXEL_RATE, pixel_rate[0],
				  pixel_rate[0], 1, pixel_rate[0]);
	tevs->pixel_rate->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	tevs->bsl = v4l2_ctrl_new_custom(&ctrl_hdl->ctrl_handler, &tevs_bsl_mode, NULL);

	tevs->max_fps = v4l2_ctrl_new_custom(&ctrl_hdl->ctrl_handler, &tevs_max_fps, NULL);
	ret = tevs_i2c_read_16b(tevs, TEVS_MAX_FPS, &val);
	if (ret)
		goto error;
	tevs->max_fps->default_value = tevs->max_fps->cur.val =
		val & TEVS_MAX_FPS_MASK;

	tevs->denoise = v4l2_ctrl_new_custom(&ctrl_hdl->ctrl_handler, &tevs_denoise, NULL);
	ret = tevs_i2c_read_16b(tevs, TEVS_DENOISE, &val);
	ctrl_def = val & TEVS_DENOISE_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_DENOISE_MAX, &val);
	ctrl_max = val & TEVS_DENOISE_MASK;
	ret += tevs_i2c_read_16b(tevs, TEVS_DENOISE_MIN, &val);
	ctrl_min = val & TEVS_DENOISE_MASK;
	if (ret)
		goto error;
	tevs->denoise->default_value = tevs->denoise->cur.val = ctrl_def;
	tevs->denoise->maximum = ctrl_max;
	tevs->denoise->minimum = ctrl_min;

	tevs->ae_exp_upper =
		v4l2_ctrl_new_custom(&ctrl_hdl->ctrl_handler, &tevs_ae_exp_upper, NULL);
	ret = tevs_i2c_read(tevs, TEVS_AE_AUTO_EXP_TIME_UPPER, exp, 4);
	ctrl_def = be32_to_cpup((__be32 *)exp) & TEVS_AE_AUTO_EXP_TIME_MASK;
	ret += tevs_i2c_read(tevs, TEVS_AE_MANUAL_EXP_TIME_MAX, exp, 4);
	ctrl_max = be32_to_cpup((__be32 *)exp) & TEVS_AE_MANUAL_EXP_TIME_MASK;
	ret += tevs_i2c_read(tevs, TEVS_AE_MANUAL_EXP_TIME_MIN, exp, 4);
	ctrl_min = be32_to_cpup((__be32 *)exp) & TEVS_AE_MANUAL_EXP_TIME_MASK;
	if (ret)
		goto error;
	tevs->ae_exp_upper->default_value = tevs->ae_exp_upper->cur.val =
		ctrl_def;
	tevs->ae_exp_upper->maximum = ctrl_max;
	tevs->ae_exp_upper->minimum = ctrl_min;

	tevs->ae_exp_max =
		v4l2_ctrl_new_custom(&ctrl_hdl->ctrl_handler, &tevs_ae_exp_max, NULL);
	ret = tevs_i2c_read(tevs, TEVS_AE_AUTO_EXP_TIME_MAX, exp, 4);
	ctrl_def = be32_to_cpup((__be32 *)exp) & TEVS_AE_AUTO_EXP_TIME_MASK;
	if (ret)
		goto error;
	tevs->ae_exp_max->default_value = tevs->ae_exp_max->cur.val = ctrl_def;
	tevs->ae_exp_max->maximum = ctrl_max;
	tevs->ae_exp_max->minimum = ctrl_min;

	tevs->trigger =
		v4l2_ctrl_new_custom(&ctrl_hdl->ctrl_handler, &tevs_trigger_mode, NULL);
	tevs->trigger->default_value = tevs->trigger->cur.val = tevs->trigger_mode;

	if (ctrl_hdl->ctrl_handler.error) {
		dev_err(&tevs->tc_dev->client->dev, "ctrls error\n");
		ret = ctrl_hdl->ctrl_handler.error;
		v4l2_ctrl_handler_free(&ctrl_hdl->ctrl_handler);
		return ret;
	}

	/* Use same lock for controls as for everything else. */
	ctrl_hdl->ctrl_handler.lock = &tevs->lock;
	tevs->v4l2_subdev->ctrl_handler = &ctrl_hdl->ctrl_handler;

	return 0;

error:
	v4l2_ctrl_handler_free(&ctrl_hdl->ctrl_handler);

	return ret;
}

static void tevs_ctrls_free(struct tevs *tevs)
{
	v4l2_ctrl_handler_free(&tevs->ctrls);
	// mutex_destroy(&tevs->lock);
}

static int tevs_init_setting(struct tevs *tevs)
{
	int ret = 0;

	if (tevs->trigger_mode) {
		ret = tevs_set_trigger_mode(tevs, tevs->trigger_mode);
		if (ret != 0) {
			dev_err(tevs->dev, "set trigger mode failed\n");
			return ret;
		}
	}

	ret += tevs_i2c_write_16b(tevs,
				HOST_COMMAND_ISP_CTRL_PREVIEW_FORMAT,
				tevs->s_data->sensor_props.sensor_modes->
					image_properties.pixel_format == V4L2_PIX_FMT_GREY ?
				TEVS_IMG_FORMAT_Y8 : TEVS_IMG_FORMAT_UYVY);
	ret += tevs_i2c_write_16b(tevs,
				HOST_COMMAND_ISP_CTRL_PREVIEW_HINF_CTRL,
				0x10 | (tevs->continuous_clock << 5) | (tevs->data_lanes));
	ret += tevs_i2c_write_16b(tevs,
				HOST_COMMAND_ISP_CTRL_PREVIEW_MIPI_CTRL,
				tevs->s_data->sensor_props.sensor_modes->
					image_properties.pixel_format ==
					V4L2_PIX_FMT_GREY ?
					(0x2A << 2 | tevs->vc_id) :
					(0x3F << 2 | tevs->vc_id));
	return ret;
}

static int tevs_power_on(struct camera_common_data *s_data)
{
	struct tevs *tevs = (struct tevs*)s_data->priv;
	int ret = 0;

	dev_dbg(tevs->dev, "%s()\n", __func__);

	gpiod_set_value_cansleep(tevs->reset_gpio, 1);

	msleep(TEVS_BOOT_TIME);

	ret = tevs_check_boot_state(tevs);
	if (ret != 0)
		return ret;

	if ((tevs->hw_reset_mode | tevs->trigger_mode)) {
		ret = tevs_init_setting(tevs);
		if (ret != 0)
			dev_err(tevs->dev, "init setting failed\n");
	}

	return ret;
}

static int tevs_power_off(struct camera_common_data *s_data)
{
	struct tevs *tevs = (struct tevs*)s_data->priv;
	dev_dbg(tevs->dev, "%s()\n", __func__);

	if (tevs->hw_reset_mode) {
		gpiod_set_value_cansleep(tevs->reset_gpio, 0);
		gpiod_set_value_cansleep(tevs->standby_gpio, 0);
	}

	return 0;
}

static int tevs_power_put(struct tegracam_device *tc_dev)
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

static int tevs_power_get(struct tegracam_device *tc_dev)
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

	pw->state = SWITCH_OFF;

	return err;
}

static struct camera_common_pdata *tevs_parse_dt(
	struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct device_node *np = dev->of_node;
	struct camera_common_pdata *board_priv_pdata;
	int err = 0;

	if (!np)
		return NULL;

	board_priv_pdata = devm_kzalloc(dev,
		sizeof(*board_priv_pdata), GFP_KERNEL);
	if (!board_priv_pdata)
		return NULL;

	err = of_property_read_string(np, "mclk", &board_priv_pdata->mclk_name);
	if (err)
		dev_dbg(dev, "mclk name not present, "
			"assume sensor driven externally\n");

	return board_priv_pdata;
}

static bool tevs_fps_is_supported(struct tevs *tevs, unsigned int mode,
				  unsigned int fps)
{
	const struct camera_common_frmfmt *fmt;
	unsigned int i;

	if (mode >= tevs_sensor_table[tevs->selected_sensor].res_list_size)
		return false;

	fmt = &tevs_sensor_table[tevs->selected_sensor].frmfmt[mode];
	for (i = 0; i < fmt->num_framerates; i++) {
		if (fmt->framerates[i] == fps)
			return true;
	}

	return false;
}

static int tevs_set_mode(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct tevs *tevs = tc_dev->priv;
	int i;
	dev_dbg(tc_dev->dev,
		"%s() , {%d}-{%d}, numfmt=%d, fmt=0x%x, fmt_width=%d, fmt_height=%d\n",
		__func__,
		s_data->mode,
		s_data->mode_prop_idx,
		s_data->numfmts,
		s_data->colorfmt->code,
		s_data->fmt_width,
		s_data->fmt_height);

	for (i = 0 ; i < tevs_sensor_table[tevs->selected_sensor].res_list_size ; i++) {
		if (s_data->fmt_width == tevs_sensor_table[tevs->selected_sensor].frmfmt[i].size.width &&
			s_data->fmt_height == tevs_sensor_table[tevs->selected_sensor].frmfmt[i].size.height)
			break;
	}

	if (i >= tevs_sensor_table[tevs->selected_sensor].res_list_size) {
		return -EINVAL;
	}

	tevs->selected_mode = i;

	if (!tevs_fps_is_supported(tevs, tevs->selected_mode, tevs->fps))
		tevs->fps = tevs_sensor_table[tevs->selected_sensor]
				    .frmfmt[tevs->selected_mode].framerates[0];

	return 0;
}

static int tevs_start_streaming(struct tegracam_device *tc_dev)
{
	struct tevs *tevs = tc_dev->priv;
	int ret = 0;
	u8 exp[4] = { 0 };
	dev_dbg(tc_dev->dev, "%s()\n", __func__);

	if (tevs->selected_mode >=
	    tevs_sensor_table[tevs->selected_sensor].res_list_size)
		return -EINVAL;

	if (!(tevs_check_trigger_mode(tevs) | tevs->hw_reset_mode))
		ret = tevs_standby(tevs, 0);

	dev_dbg(tc_dev->dev, "%s() width=%d, height=%d, mode=%d\n",
		__func__,
		tevs_sensor_table[tevs->selected_sensor]
			.frmfmt[tevs->selected_mode]
			.size.width,
		tevs_sensor_table[tevs->selected_sensor]
			.frmfmt[tevs->selected_mode]
			.size.height,
		tevs_sensor_table[tevs->selected_sensor]
			.frmfmt[tevs->selected_mode]
			.mode);

	tevs_i2c_write_16b(tevs,
			HOST_COMMAND_ISP_CTRL_PREVIEW_MIPI_CTRL,
			tevs->s_data->sensor_props.sensor_modes->
				image_properties.pixel_format ==
				V4L2_PIX_FMT_GREY ?
				(0x2A << 2 | tevs->vc_id) :
				(0x3F << 2 | tevs->vc_id));
	tevs_i2c_write_16b(tevs,
			HOST_COMMAND_ISP_CTRL_PREVIEW_FORMAT,
			tevs->s_data->sensor_props.sensor_modes->
				image_properties.pixel_format == V4L2_PIX_FMT_GREY ?
			TEVS_IMG_FORMAT_Y8 : TEVS_IMG_FORMAT_UYVY);
	tevs_i2c_write_16b(
		tevs,
		HOST_COMMAND_ISP_CTRL_PREVIEW_HINF_CTRL,
		0x10 | (tevs->continuous_clock << 5) | (tevs->data_lanes));
	tevs_i2c_write_16b(
		tevs,
		HOST_COMMAND_ISP_CTRL_PREVIEW_SENSOR_MODE,
		tevs_sensor_table[tevs->selected_sensor]
			.frmfmt[tevs->selected_mode]
			.mode);
	tevs_i2c_write_16b(
		tevs,
		HOST_COMMAND_ISP_CTRL_PREVIEW_WIDTH,
		tevs_sensor_table[tevs->selected_sensor]
			.frmfmt[tevs->selected_mode]
			.size.width);
	tevs_i2c_write_16b(
		tevs,
		HOST_COMMAND_ISP_CTRL_PREVIEW_HEIGHT,
		tevs_sensor_table[tevs->selected_sensor]
			.frmfmt[tevs->selected_mode]
			.size.height);
	tevs_i2c_write_16b(
				tevs, HOST_COMMAND_ISP_CTRL_PREVIEW_MAX_FPS,
				tevs->fps);
	if (tevs->max_fps)
		tevs->max_fps->cur.val = tevs->fps;
	tevs_i2c_read(tevs, TEVS_AE_MANUAL_EXP_TIME, exp, 4);
	tevs->exp_time->cur.val = be32_to_cpup((__be32 *)exp) &
			TEVS_AE_MANUAL_EXP_TIME_MASK;
	tevs_i2c_read(tevs, TEVS_AE_AUTO_EXP_TIME_UPPER, exp, 4);
	tevs->ae_exp_upper->cur.val = be32_to_cpup((__be32 *)exp) &
			TEVS_AE_MANUAL_EXP_TIME_MASK;
	tevs_i2c_read(tevs, TEVS_AE_AUTO_EXP_TIME_MAX, exp, 4);
	tevs->ae_exp_max->cur.val = be32_to_cpup((__be32 *)exp) &
			TEVS_AE_MANUAL_EXP_TIME_MASK;

	return ret;
}

static int tevs_stop_streaming(struct tegracam_device *tc_dev)
{
	struct tevs *tevs = tc_dev->priv;
	int ret = 0;

	if (!(tevs_check_trigger_mode(tevs) | tevs->hw_reset_mode))
		ret = tevs_standby(tevs, 1);

	if (tevs->continuous_clock) {
		ret += tevs_i2c_write_16b(tevs,
				HOST_COMMAND_ISP_CTRL_PREVIEW_HINF_CTRL,
				0x10 | (TEVS_CONTINUOUS_CLOCK_DEFAULT << 5) | (tevs->data_lanes));
	}
	return ret;
}

static inline int tevs_read_reg(struct camera_common_data *s_data,
	u16 addr, u8 *val)
{
	int err = 0;
	u32 reg_val = 0;

	err = regmap_read(s_data->regmap, addr, &reg_val);
	*val = reg_val & 0xff;

	return err;
}

static inline int tevs_write_reg(struct camera_common_data *s_data,
	u16 addr, u8 val)
{
	int err = 0;

	err = regmap_write(s_data->regmap, addr, val);
	if (err)
		dev_err(s_data->dev, "%s: i2c write failed, 0x%x = %x",
			__func__, addr, val);

	return err;
}

static int tevs_g_frame_interval(struct v4l2_subdev *sd,
				 struct v4l2_subdev_frame_interval *interval)
{
	struct camera_common_data *s_data =
		to_camera_common_data(sd->dev);
	struct tevs *tevs = s_data ? s_data->priv : NULL;

	if (!tevs->fps)
		return -EINVAL;

	interval->interval.numerator = 1;
	interval->interval.denominator = tevs->fps;

	dev_dbg(s_data->dev, "%s: fps = %d\n", __func__, tevs->fps);

	return 0;
}

static int tevs_s_frame_interval(struct v4l2_subdev *sd,
				 struct v4l2_subdev_frame_interval *interval)
{
	struct camera_common_data *s_data =
		to_camera_common_data(sd->dev);
	struct tevs *tevs = s_data ? s_data->priv : NULL;
	unsigned int fps;
	int ret;

	if (!interval->interval.numerator ||
	    !interval->interval.denominator)
		return -EINVAL;

	if (interval->interval.denominator % interval->interval.numerator)
		return -EINVAL;

	fps = interval->interval.denominator / interval->interval.numerator;
	if (!tevs_fps_is_supported(tevs, s_data->mode_prop_idx, fps))
		return -EINVAL;

	if (tevs->tc_dev->is_streaming) {
		ret = tevs_set_max_fps(tevs, fps);
		if (ret)
			return ret;
	} else {
		tevs->fps = fps;
	}

	interval->interval.numerator = 1;
	interval->interval.denominator = fps;

	dev_dbg(s_data->dev, "%s: fps = %d\n", __func__, tevs->fps);

	return 0;
}

static struct camera_common_sensor_ops tevs_common_ops = {
	.numfrmfmts = ARRAY_SIZE(sensor_frmfmt),
	.frmfmt_table = sensor_frmfmt,
	.power_on = tevs_power_on,
	.power_off = tevs_power_off,
	.write_reg = tevs_write_reg,
	.read_reg = tevs_read_reg,
	.parse_dt = tevs_parse_dt,
	.power_get = tevs_power_get,
	.power_put = tevs_power_put,
	.set_mode = tevs_set_mode,
	.start_streaming = tevs_start_streaming,
	.stop_streaming = tevs_stop_streaming,
};

static int tevs_try_on(struct tevs *tevs)
{
	tevs_power_off(tevs->s_data);
	return tevs_power_on(tevs->s_data);
}

static int tevs_setup(struct tevs *tevs)
{
	int i = 0;
	int ret = 0;
	struct device_node *endpoint = NULL;
	struct device_node *ports;
	struct device_node *port;
	struct fwnode_handle *ep;
	struct v4l2_fwnode_endpoint ep_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};

	tevs->reset_gpio =
		devm_gpiod_get_optional(tevs->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR_OR_NULL(tevs->reset_gpio)) {
		ret = PTR_ERR(tevs->reset_gpio);
		if (ret != -EPROBE_DEFER) {
			dev_err(tevs->dev, "Cannot get reset GPIO (%d)", ret);
			ret = -EINVAL;
		}
		return ret;
	}

	tevs->standby_gpio =
		devm_gpiod_get_optional(tevs->dev, "standby", GPIOD_OUT_LOW);
	if (IS_ERR_OR_NULL(tevs->standby_gpio)) {
		ret = PTR_ERR(tevs->standby_gpio);
		if (ret != -EPROBE_DEFER) {
			dev_err(tevs->dev, "Cannot get standby GPIO (%d)", ret);
			ret = -EINVAL;
		}
		return ret;
	}
	gpiod_set_value_cansleep(tevs->standby_gpio, 0);

	tevs->vc_id = 0;
	if (of_property_read_u32(tevs->dev->of_node, "vc-id",
				 &tevs->vc_id) == 0) {
		if (tevs->vc_id > 3) {
			dev_err(tevs->dev,
				"value of 'vc-id = <%d>' property is invaild\n",
				tevs->vc_id);
			return -EINVAL;
		}
	}

	tevs->hw_reset_mode =
		of_property_read_bool(tevs->dev->of_node, "hw-reset");

	tevs->trigger_mode = 0;
	if (of_property_read_u32(tevs->dev->of_node, "trigger-mode",
				 &tevs->trigger_mode) == 0) {
		if (tevs->trigger_mode > 3) {
			dev_err(tevs->dev,
				"value of 'trigger-mode = <%d>' property is invaild\n",
				tevs->trigger_mode);
			return -EINVAL;
		}
	}

	ports = of_get_child_by_name(tevs->dev->of_node, "ports");
	if (ports == NULL)
		ports = tevs->dev->of_node;

	for_each_child_of_node(ports, port) {
		if (!port->name || of_node_cmp(port->name, "port"))
			continue;

		for_each_child_of_node(port, endpoint) {
			if (!endpoint->name || of_node_cmp(endpoint->name, "endpoint"))
				continue;
			ret = of_property_read_u32(endpoint, "bus-width", &tevs->data_lanes);
			if (ret < 0)
				dev_err(tevs->dev, "No bus width info\n");
		}
	}

	/* Check the number of MIPI CSI2 data lanes */
	if (tevs->data_lanes != 2 && tevs->data_lanes != 4) {
		dev_err(tevs->dev, "only 2 or 4 data lanes are currently supported\n");
		ret = -EINVAL;
		goto error_out;
	}

	ep = fwnode_graph_get_endpoint_by_id(dev_fwnode(tevs->dev), 0, 0,
					     FWNODE_GRAPH_ENDPOINT_NEXT);
	if (!ep) {
		dev_err(tevs->dev, "no sink port found");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &ep_cfg);
	if (ret < 0) {
		dev_err(tevs->dev, "failed to parse bus configuration\n");
		goto error_out;
	}

	/* Check the link frequency set in device tree */
	if (ep_cfg.nr_of_link_frequencies == 0)
		tevs->data_frequency = div_u64(((u64)TEVS_LINK_FREQUENCY_DEFAULT * 2),
							1000000ULL);
	else if (ep_cfg.nr_of_link_frequencies == 1)
		tevs->data_frequency = div_u64((ep_cfg.link_frequencies[0] * 2),
							1000000ULL);
	else {
		dev_err(tevs->dev, "invalid link frequencies %u on port\n",
				ep_cfg.nr_of_link_frequencies);
		ret = -EINVAL;
		goto error_out;
	}

	if ((tevs->data_frequency != 0) &&
		((tevs->data_frequency < 100) || (tevs->data_frequency > 1200))) {
		dev_err(tevs->dev,
			"value of data-frequency [%d] is invaild\n",
			tevs->data_frequency);
		ret = -EINVAL;
		goto error_out;
	}

	tevs->continuous_clock = !(ep_cfg.bus.mipi_csi2.flags &
				 V4L2_MBUS_CSI2_NONCONTINUOUS_CLOCK);

	dev_dbg(tevs->dev,
		"data-lanes [%d] ,continuous-clock [%d],"
		" vc-id [%d], hw-reset [%d], trigger-mode [%d]\n",
		tevs->data_lanes, tevs->continuous_clock,
		tevs->vc_id, tevs->hw_reset_mode, tevs->trigger_mode);

	if (tevs_try_on(tevs) != 0) {
		dev_err(tevs->dev, "cannot find tevs camera\n");
		return -EINVAL;
	}

	if (tevs->data_frequency != 0) {
		ret = tevs_i2c_write_16b(tevs,
					HOST_COMMAND_ISP_CTRL_MIPI_FREQ,
					tevs->data_frequency);
		msleep(TEVS_BOOT_TIME);
		if (tevs_check_boot_state(tevs) != 0) {
			dev_err(tevs->dev, "check tevs bootup status failed\n");
			return -EINVAL;
		}
		if (ret < 0) {
			dev_err(tevs->dev, "set mipi frequency failed\n");
			return -EINVAL;
		}
	}

	tevs->header_info = devm_kzalloc(
			tevs->dev, sizeof(struct header_info), GFP_KERNEL);
	if (tevs->header_info == NULL) {
		dev_err(tevs->dev, "allocate header_info failed\n");
		return -EINVAL;
	}

	ret = tevs_check_version(tevs);
	if (ret < 0) {
		dev_err(tevs->dev, "dev init failed\n");
		return -EINVAL;
	}

	ret = tevs_load_header_info(tevs);
	if (ret < 0) {
		dev_err(tevs->dev, "otp flash init failed\n");
		return -EINVAL;
	}

	ret = tevs_get_chip_id(tevs);
	if (ret < 0) {
		dev_err(tevs->dev, "get chip ID failed\n");
		return -EINVAL;
	}

	if (tevs->chip_id == SENSOR_CHIP_ID_NONE) {
		for (i = 0; i < ARRAY_SIZE(tevs_sensor_table); i++) {
			if (strcmp((const char *)tevs->header_info->product_name,
				tevs_sensor_table[i].sensor_name) == 0)
				break;
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(tevs_sensor_table); i++) {
			if (tevs->chip_id == tevs_sensor_table[i].chip_id)
				break;
		}
	}

	if (i >= ARRAY_SIZE(tevs_sensor_table)) {
        if (tevs->chip_id == SENSOR_CHIP_ID_NONE)
            dev_err(tevs->dev, "cannot not support the product: %s\n",
				(const char *)tevs->header_info->product_name);
        else
            dev_err(tevs->dev, "cannot not support the chip ID: 0x%.4X\n",
				tevs->chip_id);
		return -ENODEV;
	}

	tevs->selected_sensor = i;
	dev_dbg(tevs->dev, "selected_sensor:%d, sensor_name:%s\n", i,
		tevs->header_info->product_name);

	tevs->s_data->frmfmt = tevs_sensor_table[tevs->selected_sensor].frmfmt;
	tevs->s_data->numfmts =
			tevs_sensor_table[tevs->selected_sensor].res_list_size;
	tevs->fps = tevs_sensor_table[tevs->selected_sensor].frmfmt[0].framerates[0];

	if ((ret = tevs_init_setting(tevs)) != 0) {
		dev_err(tevs->dev, "init setting failed\n");
		return ret;
	}

error_out:
	v4l2_fwnode_endpoint_free(&ep_cfg);
	fwnode_handle_put(ep);

	return ret;
}

/*
 * tegracam_v4l2 uses the same getter for both g_frame_interval and
 * s_frame_interval.  Keep the shared tegracam operations, but override the
 * two frame-interval callbacks for TEVS so VIDIOC_S_PARM reaches the sensor.
 */
static int tevs_install_frame_interval_ops(struct tevs *tevs)
{
	struct v4l2_subdev *sd = tevs->v4l2_subdev;

	if (!sd->ops || !sd->ops->video)
		return -EINVAL;

	tevs->subdev_ops = *sd->ops;
	tevs->video_ops = *sd->ops->video;
	tevs->video_ops.g_frame_interval = tevs_g_frame_interval;
	tevs->video_ops.s_frame_interval = tevs_s_frame_interval;
	tevs->subdev_ops.video = &tevs->video_ops;
	sd->ops = &tevs->subdev_ops;

	return 0;
}

static int tevs_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tevs *tevs = NULL;
	struct device *dev = &client->dev;
	struct tegracam_device *tc_dev;
	int ret;

	dev_info(dev, "%s() device node: %s\n", __func__,
		 client->dev.of_node->full_name);

	tevs = devm_kzalloc(dev, sizeof(struct tevs), GFP_KERNEL);
	if (tevs == NULL) {
		dev_err(dev, "allocate memory failed\n");
		return -ENOMEM;
	}

	tc_dev = devm_kzalloc(dev,
			sizeof(struct tegracam_device), GFP_KERNEL);
	if (!tc_dev)
		return -ENOMEM;
	tc_dev->client = client;
	tc_dev->dev = dev;
	tc_dev->dev_regmap_config = &tevs_regmap_config;
	tc_dev->sensor_ops = &tevs_common_ops;
#ifdef	__ENABLE_NV_CTRL
	tc_dev->tcctrl_ops = &tevs_nv_ctrl_ops;
#endif

	ret = tegracam_device_register(tc_dev);
	if (ret) {
		dev_err(dev, "tegra camera driver registration failed\n");
		return ret;
	}

	tevs->dev = dev;
	tevs->tc_dev = tc_dev;
	tevs->s_data = tc_dev->s_data;
	tevs->regmap = tc_dev->s_data->regmap;
	tevs->v4l2_subdev = &tc_dev->s_data->subdev;
	tevs->v4l2_subdev->flags |=
		(V4L2_SUBDEV_FL_HAS_EVENTS | V4L2_SUBDEV_FL_HAS_DEVNODE);
	tegracam_set_privdata(tc_dev, (void *)tevs);

	ret = tevs_setup(tevs);
	if (ret != 0) {
		tegracam_device_unregister(tc_dev);
		dev_err(dev, "tevs setup failed\n");
		return ret;
	}

	ret = tegracam_v4l2subdev_register(tc_dev, true);
	if (ret) {
		dev_err(dev, "tegra camera subdev registration failed\n");
		return ret;
	}

	/*
	 * tegracam_v4l2 uses the same getter for both g_frame_interval and
	 * s_frame_interval.  Keep the shared tegracam operations, but override the
	 * two frame-interval callbacks for TEVS so VIDIOC_S_PARM reaches the sensor.
	 */
	ret = tevs_install_frame_interval_ops(tevs);
	if (ret) {
		dev_err(dev, "failed to install frame interval operations\n");
		goto error_probe;
	}

	ret = tevs_ctrls_init(tevs);
	if (ret) {
		dev_err(&client->dev, "failed to init controls: %d", ret);
		goto error_probe;
	}

	if (!(tevs_check_trigger_mode(tevs) | tevs->hw_reset_mode)) {
		ret = tevs_standby(tevs, 1);

		if (tevs->continuous_clock) {
			ret += tevs_i2c_write_16b(tevs,
					HOST_COMMAND_ISP_CTRL_PREVIEW_HINF_CTRL,
					0x10 | (TEVS_CONTINUOUS_CLOCK_DEFAULT << 5) | (tevs->data_lanes));
		}
		if (ret != 0) {
			dev_err(tevs->dev, "set standby mode failed\n");
			return ret;
		}
	}

	if (ret == 0)
		dev_info(dev, "probe success\n");
	else
		dev_err(dev, "probe failed\n");

error_probe:
	mutex_destroy(&tevs->lock);

	return ret;
}

static int tevs_remove(struct i2c_client *client)
{
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct tevs *tevs = (struct tevs *)s_data->priv;
	tevs_ctrls_free(tevs);
	tegracam_v4l2subdev_unregister(tevs->tc_dev);
	tegracam_device_unregister(tevs->tc_dev);
	return 0;
}

static const struct i2c_device_id sensor_id[] = { { DRIVER_NAME, 0 }, {} };
MODULE_DEVICE_TABLE(i2c, sensor_id);

static const struct of_device_id sensor_of[] = { { .compatible =
							   "tn," DRIVER_NAME },
						 { /* sentinel */ } };
MODULE_DEVICE_TABLE(of, sensor_of);

static struct i2c_driver sensor_i2c_driver = {
	.driver = {
		.of_match_table = of_match_ptr(sensor_of),
		.name  = DRIVER_NAME,
	},
	.probe = tevs_probe,
	.remove = tevs_remove,
	.id_table = sensor_id,
};

module_i2c_driver(sensor_i2c_driver);

MODULE_AUTHOR("TECHNEXION Inc.");
MODULE_DESCRIPTION("TechNexion driver for TEVS");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.0");
MODULE_ALIAS("Camera");
