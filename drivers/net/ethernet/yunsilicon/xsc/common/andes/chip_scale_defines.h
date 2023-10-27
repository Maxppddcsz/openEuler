/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 - 2023, Shanghai Yunsilicon Technology Co., Ltd.
 * All rights reserved.
 */

#ifndef _CHIP_SCALE_DEFINES_H_
#define _CHIP_SCALE_DEFINES_H_

#define MAIN_CLK_FREQ   (100*1000*1000)
#define PCIE_PORT_NUM   1
#define NIF_PORT_NUM    2
#define FUNC_ID_NUM     514
#define MSIX_VEC_NUM    2048
#define PIO_TLPQ_NUM    2

#define PRI_NUM         4
#define QP_NUM_MAX      2048
#define CQ_NUM_MAX      2048
#define SQ_SIZE_MAX     1024    // unit: send wqe
#define RQ_SIZE_MAX     1024    // unit: recv wqe
#define CQ_SIZE_MAX     32768   // unit: cqe
#define GRP_NUM_MAX     1024
#define CLUSTER_NUM_MAX 1

#endif
