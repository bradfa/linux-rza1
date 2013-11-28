/*
* Renesas VDC5 Framebuffer
*
* Based on ren_vdc4fb.c by Phil Edworthy
* Copyright (c) 2013 Carlo Caione <carlo.caione@gmail.com>
*
* This file is subject to the terms and conditions of the GNU General Public
* License.  See the file "COPYING" in the main directory of this archive
* for more details.
*/

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <mach/rza1.h>
#include <video/ren_vdc5fb.h>

#define DRIVER_NAME	"ren_vdc5fb"
#define PALETTE_NR	16

struct ren_vdc5_priv {
	void __iomem *base;
	void __iomem *base_lvds;
	dma_addr_t dma_handle;
	struct fb_info *info;
	struct ren_vdc5_info *pdata;
	struct clk *dot_clk;
	struct clk *clk;
	struct clk *lvds_clk;
	u32 pseudo_palette[PALETTE_NR];
	int channel;
};

enum {
	LVDS_UPDATE, LVDSFCL, LCLKSELR, LPLLSETR, LPLLMONR
};

enum {
	SC0_SCL0_UPDATE, SC0_SCL0_FRC1, SC0_SCL0_FRC2, SC0_SCL0_FRC3,
	SC0_SCL0_FRC4, SC0_SCL0_FRC5, SC0_SCL0_FRC6, SC0_SCL0_FRC7,
	SC0_SCL0_DS1, SC0_SCL0_US1,

	GR0_UPDATE, GR0_AB1,

	GR1_UPDATE, GR1_AB1,

	GR2_UPDATE, GR2_FLM_RD, GR2_AB1, GR2_FLM2, GR2_FLM3,
	GR2_FLM5, GR2_FLM6,

	GR2_AB2, GR2_AB3,

	GR3_UPDATE, GR3_FLM5,  GR3_AB1,

	TCON_UPDATE, TCON_TIM, TCON_TIM_STVA1, TCON_TIM_STVA2,
	TCON_TIM_STVB1, TCON_TIM_STVB2, TCON_TIM_STH1,
	TCON_TIM_STH2, TCON_TIM_STB1, TCON_TIM_STB2,
	TCON_TIM_DE,

	OUT_UPDATE, OUT_SET, OUT_CLK_PHASE,

	SYSCNT_INT1, SYSCNT_INT2, SYSCNT_INT3, SYSCNT_INT4,
	SYSCNT_INT5, SYSCNT_INT6, SYSCNT_PANEL_CLK, SYSCNT_CLUT,
};

static const int panel_dcdr[VDC5_PANEL_CLKDIV_NUM] = {
	[VDC5_PANEL_CLKDIV_1_1]		= 1,
	[VDC5_PANEL_CLKDIV_1_2]		= 2,
	[VDC5_PANEL_CLKDIV_1_3]		= 3,
	[VDC5_PANEL_CLKDIV_1_4]		= 4,
	[VDC5_PANEL_CLKDIV_1_5]		= 5,
	[VDC5_PANEL_CLKDIV_1_6]		= 6,
	[VDC5_PANEL_CLKDIV_1_7]		= 7,
	[VDC5_PANEL_CLKDIV_1_8]		= 8,
	[VDC5_PANEL_CLKDIV_1_9]		= 9,
	[VDC5_PANEL_CLKDIV_1_12]	= 12,
	[VDC5_PANEL_CLKDIV_1_16]	= 16,
	[VDC5_PANEL_CLKDIV_1_24]	= 24,
	[VDC5_PANEL_CLKDIV_1_32]	= 32,
};

static const int panel_icksel[VDC5_PANEL_ICKSEL_NUM] = {
	[VDC5_PANEL_ICKSEL_IMG]		= 0x0000,
	[VDC5_PANEL_ICKSEL_IMG_DV]	= 0x0000,
	[VDC5_PANEL_ICKSEL_EXT_0]	= 0x1000,
	[VDC5_PANEL_ICKSEL_EXT_1]	= 0x2000,
	[VDC5_PANEL_ICKSEL_PERI]	= 0x3000,
	[VDC5_PANEL_ICKSEL_LVDS]	= 0x0400,
	[VDC5_PANEL_ICKSEL_LVDS_DIV7]	= 0x0800,
};

