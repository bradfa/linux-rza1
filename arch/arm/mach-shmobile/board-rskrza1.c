/*
 * rskrza1 board support
 *
 * Copyright (C) 2013-2014  Renesas Solutions Corp.
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

/* MTD */
static struct mtd_partition nor_flash_partitions[] = {
	{
		.name		= "loader",
		.offset		= 0x00000000,
		.size		= 256 * 1024 * 2,
	},
	{
		.name		= "bootenv",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 128 * 1024 * 2,
		.mask_flags	= MTD_WRITEABLE,
	},
	{
		.name		= "kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 4 * 1024 * 1024,
	},
	{
		.name		= "data",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
	},
};

static struct physmap_flash_data nor_flash_data = {
	.width		= 2,
	.parts		= nor_flash_partitions,
	.nr_parts	= ARRAY_SIZE(nor_flash_partitions),
};

static struct resource nor_flash_resources[] = {
	[0]	= {
		.start	= 0x00000000,
		.end	= 0x08000000 - 1,
		.flags	= IORESOURCE_MEM,
	}
};

static struct platform_device nor_flash_device = {
	.name		= "physmap-flash",
	.dev		= {
		.platform_data	= &nor_flash_data,
	},
	.num_resources	= ARRAY_SIZE(nor_flash_resources),
	.resource	= nor_flash_resources,
};

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

