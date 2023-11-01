/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Client-side ENFS adapt header.
 *
 *  Copyright (c) 2023. Huawei Technologies Co., Ltd. All rights reserved.
 */
#ifndef _NFS_MULTIPATH_H_
#define _NFS_MULTIPATH_H_

#include <linux/parser.h>
#include "internal.h"

#if IS_ENABLED(CONFIG_ENFS)
enum nfs_multi_path_options {
	REMOTEADDR,
	LOCALADDR,
	REMOTEDNSNAME,
	REMOUNTREMOTEADDR,
	REMOUNTLOCALADDR,
	INVALID_OPTION
};


struct enfs_adapter_ops {
	const char *name;
	struct module *owner;
	int (*parse_mount_options)(enum nfs_multi_path_options option,
		char *str, void **enfs_option, struct net *net_ns);

	void (*free_mount_options)(void **data);

	int (*client_info_init)(void **data,
				const struct nfs_client_initdata *cl_init);
	void (*client_info_free)(void *data);
	int (*client_info_match)(void *src, void *dst);
	int (*nfs4_client_info_match)(void *src, void *dst);
	void (*client_info_show)(struct seq_file *mount_option, void *data);
	int (*remount_ip_list)(struct nfs_client *nfs_client,
						void *enfs_option);
};

int enfs_parse_mount_options(enum nfs_multi_path_options option, char *str,
			struct nfs_parsed_mount_data *mnt);
void enfs_free_mount_options(struct nfs_parsed_mount_data *data);
int nfs_create_multi_path_client(struct nfs_client *client,
			const struct nfs_client_initdata *cl_init);
void nfs_free_multi_path_client(struct nfs_client *clp);
int nfs_multipath_client_match(struct nfs_client *clp,
			const struct nfs_client_initdata *sap);
int nfs4_multipath_client_match(struct nfs_client *src, struct nfs_client *dst);
void nfs_multipath_show_client_info(struct seq_file *mount_option,
			struct nfs_server *server);
int enfs_adapter_register(struct enfs_adapter_ops *ops);
int enfs_adapter_unregister(struct enfs_adapter_ops *ops);
int nfs_remount_iplist(struct nfs_client *nfs_client, void *data);
int nfs4_create_multi_path(struct nfs_server *server,
	struct nfs_parsed_mount_data *data,
	const struct rpc_timeout *timeparms);
int enfs_check_mount_parse_info(char *p, int token,
		struct nfs_parsed_mount_data *mnt, const substring_t *args);

#else
static inline
void nfs_free_multi_path_client(struct nfs_client *clp)
{

}

static inline
int nfs_multipath_client_match(struct nfs_client *clp,
			const struct nfs_client_initdata *sap)
{
	return 1;
}

static inline
int nfs_create_multi_path_client(struct nfs_client *client,
			const struct nfs_client_initdata *cl_init)
{
	return 0;
}

static inline
void nfs_multipath_show_client_info(struct seq_file *mount_option,
			struct nfs_server *server)
{

}

static inline
int nfs4_multipath_client_match(struct nfs_client *src,
			struct nfs_client *dst)
{
	return 1;
}

static inline
void enfs_free_mount_options(struct nfs_parsed_mount_data *data)
{

}

static inline
int enfs_check_mount_parse_info(char *p, int token,
		struct nfs_parsed_mount_data *mnt, const substring_t *args)
{
	return 1;
}

static inline
int nfs_remount_iplist(struct nfs_client *nfs_client, void *data)
{
	return 0;
}
#endif // CONFIG_ENFS
#endif // _NFS_MULTIPATH_H_