static unsigned long vdc5_lvds_offset[] = {
	[LVDS_UPDATE]		= 0x0000,
	[LVDSFCL]		= 0x0004,
	[LCLKSELR]		= 0x0020,
	[LPLLSETR]		= 0x0024,
	[LPLLMONR]		= 0x0028,
};

static unsigned long vdc5_offsets[] = {
	[SC0_SCL0_UPDATE]	= 0x0100,
	[SC0_SCL0_FRC1]		= 0x0104,
	[SC0_SCL0_FRC2]		= 0x0108,
	[SC0_SCL0_FRC3]		= 0x010c,
	[SC0_SCL0_FRC4]		= 0x0110,
	[SC0_SCL0_FRC5]		= 0x0114,
	[SC0_SCL0_FRC6]		= 0x0118,
	[SC0_SCL0_FRC7]		= 0x011c,
	[SC0_SCL0_DS1]		= 0x012c,
	[SC0_SCL0_US1]		= 0x0148,
	[GR0_UPDATE]		= 0x0200,
	[GR0_AB1]		= 0x0220,
	[GR1_UPDATE]		= 0x0900,
	[GR1_AB1]		= 0x0920,
	[GR2_UPDATE]		= 0x0300,
	[GR2_FLM_RD]		= 0x0304,
	[GR2_AB1]		= 0x0320,
	[GR2_FLM2]		= 0x030c,
	[GR2_FLM3]		= 0x0310,
	[GR2_FLM5]		= 0x0318,
	[GR2_FLM6]		= 0x031c,
	[GR2_AB2]		= 0x0324,
	[GR2_AB3]		= 0x0328,
	[GR3_UPDATE]		= 0x0380,
	[GR3_FLM5]		= 0x0398,
	[GR3_AB1]		= 0x03a0,
	[TCON_UPDATE]		= 0x0580,
	[TCON_TIM]		= 0x0584,
	[TCON_TIM_STVA1]	= 0x0588,
	[TCON_TIM_STVA2]	= 0x058c,
	[TCON_TIM_STVB1]	= 0x0590,
	[TCON_TIM_STVB2]	= 0x0594,
	[TCON_TIM_STH1]		= 0x0598,
	[TCON_TIM_STH2]		= 0x059c,
	[TCON_TIM_STB1]		= 0x05a0,
	[TCON_TIM_STB2]		= 0x05a4,
	[TCON_TIM_DE]		= 0x05c0,
	[OUT_UPDATE]		= 0x0600,
	[OUT_SET]		= 0x0604,
	[OUT_CLK_PHASE]		= 0x0624,
	[SYSCNT_INT1]		= 0x0680,
	[SYSCNT_INT2]		= 0x0684,
	[SYSCNT_INT3]		= 0x0688,
	[SYSCNT_INT4]		= 0x068c,
	[SYSCNT_INT5]		= 0x0690,
	[SYSCNT_INT6]		= 0x0694,
	[SYSCNT_PANEL_CLK]	= 0x0698, /* 16-bit */
	[SYSCNT_CLUT]		= 0x069a, /* 16-bit */
};

/* SYSCNT */
#define ICKEN			(1 << 8)

/* SCL Syncs */
#define FREE_RUN_VSYNC		0x0001

/* OUTPUT */
#define OUT_FMT_RGB666		(1 << 12)
#define OUT_FMT_RGB888		(0 << 12)

/* TCON Timings */
#define STVB_SEL_BITS		0x0007
#define STVB_VS_SEL		0

#define STVA_SEL_BITS		0x0007
#define STVA_HS_SEL		2

/* OUTCLK */
#define LCD_DATA_EDGE		BIT(8)
#define STVA_EDGE		BIT(6)
#define STVB_EDGE		BIT(5)
#define STH_EDGE		BIT(4)
#define STB_EDGE		BIT(3)
#define CPV_EDGE		BIT(2)
#define POLA_EDGE		BIT(1)
#define POLB_EDGE		BIT(0)

/* SCL_UPDATE */
#define SCL0_UPDATE_BIT		0x0100
#define SCL0_VEN_BIT		0x0010

