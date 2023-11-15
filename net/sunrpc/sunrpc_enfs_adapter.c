// SPDX-License-Identifier: GPL-2.0
/* Client-side SUNRPC ENFS adapter header.
 * Copyright (c) 2023. Huawei Technologies Co., Ltd. All rights reserved.
 */
#include <linux/sunrpc/sunrpc_enfs_adapter.h>

struct rpc_multipath_ops __rcu *multipath_ops;

void rpc_init_task_retry_counters(struct rpc_task *task)
{
	/* Initialize retry counters */
	task->tk_garb_retry = 2;
	task->tk_cred_retry = 2;
	task->tk_rebind_retry = 2;
}
EXPORT_SYMBOL_GPL(rpc_init_task_retry_counters);

struct rpc_xprt *
rpc_task_get_xprt(struct rpc_clnt *clnt, struct rpc_xprt *xprt)
{
	struct rpc_xprt_switch *xps;

	if (!xprt)
		return NULL;
	rcu_read_lock();
	xps = rcu_dereference(clnt->cl_xpi.xpi_xpswitch);
	atomic_long_inc(&xps->xps_queuelen);
	rcu_read_unlock();
	atomic_long_inc(&xprt->queuelen);

	return xprt;
}

int rpc_multipath_ops_register(struct rpc_multipath_ops *ops)
{
	struct rpc_multipath_ops *old;

	old = cmpxchg((struct rpc_multipath_ops **)&multipath_ops, NULL, ops);
	if (!old || old == ops)
		return 0;
	pr_err("regist rpc_multipath ops %p fail. old %p\n", ops, old);
	return -EPERM;
}
EXPORT_SYMBOL_GPL(rpc_multipath_ops_register);

int rpc_multipath_ops_unregister(struct rpc_multipath_ops *ops)
{
	struct rpc_multipath_ops *old;

	old = cmpxchg((struct rpc_multipath_ops **)&multipath_ops, ops, NULL);
	if (!old || old == ops)
		return 0;
	pr_err("regist rpc_multipath ops %p fail. old %p\n", ops, old);
	return -EPERM;
}
EXPORT_SYMBOL_GPL(rpc_multipath_ops_unregister);

struct rpc_multipath_ops *rpc_multipath_ops_get(void)
{
	struct rpc_multipath_ops *ops;

	rcu_read_lock();
	ops = rcu_dereference(multipath_ops);
	if (!ops) {
		rcu_read_unlock();
		return NULL;
	}
	if (!try_module_get(ops->owner))
		ops = NULL;
	rcu_read_unlock();
	return ops;
}
EXPORT_SYMBOL_GPL(rpc_multipath_ops_get);

void rpc_multipath_ops_put(struct rpc_multipath_ops *ops)
{
	if (ops)
		module_put(ops->owner);
}
EXPORT_SYMBOL_GPL(rpc_multipath_ops_put);

void rpc_task_release_xprt(struct rpc_clnt *clnt, struct rpc_xprt *xprt)
{
	struct rpc_xprt_switch *xps;

	atomic_long_dec(&xprt->queuelen);
	rcu_read_lock();
	xps = rcu_dereference(clnt->cl_xpi.xpi_xpswitch);
	atomic_long_dec(&xps->xps_queuelen);
	rcu_read_unlock();

	xprt_put(xprt);
}

void rpc_multipath_ops_create_clnt(struct rpc_create_args *args,
				   struct rpc_clnt *clnt)
{
	struct rpc_multipath_ops *mops;

	if (args->multipath_option) {
		mops = rpc_multipath_ops_get();
		if (mops && mops->create_clnt)
			mops->create_clnt(args, clnt);
		rpc_multipath_ops_put(mops);
	}
}

void rpc_multipath_ops_releas_clnt(struct rpc_clnt *clnt)
{
	struct rpc_multipath_ops *mops;

	mops = rpc_multipath_ops_get();
	if (mops && mops->releas_clnt)
		mops->releas_clnt(clnt);

	rpc_multipath_ops_put(mops);
}

bool rpc_multipath_ops_create_xprt(struct rpc_xprt *xprt)
{
	struct rpc_multipath_ops *mops = NULL;

	mops = rpc_multipath_ops_get();
	if (mops && mops->create_xprt) {
		mops->create_xprt(xprt);
		if (!xprt->multipath_context) {
			rpc_multipath_ops_put(mops);
			return true;
		}
	}
	rpc_multipath_ops_put(mops);
	return false;
}

void rpc_multipath_ops_destroy_xprt(struct rpc_xprt *xprt)
{
	struct rpc_multipath_ops *mops;

	if (xprt->multipath_context) {
		mops = rpc_multipath_ops_get();
		if (mops && mops->destroy_xprt)
			mops->destroy_xprt(xprt);
		rpc_multipath_ops_put(mops);
	}
}

void rpc_multipath_ops_xprt_iostat(struct rpc_task *task)
{
	struct rpc_multipath_ops *mops;

	mops = rpc_multipath_ops_get();
	if (task->tk_client && mops && mops->xprt_iostat)
		mops->xprt_iostat(task);
	rpc_multipath_ops_put(mops);
}

void rpc_multipath_ops_failover_handle(struct rpc_task *task)
{
	struct rpc_multipath_ops *mpath_ops = NULL;

	mpath_ops = rpc_multipath_ops_get();
	if (mpath_ops && mpath_ops->failover_handle)
		mpath_ops->failover_handle(task);
	rpc_multipath_ops_put(mpath_ops);
}

bool rpc_multipath_ops_task_need_call_start_again(struct rpc_task *task)
{
	struct rpc_multipath_ops *mpath_ops = NULL;
	bool ret = false;

	mpath_ops = rpc_multipath_ops_get();
	if (mpath_ops && mpath_ops->task_need_call_start_again)
		ret = mpath_ops->task_need_call_start_again(task);
	rpc_multipath_ops_put(mpath_ops);
	return ret;
}

void rpc_multipath_ops_adjust_task_timeout(struct rpc_task *task,
					   void *condition)
{
	struct rpc_multipath_ops *mops = NULL;

	mops = rpc_multipath_ops_get();
	if (mops && mops->adjust_task_timeout)
		mops->adjust_task_timeout(task, NULL);
	rpc_multipath_ops_put(mops);
}

void rpc_multipath_ops_init_task_req(struct rpc_task *task,
				     struct rpc_rqst *req)
{
	struct rpc_multipath_ops *mops = NULL;

	mops = rpc_multipath_ops_get();
	if (mops && mops->init_task_req)
		mops->init_task_req(task, req);
	rpc_multipath_ops_put(mops);
}

bool rpc_multipath_ops_prepare_transmit(struct rpc_task *task)
{
	struct rpc_multipath_ops *mops = NULL;

	mops = rpc_multipath_ops_get();
	if (mops && mops->prepare_transmit) {
		if (!(mops->prepare_transmit(task))) {
			rpc_multipath_ops_put(mops);
			return true;
		}
	}
	rpc_multipath_ops_put(mops);
	return false;
}
