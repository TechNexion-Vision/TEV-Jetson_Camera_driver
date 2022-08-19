#ifndef __TEVI_AP1302_I2C_TABLES__
#define __TEVI_AP1302_I2C_TABLES__

/*
 * WARNING: frmfmt ordering need to match mode definition in
 * device tree!
 * struct camera_common_frmfmt {
 *		struct v4l2_frmsize_discrete	size;
 *		const int	*framerates;
 *		int	num_framerates;
 *		bool	hdr_en;
 *		int	mode;  // number depend on dts mode list
 * };
 */
enum
{
	TEVI_AP1302_AR0144 = 0,
	TEVI_AP1302_AR0234,
	TEVI_AP1302_AR0521,
	TEVI_AP1302_AR0522,
	TEVI_AP1302_AR0821,
	TEVI_AP1302_AR1335,
};

static const int __20fps = 20;
static const int __25fps = 25;
static const int __30fps = 30;
static const int __40fps = 40;
static const int __60fps = 60;
static const int __100fps = 100;
static const struct camera_common_frmfmt sensor_frmfmt[] = {
	{{1280, 720}, &__60fps, 1, 0, 0},
	{{1920, 1080}, &__60fps, 1, 0, 1},
};

static const struct camera_common_frmfmt ar0144_frmfmt[] = {
	{{1280, 720}, &__60fps, 1, 0, 0},
	{{1280, 800}, &__60fps, 1, 0, 1},
};
static const struct camera_common_frmfmt ar0234_frmfmt[] = {
	{{1280, 720}, &__100fps, 1, 0, 0},
	{{1920, 1080}, &__100fps, 1, 0, 1},
	{{1920, 1200}, &__100fps, 1, 0, 2},
};

static const struct camera_common_frmfmt ar0521_frmfmt[] = {
	{{1280, 720}, &__30fps, 1, 0, 0},
	{{1920, 1080}, &__30fps, 1, 0, 1},
	{{2592, 1944}, &__30fps, 1, 0, 2},
};
static const struct camera_common_frmfmt ar0821_frmfmt[] = {
	{{1280, 720}, &__30fps, 1, 0, 0},
	{{1920, 1080}, &__30fps, 1, 0, 1},
	{{2560, 1440}, &__30fps, 1, 0, 2},
	{{3840, 2160}, &__25fps, 1, 0, 3},
};
static const struct camera_common_frmfmt ar1335_frmfmt[] = {
	{{1280, 720}, &__30fps, 1, 0, 0},
	{{1920, 1080}, &__30fps, 1, 0, 1},
	{{2560, 1440}, &__30fps, 1, 0, 2},
	{{3840, 2160}, &__20fps, 1, 0, 3},
	// {{4192, 3120}, &__20fps, 1, 0, 4},
	// {{4208, 3120}, &__20fps, 1, 0, 5},
};

struct sensor_info {
	const char* sensor_name;
	const struct camera_common_frmfmt *frmfmt;
	u32 res_list_size;
};

static struct sensor_info ap1302_sensor_table[] = {
	{
		.sensor_name = "TEVI-AR0144",
		.frmfmt = ar0144_frmfmt,
		.res_list_size = ARRAY_SIZE(ar0144_frmfmt)
	},
	{
		.sensor_name = "TEVI-AR0234",
		.frmfmt = ar0234_frmfmt,
		.res_list_size = ARRAY_SIZE(ar0234_frmfmt)
	},
	{
		.sensor_name = "TEVI-AR0521",
		.frmfmt = ar0521_frmfmt,
		.res_list_size = ARRAY_SIZE(ar0521_frmfmt)
	},
	{
		.sensor_name = "TEVI-AR0522",
		.frmfmt = ar0521_frmfmt,
		.res_list_size = ARRAY_SIZE(ar0521_frmfmt)
	},
	{
		.sensor_name = "TEVI-AR0821",
		.frmfmt = ar0821_frmfmt,
		.res_list_size = ARRAY_SIZE(ar0821_frmfmt)
	},
	{
		.sensor_name = "TEVI-AR1335",
		.frmfmt = ar1335_frmfmt,
		.res_list_size = ARRAY_SIZE(ar1335_frmfmt)
	},
};

#endif //__TEVI_AP1302_I2C_TABLES__