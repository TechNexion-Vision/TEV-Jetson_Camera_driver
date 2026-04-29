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
	u16 framerates;
	u16 mode;
};

static struct resolution ar0144_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 60, .mode = 0 },
	{ .width = 1280, .height = 720, .framerates = 60, .mode = 0 },
	{ .width = 1280, .height = 800, .framerates = 60, .mode = 0 },
};

static struct resolution ar0145_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 115, .mode = 0 },
	{ .width = 1280, .height = 720, .framerates = 115, .mode = 0 },
	{ .width = 1280, .height = 800, .framerates = 115, .mode = 0 },
};

static struct resolution ar0234_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 120, .mode = 1 },
	{ .width = 1280, .height = 720, .framerates = 120, .mode = 0 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 0 },
	{ .width = 1920, .height = 1200, .framerates = 60, .mode = 0 },
};

static struct resolution ar0235_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 120, .mode = 0 },
	{ .width = 1280, .height = 720, .framerates = 120, .mode = 0 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 0 },
	{ .width = 1920, .height = 1200, .framerates = 60, .mode = 0 },
};

static struct resolution ar0246_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 30, .mode = 0 },
	{ .width = 1280, .height = 720, .framerates = 30, .mode = 0 },
	{ .width = 1920, .height = 1080, .framerates = 30, .mode = 0 },
};

static struct resolution ar0521_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 120, .mode = 3 },
	{ .width = 1280, .height = 720, .framerates = 60, .mode = 3 },
	{ .width = 1280, .height = 960, .framerates = 60, .mode = 3 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 1 },
	{ .width = 2560, .height = 1440, .framerates = 32, .mode = 1 },
	{ .width = 2592, .height = 1944, .framerates = 24, .mode = 1 },
};

static struct resolution ar0522_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 120, .mode = 3 },
	{ .width = 1280, .height = 720, .framerates = 60, .mode = 3 },
	{ .width = 1280, .height = 960, .framerates = 60, .mode = 3 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 1 },
	{ .width = 2560, .height = 1440, .framerates = 32, .mode = 1 },
	{ .width = 2592, .height = 1944, .framerates = 24, .mode = 1 },
};

static struct resolution ar0544_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 120, .mode = 3 },
	{ .width = 1280, .height = 720, .framerates = 60, .mode = 2 },
	{ .width = 1280, .height = 960, .framerates = 60, .mode = 2 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 2 },
	{ .width = 2560, .height = 1440, .framerates = 32, .mode = 0 },
	{ .width = 2592, .height = 1944, .framerates = 24, .mode = 0 },
};

static struct resolution ar0821_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 60, .mode = 2 },
	{ .width = 1280, .height = 720, .framerates = 60, .mode = 2 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 2 },
	{ .width = 2560, .height = 1440, .framerates = 30, .mode = 0 },
	{ .width = 3840, .height = 2160, .framerates = 15, .mode = 0 },
};

static struct resolution ar0822_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 60, .mode = 1 },
	{ .width = 1280, .height = 720, .framerates = 60, .mode = 1 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 1 },
	{ .width = 2560, .height = 1440, .framerates = 30, .mode = 0 },
	{ .width = 3840, .height = 2160, .framerates = 15, .mode = 0 },
};

static struct resolution ar0830_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 60, .mode = 3 },
	{ .width = 1280, .height = 720, .framerates = 60, .mode = 2 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 2 },
	{ .width = 2560, .height = 1440, .framerates = 30, .mode = 1 },
	{ .width = 3840, .height = 2160, .framerates = 15, .mode = 1 },
};

static struct resolution ar1335_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 60, .mode = 4 },
	{ .width = 1280, .height = 720, .framerates = 120, .mode = 4 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 3 },
	{ .width = 2560, .height = 1440, .framerates = 30, .mode = 1 },
	{ .width = 3840, .height = 2160, .framerates = 15, .mode = 0 },
	{ .width = 4224, .height = 3120, .framerates = 10, .mode = 0 },
};

