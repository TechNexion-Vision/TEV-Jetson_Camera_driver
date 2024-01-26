#ifndef __SENSOR_TABLES_H__
#define __SENSOR_TABLES_H__

enum
{
	TEVS_AR0144 = 0,
	TEVS_AR0234,
	TEVS_AR0521,
	TEVS_AR0522,
	TEVS_AR0821,
	TEVS_AR0822,
	TEVS_AR1335,
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

static struct resolution ar0234_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 120, .mode = 1 },
	{ .width = 1280, .height = 720, .framerates = 120, .mode = 0 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 0 },
	{ .width = 1920, .height = 1200, .framerates = 60, .mode = 0 },
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

static struct resolution ar1335_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 60, .mode = 4 },
	{ .width = 1280, .height = 720, .framerates = 120, .mode = 4 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 3 },
	{ .width = 2560, .height = 1440, .framerates = 30, .mode = 1 },
	{ .width = 3840, .height = 2160, .framerates = 15, .mode = 0 },
	{ .width = 4208, .height = 3120, .framerates = 10, .mode = 0 },
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
		.sensor_name = "TEVS-AR0234",
		.res_list = ar0234_res_list,
		.res_list_size = ARRAY_SIZE(ar0234_res_list)
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
		.sensor_name = "TEVS-AR1335",
		.res_list = ar1335_res_list,
		.res_list_size = ARRAY_SIZE(ar1335_res_list)
	},
};
#else
static const int __10fps = 10;
static const int __15fps = 15;
static const int __20fps = 20;
static const int __24fps = 24;
static const int __30fps = 30;
static const int __32fps = 32;
static const int __60fps = 60;
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
static const struct camera_common_frmfmt ar0234_frmfmt[] = {
	{{640, 480}, &__120fps, 1, 0, 1},
	{{1280, 720}, &__120fps, 1, 0, 0},
	{{1920, 1080}, &__60fps, 1, 0, 0},
	{{1920, 1200}, &__60fps, 1, 0, 0},
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
static const struct camera_common_frmfmt ar1335_frmfmt[] = {
	{{640, 480}, &__60fps, 1, 0, 4},
	{{1280, 720}, &__120fps, 1, 0, 4},
	{{1920, 1080}, &__60fps, 1, 0, 3},
	{{2560, 1440}, &__30fps, 1, 0, 1},
	{{3840, 2160}, &__15fps, 1, 0, 0},
	{{4208, 3120}, &__10fps, 1, 0, 0},
};

struct sensor_info {
	const char* sensor_name;
	const struct camera_common_frmfmt *frmfmt;
	u32 res_list_size;
};


static struct sensor_info tevs_sensor_table[] = {
	{
		.sensor_name = "TEVS-AR0144",
		.frmfmt = ar0144_frmfmt,
		.res_list_size = ARRAY_SIZE(ar0144_frmfmt)
	},
	{
		.sensor_name = "TEVS-AR0234",
		.frmfmt = ar0234_frmfmt,
		.res_list_size = ARRAY_SIZE(ar0234_frmfmt)
	},
	{
		.sensor_name = "TEVS-AR0521",
		.frmfmt = ar0521_frmfmt,
		.res_list_size = ARRAY_SIZE(ar0521_frmfmt)
	},
	{
		.sensor_name = "TEVS-AR0522",
		.frmfmt = ar0522_frmfmt,
		.res_list_size = ARRAY_SIZE(ar0522_frmfmt)
	},
	{
		.sensor_name = "TEVS-AR0821",
		.frmfmt = ar0821_frmfmt,
		.res_list_size = ARRAY_SIZE(ar0821_frmfmt)
	},
	{
		.sensor_name = "TEVS-AR0822",
		.frmfmt = ar0822_frmfmt,
		.res_list_size = ARRAY_SIZE(ar0822_frmfmt)
	},
	{
		.sensor_name = "TEVS-AR1335",
		.frmfmt = ar1335_frmfmt,
		.res_list_size = ARRAY_SIZE(ar1335_frmfmt)
	},
};

#endif

#endif //__SENSOR_TABLES_H__