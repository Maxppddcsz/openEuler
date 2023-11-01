// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#include "enfs_multipath.h"
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/kallsyms.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/atomic.h>
#include <linux/sunrpc/addr.h>
#include <linux/sunrpc/bc_xprt.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/metrics.h>
#include <linux/sunrpc/rpc_pipe_fs.h>
#include <linux/sunrpc/xprt.h>
#include <linux/sunrpc/xprtmultipath.h>
#include <linux/types.h>
#include <linux/un.h>
#include <linux/utsname.h>
#include <linux/workqueue.h>
#include <trace/events/sunrpc.h>
#include <linux/sunrpc/sunrpc_enfs_adapter.h>

#include "enfs.h"
#include "enfs_log.h"
#include "enfs_multipath_parse.h"
#include "enfs_path.h"
#include "enfs_proc.h"
#include "enfs_remount.h"
#include "enfs_roundrobin.h"
#include "failover_path.h"
#include "failover_time.h"
#include "pm_ping.h"
#include "pm_state.h"

struct xprt_attach_callback_data {
	atomic_t *conditon;
	wait_queue_head_t *waitq;
};

struct xprt_attach_info {
	struct sockaddr_storage *localAddress;
	struct sockaddr_storage *remoteAddress;
	struct rpc_xprt *xprt;
	struct xprt_attach_callback_data *data;
};

static DECLARE_WAIT_QUEUE_HEAD(path_attach_wait_queue);


static void sockaddr_set_port(struct sockaddr *addr, int port)
{
	switch (addr->sa_family) {
	case AF_INET:
		((struct sockaddr_in *)addr)->sin_port = htons(port);
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)addr)->sin6_port = htons(port);
		break;
	}
}

static int sockaddr_ip_to_str(struct sockaddr *addr, char *buf, int len)
{
	switch (addr->sa_family) {
	case AF_INET: {
		struct sockaddr_in *sin = (struct sockaddr_in *)addr;

		snprintf(buf, len, "%pI4", &sin->sin_addr);
		return 0;
	}
	case AF_INET6: {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;

		snprintf(buf, len, "%pI6", &sin6->sin6_addr);
		return 0;
	}
	default:
		break;
	}
	return 1;
}

void print_enfs_multipath_addr(struct sockaddr *local, struct sockaddr *remote)
{
	char buf1[128];
	char buf2[128];

	sockaddr_ip_to_str(local, buf1, sizeof(buf1));
	sockaddr_ip_to_str(remote, buf2, sizeof(buf2));

	pr_info("local:%s remote:%s\n", buf1, buf2);
}

static int enfs_servername(char *servername, unsigned long long len,
						   struct rpc_create_args *args)
{
	struct sockaddr_un *sun = (struct sockaddr_un *)args->address;
	struct sockaddr_in *sin = (struct sockaddr_in *)args->address;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)args->address;

	servername[0] = '\0';
	switch (args->address->sa_family) {
	case AF_LOCAL:
		snprintf(servername, len, "%s", sun->sun_path);
		break;
	case AF_INET:
		snprintf(servername, len, "%pI4", &sin->sin_addr.s_addr);
		break;
	case AF_INET6:
		snprintf(servername, len, "%pI6", &sin6->sin6_addr);
		break;
	default:
		pr_info("invalid family:%d\n",
				args->address->sa_family);
		return -EINVAL;
	}
	return 0;
}

static void pm_xprt_ping_callback(void *data)
{
	struct xprt_attach_callback_data *attach_callback_data =
	(struct xprt_attach_callback_data *)data;

	atomic_dec(attach_callback_data->conditon);
	wake_up(attach_callback_data->waitq);
}

static
int enfs_add_xprt_setup(struct rpc_clnt *clnt, struct rpc_xprt_switch *xps,
			struct rpc_xprt *xprt, void *data)
{
	int ret;
	struct enfs_xprt_context *ctx;
	struct xprt_attach_info *attach_info = data;
	struct sockaddr_storage *srcaddr = attach_info->localAddress;

	ctx = (struct enfs_xprt_context *)xprt->multipath_context;
	ctx->stats = rpc_alloc_iostats(clnt);
	ctx->main = false;
	ctx->srcaddr = *srcaddr;
	pm_set_path_state(xprt, PM_STATE_INIT);
	pm_ping_set_path_check_state(xprt, PM_CHECK_INIT);

	attach_info->xprt = xprt;
	xprt_get(xprt);

	ret = pm_ping_rpc_test_xprt_with_callback(clnt, xprt,
				pm_xprt_ping_callback, attach_info->data);
	if (ret != 1) {
		enfs_log_error("Failed to add ping task.\n");
		atomic_dec(attach_info->data->conditon);
	}

	// so that rpc_clnt_add_xprt
	// does not call rpc_xprt_switch_add_xprt
	return 1;
}

