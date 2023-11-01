// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/metrics.h>
#include <linux/sunrpc/xprtsock.h>
#include <net/netns/generic.h>

#include "../../../net/sunrpc/netns.h"

#include "enfs.h"
#include "enfs_log.h"
#include "enfs_proc.h"
#include "enfs_multipath.h"
#include "pm_state.h"

#define ENFS_PROC_DIR "enfs"
#define ENFS_PROC_PATH_STATUS_LEN 256

static struct proc_dir_entry *enfs_proc_parent;

void
enfs_iterate_each_rpc_clnt(int (*fn)(struct rpc_clnt *clnt, void *data),
			void *data)
{
	struct net *net;
	struct sunrpc_net *sn;
	struct rpc_clnt *clnt;

	rcu_read_lock();
	for_each_net_rcu(net) {
		sn = net_generic(net, sunrpc_net_id);
		if (sn == NULL)
			continue;
		spin_lock(&sn->rpc_client_lock);
		list_for_each_entry(clnt, &sn->all_clients, cl_clients) {
			fn(clnt, data);
		}
		spin_unlock(&sn->rpc_client_lock);
	}
	rcu_read_unlock();
}

struct proc_dir_entry *enfs_get_proc_parent(void)
{
	return enfs_proc_parent;
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

static bool should_print(const char *name)
{
	int i;
	static const char * const proc_names[] = {
	    "READ",
	    "WRITE",
	};

	if (name == NULL)
		return false;

	for (i = 0; i < ARRAY_SIZE(proc_names); i++) {
		if (strcmp(name, proc_names[i]) == 0)
			return true;
	}
	return false;
}

struct enfs_xprt_iter {
	unsigned int id;
	struct seq_file *seq;
	unsigned int max_addrs_length;
};

static int debug_show_xprt(struct rpc_clnt *clnt,
			struct rpc_xprt *xprt,
			void *data)
{
	struct enfs_xprt_context *ctx = NULL;

	if (xprt->multipath_context)
		ctx = xprt->multipath_context;

	pr_info("	xprt:%p ctx:%p main:%d queue_len:%lu.\n", xprt,
			xprt->multipath_context,
			ctx ? ctx->main : false,
			atomic_long_read(&xprt->queuelen));
	return 0;
}

static int debug_show_clnt(struct rpc_clnt *clnt, void *data)
{
	pr_info("    clnt %d addr:%p enfs:%d\n",
			clnt->cl_clid, clnt,
			clnt->cl_enfs);
	rpc_clnt_iterate_for_each_xprt(clnt, debug_show_xprt, NULL);
	return 0;
}

static void debug_print_all_xprt(void)
{
	enfs_iterate_each_rpc_clnt(debug_show_clnt, NULL);
}

static
void enfs_proc_format_xprt_addr_display(struct rpc_clnt *clnt,
			struct rpc_xprt *xprt,
			char *local_name_buf,
			int local_name_buf_len,
			char *remote_name_buf,
			int remote_name_buf_len)
{
	int err;
	struct sockaddr_storage srcaddr;
	struct enfs_xprt_context *ctx;

	ctx = (struct enfs_xprt_context *)xprt->multipath_context;

	sockaddr_ip_to_str((struct sockaddr *)&xprt->addr,
				remote_name_buf, remote_name_buf_len);

	// get local address depend one main or not
	if (enfs_is_main_xprt(xprt)) {
		err = rpc_localaddr(clnt, (struct sockaddr *)&srcaddr,
					sizeof(srcaddr));
		if (err != 0)
			(void)snprintf(local_name_buf,
						local_name_buf_len, "Unknown");
		else
			sockaddr_ip_to_str((struct sockaddr *)&srcaddr,
						local_name_buf,
						local_name_buf_len);
	} else {
		sockaddr_ip_to_str((struct sockaddr *)&ctx->srcaddr,
					local_name_buf,
					local_name_buf_len);
	}
}

static int enfs_show_xprt_stats(struct rpc_clnt *clnt,
			struct rpc_xprt *xprt,
			void *data)
{
	unsigned int op;
	unsigned int maxproc = clnt->cl_maxproc;
	struct enfs_xprt_iter *iter = (struct enfs_xprt_iter *)data;
	struct enfs_xprt_context *ctx;
	char local_name[INET6_ADDRSTRLEN];
	char remote_name[INET6_ADDRSTRLEN];

	if (!xprt->multipath_context)
		return 0;

	ctx = (struct enfs_xprt_context *)xprt->multipath_context;

	enfs_proc_format_xprt_addr_display(clnt, xprt, local_name,
					sizeof(local_name),
					remote_name, sizeof(remote_name));

	seq_printf(iter->seq, "%-6u%-*s%-*s", iter->id,
			iter->max_addrs_length + 4,
			local_name,
			iter->max_addrs_length + 4,
			remote_name);

	iter->id++;

	for (op = 0; op < maxproc; op++) {
		if (!should_print(clnt->cl_procinfo[op].p_name))
			continue;

		seq_printf(iter->seq, "%-22lu%-22Lu%-22Lu",
			ctx->stats[op].om_ops,
			ctx->stats[op].om_ops == 0 ? 0 :
			ktime_to_ms(ctx->stats[op].om_rtt) /
			ctx->stats[op].om_ops,
			ctx->stats[op].om_ops == 0 ? 0 :
			ktime_to_ms(ctx->stats[op].om_execute) /
			ctx->stats[op].om_ops);
	}
	seq_puts(iter->seq, "\n");
	return 0;
}

static int rpc_proc_show_path_status(struct rpc_clnt *clnt,
			struct rpc_xprt *xprt,
			void *data)
{
	struct enfs_xprt_iter *iter = (struct enfs_xprt_iter *)data;
	struct enfs_xprt_context *ctx = NULL;
	char local_name[INET6_ADDRSTRLEN] = {0};
	char remote_name[INET6_ADDRSTRLEN] = {0};
	char multiapth_status[ENFS_PROC_PATH_STATUS_LEN] = {0};
	char xprt_status[ENFS_PROC_PATH_STATUS_LEN] = {0};

	if (!xprt->multipath_context) {
		enfs_log_debug("multipath_context is null.\n");
		return 0;
	}
	ctx = (struct enfs_xprt_context *)xprt->multipath_context;

	enfs_proc_format_xprt_addr_display(clnt, xprt,
				local_name,
				sizeof(local_name),
				remote_name, sizeof(remote_name));

	pm_get_path_state_desc(xprt,
				multiapth_status,
				ENFS_PROC_PATH_STATUS_LEN);

	pm_get_xprt_state_desc(xprt,
				xprt_status,
				ENFS_PROC_PATH_STATUS_LEN);

	seq_printf(iter->seq, "%-6u%-*s%-*s%-12s%-12s\n",
			iter->id, iter->max_addrs_length + 4,
			local_name, iter->max_addrs_length + 4,
			remote_name, multiapth_status,
			xprt_status);
	iter->id++;
	return 0;
}

static int enfs_get_max_addrs_length(struct rpc_clnt *clnt,
			struct rpc_xprt *xprt,
			void *data)
{
	struct enfs_xprt_iter *iter = (struct enfs_xprt_iter *)data;
	char local_name[INET6_ADDRSTRLEN];
	char remote_name[INET6_ADDRSTRLEN];

	enfs_proc_format_xprt_addr_display(clnt, xprt,
				local_name, sizeof(local_name),
				remote_name, sizeof(remote_name));

	if (iter->max_addrs_length < strlen(local_name))
		iter->max_addrs_length = strlen(local_name);

	if (iter->max_addrs_length < strlen(remote_name))
		iter->max_addrs_length = strlen(remote_name);

	return 0;
}

static int rpc_proc_clnt_showpath(struct seq_file *seq, void *v)
{
	struct rpc_clnt *clnt = seq->private;
	struct enfs_xprt_iter iter;

	iter.seq = seq;
	iter.id = 0;
	iter.max_addrs_length = 0;

	rpc_clnt_iterate_for_each_xprt(clnt,
				enfs_get_max_addrs_length,
				(void *)&iter);

	seq_printf(seq, "%-6s%-*s%-*s%-12s%-12s\n", "id",
			iter.max_addrs_length + 4,
			"local_addr",
			iter.max_addrs_length + 4,
			"remote_addr",
			"path_state",
			"xprt_state");

	rpc_clnt_iterate_for_each_xprt(clnt,
				rpc_proc_show_path_status,
				(void *)&iter);
	return 0;
}

static int enfs_rpc_proc_show(struct seq_file *seq, void *v)
{
	struct rpc_clnt *clnt = seq->private;
	struct enfs_xprt_iter iter;

	iter.seq = seq;
	iter.id = 0;
	iter.max_addrs_length = 0;

	debug_print_all_xprt();
	pr_info("enfs proc clnt:%p\n", clnt);

	rpc_clnt_iterate_for_each_xprt(clnt,
				enfs_get_max_addrs_length,
				(void *)&iter);

	seq_printf(seq, "%-6s%-*s%-*s%-22s%-22s%-22s%-22s%-22s%-22s\n", "id",
			iter.max_addrs_length + 4, "local_addr",
			iter.max_addrs_length + 4,
			"remote_addr", "r_count",
			"r_rtt", "r_exec", "w_count", "w_rtt", "w_exec");

	// rpc_clnt_show_stats(seq, clnt);
	rpc_clnt_iterate_for_each_xprt(clnt,
				enfs_show_xprt_stats,
				(void *)&iter);
	return 0;
}

static int rpc_proc_open(struct inode *inode, struct file *file)
{
	struct rpc_clnt *clnt = PDE_DATA(inode);

	pr_info("%s %p\n", __func__, clnt);
	return single_open(file, enfs_rpc_proc_show, clnt);
}

static int enfs_reset_xprt_stats(struct rpc_clnt *clnt,
			struct rpc_xprt *xprt,
			void *data)
{
	unsigned int op;
	struct enfs_xprt_context *ctx;
	unsigned int maxproc = clnt->cl_maxproc;
	struct rpc_iostats stats = {0};

	if (!xprt->multipath_context)
		return 0;
	ctx = (struct enfs_xprt_context *)xprt->multipath_context;

	for (op = 0; op < maxproc; op++) {
		spin_lock(&ctx->stats[op].om_lock);
		ctx->stats[op] = stats;
		spin_unlock(&ctx->stats[op].om_lock);
	}
	return 0;
}

static void trim_newline_ch(char *str, int len)
{
	int i;

	for (i = 0; str[i] != '\0' && i < len; i++) {
		if (str[i] == '\n')
			str[i] = '\0';
	}
}

static ssize_t enfs_proc_write(struct file *file,
			const char __user *user_buf,
			size_t len,
			loff_t *offset)
{
	char buffer[128];
	struct rpc_clnt *clnt =
	((struct seq_file *)file->private_data)->private;

	if (len >= sizeof(buffer))
		return -E2BIG;

	if (copy_from_user(buffer, user_buf, len) != 0)
		return -EFAULT;

	buffer[len] = '\0';
	trim_newline_ch(buffer, len);
	if (strcmp(buffer, "reset") != 0)
		return -EINVAL;

	rpc_clnt_iterate_for_each_xprt(clnt, enfs_reset_xprt_stats, NULL);
	return len;
}

static int rpc_proc_show_path(struct inode *inode, struct file *file)
{
	struct rpc_clnt *clnt = PDE_DATA(inode);

	return single_open(file, rpc_proc_clnt_showpath, clnt);
}

static const struct file_operations rpc_proc_fops = {
	.owner = THIS_MODULE,
	.open = rpc_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = enfs_proc_write,
};

static const struct file_operations rpc_show_path_fops = {
	.owner = THIS_MODULE,
	.open = rpc_proc_show_path,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int clnt_proc_name(struct rpc_clnt *clnt, char *buf, int len)
{
	int ret;

	ret = snprintf(buf, len, "%s_%u",
				rpc_peeraddr2str(clnt, RPC_DISPLAY_ADDR),
				clnt->cl_clid);
	if (ret > len)
		return -E2BIG;
	return 0;
}

static int enfs_proc_create_file(struct rpc_clnt *clnt)
{
	int err;
	char buf[128];

	struct proc_dir_entry *clnt_entry;
	struct proc_dir_entry *stat_entry;

	err = clnt_proc_name(clnt, buf, sizeof(buf));
	if (err)
		return err;

	clnt_entry = proc_mkdir(buf, enfs_proc_parent);
	if (clnt_entry == NULL)
		return -EINVAL;

	stat_entry = proc_create_data("stat",
				0, clnt_entry,
				&rpc_proc_fops, clnt);

	if (stat_entry == NULL)
		return -EINVAL;

	stat_entry = proc_create_data("path",
				0, clnt_entry,
				&rpc_show_path_fops, clnt);

	if (stat_entry == NULL)
		return -EINVAL;

	return 0;
}

void enfs_count_iostat(struct rpc_task *task)
{
	struct enfs_xprt_context *ctx = task->tk_xprt->multipath_context;

	if (!ctx || !ctx->stats)
		return;
	rpc_count_iostats(task, ctx->stats);
}

static void enfs_proc_delete_file(struct rpc_clnt *clnt)
{
	int err;
	char buf[128];

	err = clnt_proc_name(clnt, buf, sizeof(buf));
	if (err) {
		pr_err("gen clnt name failed.\n");
		return;
	}
	remove_proc_subtree(buf, enfs_proc_parent);
}

// create proc file "/porc/enfs/[mount_ip]_[id]/stat"
int enfs_proc_create_clnt(struct rpc_clnt *clnt)
{
	int err;

	err = enfs_proc_create_file(clnt);
	if (err) {
		pr_err("create client %d\n", err);
		return err;
	}

	return 0;
}

void enfs_proc_delete_clnt(struct rpc_clnt *clnt)
{
	if (clnt->cl_enfs)
		enfs_proc_delete_file(clnt);
}

static int enfs_proc_create_parent(void)
{
	enfs_proc_parent = proc_mkdir(ENFS_PROC_DIR, NULL);

	if (enfs_proc_parent == NULL) {
		pr_err("Enfs create proc dir err\n");
		return -ENOMEM;
	}
	return 0;
}

static void enfs_proc_delete_parent(void)
{
	remove_proc_entry(ENFS_PROC_DIR, NULL);
}

static int enfs_proc_init_create_clnt(struct rpc_clnt *clnt, void *data)
{
	if (clnt->cl_enfs)
		enfs_proc_create_file(clnt);
	return 0;
}

static int enfs_proc_destroy_clnt(struct rpc_clnt *clnt, void *data)
{
	if (clnt->cl_enfs)
		enfs_proc_delete_file(clnt);
	return 0;
}

int enfs_proc_init(void)
{
	int err;

	err = enfs_proc_create_parent();
	if (err)
		return err;

	enfs_iterate_each_rpc_clnt(enfs_proc_init_create_clnt, NULL);
	return 0;
}

void enfs_proc_exit(void)
{
	enfs_iterate_each_rpc_clnt(enfs_proc_destroy_clnt, NULL);
	enfs_proc_delete_parent();
}