static struct resolution ar2020_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 120, .mode = 3 },
	{ .width = 1280, .height = 720, .framerates = 120, .mode = 3 },
	{ .width = 1280, .height = 960, .framerates = 100, .mode = 3 },
	{ .width = 1920, .height = 1440, .framerates = 40, .mode = 2 },
	{ .width = 2160, .height = 1920, .framerates = 25, .mode = 2 },
	{ .width = 4208, .height = 3156, .framerates = 10, .mode = 0 },
};

struct sensor_info {
	const char* sensor_name;
	const struct resolution *res_list;
	u32 res_list_size;
};

static struct sensor_info tevs_sensor_table[] = {
	{
		.sensor_name = "TEVS-AR0144",
		.res_list = ar0144_res_list,
		.res_list_size = ARRAY_SIZE(ar0144_res_list)
	},
	{
		.sensor_name = "TEVS-AR0145",
		.res_list = ar0144_res_list,
		.res_list_size = ARRAY_SIZE(ar0145_res_list)
	},
	{
		.sensor_name = "TEVS-AR0234",
		.res_list = ar0234_res_list,
		.res_list_size = ARRAY_SIZE(ar0234_res_list)
	},
	{
		.sensor_name = "TEVS-AR0235",
		.res_list = ar0235_res_list,
		.res_list_size = ARRAY_SIZE(ar0235_res_list)
	},
	{
		.sensor_name = "TEVS-AR0246",
		.res_list = ar0246_res_list,
		.res_list_size = ARRAY_SIZE(ar0246_res_list)
	},
	{
		.sensor_name = "TEVS-AR0521",
		.res_list = ar0521_res_list,
		.res_list_size = ARRAY_SIZE(ar0521_res_list)
	},
	{
		.sensor_name = "TEVS-AR0522",
		.res_list = ar0522_res_list,
		.res_list_size = ARRAY_SIZE(ar0522_res_list)
	},
	{
		.sensor_name = "TEVS-AR0544",
		.res_list = ar0544_res_list,
		.res_list_size = ARRAY_SIZE(ar0544_res_list)
	},
	{
		.sensor_name = "TEVS-AR0821",
		.res_list = ar0821_res_list,
		.res_list_size = ARRAY_SIZE(ar0821_res_list)
	},
	{
		.sensor_name = "TEVS-AR0822",
		.res_list = ar0822_res_list,
		.res_list_size = ARRAY_SIZE(ar0822_res_list)
	},
	{
		.sensor_name = "TEVS-AR0830",
		.res_list = ar0830_res_list,
		.res_list_size = ARRAY_SIZE(ar0830_res_list)
	},
	{
		.sensor_name = "TEVS-AR1335",
		.res_list = ar1335_res_list,
		.res_list_size = ARRAY_SIZE(ar1335_res_list)
	},
	{
		.sensor_name = "TEVS-AR2020",
		.res_list = ar2020_res_list,
		.res_list_size = ARRAY_SIZE(ar2020_res_list)
	},
};
#else
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

static const struct camera_common_frmfmt ar0144_frmfmt[] = {
	{{640, 480}, &__60fps, 1, 0, 0},
	{{1280, 720}, &__60fps, 1, 0, 0},
	{{1280, 800}, &__60fps, 1, 0, 0},
};

static const struct camera_common_frmfmt ar0145_frmfmt[] = {
	{{640, 480}, &__115fps, 1, 0, 0},
	{{1280, 720}, &__115fps, 1, 0, 0},
	{{1280, 800}, &__115fps, 1, 0, 0},
};

static const struct camera_common_frmfmt ar0234_frmfmt[] = {
	{{640, 480}, &__120fps, 1, 0, 1},
	{{1280, 720}, &__120fps, 1, 0, 0},
	{{1920, 1080}, &__60fps, 1, 0, 0},
	{{1920, 1200}, &__60fps, 1, 0, 0},
};

static const struct camera_common_frmfmt ar0235_frmfmt[] = {
	{{640, 480}, &__120fps, 1, 0, 0},
	{{1280, 720}, &__120fps, 1, 0, 0},
	{{1920, 1080}, &__60fps, 1, 0, 0},
	{{1920, 1200}, &__60fps, 1, 0, 0},
};

