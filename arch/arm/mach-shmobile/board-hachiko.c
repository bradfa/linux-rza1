/*
 * hachiko board support
 *
 * Copyright (C) 2014  Carlo Caione
 *
 * Based on: board-rskrza1.c
 *
 * Copyright (C) 2013  Renesas Solutions Corp.
 * Copyright (C) 2010  Magnus Damm
 * Copyright (C) 2008  Yoshihiro Shimoda
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/spi/smanalog.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sh_mmcif.h>
#include <linux/mmc/sh_mobile_sdhi.h>
#include <linux/mfd/tmio.h>
#include <linux/platform_data/dma-rza1.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <mach/common.h>
#include <mach/rza1.h>
#include <linux/i2c.h>
#include <linux/sh_intc.h>
#include <../sound/soc/codecs/wm8978.h>
#include <video/vdc5fb.h>

/* MMCIF */
static struct resource sh_mmcif_resources[] = {
	[0] = {
		.name	= "MMCIF",
		.start	= 0xe804c800,
		.end	= 0xe804c8ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 300,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= 301,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct sh_mmcif_plat_data sh_mmcif_platdata = {
	.ocr		= MMC_VDD_32_33,
	.caps		= MMC_CAP_4_BIT_DATA |
			  MMC_CAP_8_BIT_DATA |
			  MMC_CAP_NONREMOVABLE,
	/*.slave_id_tx	= RZA1DMA_SLAVE_MMCIF_TX,*/
	/*.slave_id_rx	= RZA1DMA_SLAVE_MMCIF_RX,*/
};

static struct platform_device mmc_device = {
	.name		= "sh_mmcif",
	.id		= -1,
	.dev		= {
		.dma_mask		= NULL,
		.coherent_dma_mask	= 0xffffffff,
		.platform_data		= &sh_mmcif_platdata,
	},
	.num_resources	= ARRAY_SIZE(sh_mmcif_resources),
	.resource	= sh_mmcif_resources,
};

/* Touchscreen */
static struct resource tsc_resources[] = {
	[0] = {
		.start  = 0xe8005800,
		.end    = 0xe80058ff,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start	= 545,
		.end	= 545,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device tsc_device = {
	.name		= "silica_tsc",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(tsc_resources),
	.resource	= tsc_resources,
};

/* SDHI0 */
static struct sh_mobile_sdhi_info sdhi0_info = {
	.dma_slave_tx	= RZA1DMA_SLAVE_SDHI0_TX,
	.dma_slave_rx	= RZA1DMA_SLAVE_SDHI0_RX,
	.tmio_caps	= MMC_CAP_SD_HIGHSPEED | MMC_CAP_SDIO_IRQ,
	.tmio_ocr_mask	= MMC_VDD_32_33,
	.tmio_flags	= TMIO_MMC_HAS_IDLE_WAIT,
};

static struct resource sdhi0_resources[] = {
	[0] = {
		.name	= "SDHI0",
		.start	= 0xe804e000,
		.end	= 0xe804e0ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.name   = SH_MOBILE_SDHI_IRQ_CARD_DETECT,
		.start	= 302,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.name   = SH_MOBILE_SDHI_IRQ_SDCARD,
		.start	= 303,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.name   = SH_MOBILE_SDHI_IRQ_SDIO,
		.start	= 304,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device sdhi0_device = {
	.name		= "sh_mobile_sdhi",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(sdhi0_resources),
	.resource	= sdhi0_resources,
	.dev	= {
		.platform_data	= &sdhi0_info,
	},
};

#include "rskrza1-vdc5fb.c"

static struct platform_device *hachiko_devices[] __initdata = {
	&mmc_device,
	&sdhi0_device,
	&tsc_device,
};

static struct mtd_partition spibsc0_flash_partitions[] = {
	{
		.name		= "spibsc0_loader",
		.offset		= 0x00000000,
		.size		= 0x00080000,
		/* .mask_flags	= MTD_WRITEABLE, */
	},
	{
		.name		= "spibsc0_bootenv",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0x00040000,
	},
	{
		.name		= "spibsc0_kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0x00800000,
	},
	{
		.name		= "spibsc0_dtb",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0x00040000,
	},
	{
		.name		= "spibsc0_rootfs",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
	},
};

static struct mtd_partition spibsc1_flash_partitions[] = {
	{
		.name		= "spibsc1_data",
		.offset		= 0x00000000,
		.size		= MTDPART_SIZ_FULL,
	},
};


static struct flash_platform_data spi_flash_data0 = {
	.name	= "m25p80",
	.parts	= spibsc0_flash_partitions,
	.nr_parts = ARRAY_SIZE(spibsc0_flash_partitions),
	.type = "s25fl512s",
};


static struct flash_platform_data spi_flash_data1 = {
	.name	= "m25p80",
	.parts	= spibsc1_flash_partitions,
	.nr_parts = ARRAY_SIZE(spibsc1_flash_partitions),
	.type = "s25fl512s",
};


static struct spi_board_info hachiko_spi_devices[] __initdata = {
	{
		/* spidev */
		.modalias		= "spidev",
		.chip_select		= 0,
		.max_speed_hz		= 5000000,
		.bus_num		= 1,
		.mode			= SPI_MODE_3,
		.clk_delay		= 2,
		.cs_negate_delay	= 2,
		.next_access_delay	= 2,
	},
	{
		/* SPI Flash0 */
		.modalias = "m25p80",
		/* .max_speed_hz = 25000000, */
		.bus_num = 5,
		.chip_select = 0,
		.platform_data = &spi_flash_data0,
	},
	{
		/* SPI Flash1 */
		.modalias = "m25p80",
		/* .max_speed_hz = 25000000, */
		.bus_num = 6,
		.chip_select = 0,
		.platform_data = &spi_flash_data1,
	},
};

static void __init hachiko_init_spi(void)
{
	/* register SPI device information */
	spi_register_board_info(hachiko_spi_devices,
		ARRAY_SIZE(hachiko_spi_devices));
}

static unsigned int rza1_gpio_test;
int __init early_rza1_gpio_test(char *str)
{
	rza1_gpio_test = 0;
	get_option(&str, &rza1_gpio_test);
	if (rza1_gpio_test == 1) {
		disable_sdhi = 1;
		vdc5fb1_opts = 0;
	}
	return 0;
}
early_param("rza1_gpio_test", early_rza1_gpio_test);

int sf3_enable;
int __init early_rza1_sf3_enable(char *str)
{
	get_option(&str, &sf3_enable);

	return 0;
}
early_param("rza1_sf3_enable", early_rza1_sf3_enable);


void __init hachiko_init(void)
{
	if (disable_sdhi)
		sdhi0_device.name = "mobile_sdhi(hidden)";

	platform_add_devices(hachiko_devices, ARRAY_SIZE(hachiko_devices));

	rza1_pinmux_setup();

	rza1_devices_setup();

	/* ADC */
	rza1_pfc_pin_assign(P1_8, ALT1, DIIO_PBDC_DIS);		/* AN0 */
	rza1_pfc_pin_assign(P1_9, ALT1, DIIO_PBDC_DIS);		/* AN1 */
	rza1_pfc_pin_assign(P1_10, ALT1, DIIO_PBDC_DIS);	/* AN2 */
	rza1_pfc_pin_assign(P1_11, ALT1, DIIO_PBDC_DIS);	/* AN3 */
	rza1_pfc_pin_assign(P1_12, ALT1, DIIO_PBDC_DIS);	/* AN4 */
	rza1_pfc_pin_assign(P1_13, ALT1, DIIO_PBDC_DIS);	/* AN5 */

#if defined(CONFIG_MMC)
	/* set SDHI0 pfc configuration */
	rza1_pfc_pin_assign(P4_8, ALT3, DIIO_PBDC_DIS);		/* SD_CD_0 */
	rza1_pfc_pin_assign(P4_9, ALT3, DIIO_PBDC_DIS);		/* SD_WP_0 */
	rza1_pfc_pin_assign(P4_10, ALT3, DIIO_PBDC_EN);		/* SD_D1_0 */
	rza1_pfc_pin_assign(P4_11, ALT3, DIIO_PBDC_EN);		/* SD_D0_0 */
	rza1_pfc_pin_assign(P4_12, ALT3, DIIO_PBDC_DIS);	/* SD_CLK_0 */
	rza1_pfc_pin_assign(P4_13, ALT3, DIIO_PBDC_EN);		/* SD_CMD_0 */
	rza1_pfc_pin_assign(P4_14, ALT3, DIIO_PBDC_EN);		/* SD_D3_0 */
	rza1_pfc_pin_assign(P4_15, ALT3, DIIO_PBDC_EN);		/* SD_D2_0 */
#endif

#if defined(CONFIG_SPI_MASTER)
	hachiko_init_spi();
#endif

#if defined(CONFIG_FB_VDC5)
	vdc5fb_setup();
#endif

	/* SPIBSC1 */
	if (sf3_enable == 1) {
		/* WARNING: P8_10-P8_15 ARE ALSO USED BY NOR FLASH MEMORY. */
		rza1_pfc_pin_assign(P8_13, PMODE, PORT_OUT_HIGH);/* SPBSSL_1  */
		rza1_pfc_pin_assign(P8_10, ALT3, DIIO_PBDC_EN);	/* SPBIO20_1 */
		rza1_pfc_pin_assign(P8_11, ALT3, DIIO_PBDC_EN);	/* SPBIO30_1 */
		rza1_pfc_pin_assign(P8_12, ALT3, DIIO_PBDC_DIS);/* SPBCLK_1  */
		rza1_pfc_pin_assign(P8_14, ALT3, DIIO_PBDC_EN);	/* SPBIO00_1 */
		rza1_pfc_pin_assign(P8_15, ALT3, DIIO_PBDC_EN);	/* SPBIO10_1 */
		rza1_pfc_pin_assign(P8_13, ALT3, DIIO_PBDC_DIS);/* SPBSSL_1  */
	}

	/* for GPIO test only */
	if (rza1_gpio_test == 1) {
		pr_err("gpio test mode ON\n");
		/* GPIO IN */
		rza1_pfc_pin_assign(P0_0,  PMODE, DIR_IN);	/* DIPSW1-1 */
		rza1_pfc_pin_assign(P0_1,  PMODE, DIR_IN);	/* DIPSW1-2 */
		rza1_pfc_pin_assign(P7_0,  PMODE, DIR_IN);	/* DIPSW1-3 */
		/* GPIO OUT: */
		rza1_pfc_pin_assign(P4_10, PMODE, PORT_OUT_LOW); /* LED1 */
		rza1_pfc_pin_assign(P4_11, PMODE, PORT_OUT_LOW); /* LED2 */
	}
}
int hachiko_board_can_pfc_assign(int channel)
{
	if (channel == 1) {
		rza1_pfc_pin_assign(P1_4, ALT3, DIIO_PBDC_DIS);
		rza1_pfc_pin_assign(P5_10, ALT5, DIIO_PBDC_DIS);
	}

	if (channel == 2) {
		rza1_pfc_pin_assign(P7_3, ALT5, DIIO_PBDC_DIS);
		rza1_pfc_pin_assign(P7_2, ALT5, DIIO_PBDC_DIS);
	}

	return 0;
}
EXPORT_SYMBOL(hachiko_board_can_pfc_assign);

int hachiko_board_i2c_pfc_assign(int id)
{
	/* set I2C pfc configuration */
	switch (id) {
	case 0:
		rza1_pfc_pin_assign(P1_0, ALT1, DIIO_PBDC_EN);	/* I2C SCL0 */
		rza1_pfc_pin_assign(P1_1, ALT1, DIIO_PBDC_EN);	/* I2C SDA0 */
		break;
	case 1:
		rza1_pfc_pin_assign(P1_2, ALT1, DIIO_PBDC_EN);	/* I2C SCL1 */
		rza1_pfc_pin_assign(P1_3, ALT1, DIIO_PBDC_EN);	/* I2C SDA1 */
		break;
	case 3:
		rza1_pfc_pin_assign(P1_6, ALT1, DIIO_PBDC_EN);	/* I2C SCL3 */
		rza1_pfc_pin_assign(P1_7, ALT1, DIIO_PBDC_EN);	/* I2C SDA3 */
		break;
	}
	return 0;
}
EXPORT_SYMBOL(hachiko_board_i2c_pfc_assign);

static const char *hachiko_boards_compat_dt[] __initdata = {
	"renesas,hachiko",
	NULL,
};

DT_MACHINE_START(RSKRZA1_DT, "hachiko")
	.nr_irqs	= NR_IRQS_LEGACY,
	.map_io		= rza1_map_io,
	.init_early	= rza1_add_early_devices,
	.init_irq	= rza1_init_irq,
	.handle_irq	= gic_handle_irq,
	.init_machine	= hachiko_init,
	.init_late	= shmobile_init_late,
	.timer		= &shmobile_timer,
	.dt_compat	= hachiko_boards_compat_dt,
MACHINE_END