int enfs_configure_xprt_to_clnt(struct xprt_create *xprtargs,
			struct rpc_clnt *clnt,
			struct xprt_attach_info *attach_info)
{
	int err = 0;

	xprtargs->srcaddr = (struct sockaddr *)attach_info->localAddress;
	xprtargs->dstaddr = (struct sockaddr *)attach_info->remoteAddress;

	sockaddr_set_port((struct sockaddr *)attach_info->localAddress, 2049);
	sockaddr_set_port((struct sockaddr *)attach_info->remoteAddress, 2049);
	print_enfs_multipath_addr((struct sockaddr *)attach_info->localAddress,
				(struct sockaddr *)attach_info->remoteAddress);

	err = rpc_clnt_add_xprt(clnt, xprtargs,
							enfs_add_xprt_setup,
							attach_info);
	if (err != 1) {
		enfs_log_error("clnt add xprt err:%d\n", err);
		return err;
	}
	return 0;
}

// Calculate the greatest common divisor of two numbers
static int enfs_cal_gcd(int num1, int num2)
{
	if (num2 == 0)
		return num1;
	return enfs_cal_gcd(num2, num1 % num2);
}

bool enfs_cmp_addrs(struct sockaddr_storage *srcaddr,
					struct sockaddr_storage *dstaddr,
					struct sockaddr_storage *localAddress,
					struct sockaddr_storage *remoteAddress)
{
	if (rpc_cmp_addr((struct sockaddr *)srcaddr,
		(struct sockaddr *)localAddress)) {

		if (rpc_cmp_addr((struct sockaddr *)dstaddr,
			(struct sockaddr *)remoteAddress))
			return true;

	}

	return false;
}

bool enfs_xprt_addrs_is_same(struct rpc_xprt *xprt,
			struct sockaddr_storage *localAddress,
			struct sockaddr_storage *remoteAddress)
{
	struct enfs_xprt_context *xprt_local_info = NULL;
	struct sockaddr_storage *srcaddr = NULL;
	struct sockaddr_storage *dstaddr = NULL;

	if (xprt == NULL)
		return true;
	xprt_local_info = (struct enfs_xprt_context *)xprt->multipath_context;
	srcaddr = &xprt_local_info->srcaddr;
	dstaddr = &xprt->addr;

	return enfs_cmp_addrs(srcaddr, dstaddr, localAddress, remoteAddress);
}

bool enfs_already_have_xprt(struct rpc_clnt *clnt,
			struct sockaddr_storage *localAddress,
			struct sockaddr_storage *remoteAddress)
{
	struct rpc_xprt *pos = NULL;
	struct rpc_xprt_switch *xps = NULL;

	rcu_read_lock();
	xps = xprt_switch_get(rcu_dereference(clnt->cl_xpi.xpi_xpswitch));
	if (xps == NULL) {
		rcu_read_unlock();
		return false;
	}
	list_for_each_entry_rcu(pos, &xps->xps_xprt_list, xprt_switch) {
		if (enfs_xprt_addrs_is_same(pos, localAddress, remoteAddress)) {
			xprt_switch_put(xps);
			rcu_read_unlock();
			return true;
		}
	}
	xprt_switch_put(xps);
	rcu_read_unlock();
	return false;
}

static void enfs_xprt_switch_add_xprt(struct rpc_clnt *clnt,
			struct rpc_xprt *xprt)
{
	struct rpc_xprt_switch *xps;

	rcu_read_lock();
	xps = rcu_dereference(clnt->cl_xpi.xpi_xpswitch);
	spin_lock(&xps->xps_lock);
	if (xps->xps_net == xprt->xprt_net || xps->xps_net == NULL)
		xprt_switch_add_xprt_locked(xps, xprt);
	spin_unlock(&xps->xps_lock);
	rcu_read_unlock();
}

static void enfs_add_xprts_to_clnt(struct rpc_clnt *clnt,
			struct xprt_attach_info *attach_infos,
			int total_combinations)
{
	struct rpc_xprt *xprt;
	enum pm_path_state state;
	int i;
	int link_count = 0;

	for (i = 0; i < total_combinations; i++) {
		xprt = attach_infos[i].xprt;

		if (xprt == NULL)
			continue;

		state = pm_get_path_state(xprt);

		if (link_count < MAX_XPRT_NUM_PER_CLIENT &&
			state == PM_STATE_NORMAL) {

			enfs_xprt_switch_add_xprt(clnt, xprt);
			link_count++;
			continue;

		}

		xprt_put(xprt);
	}
}

static void enfs_combine_addr(struct xprt_create *xprtargs,
			struct rpc_clnt *clnt,
			struct nfs_ip_list *local,
			struct nfs_ip_list *remote)
{
	int i;
	int err;
	int local_index;
	int remote_index;
	int link_count = 0;
	int local_total = local->count;
	int remote_total = remote->count;
	int local_remote_total_lcm;
	int total_combinations = local_total * remote_total;
	struct xprt_attach_info *attach_infos;
	atomic_t wait_queue_condition;