/* TCON_UPDATE */
#define TCON_VEN_BIT		0x0001

/* OUT_UPDATE */
#define OUTCNT_VEN_BIT		0x0001

/* GR_UPDATE */
#define P_VEN_UPDATE		0x0010
#define IBUS_VEN_UPDATE		0x0001
#define GR_UPDATE		0x0100

/* GR_AB1 */
#define DISPSEL_BCKGND		0x0000
#define DISPSEL_LOWER		0x0001
#define DISPSEL_CUR		0x0002

/* GR_FLM_RD */
#define FB_R_ENB		0x01

#define RDSWA			6

/* LVDS */
#define LVDS_CLK_EN		BIT(4)
#define LVDSPLL_PD		BIT(0)

#define LVDS_VDC_SEL		BIT(1)
#define LVDS_ODIV_SET		0x00000300
#define LVDSPLL_TST		0x0000fc00
#define LVDS_IDIV_SET		0x00030000
#define LVDS_IN_CLK_SEL		0x07000000

#define LVDSPLL_OD		0x00000030
#define LVDSPLL_RD		0x00001f00
#define LVDSPLL_FD		0x07ff0000
#define LVDSPLL_LD		BIT(0)

static void vdc5_write(struct ren_vdc5_priv *priv,
		       unsigned long reg_offs,
		       unsigned long data)
{
	if ((SYSCNT_PANEL_CLK == reg_offs) || (SYSCNT_CLUT == reg_offs))
		iowrite16(data, priv->base + vdc5_offsets[reg_offs]);
	else
		iowrite32(data, priv->base + vdc5_offsets[reg_offs]);
}

static unsigned long vdc5_read(struct ren_vdc5_priv *priv,
			       unsigned long reg_offs)
{
	if ((SYSCNT_PANEL_CLK == reg_offs) || (SYSCNT_CLUT == reg_offs))
		return ioread16(priv->base + vdc5_offsets[reg_offs]);
	else
		return ioread32(priv->base + vdc5_offsets[reg_offs]);
}

static void vdc5_lvds_write(struct ren_vdc5_priv *priv,
			    unsigned long reg_offs,
			    unsigned long data)
{
	iowrite32(data, priv->base_lvds + vdc5_lvds_offset[reg_offs]);
}

static unsigned long vdc5_lvds_read(struct ren_vdc5_priv *priv,
				    unsigned long reg_offs)
{
	return ioread32(priv->base_lvds + vdc5_lvds_offset[reg_offs]);
}