static struct platform_device *rskrza1_devices[] __initdata = {
	&nor_flash_device,
	&mmc_device,
	&sdhi0_device,
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
		.size		= 0x00400000,
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


static struct spi_board_info rskrza1_spi_devices[] __initdata = {
#if (defined(CONFIG_SPI_MASTER) && defined(CONFIG_MACH_RSKRZA1))
	{
		.modalias = "wm8978",
		.max_speed_hz = 3125000,
		/* max spi clock (SCK) speed in HZ */
		.bus_num = 4,
		.chip_select = 0,
		.mode = SPI_MODE_3,
	},
#endif
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

static void __init rskrza1_init_spi(void)
{
	/* register SPI device information */
	spi_register_board_info(rskrza1_spi_devices,
				ARRAY_SIZE(rskrza1_spi_devices));
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


void __init rskrza1_init(void)
{
	if (disable_sdhi)
		sdhi0_device.name = "mobile_sdhi(hidden)";

	platform_add_devices(rskrza1_devices, ARRAY_SIZE(rskrza1_devices));

	rza1_pinmux_setup();

	rza1_devices_setup();

	/* GPIO settings. */
#if defined(CONFIG_SND_SCUX_GENMAI)
	/* Audio Codec(WM8978) is use SSIF0(P4_4-P4_7) */
	rza1_pfc_pin_assign(P4_4, ALT5, DIIO_PBDC_EN);		/* SSISCK0 */
	rza1_pfc_pin_assign(P4_5, ALT5, DIIO_PBDC_EN);		/* SSIWS0 */
	rza1_pfc_pin_assign(P4_6, ALT5, DIIO_PBDC_EN);		/* SSIRxD0 */
	rza1_pfc_pin_assign(P4_7, ALT5, SWIO_OUT_PBDCEN);	/* SSITxD0 */
#else
	rza1_pfc_pin_assign(P4_4, ALT2, DIIO_PBDC_EN);		/* SPI RSPCK1 */
	rza1_pfc_pin_assign(P4_5, ALT2, DIIO_PBDC_EN);		/* SPI SSL10 */
	rza1_pfc_pin_assign(P4_6, ALT2, DIIO_PBDC_EN);		/* SPI MOSI1 */
	rza1_pfc_pin_assign(P4_7, ALT2, DIIO_PBDC_EN);		/* SPI MISO1 */
#endif

	/* ADC */
	rza1_pfc_pin_assign(P1_8, ALT1, DIIO_PBDC_DIS);		/* AN0 */
	rza1_pfc_pin_assign(P1_9, ALT1, DIIO_PBDC_DIS);		/* AN1 */
	rza1_pfc_pin_assign(P1_10, ALT1, DIIO_PBDC_DIS);	/* AN2 */
	rza1_pfc_pin_assign(P1_11, ALT1, DIIO_PBDC_DIS);	/* AN3 */

#if defined(CONFIG_SND_SCUX_GENMAI) || defined(CONFIG_PWM_RZA1)
#if defined(CONFIG_SND_SCUX_GENMAI)
	/* Audio Codec(WM8978) is use RSPCK4 */
	rza1_pfc_pin_assign(P4_0, ALT7, DIIO_PBDC_DIS);		/* RSPCK4 */
	rza1_pfc_pin_assign(P4_1, ALT7, DIIO_PBDC_DIS);		/* SSL40 */
	rza1_pfc_pin_assign(P4_2, ALT7, DIIO_PBDC_DIS);		/* MOSI4 */
#endif
#if defined(CONFIG_PWM_RZA1)
	rza1_pfc_pin_assign(P3_8, ALT6, SWIO_OUT_PBDCEN);	/* TIOC4A */
#endif
#else
	/* set MMCIF pfc configuration */
	rza1_pfc_pin_assign(P3_8, ALT8, DIIO_PBDC_DIS);		/* MMC_CD */
	rza1_pfc_pin_assign(P3_10, ALT8, DIIO_PBDC_DIS);	/* MMC_D1 */
	rza1_pfc_pin_assign(P3_11, ALT8, DIIO_PBDC_DIS);	/* MMC_D0 */
	rza1_pfc_pin_assign(P3_12, ALT8, DIIO_PBDC_DIS);	/* MMC_CLK */
	rza1_pfc_pin_assign(P3_13, ALT8, DIIO_PBDC_DIS);	/* MMC_CMD */
	rza1_pfc_pin_assign(P3_14, ALT8, DIIO_PBDC_DIS);	/* MMC_D3 */
	rza1_pfc_pin_assign(P3_15, ALT8, DIIO_PBDC_DIS);	/* MMC_D2 */
	rza1_pfc_pin_assign(P4_0, ALT8, DIIO_PBDC_DIS);		/* MMC_D4 */
	rza1_pfc_pin_assign(P4_1, ALT8, DIIO_PBDC_DIS);		/* MMC_D5 */
	rza1_pfc_pin_assign(P4_2, ALT8, DIIO_PBDC_DIS);		/* MMC_D6 */
	rza1_pfc_pin_assign(P4_3, ALT8, DIIO_PBDC_DIS);		/* MMC_D7 */
#endif

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
	rskrza1_init_spi();
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

int rskrza1_board_i2c_pfc_assign(int id)
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
	case 2:
		rza1_pfc_pin_assign(P1_4, ALT1, DIIO_PBDC_EN);	/* I2C SCL2 */
		rza1_pfc_pin_assign(P1_5, ALT1, DIIO_PBDC_EN);	/* I2C SDA2 */
		break;
	case 3:
		rza1_pfc_pin_assign(P1_6, ALT1, DIIO_PBDC_EN);	/* I2C SCL3 */
		rza1_pfc_pin_assign(P1_7, ALT1, DIIO_PBDC_EN);	/* I2C SDA3 */
		break;
	}
	return 0;
}
EXPORT_SYMBOL(rskrza1_board_i2c_pfc_assign);

static const char *rskrza1_boards_compat_dt[] __initdata = {
	"renesas,rskrza1",
	NULL,
};

DT_MACHINE_START(RSKRZA1_DT, "rskrza1")
	.nr_irqs	= NR_IRQS_LEGACY,
	.map_io		= rza1_map_io,
	.init_early	= rza1_add_early_devices,
	.init_irq	= rza1_init_irq,
	.handle_irq	= gic_handle_irq,
	.init_machine	= rskrza1_init,
	.init_late	= shmobile_init_late,
	.timer		= &shmobile_timer,
	.dt_compat	= rskrza1_boards_compat_dt,
MACHINE_END
