/* SPDX-License-Identifier: GPL-2.0 */
/* Client-side SUNRPC ENFS adapter header.
 * Copyright (c) 2023. Huawei Technologies Co., Ltd. All rights reserved.
 */
#ifndef _SUNRPC_ENFS_ADAPTER_H_
#define _SUNRPC_ENFS_ADAPTER_H_
#include <linux/sunrpc/clnt.h>

#if IS_ENABLED(CONFIG_ENFS)

static inline void rpc_xps_nactive_add_one(struct rpc_xprt_switch *xps)
{
	xps->xps_nactive--;
}

static inline void rpc_xps_nactive_sub_one(struct rpc_xprt_switch *xps)
{
	xps->xps_nactive--;
}

struct rpc_xprt	*rpc_task_get_xprt
(struct rpc_clnt *clnt, struct rpc_xprt *xprt);

struct rpc_multipath_ops {
	struct module *owner;
	void (*create_clnt)(struct rpc_create_args *args,
			    struct rpc_clnt *clnt);
	void (*releas_clnt)(struct rpc_clnt *clnt);
	void (*create_xprt)(struct rpc_xprt *xprt);
	void (*destroy_xprt)(struct rpc_xprt *xprt);
	void (*xprt_iostat)(struct rpc_task *task);
	void (*failover_handle)(struct rpc_task *task);
	bool (*task_need_call_start_again)(struct rpc_task *task);
	void (*adjust_task_timeout)(struct rpc_task *task, void *condition);
	void (*init_task_req)(struct rpc_task *task, struct rpc_rqst *req);
	bool (*prepare_transmit)(struct rpc_task *task);
};

extern struct rpc_multipath_ops __rcu *multipath_ops;
void rpc_init_task_retry_counters(struct rpc_task *task);
int rpc_multipath_ops_register(struct rpc_multipath_ops *ops);
int rpc_multipath_ops_unregister(struct rpc_multipath_ops *ops);
struct rpc_multipath_ops *rpc_multipath_ops_get(void);
void rpc_multipath_ops_put(struct rpc_multipath_ops *ops);
void rpc_task_release_xprt(struct rpc_clnt *clnt, struct rpc_xprt *xprt);
void rpc_multipath_ops_create_clnt(struct rpc_create_args *args,
				   struct rpc_clnt *clnt);
void rpc_multipath_ops_releas_clnt(struct rpc_clnt *clnt);
bool rpc_multipath_ops_create_xprt(struct rpc_xprt *xprt);
void rpc_multipath_ops_destroy_xprt(struct rpc_xprt *xprt);
void rpc_multipath_ops_xprt_iostat(struct rpc_task *task);
void rpc_multipath_ops_failover_handle(struct rpc_task *task);
bool rpc_multipath_ops_task_need_call_start_again(struct rpc_task *task);
void rpc_multipath_ops_adjust_task_timeout(struct rpc_task *task,
					   void *condition);
void rpc_multipath_ops_init_task_req(struct rpc_task *task,
				     struct rpc_rqst *req);
bool rpc_multipath_ops_prepare_transmit(struct rpc_task *task);

#else
static inline struct rpc_xprt *rpc_task_get_xprt(struct rpc_clnt *clnt,
						 struct rpc_xprt *xprt)
{
	return NULL;
}

static inline void rpc_task_release_xprt(struct rpc_clnt *clnt,
					 struct rpc_xprt *xprt)
{
}

static inline void rpc_xps_nactive_add_one(struct rpc_xprt_switch *xps)
{
}

static inline void rpc_xps_nactive_sub_one(struct rpc_xprt_switch *xps)
{
}

static inline void rpc_multipath_ops_create_clnt
(struct rpc_create_args *args, struct rpc_clnt *clnt)
{
}

static inline void rpc_multipath_ops_releas_clnt(struct rpc_clnt *clnt)
{
}

static inline bool rpc_multipath_ops_create_xprt(struct rpc_xprt *xprt)
{
	return false;
}

static inline void rpc_multipath_ops_destroy_xprt(struct rpc_xprt *xprt)
{
}

static inline void rpc_multipath_ops_xprt_iostat(struct rpc_task *task)
{
}

static inline void rpc_multipath_ops_failover_handle(struct rpc_task *task)
{
}

static inline
bool rpc_multipath_ops_task_need_call_start_again(struct rpc_task *task)
{
	return false;
}

static inline void
rpc_multipath_ops_adjust_task_timeout(struct rpc_task *task, void *condition)
{
}

static inline void
rpc_multipath_ops_init_task_req(struct rpc_task *task, struct rpc_rqst *req)
{
}

static inline bool rpc_multipath_ops_prepare_transmit(struct rpc_task *task)
{
	return false;
}

#endif
#endif // _SUNRPC_ENFS_ADAPTER_H_