static void restart_tft_display(struct ren_vdc5_priv *priv,
				int clock_source)
{
	struct fb_videomode *lcd;
	unsigned long h;
	unsigned long v;
	unsigned long tmp;

	/* FB setup */
	lcd = &priv->pdata->lcd_cfg;

	/* VDC clock Setup */
	tmp = panel_dcdr[priv->pdata->clock_divider];
	tmp |= panel_icksel[clock_source];
	tmp |= ICKEN;
	vdc5_write(priv, SYSCNT_PANEL_CLK, tmp);

	/* Clear and Disable all interrupts */
	vdc5_write(priv, SYSCNT_INT1, 0);
	vdc5_write(priv, SYSCNT_INT2, 0);
	vdc5_write(priv, SYSCNT_INT3, 0);
	vdc5_write(priv, SYSCNT_INT4, 0);
	vdc5_write(priv, SYSCNT_INT5, 0);
	vdc5_write(priv, SYSCNT_INT6, 0);

	/* Setup free-running syncs */
	vdc5_write(priv, SC0_SCL0_FRC3, FREE_RUN_VSYNC);

	/* Disable scale up/down */
	vdc5_write(priv, SC0_SCL0_DS1, 0);
	vdc5_write(priv, SC0_SCL0_US1, 0);

	/* Timing registers */
	h = lcd->hsync_len + lcd->left_margin  + lcd->xres + lcd->right_margin;
	v = lcd->vsync_len + lcd->upper_margin + lcd->yres + lcd->lower_margin;
	tmp = (v - 1) << 16;
	tmp |= h - 1;
	vdc5_write(priv, SC0_SCL0_FRC4, tmp);

	vdc5_write(priv, TCON_TIM, (((h - 1) / 2) << 16));

	/* Full-screen enable signal */
	tmp = (lcd->vsync_len + lcd->upper_margin) << 16;
	tmp |= lcd->yres;
	vdc5_write(priv, SC0_SCL0_FRC6, tmp);
	vdc5_write(priv, TCON_TIM_STVB1, tmp);
	vdc5_write(priv, GR2_AB2, tmp);

	tmp = (lcd->hsync_len + lcd->left_margin) << 16;
	tmp |= lcd->xres;
	vdc5_write(priv, SC0_SCL0_FRC7, tmp);
	vdc5_write(priv, TCON_TIM_STB1, tmp);
	vdc5_write(priv, GR2_AB3, tmp);

	vdc5_write(priv, SC0_SCL0_FRC1, 0);
	vdc5_write(priv, SC0_SCL0_FRC2, 0);
	vdc5_write(priv, SC0_SCL0_FRC5, 0);

	/* Set output format */
	vdc5_write(priv, OUT_SET, OUT_FMT_RGB888);

	/* VS */
	tmp = priv->pdata->vs_pulse_width;
	tmp |= priv->pdata->vs_start_pos << 16;
	vdc5_write(priv, TCON_TIM_STVA1, tmp);

	tmp = vdc5_read(priv, TCON_TIM_STVB2);
	tmp &= ~STVB_SEL_BITS;
	tmp |= STVB_VS_SEL;
	vdc5_write(priv, TCON_TIM_STVB2, tmp);

	tmp = vdc5_read(priv, OUT_CLK_PHASE);
	tmp |= STVB_EDGE;
	vdc5_write(priv, OUT_CLK_PHASE, tmp);

	/* HS */
	tmp = priv->pdata->hs_pulse_width;
	tmp |= priv->pdata->hs_start_pos << 16;
	vdc5_write(priv, TCON_TIM_STH1, tmp);

	tmp = vdc5_read(priv, TCON_TIM_STVA2);
	tmp &= ~STVA_SEL_BITS;
	tmp |= STVA_HS_SEL;
	vdc5_write(priv, TCON_TIM_STVA2, tmp);

	tmp = vdc5_read(priv, OUT_CLK_PHASE);
	tmp |= STVA_EDGE;
	vdc5_write(priv, OUT_CLK_PHASE, tmp);

	/* Output clock falling edge */
	tmp = vdc5_read(priv, OUT_CLK_PHASE);
	tmp |= LCD_DATA_EDGE;
	vdc5_write(priv, OUT_CLK_PHASE, tmp);

	vdc5_write(priv, GR0_AB1, DISPSEL_BCKGND);
	vdc5_write(priv, GR1_AB1, DISPSEL_LOWER);
	vdc5_write(priv, GR2_AB1, DISPSEL_CUR);
	vdc5_write(priv, GR3_AB1, DISPSEL_LOWER);


	/* Setup framebuffer base/output */
	vdc5_write(priv, GR2_FLM_RD, FB_R_ENB);

	tmp = ((unsigned long)priv->dma_handle & ~0x00000007);
	vdc5_write(priv, GR2_FLM2, tmp);

	tmp = (lcd->xres * 2) << 16;
	vdc5_write(priv, GR2_FLM3, tmp);

	tmp = vdc5_read(priv, GR3_FLM5);
	tmp |= lcd->yres << 16;
	vdc5_write(priv, GR2_FLM5, tmp);

	tmp = lcd->xres << 16;
	tmp |= RDSWA << 10;
	vdc5_write(priv, GR2_FLM6, tmp);

	vdc5_write(priv, SC0_SCL0_UPDATE, SCL0_VEN_BIT | SCL0_UPDATE_BIT);
	vdc5_write(priv, TCON_UPDATE, TCON_VEN_BIT);
	vdc5_write(priv, OUT_UPDATE, OUTCNT_VEN_BIT);
	vdc5_write(priv, GR0_UPDATE, P_VEN_UPDATE | IBUS_VEN_UPDATE);
	vdc5_write(priv, GR1_UPDATE, P_VEN_UPDATE | IBUS_VEN_UPDATE);
	vdc5_write(priv, GR2_UPDATE, P_VEN_UPDATE | IBUS_VEN_UPDATE | GR_UPDATE);
	vdc5_write(priv, GR3_UPDATE, P_VEN_UPDATE | IBUS_VEN_UPDATE);
}

