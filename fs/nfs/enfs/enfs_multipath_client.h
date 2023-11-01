/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Client-side ENFS adapter.
 *
 * Copyright (c) 2023. Huawei Technologies Co., Ltd. All rights reserved.
 */
#ifndef _ENFS_MULTIPATH_CLIENT_H_
#define _ENFS_MULTIPATH_CLIENT_H_

#include "enfs.h"

struct multipath_client_info {
	struct work_struct   work;
	struct nfs_ip_list  *remote_ip_list;
	struct nfs_ip_list  *local_ip_list;
	struct NFS_ROUTE_DNS_INFO_S    *pRemoteDnsInfo;
	s64 client_id;
};

int nfs_multipath_client_info_init(void **data,
			const struct nfs_client_initdata *cl_init);
void nfs_multipath_client_info_free(void *data);
int nfs_multipath_client_info_match(void *src, void *dst);
void nfs_multipath_client_info_show(struct seq_file *mount_option, void *data);

#endif
