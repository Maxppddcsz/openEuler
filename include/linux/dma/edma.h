/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018-2019 Synopsys, Inc. and/or its affiliates.
 * Synopsys DesignWare eDMA core driver
 *
 * Author: Gustavo Pimentel <gustavo.pimentel@synopsys.com>
 */

#ifndef _DW_EDMA_H
#define _DW_EDMA_H

#include <linux/device.h>
#include <linux/dmaengine.h>

struct dw_edma;

/**
 * enum dw_edma_chip_flags - Flags specific to an eDMA chip
 * @DW_EDMA_CHIP_LOCAL:		eDMA is used locally by an endpoint
 */
enum dw_edma_chip_flags {
	DW_EDMA_CHIP_LOCAL	= BIT(0),
};

/**
 * struct dw_edma_chip - representation of DesignWare eDMA controller hardware
 * @dev:		 struct device of the eDMA controller
 * @id:			 instance ID
 * @irq:		 irq line
 * @dw:			 struct dw_edma that is filed by dw_edma_probe()
 * @flags		 dw_edma_chip_flags
 */
struct dw_edma_chip {
	struct device		*dev;
	int			id;
	int			irq;
	u32			flags;
	struct dw_edma		*dw;
};

/* Export to the platform drivers */
#if IS_ENABLED(CONFIG_DW_EDMA)
int dw_edma_probe(struct dw_edma_chip *chip);
int dw_edma_remove(struct dw_edma_chip *chip);
#else
static inline int dw_edma_probe(struct dw_edma_chip *chip)
{
	return -ENODEV;
}

static inline int dw_edma_remove(struct dw_edma_chip *chip)
{
	return 0;
}
#endif /* CONFIG_DW_EDMA */

#endif /* _DW_EDMA_H */