static void ren_vdc5_lvds_init(struct ren_vdc5_priv *priv)
{
	struct ren_vdc5_lvds_info *lvds = &priv->pdata->lvds;
	unsigned long tmp;
	int t;

	/* Output from the LVDS PLL is disabled. */
	tmp = vdc5_lvds_read(priv, LCLKSELR);
	tmp &= ~LVDS_CLK_EN;
	vdc5_lvds_write(priv, LCLKSELR, tmp);

	/* Controls power-down for the LVDS PLL: Power-down state */
	tmp = vdc5_lvds_read(priv, LPLLSETR);
	tmp |= LVDSPLL_PD;
	vdc5_lvds_write(priv, LPLLSETR, tmp);

	/* This is a delay (1 usec) while waiting for PLL PD to settle. */
	udelay(1);

	/* LCLKSELR: LVDS clock select register */
	tmp = vdc5_lvds_read(priv, LCLKSELR);
	tmp &= ~(LVDS_VDC_SEL | LVDS_ODIV_SET | LVDSPLL_TST |
			LVDS_IDIV_SET | LVDS_IN_CLK_SEL);

	/* The clock input to frequency divider 1 */
	tmp |= lvds->lvds_in_clk_sel << 24u;
	/* The frequency dividing value (NIDIV) for frequency divider 1 */
	tmp |= lvds->lvds_idiv_set << 16u;
	/* Internal parameter setting for LVDS PLL */
	tmp |= lvds->lvdspll_tst << 10u;
	/* The frequency dividing value (NODIV) for frequency divider 2 */
	tmp |= lvds->lvds_odiv_set << 8u;

	if (priv->pdata->channel != 0) {
		/* A channel in VDC5 whose data is to be output through the LVDS */
		tmp |= LVDS_VDC_SEL;
	}
	vdc5_lvds_write(priv, LCLKSELR, tmp);

	/* LPLLSETR: LVDS PLL setting register */
	tmp = vdc5_lvds_read(priv, LPLLSETR);
	tmp &= ~(LVDSPLL_OD | LVDSPLL_RD | LVDSPLL_FD);

	/* The frequency dividing value (NFD) for the feedback frequency */
	tmp |= lvds->lvdspll_fd << 16u;
	/* The frequency dividing value (NRD) for the input frequency */
	tmp |= lvds->lvdspll_rd << 8u;
	/* The frequency dividing value (NOD) for the output frequency */
	tmp |= lvds->lvdspll_od << 4u;
	vdc5_lvds_write(priv, LPLLSETR, tmp);

	/* Controls power-down for the LVDS PLL: Normal operation */
	tmp &= ~LVDSPLL_PD;
	vdc5_lvds_write(priv, LPLLSETR, tmp);

	/* This is a delay (1 usec) while waiting for PLL PD to settle. */
	udelay(1);

	for (t = 0; t < 10; t++)
		if (vdc5_lvds_read(priv, LPLLMONR) & LVDSPLL_LD)
			break;

	tmp = vdc5_lvds_read(priv, LCLKSELR);
	tmp |= LVDS_CLK_EN;
	vdc5_lvds_write(priv, LCLKSELR, tmp);
}

static int ren_vdc5_start(struct ren_vdc5_priv *priv,
		int clock_source)
{
	int ret = 0;

	ret = clk_enable(priv->clk);
	if (ret < 0)
		return ret;

	if (priv->dot_clk) {
		ret = clk_enable(priv->dot_clk);
		if (ret < 0)
			return ret;
	}

	if (priv->channel == 1) {
		ret = clk_enable(priv->lvds_clk);
		if (ret < 0)
			return ret;
	}

	ren_vdc5_lvds_init(priv);
	restart_tft_display(priv, clock_source);

	return ret;
}

