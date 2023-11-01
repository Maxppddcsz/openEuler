/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: failover time commont header file
 * Create: 2023-08-02
 */
#ifndef FAILOVER_COMMON_H
#define FAILOVER_COMMON_H

static inline bool failover_is_enfs_clnt(struct rpc_clnt *clnt)
{
	struct rpc_clnt *next = clnt->cl_parent;

	while (next) {
		if (next == next->cl_parent)
			break;
		next = next->cl_parent;
	}

	return next != NULL ? next->cl_enfs : clnt->cl_enfs;
}

#endif // FAILOVER_COMMON_H
