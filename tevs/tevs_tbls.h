#ifndef __SENSOR_TABLES_H__
#define __SENSOR_TABLES_H__

#define SENSOR_CHIP_ID_NONE					0x0000
#define SENSOR_CHIP_ID_ONSEMI_AR0144		0x0356
#define SENSOR_CHIP_ID_ONSEMI_AR0145		0x1750
#define SENSOR_CHIP_ID_ONSEMI_AR0234		0x0A56
#define SENSOR_CHIP_ID_ONSEMI_AR0235		0x1850
#define SENSOR_CHIP_ID_ONSEMI_AR0246		0x1F56
#define SENSOR_CHIP_ID_ONSEMI_AR0521		0x0457
#define SENSOR_CHIP_ID_ONSEMI_AR0522		0x1457
#define SENSOR_CHIP_ID_ONSEMI_AR0544		0x0453
#define SENSOR_CHIP_ID_ONSEMI_AR0821		0x2557
#define SENSOR_CHIP_ID_ONSEMI_AR0822		0x0F56
#define SENSOR_CHIP_ID_ONSEMI_AR0830		0x0553
#define SENSOR_CHIP_ID_ONSEMI_AR1335		0x0153
#define SENSOR_CHIP_ID_ONSEMI_AR2020		0x0653

enum
{
	TEVS_AR0144 = 0,
	TEVS_AR0145,
	TEVS_AR0234,
	TEVS_AR0235,
	TEVS_AR0246,
	TEVS_AR0521,
	TEVS_AR0522,
	TEVS_AR0544,
	TEVS_AR0821,
	TEVS_AR0822,
	TEVS_AR0830,
	TEVS_AR1335,
	TEVS_AR2020,
};

#ifdef __OLD_RES__
struct resolution {
	u16 width;
	u16 height;
	u16 *framerates;
	u16 framerates_size;
	u16 mode;
};

/* AR0144 default setting for 4 data lanes and data frequency 800 MHz */
static u32 ar0144_code_list[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
};