static void ren_vdc5_stop(struct ren_vdc5_priv *priv)
{
	if (priv->dot_clk)
		clk_disable(priv->dot_clk);
	if (priv->lvds_clk)
		clk_disable(priv->lvds_clk);
	clk_disable(priv->clk);
}

static int ren_vdc5_setup_clocks(struct platform_device *pdev,
		int clock_source,
		struct ren_vdc5_priv *priv)
{
	struct ren_vdc5_info *pdata = pdev->dev.platform_data;
	char vdc5chan[6];

	sprintf(vdc5chan, "vdc5%d", pdata->channel);

	priv->clk = devm_clk_get(&pdev->dev, vdc5chan);
	if (IS_ERR(priv->clk)) {
		dev_err(&pdev->dev, "cannot get clock \"vdc5\"\n");
		return PTR_ERR(priv->clk);
	}

	if (pdata->channel == 1) {
		priv->lvds_clk = devm_clk_get(&pdev->dev, "vdc50");
		if (IS_ERR(priv->lvds_clk)) {
			dev_err(&pdev->dev, "cannot get clock \"lvds\"\n");
			return PTR_ERR(priv->lvds_clk);
		}
	}

	if (clock_source == VDC5_PANEL_ICKSEL_PERI) {
		priv->dot_clk = devm_clk_get(&pdev->dev, "peripheral_clk");
		if (IS_ERR(priv->dot_clk)) {
			dev_err(&pdev->dev, "cannot get peripheral clock\n");
			return PTR_ERR(priv->dot_clk);
		}
	}

	return 0;
}

static int ren_vdc5_set_bpp(struct fb_var_screeninfo *var, int bpp)
{
	switch (bpp) {
	case 16: /* RGB 565 */
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	default:
		return -EINVAL;
	}

	var->bits_per_pixel = bpp;
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;
	return 0;
}

static int ren_vdc5_setcolreg(u_int regno,
	u_int red, u_int green, u_int blue,
	u_int transp, struct fb_info *info)
{
	u32 *palette = info->pseudo_palette;

	if (regno >= PALETTE_NR)
		return -EINVAL;

	/* only FB_VISUAL_TRUECOLOR supported */

	red    >>= 16 - info->var.red.length;
	green  >>= 16 - info->var.green.length;
	blue   >>= 16 - info->var.blue.length;
	transp >>= 16 - info->var.transp.length;

	palette[regno] = (red << info->var.red.offset) |
		(green << info->var.green.offset) |
		(blue << info->var.blue.offset) |
		(transp << info->var.transp.offset);

	return 0;
}

static struct fb_fix_screeninfo ren_vdc5_fix = {
	.id		= "Renesas VDC5FB",
	.type		= FB_TYPE_PACKED_PIXELS,
	.visual		= FB_VISUAL_TRUECOLOR,
	.accel		= FB_ACCEL_NONE,
};

static struct fb_ops ren_vdc5_ops = {
	.owner          = THIS_MODULE,
	.fb_setcolreg	= ren_vdc5_setcolreg,
	.fb_read        = fb_sys_read,
	.fb_write       = fb_sys_write,
	.fb_fillrect	= sys_fillrect,
	.fb_copyarea	= sys_copyarea,
	.fb_imageblit	= sys_imageblit,
};

