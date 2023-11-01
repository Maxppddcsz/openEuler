/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: nfs path failover header file
 * Create: 2023-08-02
 */

#ifndef FAILOVER_PATH_H
#define FAILOVER_PATH_H

#include <linux/sunrpc/sched.h>

void failover_handle(struct rpc_task *task);
bool failover_prepare_transmit(struct rpc_task *task);
bool failover_task_need_call_start_again(struct rpc_task *task);
#endif // FAILOVER_PATH_H