	struct xprt_attach_callback_data attach_callback_data = {
		&wait_queue_condition, &path_attach_wait_queue};

	atomic_set(&wait_queue_condition, total_combinations);

	pr_info("local count:%d remote count:%d\n", local_total, remote_total);
	if (local_total == 0 || remote_total == 0) {
		pr_err("no ip list is present.\n");
		return;
	}

	attach_infos = kzalloc(
			(total_combinations) * sizeof(struct xprt_attach_info),
			GFP_KERNEL);
	if (attach_infos == NULL) {
		enfs_log_error("Failed to kzalloc memory\n");
		return;
	}
	// Calculate the least common multiple of local_total and remote_total
	local_remote_total_lcm = total_combinations /
				enfs_cal_gcd(local_total, remote_total);

	// It needs to be offset one for lcm times of
	// cycle so that all possible link setup method would be traversed
	for (i = 0; i < total_combinations; i++) {
		local_index = i % local_total;
		remote_index = (i + link_count / local_remote_total_lcm) %
						remote_total;

		if (enfs_already_have_xprt(clnt,
			&local->address[local_index],
			&remote->address[remote_index])) {

			atomic_dec(&wait_queue_condition);
			link_count++;
			continue;

		}

		attach_infos[i].localAddress = &local->address[local_index];
		attach_infos[i].remoteAddress = &remote->address[remote_index];
		attach_infos[i].data = &attach_callback_data;

		err = enfs_configure_xprt_to_clnt(xprtargs,
					clnt, &attach_infos[i]);
		if (err) {
			pr_err("add xprt ippair err:%d\n", err);
			atomic_dec(&wait_queue_condition);
		}
		link_count++;
	}

	wait_event(path_attach_wait_queue,
			atomic_read(&wait_queue_condition) == 0);

	enfs_add_xprts_to_clnt(clnt, attach_infos, total_combinations);

	kfree(attach_infos);
}

void enfs_xprt_ippair_create(struct xprt_create *xprtargs,
			struct rpc_clnt *clnt,
			void *data)
{
	struct multipath_mount_options *mopt =
				(struct multipath_mount_options *)data;

	if (mopt == NULL) {
		pr_err("ip list is NULL.\n");
		return;
	}

	enfs_combine_addr(xprtargs, clnt,
				mopt->local_ip_list,
				mopt->remote_ip_list);
	enfs_lb_set_policy(clnt);
}

struct xprts_options_and_clnt {
	struct rpc_create_args *args;
	struct rpc_clnt *clnt;
	void *data;
};

static void set_clnt_enfs_flag(struct rpc_clnt *clnt)
{
	clnt->cl_enfs = true;
}

int enfs_config_xprt_create_args(struct xprt_create *xprtargs,
			struct rpc_create_args *args,
			char *servername, size_t length)
{
	int errno = 0;

	xprtargs->ident = args->protocol;
	xprtargs->net = args->net;
	xprtargs->addrlen = args->addrsize;
	xprtargs->servername = args->servername;

	if (args->flags & RPC_CLNT_CREATE_INFINITE_SLOTS)
		xprtargs->flags |= XPRT_CREATE_INFINITE_SLOTS;
	if (args->flags & RPC_CLNT_CREATE_NO_IDLE_TIMEOUT)
		xprtargs->flags |= XPRT_CREATE_NO_IDLE_TIMEOUT;

	if (xprtargs->servername == NULL) {
		errno = enfs_servername(servername, length, args);
		if (errno)
			return errno;
		xprtargs->servername = servername;
	}

	return 0;
}

int enfs_multipath_create_thread(void *data)
{
	int errno;
	char servername[48];
	struct xprts_options_and_clnt *create_args =
			(struct xprts_options_and_clnt *)data;
	struct multipath_mount_options *mount_options =
			(struct multipath_mount_options *)
			create_args->args->multipath_option;
	struct xprt_create xprtargs;

	memset(&xprtargs, 0, sizeof(struct xprt_create));

	if (mount_options == NULL) {
		enfs_log_error("enfs: local and remot addr empty\n");
		return -EINVAL;
	}

	errno = enfs_config_xprt_create_args(&xprtargs,
				create_args->args, servername,
				sizeof(servername));
	if (errno) {
		enfs_log_error("enfs: create xprt failed! errno:%d\n",
			errno);
		return errno;
	}

	//mount : localaddrs or remoteaddrs is empty
	if (mount_options->local_ip_list->count == 0) {
		errno = rpc_localaddr(create_args->clnt,
				(struct sockaddr *)
				&mount_options->local_ip_list->address[0],
				sizeof(struct sockaddr_storage));
		if (errno) {
			enfs_log_error("enfs: get clnt srcaddr errno:%d\n",
				errno);
			return errno;
		}
		mount_options->local_ip_list->count = 1;
	}

	if (mount_options->remote_ip_list->count == 0) {
		errno = rpc_peeraddr(create_args->clnt,
				(struct sockaddr *)
				&mount_options->remote_ip_list->address[0],
				sizeof(struct sockaddr_storage));
		if (errno == 0) {
			enfs_log_error("enfs: get clnt dstaddr errno:%d\n",
				errno);
			return errno;
		}
		mount_options->remote_ip_list->count = 1;
	}

	errno = enfs_proc_create_clnt(create_args->clnt);
	if (errno != 0)
		pr_err("create clnt proc failed.\n");

	set_clnt_enfs_flag(create_args->clnt);
	enfs_xprt_ippair_create(&xprtargs, create_args->clnt, mount_options);

	kfree(create_args->args);
	kfree(data);
	return 0;
}

