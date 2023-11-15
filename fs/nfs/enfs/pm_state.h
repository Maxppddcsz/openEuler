/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: path state header file
 * Create: 2023-08-12
 */

#ifndef PM_STATE_H
#define PM_STATE_H

#include <linux/types.h>
#include <linux/sunrpc/xprt.h>

enum pm_path_state {
	PM_STATE_INIT,
	PM_STATE_NORMAL,
	PM_STATE_FAULT,
	PM_STATE_UNDEFINED  // xprt is not multipath xprt
};

void pm_set_path_state(struct rpc_xprt *xprt, enum pm_path_state state);
enum pm_path_state pm_get_path_state(struct rpc_xprt *xprt);

void pm_get_path_state_desc(struct rpc_xprt *xprt, char *buf, int len);
void pm_get_xprt_state_desc(struct rpc_xprt *xprt, char *buf, int len);

#endif // PM_STATE_H
