// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: failover time file
 * Create: 2023-08-02
 */

#include "failover_time.h"
#include <linux/jiffies.h>
#include <linux/sunrpc/clnt.h>
#include "enfs_config.h"
#include "enfs_log.h"
#include "failover_com.h"
#include "pm_ping.h"

static unsigned long failover_get_mulitipath_timeout(struct rpc_clnt *clnt)
{
	unsigned long config_tmo = enfs_get_config_multipath_timeout() * HZ;
	unsigned long clnt_tmo = clnt->cl_timeout->to_initval;

	if (config_tmo == 0)
		return clnt_tmo;

	return config_tmo > clnt_tmo ? clnt_tmo : config_tmo;
}

void failover_adjust_task_timeout(struct rpc_task *task, void *condition)
{
	struct rpc_clnt *clnt = NULL;
	unsigned long tmo;
	int disable_mpath = enfs_get_config_multipath_state();

	if (disable_mpath != ENFS_MULTIPATH_ENABLE) {
		enfs_log_debug("Multipath is not enabled.\n");
		return;
	}

	clnt = task->tk_client;
	if (unlikely(clnt == NULL)) {
		enfs_log_error("task associate client is NULL.\n");
		return;
	}

	if (!failover_is_enfs_clnt(clnt)) {
		enfs_log_debug("The clnt is not a enfs-managed type.\n");
		return;
	}

	tmo = failover_get_mulitipath_timeout(clnt);
	if (tmo == 0) {
		enfs_log_debug("Multipath is not enabled.\n");
		return;
	}

	if (task->tk_timeout != 0)
		task->tk_timeout =
		task->tk_timeout < tmo ? task->tk_timeout : tmo;
	else
		task->tk_timeout = tmo;
}

void failover_init_task_req(struct rpc_task *task, struct rpc_rqst *req)
{
	struct rpc_clnt *clnt = NULL;
	int disable_mpath = enfs_get_config_multipath_state();

	if (disable_mpath != ENFS_MULTIPATH_ENABLE) {
		enfs_log_debug("Multipath is not enabled.\n");
		return;
	}

	clnt = task->tk_client;
	if (unlikely(clnt == NULL)) {
		enfs_log_error("task associate client is NULL.\n");
		return;
	}

	if (!failover_is_enfs_clnt(clnt)) {
		enfs_log_debug("The clnt is not a enfs-managed type.\n");
		return;
	}

	if (!pm_ping_is_test_xprt_task(task))
		req->rq_timeout = failover_get_mulitipath_timeout(clnt);
	else {
		req->rq_timeout = enfs_get_config_path_detect_timeout() * HZ;
		req->rq_majortimeo = req->rq_timeout + jiffies;
	}

	/*
	 * when task is retried, the req is new, we lost major-timeout times,
	 * so we have to restore req major
	 * timeouts from the task, if it is stored.
	 */
	if (task->tk_major_timeo != 0)
		req->rq_majortimeo = task->tk_major_timeo;
	else
		task->tk_major_timeo = req->rq_majortimeo;
}
