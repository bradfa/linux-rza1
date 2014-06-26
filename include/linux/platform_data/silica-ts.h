#ifndef __LINUX_PLATFORM_DATA_SILICA_TSC_H__
#define __LINUX_PLATFORM_DATA_SILICA_TSC_H__

struct silica_tsc_pdata {
	int x_min;
	int x_max;
	int y_min;
	int y_max;
	unsigned char ain_x;
	unsigned char ain_y;
};

#endif /* __LINUX_PLATFORM_DATA_SILICA_TSC_H__ */
