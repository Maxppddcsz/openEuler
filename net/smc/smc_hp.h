/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Shared Memory Communications over RDMA (SMC-R) and RoCE
 *
 *  smc_hp.h: smc high performance interface of SMC subsystem.
 *
 *  Copyright (c) 2023, Sangfor Technologies Inc.
 *
 *  Author: Gengbiao Shen <shengengbiao@sangfor.com.cn>
 *
 */

#ifndef SMC_HP_H
#define SMC_HP_H

#include <linux/types.h>

/* params for smc high performance mode */
#define SMC_HP_MODE true
#define SMC_RX_MICRO_LOOP_US 100
#define SMC_TX_MICRO_LOOP_US 0

extern bool smc_hp_mode;
extern ushort smc_rx_micro_loop_us;
extern ushort smc_tx_micro_loop_us;

#endif /* SMC_HP_H */