static int ren_vdc5_probe(struct platform_device *pdev)
{
	struct fb_info *info;
	struct ren_vdc5_priv *priv;
	struct ren_vdc5_info *pdata = pdev->dev.platform_data;
	struct resource *res;
	struct resource *res_lvds;
	void *buf;
	int error;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data defined\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, pdata->channel);
	if (!res)
		return -ENOENT;

	res_lvds = platform_get_resource_byname(pdev, IORESOURCE_MEM, "lvds");
	if (!res)
		return -ENOENT;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);

	priv->pdata = pdata;

	error = ren_vdc5_setup_clocks(pdev, pdata->clock_source, priv);
	if (error) {
		dev_err(&pdev->dev, "unable to setup clocks\n");
		return error;
	}

	rskrza1_board_vdc5_pfc_assign(pdata->channel);

	priv->channel = pdata->channel;

	priv->base_lvds = devm_ioremap(&pdev->dev, res_lvds->start,
			resource_size(res_lvds));
	if (IS_ERR(priv->base_lvds))
		return PTR_ERR(priv->base_lvds);

	priv->base = devm_ioremap_nocache(&pdev->dev, res->start,
			resource_size(res));
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	priv->info = framebuffer_alloc(0, &pdev->dev);
	if (!priv->info)
		return -ENOMEM;

	info = priv->info;
	info->fbops = &ren_vdc5_ops;
	info->var.xres = info->var.xres_virtual = pdata->lcd_cfg.xres;
	info->var.yres = info->var.yres_virtual = pdata->lcd_cfg.yres;
	info->var.left_margin = pdata->lcd_cfg.left_margin;
	info->var.right_margin = pdata->lcd_cfg.right_margin;
	info->var.upper_margin = pdata->lcd_cfg.upper_margin;
	info->var.lower_margin = pdata->lcd_cfg.lower_margin;
	info->var.hsync_len = pdata->lcd_cfg.hsync_len;
	info->var.vsync_len = pdata->lcd_cfg.vsync_len;
	info->var.width = pdata->panel_width;
	info->var.height = pdata->panel_height;
	info->var.activate = FB_ACTIVATE_NOW;
	info->pseudo_palette = priv->pseudo_palette;

	error = ren_vdc5_set_bpp(&info->var, pdata->bpp);
	if (error) {
		goto failed_release_fb;
	}

	info->fix = ren_vdc5_fix;
	info->fix.line_length = pdata->lcd_cfg.xres * (pdata->bpp / 8);
	info->fix.smem_len = info->fix.line_length * pdata->lcd_cfg.yres;

	buf = dma_alloc_coherent(&pdev->dev, info->fix.smem_len,
			&priv->dma_handle, GFP_KERNEL);
	if (!buf) {
		dev_err(&pdev->dev, "unable to allocate buffer\n");
		error = -ENOMEM;
		goto failed_release_fb;
	}

	info->fix.smem_start = priv->dma_handle;
	info->screen_base = buf;
	info->flags = FBINFO_FLAG_DEFAULT;

	error = fb_alloc_cmap(&info->cmap, PALETTE_NR, 0);
	if (error < 0) {
		dev_err(&pdev->dev, "unable to allocate cmap\n");
		goto failed_free_dma;
	}

	info->device = &pdev->dev;
	info->par = priv;

	error = ren_vdc5_start(priv, pdata->clock_source);
	if (error) {
		dev_err(&pdev->dev, "unable to start hardware\n");
		goto failed_dealloc_cmap;
	}

	error = register_framebuffer(info);
	if (error < 0) {
		goto failed_vdc5_stop;
	}

	dev_info(info->dev,
		"registered %s as %udx%ud %dbpp.\n",
		pdev->name,
		(int) pdata->lcd_cfg.xres,
		(int) pdata->lcd_cfg.yres,
		pdata->bpp);

	return 0;

failed_vdc5_stop:
	ren_vdc5_stop(priv);
failed_dealloc_cmap:
	fb_dealloc_cmap(&info->cmap);
failed_free_dma:
	dma_free_coherent(&pdev->dev, info->fix.smem_len,
			info->screen_base, info->fix.smem_start);
failed_release_fb:
	framebuffer_release(info);

	return error;
}

static int ren_vdc5_remove(struct platform_device *pdev)
{
	struct ren_vdc5_priv *priv = platform_get_drvdata(pdev);
	struct fb_info *info = priv->info;

	unregister_framebuffer(info);
	dma_free_coherent(&pdev->dev, info->fix.smem_len,
		info->screen_base, info->fix.smem_start);
	fb_dealloc_cmap(&info->cmap);
	ren_vdc5_stop(priv);
	framebuffer_release(info);

	return 0;
}

static struct platform_driver ren_vdc5_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe	= ren_vdc5_probe,
	.remove	= ren_vdc5_remove,
};

module_platform_driver(ren_vdc5_driver);

MODULE_DESCRIPTION("Renesas VDC5 Framebuffer driver");
MODULE_AUTHOR("Carlo Caione <carlo.caione@gmail.com>");
