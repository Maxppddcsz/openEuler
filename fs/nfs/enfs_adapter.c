// SPDX-License-Identifier: GPL-2.0
/*
 * Client-side ENFS adapter.
 *
 * Copyright (c) 2023. Huawei Technologies Co., Ltd. All rights reserved.
 */
#include <linux/types.h>
#include <linux/sunrpc/clnt.h>
#include <linux/nfs.h>
#include <linux/nfs4.h>
#include <linux/nfs3.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_fs_sb.h>
#include <linux/sunrpc/sched.h>
#include <linux/nfs_iostat.h>
#include "enfs_adapter.h"
#include "iostat.h"

struct enfs_adapter_ops __rcu *enfs_adapter;

int enfs_adapter_register(struct enfs_adapter_ops *ops)
{
	struct enfs_adapter_ops *old;

	old = cmpxchg((struct enfs_adapter_ops **)&enfs_adapter, NULL, ops);
	if (old == NULL || old == ops)
		return 0;
	pr_err("regist %s ops %p failed. old %p\n", __func__, ops, old);
	return -EPERM;
}
EXPORT_SYMBOL_GPL(enfs_adapter_register);

int enfs_adapter_unregister(struct enfs_adapter_ops *ops)
{
	struct enfs_adapter_ops *old;

	old = cmpxchg((struct enfs_adapter_ops **)&enfs_adapter, ops, NULL);
	if (old == ops || old == NULL)
		return 0;
	pr_err("unregist %s ops %p failed. old %p\n", __func__, ops, old);
	return -EPERM;
}
EXPORT_SYMBOL_GPL(enfs_adapter_unregister);

struct enfs_adapter_ops *nfs_multipath_router_get(void)
{
	struct enfs_adapter_ops *ops;

	rcu_read_lock();
	ops = rcu_dereference(enfs_adapter);
	if (ops == NULL) {
		rcu_read_unlock();
		return NULL;
	}
	if (!try_module_get(ops->owner))
		ops = NULL;
	rcu_read_unlock();
	return ops;
}

void nfs_multipath_router_put(struct enfs_adapter_ops *ops)
{
	if (ops)
		module_put(ops->owner);
}

bool is_valid_option(enum nfs_multi_path_options option)
{
	if (option < REMOTEADDR || option >= INVALID_OPTION) {
		pr_warn("%s: ENFS invalid option %d\n", __func__, option);
		return false;
	}

	return true;
}

int enfs_parse_mount_options(enum nfs_multi_path_options option, char *str,
			struct nfs_parsed_mount_data *mnt)
{
	int rc;
	struct enfs_adapter_ops *ops;

	ops = nfs_multipath_router_get();
	if ((ops == NULL) || (ops->parse_mount_options == NULL) ||
		!is_valid_option(option)) {
		nfs_multipath_router_put(ops);
		dfprintk(MOUNT,
			"NFS: parsing nfs mount option enfs not load[%s]\n"
		,  __func__);
		return -EOPNOTSUPP;
	}
    // nfs_multipath_parse_options
	dfprintk(MOUNT, "NFS:   parsing nfs mount option '%s' type: %d[%s]\n"
			, str, option, __func__);
	rc = ops->parse_mount_options(option, str, &mnt->enfs_option, mnt->net);
	nfs_multipath_router_put(ops);
	return rc;
}

void enfs_free_mount_options(struct nfs_parsed_mount_data *data)
{
	struct enfs_adapter_ops *ops;

	if (data->enfs_option == NULL)
		return;

	ops = nfs_multipath_router_get();
	if ((ops == NULL) || (ops->free_mount_options == NULL)) {
		nfs_multipath_router_put(ops);
		return;
	}
	ops->free_mount_options((void *)&data->enfs_option);
	nfs_multipath_router_put(ops);
}

int nfs_create_multi_path_client(struct nfs_client *client,
			const struct nfs_client_initdata *cl_init)
{
	int ret = 0;
	struct enfs_adapter_ops *ops;

	if (cl_init->enfs_option == NULL)
		return 0;

	ops = nfs_multipath_router_get();
	if (ops != NULL && ops->client_info_init != NULL)
		ret = ops->client_info_init(
			(void *)&client->cl_multipath_data, cl_init);
	nfs_multipath_router_put(ops);

	return ret;
}
EXPORT_SYMBOL_GPL(nfs_create_multi_path_client);

