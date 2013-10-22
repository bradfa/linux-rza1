/*
 * Copyright (C) 2013 Renesas Solutions Corp.
 * Copyright 2004-2009 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_RZA1_DMA_H__
#define __ASM_ARCH_RZA1_DMA_H__

#include <linux/scatterlist.h>
#include <linux/device.h>
#include <linux/dmaengine.h>

/* DMA slave IDs */
enum {
	RZA1DMA_SLAVE_SDHI0_TX = 1,
	RZA1DMA_SLAVE_SDHI0_RX,
	RZA1DMA_SLAVE_SDHI1_TX,
	RZA1DMA_SLAVE_SDHI1_RX,
	RZA1DMA_SLAVE_MMCIF_TX,
	RZA1DMA_SLAVE_MMCIF_RX,
};

struct chcfg_reg {
	u32	reqd:1;
	u32	loen:1;
	u32	hien:1;
	u32	lvl:1;
	u32	am:3;
	u32	sds:4;
	u32	dds:4;
	u32	tm:1;
};

struct dmars_reg {
	u32	rid:2;
	u32	mid:7;
};

struct rza1_dma_slave_config {
	int			slave_id;
	dma_addr_t		addr;
	struct chcfg_reg	chcfg;
	struct dmars_reg	dmars;
};

struct rza1_dma_pdata {
	const struct rza1_dma_slave_config *slave;
	int slave_num;
	int channel_num;
};

bool rza1dma_chan_filter(struct dma_chan *chan, void *arg);
#endif