static u16 ar0144_framerates_640x480[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0144_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0144_framerates_1280x800[] = { 60, 30, 20, 15, 10, 5 };

static struct resolution ar0144_res_list[] = {
	{ .width = 640,
	  .height = 480,
	  .framerates = ar0144_framerates_640x480,
	  .framerates_size = ARRAY_SIZE(ar0144_framerates_640x480),
	  .mode = 0 },
	{ .width = 1280,
	  .height = 720,
	  .framerates = ar0144_framerates_1280x720,
	  .framerates_size = ARRAY_SIZE(ar0144_framerates_1280x720),
	  .mode = 0 },
	{ .width = 1280,
	  .height = 800,
	  .framerates = ar0144_framerates_1280x800,
	  .framerates_size = ARRAY_SIZE(ar0144_framerates_1280x800),
	  .mode = 0 },
};

/* AR0145 default setting for 4 data lanes and data frequency 800 MHz */
static u32 ar0145_code_list[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
};

static u16 ar0145_framerates_640x480[] = { 115, 60, 30, 20, 15, 10, 5 };
static u16 ar0145_framerates_1280x720[] = { 115, 60, 30, 20, 15, 10, 5 };
static u16 ar0145_framerates_1280x800[] = { 115, 60, 30, 20, 15, 10, 5 };

static struct resolution ar0145_res_list[] = {
	{ .width = 640,
	  .height = 480,
	  .framerates = ar0145_framerates_640x480,
	  .framerates_size = ARRAY_SIZE(ar0145_framerates_640x480),
	  .mode = 0 },
	{ .width = 1280,
	  .height = 720,
	  .framerates = ar0145_framerates_1280x720,
	  .framerates_size = ARRAY_SIZE(ar0145_framerates_1280x720),
	  .mode = 0 },
	{ .width = 1280,
	  .height = 800,
	  .framerates = ar0145_framerates_1280x800,
	  .framerates_size = ARRAY_SIZE(ar0145_framerates_1280x800),
	  .mode = 0 },
};

/* AR0234 default setting for 4 data lanes and data frequency 800 MHz */
static u32 ar0234_code_list[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
};

static u16 ar0234_framerates_640x480[] = { 120, 60, 30, 20, 15, 10, 5 };
static u16 ar0234_framerates_1280x720[] = { 120, 60, 30, 20, 15, 10, 5 };
static u16 ar0234_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0234_framerates_1920x1200[] = { 60, 30, 20, 15, 10, 5 };

static struct resolution ar0234_res_list[] = {
	{ .width = 640,
	  .height = 480,
	  .framerates = ar0234_framerates_640x480,
	  .framerates_size = ARRAY_SIZE(ar0234_framerates_640x480),
	  .mode = 1 },
	{ .width = 1280,
	  .height = 720,
	  .framerates = ar0234_framerates_1280x720,
	  .framerates_size = ARRAY_SIZE(ar0234_framerates_1280x720),
	  .mode = 0 },
	{ .width = 1920,
	  .height = 1080,
	  .framerates = ar0234_framerates_1920x1080,
	  .framerates_size = ARRAY_SIZE(ar0234_framerates_1920x1080),
	  .mode = 0 },
	{ .width = 1920,
	  .height = 1200,
	  .framerates = ar0234_framerates_1920x1200,
	  .framerates_size = ARRAY_SIZE(ar0234_framerates_1920x1200),
	  .mode = 0 },
};

/* AR0235 default setting for 4 data lanes and data frequency 800 MHz */
static u32 ar0235_code_list[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
};

static u16 ar0235_framerates_640x480[] = { 120, 60, 30, 20, 15, 10, 5 };
static u16 ar0235_framerates_1280x720[] = { 120, 60, 30, 20, 15, 10, 5 };
static u16 ar0235_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0235_framerates_1920x1200[] = { 60, 30, 20, 15, 10, 5 };

static struct resolution ar0235_res_list[] = {
	{ .width = 640,
	  .height = 480,
	  .framerates = ar0235_framerates_640x480,
	  .framerates_size = ARRAY_SIZE(ar0235_framerates_640x480),
	  .mode = 0 },
	{ .width = 1280,
	  .height = 720,
	  .framerates = ar0235_framerates_1280x720,
	  .framerates_size = ARRAY_SIZE(ar0235_framerates_1280x720),
	  .mode = 0 },
	{ .width = 1920,
	  .height = 1080,
	  .framerates = ar0235_framerates_1920x1080,
	  .framerates_size = ARRAY_SIZE(ar0235_framerates_1920x1080),
	  .mode = 0 },
	{ .width = 1920,
	  .height = 1200,
	  .framerates = ar0235_framerates_1920x1200,
	  .framerates_size = ARRAY_SIZE(ar0235_framerates_1920x1200),
	  .mode = 0 },
};

/* AR0246 default setting for 4 data lanes and data frequency 800 MHz */
static u32 ar0246_code_list[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
};

static u16 ar0246_framerates_640x480[] = { 30, 20, 15, 10, 5 };
static u16 ar0246_framerates_1280x720[] = { 30, 20, 15, 10, 5 };
static u16 ar0246_framerates_1920x1080[] = { 30, 20, 15, 10, 5 };

static struct resolution ar0246_res_list[] = {
	{ .width = 640,
	  .height = 480,
	  .framerates = ar0246_framerates_640x480,
	  .framerates_size = ARRAY_SIZE(ar0246_framerates_640x480),
	  .mode = 0 },
	{ .width = 1280,
	  .height = 720,
	  .framerates = ar0246_framerates_1280x720,
	  .framerates_size = ARRAY_SIZE(ar0246_framerates_1280x720),
	  .mode = 0 },
	{ .width = 1920,
	  .height = 1080,
	  .framerates = ar0246_framerates_1920x1080,
	  .framerates_size = ARRAY_SIZE(ar0246_framerates_1920x1080),
	  .mode = 0 },
};

/* AR0521 default setting for 4 data lanes and data frequency 800 MHz */
static u32 ar0521_code_list[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
};

static u16 ar0521_framerates_640x480[] = { 120, 60, 30, 20, 15, 10, 5 };
static u16 ar0521_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0521_framerates_1280x960[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0521_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0521_framerates_2560x1440[] = { 32, 30, 20, 15, 10, 5 };
static u16 ar0521_framerates_2592x1944[] = { 24, 20, 15, 10, 5 };

static struct resolution ar0521_res_list[] = {
	{ .width = 640,
	  .height = 480,
	  .framerates = ar0521_framerates_640x480,
	  .framerates_size = ARRAY_SIZE(ar0521_framerates_640x480),
	  .mode = 3 },
	{ .width = 1280,
	  .height = 720,
	  .framerates = ar0521_framerates_1280x720,
	  .framerates_size = ARRAY_SIZE(ar0521_framerates_1280x720),
	  .mode = 3 },
	{ .width = 1280,
	  .height = 960,
	  .framerates = ar0521_framerates_1280x960,
	  .framerates_size = ARRAY_SIZE(ar0521_framerates_1280x960),
	  .mode = 3 },
	{ .width = 1920,
	  .height = 1080,
	  .framerates = ar0521_framerates_1920x1080,
	  .framerates_size = ARRAY_SIZE(ar0521_framerates_1920x1080),
	  .mode = 1 },
	{ .width = 2560,
	  .height = 1440,
	  .framerates = ar0521_framerates_2560x1440,
	  .framerates_size = ARRAY_SIZE(ar0521_framerates_2560x1440),
	  .mode = 1 },
	{ .width = 2592,
	  .height = 1944,
	  .framerates = ar0521_framerates_2592x1944,
	  .framerates_size = ARRAY_SIZE(ar0521_framerates_2592x1944),
	  .mode = 1 },
};

/* AR0522 default setting for 4 data lanes and data frequency 800 MHz */
static u32 ar0522_code_list[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
};

static u16 ar0522_framerates_640x480[] = { 120, 60, 30, 20, 15, 10, 5 };
static u16 ar0522_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0522_framerates_1280x960[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0522_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0522_framerates_2560x1440[] = { 32, 30, 20, 15, 10, 5 };
static u16 ar0522_framerates_2592x1944[] = { 24, 20, 15, 10, 5 };

static struct resolution ar0522_res_list[] = {
	{ .width = 640,
	  .height = 480,
	  .framerates = ar0522_framerates_640x480,
	  .framerates_size = ARRAY_SIZE(ar0522_framerates_640x480),
	  .mode = 3 },
	{ .width = 1280,
	  .height = 720,
	  .framerates = ar0522_framerates_1280x720,
	  .framerates_size = ARRAY_SIZE(ar0522_framerates_1280x720),
	  .mode = 3 },
	{ .width = 1280,
	  .height = 960,
	  .framerates = ar0522_framerates_1280x960,
	  .framerates_size = ARRAY_SIZE(ar0522_framerates_1280x960),
	  .mode = 3 },
	{ .width = 1920,
	  .height = 1080,
	  .framerates = ar0522_framerates_1920x1080,
	  .framerates_size = ARRAY_SIZE(ar0522_framerates_1920x1080),
	  .mode = 1 },
	{ .width = 2560,
	  .height = 1440,
	  .framerates = ar0522_framerates_2560x1440,
	  .framerates_size = ARRAY_SIZE(ar0522_framerates_2560x1440),
	  .mode = 1 },
	{ .width = 2592,
	  .height = 1944,
	  .framerates = ar0522_framerates_2592x1944,
	  .framerates_size = ARRAY_SIZE(ar0522_framerates_2592x1944),
	  .mode = 1 },
};

/* AR0544 default setting for 4 data lanes and data frequency 800 MHz */
static u32 ar0544_code_list[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
};

static u16 ar0544_framerates_640x480[] = { 120, 60, 30, 20, 15, 10, 5 };
static u16 ar0544_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0544_framerates_1280x960[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0544_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0544_framerates_2560x1440[] = { 32, 30, 20, 15, 10, 5 };
static u16 ar0544_framerates_2592x1944[] = { 24, 20, 15, 10, 5 };

static struct resolution ar0544_res_list[] = {
	{ .width = 640,
	  .height = 480,
	  .framerates = ar0544_framerates_640x480,
	  .framerates_size = ARRAY_SIZE(ar0544_framerates_640x480),
	  .mode = 3 },
	{ .width = 1280,
	  .height = 720,
	  .framerates = ar0544_framerates_1280x720,
	  .framerates_size = ARRAY_SIZE(ar0544_framerates_1280x720),
	  .mode = 2 },
	{ .width = 1280,
	  .height = 960,
	  .framerates = ar0544_framerates_1280x960,
	  .framerates_size = ARRAY_SIZE(ar0544_framerates_1280x960),
	  .mode = 2 },
	{ .width = 1920,
	  .height = 1080,
	  .framerates = ar0544_framerates_1920x1080,
	  .framerates_size = ARRAY_SIZE(ar0544_framerates_1920x1080),
	  .mode = 2 },
	{ .width = 2560,
	  .height = 1440,
	  .framerates = ar0544_framerates_2560x1440,
	  .framerates_size = ARRAY_SIZE(ar0544_framerates_2560x1440),
	  .mode = 0 },
	{ .width = 2592,
	  .height = 1944,
	  .framerates = ar0544_framerates_2592x1944,
	  .framerates_size = ARRAY_SIZE(ar0544_framerates_2592x1944),
	  .mode = 0 },
};

/* AR0821 default setting for 4 data lanes and data frequency 800 MHz */
static u32 ar0821_code_list[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
};

static u16 ar0821_framerates_640x480[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0821_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0821_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0821_framerates_2560x1440[] = { 30, 20, 15, 10, 5 };
static u16 ar0821_framerates_3840x2160[] = { 15, 10, 5 };

static struct resolution ar0821_res_list[] = {
	{ .width = 640,
	  .height = 480,
	  .framerates = ar0821_framerates_640x480,
	  .framerates_size = ARRAY_SIZE(ar0821_framerates_640x480),
	  .mode = 2 },
	{ .width = 1280,
	  .height = 720,
	  .framerates = ar0821_framerates_1280x720,
	  .framerates_size = ARRAY_SIZE(ar0821_framerates_1280x720),
	  .mode = 2 },
	{ .width = 1920,
	  .height = 1080,
	  .framerates = ar0821_framerates_1920x1080,
	  .framerates_size = ARRAY_SIZE(ar0821_framerates_1920x1080),
	  .mode = 2 },
	{ .width = 2560,
	  .height = 1440,
	  .framerates = ar0821_framerates_2560x1440,
	  .framerates_size = ARRAY_SIZE(ar0821_framerates_2560x1440),
	  .mode = 0 },
	{ .width = 3840,
	  .height = 2160,
	  .framerates = ar0821_framerates_3840x2160,
	  .framerates_size = ARRAY_SIZE(ar0821_framerates_3840x2160),
	  .mode = 0 },
};

/* AR0822 default setting for 4 data lanes and data frequency 800 MHz */
static u32 ar0822_code_list[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
};

static u16 ar0822_framerates_640x480[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0822_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0822_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0822_framerates_2560x1440[] = { 30, 20, 15, 10, 5 };
static u16 ar0822_framerates_3840x2160[] = { 15, 10, 5 };

static struct resolution ar0822_res_list[] = {
	{ .width = 640,
	  .height = 480,
	  .framerates = ar0822_framerates_640x480,
	  .framerates_size = ARRAY_SIZE(ar0822_framerates_640x480),
	  .mode = 1 },
	{ .width = 1280,
	  .height = 720,
	  .framerates = ar0822_framerates_1280x720,
	  .framerates_size = ARRAY_SIZE(ar0822_framerates_1280x720),
	  .mode = 1 },
	{ .width = 1920,
	  .height = 1080,
	  .framerates = ar0822_framerates_1920x1080,
	  .framerates_size = ARRAY_SIZE(ar0822_framerates_1920x1080),
	  .mode = 1 },
	{ .width = 2560,
	  .height = 1440,
	  .framerates = ar0822_framerates_2560x1440,
	  .framerates_size = ARRAY_SIZE(ar0822_framerates_2560x1440),
	  .mode = 0 },
	{ .width = 3840,
	  .height = 2160,
	  .framerates = ar0822_framerates_3840x2160,
	  .framerates_size = ARRAY_SIZE(ar0822_framerates_3840x2160),
	  .mode = 0 },
};

/* AR0830 default setting for 4 data lanes and data frequency 800 MHz */
static u32 ar0830_code_list[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
};

static u16 ar0830_framerates_640x480[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0830_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0830_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar0830_framerates_2560x1440[] = { 30, 20, 15, 10, 5 };
static u16 ar0830_framerates_3840x2160[] = { 15, 10, 5 };

static struct resolution ar0830_res_list[] = {
	{ .width = 640,
	  .height = 480,
	  .framerates = ar0830_framerates_640x480,
	  .framerates_size = ARRAY_SIZE(ar0830_framerates_640x480),
	  .mode = 3 },
	{ .width = 1280,
	  .height = 720,
	  .framerates = ar0830_framerates_1280x720,
	  .framerates_size = ARRAY_SIZE(ar0830_framerates_1280x720),
	  .mode = 2 },
	{ .width = 1920,
	  .height = 1080,
	  .framerates = ar0830_framerates_1920x1080,
	  .framerates_size = ARRAY_SIZE(ar0830_framerates_1920x1080),
	  .mode = 2 },
	{ .width = 2560,
	  .height = 1440,
	  .framerates = ar0830_framerates_2560x1440,
	  .framerates_size = ARRAY_SIZE(ar0830_framerates_2560x1440),
	  .mode = 1 },
	{ .width = 3840,
	  .height = 2160,
	  .framerates = ar0830_framerates_3840x2160,
	  .framerates_size = ARRAY_SIZE(ar0830_framerates_3840x2160),
	  .mode = 1 },
};

/* AR1335 default setting for 4 data lanes and data frequency 800 MHz */
static u32 ar1335_code_list[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
};

static u16 ar1335_framerates_640x480[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar1335_framerates_1280x720[] = { 120, 60, 30, 20, 15, 10, 5 };
static u16 ar1335_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static u16 ar1335_framerates_2560x1440[] = { 30, 20, 15, 10, 5 };
static u16 ar1335_framerates_3840x2160[] = { 15, 10, 5 };
static u16 ar1335_framerates_4208x3120[] = { 10, 5 };

static struct resolution ar1335_res_list[] = {
	{ .width = 640,
	  .height = 480,
	  .framerates = ar1335_framerates_640x480,
	  .framerates_size = ARRAY_SIZE(ar1335_framerates_640x480),
	  .mode = 4 },
	{ .width = 1280,
	  .height = 720,
	  .framerates = ar1335_framerates_1280x720,
	  .framerates_size = ARRAY_SIZE(ar1335_framerates_1280x720),
	  .mode = 4 },
	{ .width = 1920,
	  .height = 1080,
	  .framerates = ar1335_framerates_1920x1080,
	  .framerates_size = ARRAY_SIZE(ar1335_framerates_1920x1080),
	  .mode = 3 },
	{ .width = 2560,
	  .height = 1440,
	  .framerates = ar1335_framerates_2560x1440,
	  .framerates_size = ARRAY_SIZE(ar1335_framerates_2560x1440),
	  .mode = 1 },
	{ .width = 3840,
	  .height = 2160,
	  .framerates = ar1335_framerates_3840x2160,
	  .framerates_size = ARRAY_SIZE(ar1335_framerates_3840x2160),
	  .mode = 0 },
	{ .width = 4208,
	  .height = 3120,
	  .framerates = ar1335_framerates_4208x3120,
	  .framerates_size = ARRAY_SIZE(ar1335_framerates_4208x3120),
	  .mode = 0 },
};

/* AR2020 default setting for 4 data lanes and data frequency 800 MHz */
static u32 ar2020_code_list[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
};

// static u16 ar2020_framerates_640x480[] = { 120, 60, 30, 20, 15, 10, 5 };
// static u16 ar2020_framerates_1280x720[] = { 120, 60, 30, 20, 15, 10, 5 };
// static u16 ar2020_framerates_1280x960[] = { 100, 60, 30, 20, 15, 10, 5 };
static u16 ar2020_framerates_1920x1440[] = { 40, 30, 20, 15, 10, 5 };
static u16 ar2020_framerates_2560x1920[] = { 25, 20, 15, 10, 5 };
static u16 ar2020_framerates_4208x3156[] = { 10, 5 };

static struct resolution ar2020_res_list[] = {
	// { .width = 640,
	//   .height = 480,
	//   .framerates = ar2020_framerates_640x480,
	//   .framerates_size = ARRAY_SIZE(ar2020_framerates_640x480),
	//   .mode = 3 },
	// { .width = 1280,
	//   .height = 720,
	//   .framerates = ar2020_framerates_1280x720,
	//   .framerates_size = ARRAY_SIZE(ar2020_framerates_1280x720),
	//   .mode = 3 },
	// { .width = 1280,
	//   .height = 960,
	//   .framerates = ar2020_framerates_1280x960,
	//   .framerates_size = ARRAY_SIZE(ar2020_framerates_1280x960),
	//   .mode = 3 },
	{ .width = 1920,
	  .height = 1440,
	  .framerates = ar2020_framerates_1920x1440,
	  .framerates_size = ARRAY_SIZE(ar2020_framerates_1920x1440),
	  .mode = 2 },
	{ .width = 2560,
	  .height = 1920,
	  .framerates = ar2020_framerates_2560x1920,
	  .framerates_size = ARRAY_SIZE(ar2020_framerates_2560x1920),
	  .mode = 2 },
	{ .width = 4208,
	  .height = 3156,
	  .framerates = ar2020_framerates_4208x3156,
	  .framerates_size = ARRAY_SIZE(ar2020_framerates_4208x3156),
	  .mode = 0 },
};
#else
static const int __5fps = 5;
static const int __10fps = 10;
static const int __15fps = 15;
static const int __20fps = 20;
static const int __24fps = 24;
static const int __25fps = 25;
static const int __30fps = 30;
static const int __32fps = 32;
static const int __40fps = 40;
static const int __60fps = 60;
static const int __100fps = 100;
static const int __115fps = 115;
static const int __120fps = 120;

static const struct camera_common_frmfmt sensor_frmfmt[] = {
	{{640, 480}, &__60fps, 1, 0, 0},
	{{1280, 720}, &__60fps, 1, 0, 0},
	{{1920, 1080}, &__60fps, 1, 0, 0},
};

static const int ar0144_framerates_640x480[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0144_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0144_framerates_1280x800[] = { 60, 30, 20, 15, 10, 5 };

static const struct camera_common_frmfmt ar0144_frmfmt[] = {
	{ {640, 480},
	  ar0144_framerates_640x480,
	  ARRAY_SIZE(ar0144_framerates_640x480),
	  0,
	  0 },
	{ {1280, 720},
	  ar0144_framerates_1280x720,
	  ARRAY_SIZE(ar0144_framerates_1280x720),
	  0,
	  0 },
	{ {1280, 800},
	  ar0144_framerates_1280x800,
	  ARRAY_SIZE(ar0144_framerates_1280x800),
	  0,
	  0 },
};

static const int ar0145_framerates_640x480[] = { 115, 60, 30, 20, 15, 10, 5 };
static const int ar0145_framerates_1280x720[] = { 115, 60, 30, 20, 15, 10, 5 };
static const int ar0145_framerates_1280x800[] = { 115, 60, 30, 20, 15, 10, 5 };

static const struct camera_common_frmfmt ar0145_frmfmt[] = {
	{ {640, 480},
	  ar0145_framerates_640x480,
	  ARRAY_SIZE(ar0145_framerates_640x480),
	  0,
	  0 },
	{ {1280, 720},
	  ar0145_framerates_1280x720,
	  ARRAY_SIZE(ar0145_framerates_1280x720),
	  0,
	  0 },
	{ {1280, 800},
	  ar0145_framerates_1280x800,
	  ARRAY_SIZE(ar0145_framerates_1280x800),
	  0,
	  0 },
};

static const int ar0234_framerates_640x480[] = { 120, 60, 30, 20, 15, 10, 5 };
static const int ar0234_framerates_1280x720[] = { 120, 60, 30, 20, 15, 10, 5 };
static const int ar0234_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0234_framerates_1920x1200[] = { 60, 30, 20, 15, 10, 5 };

static const struct camera_common_frmfmt ar0234_frmfmt[] = {
	{ {640, 480},
	  ar0234_framerates_640x480,
	  ARRAY_SIZE(ar0234_framerates_640x480),
	  0,
	  1 },
	{ {1280, 720},
	  ar0234_framerates_1280x720,
	  ARRAY_SIZE(ar0234_framerates_1280x720),
	  0,
	  0 },
	{ {1920, 1080},
	  ar0234_framerates_1920x1080,
	  ARRAY_SIZE(ar0234_framerates_1920x1080),
	  0,
	  0 },
	{ {1920, 1200},
	  ar0234_framerates_1920x1200,
	  ARRAY_SIZE(ar0234_framerates_1920x1200),
	  0,
	  0 },
};

static const int ar0235_framerates_640x480[] = { 120, 60, 30, 20, 15, 10, 5 };
static const int ar0235_framerates_1280x720[] = { 120, 60, 30, 20, 15, 10, 5 };
static const int ar0235_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0235_framerates_1920x1200[] = { 60, 30, 20, 15, 10, 5 };

static const struct camera_common_frmfmt ar0235_frmfmt[] = {
	{ {640, 480},
	  ar0235_framerates_640x480,
	  ARRAY_SIZE(ar0235_framerates_640x480),
	  0,
	  0 },
	{ {1280, 720},
	  ar0235_framerates_1280x720,
	  ARRAY_SIZE(ar0235_framerates_1280x720),
	  0,
	  0 },
	{ {1920, 1080},
	  ar0235_framerates_1920x1080,
	  ARRAY_SIZE(ar0235_framerates_1920x1080),
	  0,
	  0 },
	{ {1920, 1200},
	  ar0235_framerates_1920x1200,
	  ARRAY_SIZE(ar0235_framerates_1920x1200),
	  0,
	  0 },
};

static const int ar0246_framerates_640x480[] = { 30, 20, 15, 10, 5 };
static const int ar0246_framerates_1280x720[] = { 30, 20, 15, 10, 5 };
static const int ar0246_framerates_1920x1080[] = { 30, 20, 15, 10, 5 };

static const struct camera_common_frmfmt ar0246_frmfmt[] = {
	{ {640, 480},
	  ar0246_framerates_640x480,
	  ARRAY_SIZE(ar0246_framerates_640x480),
	  0,
	  0 },
	{ {1280, 720},
	  ar0246_framerates_1280x720,
	  ARRAY_SIZE(ar0246_framerates_1280x720),
	  0,
	  0 },
	{ {1920, 1080},
	  ar0246_framerates_1920x1080,
	  ARRAY_SIZE(ar0246_framerates_1920x1080),
	  0,
	  0 },
};

static const int ar0521_framerates_640x480[] = { 120, 60, 30, 20, 15, 10, 5 };
static const int ar0521_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0521_framerates_1280x960[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0521_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0521_framerates_2560x1440[] = { 32, 30, 20, 15, 10, 5 };
static const int ar0521_framerates_2592x1944[] = { 24, 20, 15, 10, 5 };

static const struct camera_common_frmfmt ar0521_frmfmt[] = {
	{ {640, 480},
	  ar0521_framerates_640x480,
	  ARRAY_SIZE(ar0521_framerates_640x480),
	  0,
	  3 },
	{ {1280, 720},
	  ar0521_framerates_1280x720,
	  ARRAY_SIZE(ar0521_framerates_1280x720),
	  0,
	  3 },
	{ {1280, 960},
	  ar0521_framerates_1280x960,
	  ARRAY_SIZE(ar0521_framerates_1280x960),
	  0,
	  3 },
	{ {1920, 1080},
	  ar0521_framerates_1920x1080,
	  ARRAY_SIZE(ar0521_framerates_1920x1080),
	  0,
	  1  },
	{ {2560, 1440},
	  ar0521_framerates_2560x1440,
	  ARRAY_SIZE(ar0521_framerates_2560x1440),
	  0,
	  1 },
	{ {2592, 1944},
	  ar0521_framerates_2592x1944,
	  ARRAY_SIZE(ar0521_framerates_2592x1944),
	  0,
	  1 },
};

static const int ar0522_framerates_640x480[] = { 120, 60, 30, 20, 15, 10, 5 };
static const int ar0522_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0522_framerates_1280x960[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0522_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0522_framerates_2560x1440[] = { 32, 30, 20, 15, 10, 5 };
static const int ar0522_framerates_2592x1944[] = { 24, 20, 15, 10, 5 };

static const struct camera_common_frmfmt ar0522_frmfmt[] = {
	{ {640, 480},
	  ar0522_framerates_640x480,
	  ARRAY_SIZE(ar0522_framerates_640x480),
	  0,
	  3 },
	{ {1280, 720},
	  ar0522_framerates_1280x720,
	  ARRAY_SIZE(ar0522_framerates_1280x720),
	  0,
	  3 },
	{ {1280, 960},
	  ar0522_framerates_1280x960,
	  ARRAY_SIZE(ar0522_framerates_1280x960),
	  0,
	  3 },
	{ {1920, 1080},
	  ar0522_framerates_1920x1080,
	  ARRAY_SIZE(ar0522_framerates_1920x1080),
	  0,
	  1 },
	{ {2560, 1440},
	  ar0522_framerates_2560x1440,
	  ARRAY_SIZE(ar0522_framerates_2560x1440),
	  0,
	  1 },
	{ {2592, 1944},
	  ar0522_framerates_2592x1944,
	  ARRAY_SIZE(ar0522_framerates_2592x1944),
	  0,
	  1 },
};

static const int ar0544_framerates_640x480[] = { 120, 60, 30, 20, 15, 10, 5 };
static const int ar0544_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0544_framerates_1280x960[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0544_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0544_framerates_2560x1440[] = { 32, 30, 20, 15, 10, 5 };
static const int ar0544_framerates_2592x1944[] = { 24, 20, 15, 10, 5 };

static const struct camera_common_frmfmt ar0544_frmfmt[] = {
	{ {640, 480},
	  ar0544_framerates_640x480,
	  ARRAY_SIZE(ar0544_framerates_640x480),
	  0,
	  3 },
	{ {1280, 720},
	  ar0544_framerates_1280x720,
	  ARRAY_SIZE(ar0544_framerates_1280x720),
	  0,
	  2 },
	{ {1280, 960},
	  ar0544_framerates_1280x960,
	  ARRAY_SIZE(ar0544_framerates_1280x960),
	  0,
	  2 },
	{ {1920, 1080},
	  ar0544_framerates_1920x1080,
	  ARRAY_SIZE(ar0544_framerates_1920x1080),
	  0,
	  2 },
	{ {2560, 1440},
	  ar0544_framerates_2560x1440,
	  ARRAY_SIZE(ar0544_framerates_2560x1440),
	  0,
	  0 },
	{ {2592, 1944},
	  ar0544_framerates_2592x1944,
	  ARRAY_SIZE(ar0544_framerates_2592x1944),
	  0,
	  0 },
};

static const int ar0821_framerates_640x480[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0821_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0821_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0821_framerates_2560x1440[] = { 30, 20, 15, 10, 5 };
static const int ar0821_framerates_3840x2160[] = { 15, 10, 5 };

static const struct camera_common_frmfmt ar0821_frmfmt[] = {
	{ {640, 480},
	  ar0821_framerates_640x480,
	  ARRAY_SIZE(ar0821_framerates_640x480),
	  0,
	  2 },
	{ {1280, 720},
	  ar0821_framerates_1280x720,
	  ARRAY_SIZE(ar0821_framerates_1280x720),
	  0,
	  2 },
	{ {1920, 1080},
	  ar0821_framerates_1920x1080,
	  ARRAY_SIZE(ar0821_framerates_1920x1080),
	  0,
	  2 },
	{ {2560, 1440},
	  ar0821_framerates_2560x1440,
	  ARRAY_SIZE(ar0821_framerates_2560x1440),
	  0,
	  0 },
	{ {3840, 2160},
	  ar0821_framerates_3840x2160,
	  ARRAY_SIZE(ar0821_framerates_3840x2160),
	  0,
	  0 },
};

static const int ar0822_framerates_640x480[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0822_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0822_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0822_framerates_2560x1440[] = { 30, 20, 15, 10, 5 };
static const int ar0822_framerates_3840x2160[] = { 15, 10, 5 };

static const struct camera_common_frmfmt ar0822_frmfmt[] = {
	{ {640, 480},
	  ar0822_framerates_640x480,
	  ARRAY_SIZE(ar0822_framerates_640x480),
	  0,
	  1 },
	{ {1280, 720},
	  ar0822_framerates_1280x720,
	  ARRAY_SIZE(ar0822_framerates_1280x720),
	  0,
	  1 },
	{ {1920, 1080},
	  ar0822_framerates_1920x1080,
	  ARRAY_SIZE(ar0822_framerates_1920x1080),
	  0,
	  1 },
	{ {2560, 1440},
	  ar0822_framerates_2560x1440,
	  ARRAY_SIZE(ar0822_framerates_2560x1440),
	  0,
	  0 },
	{ {3840, 2160},
	  ar0822_framerates_3840x2160,
	  ARRAY_SIZE(ar0822_framerates_3840x2160),
	  0,
	  0 },
};

static const int ar0830_framerates_640x480[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0830_framerates_1280x720[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0830_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static const int ar0830_framerates_2560x1440[] = { 30, 20, 15, 10, 5 };
static const int ar0830_framerates_3840x2160[] = { 15, 10, 5 };

static const struct camera_common_frmfmt ar0830_frmfmt[] = {
	{ {640, 480},
	  ar0830_framerates_640x480,
	  ARRAY_SIZE(ar0830_framerates_640x480),
	  0,
	  3 },
	{ {1280, 720},
	  ar0830_framerates_1280x720,
	  ARRAY_SIZE(ar0830_framerates_1280x720),
	  0,
	  2 },
	{ {1920, 1080},
	  ar0830_framerates_1920x1080,
	  ARRAY_SIZE(ar0830_framerates_1920x1080),
	  0,
	  2 },
	{ {2560, 1440},
	  ar0830_framerates_2560x1440,
	  ARRAY_SIZE(ar0830_framerates_2560x1440),
	  0,
	  1 },
	{ {3840, 2160},
	  ar0830_framerates_3840x2160,
	  ARRAY_SIZE(ar0830_framerates_3840x2160),
	  0,
	  1 },
};

static const int ar1335_framerates_640x480[] = { 60, 30, 20, 15, 10, 5 };
static const int ar1335_framerates_1280x720[] = { 120, 60, 30, 20, 15, 10, 5 };
static const int ar1335_framerates_1920x1080[] = { 60, 30, 20, 15, 10, 5 };
static const int ar1335_framerates_2560x1440[] = { 30, 20, 15, 10, 5 };
static const int ar1335_framerates_3840x2160[] = { 15, 10, 5 };
static const int ar1335_framerates_4208x3120[] = { 10, 5 };

static const struct camera_common_frmfmt ar1335_frmfmt[] = {
	{ {640, 480},
	  ar1335_framerates_640x480,
	  ARRAY_SIZE(ar1335_framerates_640x480),
	  0,
	  4 },
	{ {1280, 720},
	  ar1335_framerates_1280x720,
	  ARRAY_SIZE(ar1335_framerates_1280x720),
	  0,
	  4 },
	{ {1920, 1080},
	  ar1335_framerates_1920x1080,
	  ARRAY_SIZE(ar1335_framerates_1920x1080),
	  0,
	  3 },
	{ {2560, 1440},
	  ar1335_framerates_2560x1440,
	  ARRAY_SIZE(ar1335_framerates_2560x1440),
	  0,
	  1 },
	{ {3840, 2160},
	  ar1335_framerates_3840x2160,
	  ARRAY_SIZE(ar1335_framerates_3840x2160),
	  0,
	  0 },
	{ {4224, 3120},
	  ar1335_framerates_4208x3120,
	  ARRAY_SIZE(ar1335_framerates_4208x3120),
	  0,
	  0 },
};

// static const int ar2020_framerates_640x480[] = { 120, 60, 30, 20, 15, 10, 5 };
// static const int ar2020_framerates_1280x720[] = { 120, 60, 30, 20, 15, 10, 5 };
// static const int ar2020_framerates_1280x960[] = { 100, 60, 30, 20, 15, 10, 5 };
static const int ar2020_framerates_1920x1440[] = { 40, 30, 20, 15, 10, 5 };
static const int ar2020_framerates_2560x1920[] = { 25, 20, 15, 10, 5 };
static const int ar2020_framerates_4208x3156[] = { 10, 5 };

static const struct camera_common_frmfmt ar2020_frmfmt[] = {
	// { {640, 480},
	//   ar2020_framerates_640x480,
	//   ARRAY_SIZE(ar2020_framerates_640x480),
	//   0,
	//   3 },
	// { {1280, 720},
	//   ar2020_framerates_1280x720,
	//   ARRAY_SIZE(ar2020_framerates_1280x720),
	//   0,
	//   3 },
	// { {1280, 960},
	//   ar2020_framerates_1280x960,
	//   ARRAY_SIZE(ar2020_framerates_1280x960),
	//   0,
	//   3 },
	{ {1920, 1440},
	  ar2020_framerates_1920x1440,
	  ARRAY_SIZE(ar2020_framerates_1920x1440),
	  0,
	  2 },
	{ {2560, 1920},
	  ar2020_framerates_2560x1920,
	  ARRAY_SIZE(ar2020_framerates_2560x1920),
	  0,
	  2 },
	{ {4224, 3156},
	  ar2020_framerates_4208x3156,
	  ARRAY_SIZE(ar2020_framerates_4208x3156),
	  0,
	  0 },
};

struct sensor_info {
    const u16 chip_id;
	const char *sensor_name;
	const struct camera_common_frmfmt *frmfmt;
	u32 res_list_size;
};


static struct sensor_info tevs_sensor_table[] = {
	{ .chip_id = SENSOR_CHIP_ID_ONSEMI_AR0144,
      .sensor_name = "TEVS-AR0144",
	  .frmfmt = ar0144_frmfmt,
	  .res_list_size = ARRAY_SIZE(ar0144_frmfmt) },
	{ .chip_id = SENSOR_CHIP_ID_ONSEMI_AR0145,
      .sensor_name = "TEVS-AR0145",
	  .frmfmt = ar0145_frmfmt,
	  .res_list_size = ARRAY_SIZE(ar0145_frmfmt) },
	{ .chip_id = SENSOR_CHIP_ID_ONSEMI_AR0234,
      .sensor_name = "TEVS-AR0234",
	  .frmfmt = ar0234_frmfmt,
	  .res_list_size = ARRAY_SIZE(ar0234_frmfmt) },
	{ .chip_id = SENSOR_CHIP_ID_ONSEMI_AR0235,
      .sensor_name = "TEVS-AR0235",
	  .frmfmt = ar0235_frmfmt,
	  .res_list_size = ARRAY_SIZE(ar0235_frmfmt) },
	{ .chip_id = SENSOR_CHIP_ID_ONSEMI_AR0246,
      .sensor_name = "TEVS-AR0246",
	  .frmfmt = ar0246_frmfmt,
	  .res_list_size = ARRAY_SIZE(ar0246_frmfmt) },
	{ .chip_id = SENSOR_CHIP_ID_ONSEMI_AR0521,
      .sensor_name = "TEVS-AR0521",
	  .frmfmt = ar0521_frmfmt,
	  .res_list_size = ARRAY_SIZE(ar0521_frmfmt) },
	{ .chip_id = SENSOR_CHIP_ID_ONSEMI_AR0522,
      .sensor_name = "TEVS-AR0522",
	  .frmfmt = ar0522_frmfmt,
	  .res_list_size = ARRAY_SIZE(ar0522_frmfmt) },
	{ .chip_id = SENSOR_CHIP_ID_ONSEMI_AR0544,
      .sensor_name = "TEVS-AR0544",
	  .frmfmt = ar0544_frmfmt,
	  .res_list_size = ARRAY_SIZE(ar0544_frmfmt) },
	{ .chip_id = SENSOR_CHIP_ID_ONSEMI_AR0821,
      .sensor_name = "TEVS-AR0821",
	  .frmfmt = ar0821_frmfmt,
	  .res_list_size = ARRAY_SIZE(ar0821_frmfmt) },
	{ .chip_id = SENSOR_CHIP_ID_ONSEMI_AR0822,
      .sensor_name = "TEVS-AR0822",
	  .frmfmt = ar0822_frmfmt,
	  .res_list_size = ARRAY_SIZE(ar0822_frmfmt) },
	{ .chip_id = SENSOR_CHIP_ID_ONSEMI_AR0830,
      .sensor_name = "TEVS-AR0830",
	  .frmfmt = ar0830_frmfmt,
	  .res_list_size = ARRAY_SIZE(ar0830_frmfmt) },
	{ .chip_id = SENSOR_CHIP_ID_ONSEMI_AR1335,
      .sensor_name = "TEVS-AR1335",
	  .frmfmt = ar1335_frmfmt,
	  .res_list_size = ARRAY_SIZE(ar1335_frmfmt) },
	{ .chip_id = SENSOR_CHIP_ID_ONSEMI_AR2020,
      .sensor_name = "TEVS-AR2020",
	  .frmfmt = ar2020_frmfmt,
	  .res_list_size = ARRAY_SIZE(ar2020_frmfmt) },
};

#endif

#endif //__SENSOR_TABLES_H__