void nfs_free_multi_path_client(struct nfs_client *clp)
{
	struct enfs_adapter_ops *ops;

	if (clp->cl_multipath_data == NULL)
		return;

	ops = nfs_multipath_router_get();
	if (ops != NULL && ops->client_info_free != NULL)
		ops->client_info_free(clp->cl_multipath_data);
	nfs_multipath_router_put(ops);
}

int nfs_multipath_client_match(struct nfs_client *clp,
			const struct nfs_client_initdata *sap)
{
	bool ret = true;
	struct enfs_adapter_ops *ops;

	pr_info("%s src %p dst %p\n.", __func__,
	clp->cl_multipath_data, sap->enfs_option);

	if (clp->cl_multipath_data == NULL && sap->enfs_option == NULL)
		return true;

	if ((clp->cl_multipath_data == NULL && sap->enfs_option) ||
		(clp->cl_multipath_data && sap->enfs_option == NULL)) {
		pr_err("not match client src %p dst %p\n.",
				clp->cl_multipath_data, sap->enfs_option);
		return false;
	}

	ops = nfs_multipath_router_get();
	if (ops != NULL && ops->client_info_match != NULL)
		ret = ops->client_info_match(clp->cl_multipath_data,
					sap->enfs_option);
	nfs_multipath_router_put(ops);

	return ret;
}

int nfs4_multipath_client_match(struct nfs_client *src, struct nfs_client *dst)
{
	int ret = true;
	struct enfs_adapter_ops *ops;

	if (src->cl_multipath_data == NULL && dst->cl_multipath_data == NULL)
		return true;

	if (src->cl_multipath_data == NULL || dst->cl_multipath_data == NULL)
		return false;

	ops = nfs_multipath_router_get();
	if (ops != NULL && ops->nfs4_client_info_match != NULL)
		ret = ops->nfs4_client_info_match(src->cl_multipath_data,
					src->cl_multipath_data);
	nfs_multipath_router_put(ops);

	return ret;
}
EXPORT_SYMBOL_GPL(nfs4_multipath_client_match);

void nfs_multipath_show_client_info(struct seq_file *mount_option,
			struct nfs_server *server)
{
	struct enfs_adapter_ops *ops;

	if (mount_option == NULL || server == NULL ||
		server->client == NULL ||
		server->nfs_client->cl_multipath_data == NULL)
		return;

	ops = nfs_multipath_router_get();
	if (ops != NULL && ops->client_info_show != NULL)
		ops->client_info_show(mount_option, server);
	nfs_multipath_router_put(ops);
}

int nfs_remount_iplist(struct nfs_client *nfs_client, void *data)
{
	int ret = 0;
	struct enfs_adapter_ops *ops;
	struct nfs_parsed_mount_data *parsed_data = (struct nfs_parsed_mount_data *)data;

	if (!parsed_data->enfs_option)
		return 0;

	if (nfs_client == NULL || nfs_client->cl_rpcclient == NULL)
		return 0;

	ops = nfs_multipath_router_get();
	if (ops != NULL && ops->remount_ip_list != NULL)
		ret = ops->remount_ip_list(nfs_client, parsed_data->enfs_option);
	nfs_multipath_router_put(ops);
	return ret;
}
EXPORT_SYMBOL_GPL(nfs_remount_iplist);

/*
 * Error-check and convert a string of mount options from
 * user space into a data structure. The whole mount string
 * is processed; bad options are skipped as they are encountered.
 * If there were no errors, return 1; otherwise return zero(0)
 */
int enfs_check_mount_parse_info(char *p, int token,
		struct nfs_parsed_mount_data *mnt, const substring_t *args)
{
	char *string;
	int rc;
	string = match_strdup(args);
	if (string == NULL) {
		printk(KERN_INFO "NFS: not enough memory to parse option\n");
		return 0;
	}
	rc = enfs_parse_mount_options(get_nfs_multi_path_opt(token),
					string, mnt);

	kfree(string);
	switch (rc) {
	case  0:
		return 1;
	case -ENOMEM:
		printk(KERN_INFO "NFS: not enough memory to parse option\n");
		return 0;
	case -ENOSPC:
		printk(KERN_INFO "NFS: param is more than supported limit: %d\n", rc);
		return 0;
	case -EINVAL:
		printk(KERN_INFO "NFS: bad IP address specified: %s\n", p);
		return 0;
	case -ENOTSUPP:
		printk(KERN_INFO "NFS: bad IP address specified: %s\n", p);
		return 0;
	case -EOPNOTSUPP:
		printk(KERN_INFO "NFS: bad IP address specified: %s\n", p);
		return 0;
	}
	return 1;
}