static int set_main_xprt_ctx(struct rpc_clnt *clnt, struct rpc_xprt *xprt,
			struct sockaddr_storage *srcaddr)
{
	struct enfs_xprt_context *ctx = xprt->multipath_context;

	if (!ctx) {
		enfs_log_error("main xprt not multipath ctx.\n");
		return -1;
	}

	ctx->main = true;
	ctx->stats = rpc_alloc_iostats(clnt);
	ctx->srcaddr = *srcaddr;
	pm_set_path_state(xprt, PM_STATE_NORMAL);
	pm_ping_set_path_check_state(xprt, PM_CHECK_INIT);

	return 0;
}

static int alloc_main_xprt_multicontext(struct rpc_create_args *args,
			struct rpc_clnt *clnt)
{
	int err;
	struct sockaddr_storage srcaddr;

	// avoid main xprt multicontex local addr is empty.
	err = rpc_localaddr(clnt, (struct sockaddr *)&srcaddr, sizeof(srcaddr));
	if (err) {
		enfs_log_error("get clnt localaddr err:%d\n", err);
		return err;
	}

	err = set_main_xprt_ctx(clnt, clnt->cl_xprt, &srcaddr);
	if (err)
		enfs_log_error("alloc main xprt failed.\n");

	return err;
}

void enfs_create_multi_xprt(struct rpc_create_args *args, struct rpc_clnt *clnt)
{
	// struct task_struct *th;
	struct xprts_options_and_clnt *thargs;
	struct rpc_create_args *cargs;
	int err;

	enfs_log_info("%s %p\n", __func__, clnt);

	cargs = kmalloc(sizeof(struct rpc_create_args), GFP_KERNEL);
	if (cargs == NULL)
		return;

	*cargs = *args;

	thargs = kmalloc(sizeof(struct xprts_options_and_clnt), GFP_KERNEL);
	if (thargs == NULL) {
		kfree(cargs);
		return;
	}

	alloc_main_xprt_multicontext(args, clnt);

	thargs->args = cargs;
	thargs->clnt = clnt;
	thargs->data = args->multipath_option;

	err = enfs_multipath_create_thread(thargs);

	if (err != 0) {
		kfree(cargs);
		kfree(thargs);
	}
}

static void enfs_create_xprt_ctx(struct rpc_xprt *xprt)
{
	int err;

	err = enfs_alloc_xprt_ctx(xprt);
	if (err)
		enfs_log_error("alloc xprt failed.\n");
}

static struct rpc_multipath_ops ops = {
	.owner = THIS_MODULE,
	.create_clnt = enfs_create_multi_xprt,
	.releas_clnt = enfs_proc_delete_clnt,
	.create_xprt = enfs_create_xprt_ctx,
	.destroy_xprt = enfs_free_xprt_ctx,
	.xprt_iostat = enfs_count_iostat,
	.failover_handle = failover_handle,
	.task_need_call_start_again = failover_task_need_call_start_again,
	.adjust_task_timeout = failover_adjust_task_timeout,
	.init_task_req = failover_init_task_req,
	.prepare_transmit = failover_prepare_transmit,
};

int enfs_multipath_init(void)
{
	int err;

	enfs_log_info("multipath init.\n");

	err = pm_ping_init();
	if (err != 0) {
		enfs_log_error("pm ping init err:%d\n", err);
		return err;
	}

	err = enfs_proc_init();
	if (err != 0) {
		enfs_log_error("enfs proc init err:%d\n", err);
		pm_ping_fini();
		return err;
	}

	rpc_multipath_ops_register(&ops);

	return 0;
}

void enfs_multipath_exit(void)
{
	enfs_log_info("multipath exit.\n");
	rpc_multipath_ops_unregister(&ops);
	enfs_proc_exit();
	pm_ping_fini();
}