static const struct camera_common_frmfmt ar0246_frmfmt[] = {
	{{640, 480}, &__30fps, 1, 0, 0},
	{{1280, 720}, &__30fps, 1, 0, 0},
	{{1920, 1080}, &__30fps, 1, 0, 0},
};

static const struct camera_common_frmfmt ar0521_frmfmt[] = {
	{{640, 480}, &__120fps, 1, 0, 3},
	{{1280, 720}, &__60fps, 1, 0, 3},
	{{1280, 960}, &__60fps, 1, 0, 3},
	{{1920, 1080}, &__60fps, 1, 0, 1},
	{{2560, 1440}, &__32fps, 1, 0, 1},
	{{2592, 1944}, &__24fps, 1, 0, 1},
};

static const struct camera_common_frmfmt ar0522_frmfmt[] = {
	{{640, 480}, &__120fps, 1, 0, 3},
	{{1280, 720}, &__60fps, 1, 0, 3},
	{{1280, 960}, &__60fps, 1, 0, 3},
	{{1920, 1080}, &__60fps, 1, 0, 1},
	{{2560, 1440}, &__32fps, 1, 0, 1},
	{{2592, 1944}, &__24fps, 1, 0, 1},
};

static const struct camera_common_frmfmt ar0544_frmfmt[] = {
	{{640, 480}, &__120fps, 1, 0, 3},
	{{1280, 720}, &__60fps, 1, 0, 2},
	{{1280, 960}, &__60fps, 1, 0, 2},
	{{1920, 1080}, &__60fps, 1, 0, 2},
	{{2560, 1440}, &__32fps, 1, 0, 0},
	{{2592, 1944}, &__24fps, 1, 0, 0},
};

static const struct camera_common_frmfmt ar0821_frmfmt[] = {
	{{640, 480}, &__60fps, 1, 0, 2},
	{{1280, 720}, &__60fps, 1, 0, 2},
	{{1920, 1080}, &__60fps, 1, 0, 2},
	{{2560, 1440}, &__30fps, 1, 0, 0},
	{{3840, 2160}, &__15fps, 1, 0, 0},
};

static const struct camera_common_frmfmt ar0822_frmfmt[] = {
	{{640, 480}, &__60fps, 1, 0, 1},
	{{1280, 720}, &__60fps, 1, 0, 1},
	{{1920, 1080}, &__60fps, 1, 0, 1},
	{{2560, 1440}, &__30fps, 1, 0, 0},
	{{3840, 2160}, &__15fps, 1, 0, 0},
};

static const struct camera_common_frmfmt ar0830_frmfmt[] = {
	{{640, 480}, &__60fps, 1, 0, 3},
	{{1280, 720}, &__60fps, 1, 0, 2},
	{{1920, 1080}, &__60fps, 1, 0, 2},
	{{2560, 1440}, &__30fps, 1, 0, 1},
	{{3840, 2160}, &__15fps, 1, 0, 1},
};

static const struct camera_common_frmfmt ar1335_frmfmt[] = {
	{{640, 480}, &__60fps, 1, 0, 4},
	{{1280, 720}, &__120fps, 1, 0, 4},
	{{1920, 1080}, &__60fps, 1, 0, 3},
	{{2560, 1440}, &__30fps, 1, 0, 1},
	{{3840, 2160}, &__15fps, 1, 0, 0},
	{{4224, 3120}, &__10fps, 1, 0, 0},
};

static const struct camera_common_frmfmt ar2020_frmfmt[] = {
	{{640, 480}, &__120fps, 1, 0, 3},
	{{1280, 720}, &__120fps, 1, 0, 3},
	{{1280, 960}, &__100fps, 1, 0, 3},
	{{1920, 1440}, &__40fps, 1, 0, 2},
	{{2560, 1920}, &__25fps, 1, 0, 2},
	{{4224, 3156}, &__10fps, 1, 0, 0},
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