#ifndef __REN_VDC5_H__
#define __REN_VDC5_H__

#include <linux/fb.h>

/*! Panel clock select */
enum {
	VDC5_PANEL_ICKSEL_IMG = 0,	/*!< Video image clock (VIDEO_X1) */
	VDC5_PANEL_ICKSEL_IMG_DV,	/*!< Video image clock (DV_CLK) */
	VDC5_PANEL_ICKSEL_EXT_0,	/*!< External clock (LCD_EXTCLK 0) */
	VDC5_PANEL_ICKSEL_EXT_1,	/*!< External clock (LCD_EXTCLK 1) */
	VDC5_PANEL_ICKSEL_PERI,		/*!< Peripheral clock 1 */
	VDC5_PANEL_ICKSEL_LVDS,		/*!< LVDS PLL clock */
	VDC5_PANEL_ICKSEL_LVDS_DIV7,	/*!< LVDS PLL clock divided by 7 */
	VDC5_PANEL_ICKSEL_NUM		/*!< The number of panel clock select settings */
};

/*! The clock input to frequency divider 1 */
enum {
	VDC5_LVDS_INCLK_SEL_IMG = 0,	/*!< Video image clock (VIDEO_X1) */
	VDC5_LVDS_INCLK_SEL_DV_0,	/*!< Video image clock (DV_CLK 0) */
	VDC5_LVDS_INCLK_SEL_DV_1,	/*!< Video image clock (DV_CLK 1) */
	VDC5_LVDS_INCLK_SEL_EXT_0,	/*!< External clock (LCD_EXTCLK 0) */
	VDC5_LVDS_INCLK_SEL_EXT_1,	/*!< External clock (LCD_EXTCLK 1) */
	VDC5_LVDS_INCLK_SEL_PERI,	/*!< Peripheral clock 1 */
	VDC5_LVDS_INCLK_SEL_NUM
};

enum {
	VDC5_LVDS_NDIV_1 = 0,		/*!< Div 1 */
	VDC5_LVDS_NDIV_2,		/*!< Div 2 */
	VDC5_LVDS_NDIV_4,		/*!< Div 4 */
	VDC5_LVDS_NDIV_NUM
};

/*! The frequency dividing value (NOD) for the output frequency */
enum {
	VDC5_LVDS_PLL_NOD_1 = 0,	/*!< Div 1 */
	VDC5_LVDS_PLL_NOD_2,		/*!< Div 2 */
	VDC5_LVDS_PLL_NOD_4,		/*!< Div 4 */
	VDC5_LVDS_PLL_NOD_8,		/*!< Div 8 */
	VDC5_LVDS_PLL_NOD_NUM
};

enum {
	VDC5_PANEL_CLKDIV_1_1 = 0,      /*!< Division Ratio 1/1 */
	VDC5_PANEL_CLKDIV_1_2,          /*!< Division Ratio 1/2 */
	VDC5_PANEL_CLKDIV_1_3,          /*!< Division Ratio 1/3 */
	VDC5_PANEL_CLKDIV_1_4,          /*!< Division Ratio 1/4 */
	VDC5_PANEL_CLKDIV_1_5,          /*!< Division Ratio 1/5 */
	VDC5_PANEL_CLKDIV_1_6,          /*!< Division Ratio 1/6 */
	VDC5_PANEL_CLKDIV_1_7,          /*!< Division Ratio 1/7 */
	VDC5_PANEL_CLKDIV_1_8,          /*!< Division Ratio 1/8 */
	VDC5_PANEL_CLKDIV_1_9,          /*!< Division Ratio 1/9 */
	VDC5_PANEL_CLKDIV_1_12,         /*!< Division Ratio 1/12 */
	VDC5_PANEL_CLKDIV_1_16,         /*!< Division Ratio 1/16 */
	VDC5_PANEL_CLKDIV_1_24,         /*!< Division Ratio 1/24 */
	VDC5_PANEL_CLKDIV_1_32,         /*!< Division Ratio 1/32 */
	VDC5_PANEL_CLKDIV_NUM           /*!< The number of division ratio settings */
};

struct ren_vdc5_lvds_info {
	int lvds_in_clk_sel;		/*!< The clock input to frequency divider 1 */
	int lvds_idiv_set;		/*!< The frequency dividing value (NIDIV) for frequency divider 1 */
	int lvdspll_tst;		/*!< Internal parameter setting for LVDS PLL */
	int lvds_odiv_set;		/*!< The frequency dividing value (NODIV) for frequency divider 2 */
	int lvdspll_fd;			/*!< The frequency dividing value (NFD) for the feedback frequency */
	int lvdspll_rd;			/*!< The frequency dividing value (NRD) for the input frequency */
	int lvdspll_od;			/*!< The frequency dividing value (NOD) for the output frequency */
};

struct ren_vdc5_info {
	int channel;
	int bpp;
	int clock_source;		/* panel_icksel */
	int clock_divider;		/* panel_dcdr */
	struct ren_vdc5_lvds_info lvds;
	int hs_pulse_width;
	int hs_start_pos;
	int vs_pulse_width;
	int vs_start_pos;
	struct fb_videomode lcd_cfg;
	unsigned long panel_width;
	unsigned long panel_height;
};

#endif

