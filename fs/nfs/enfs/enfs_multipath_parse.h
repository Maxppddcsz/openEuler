/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Client-side ENFS adapter.
 *
 * Copyright (c) 2023. Huawei Technologies Co., Ltd. All rights reserved.
 */
#ifndef _ENFS_MULTIPATH_PARSE_H_
#define _ENFS_MULTIPATH_PARSE_H_

#include "enfs.h"

struct multipath_mount_options {
	struct nfs_ip_list  *remote_ip_list;
	struct nfs_ip_list  *local_ip_list;
	struct NFS_ROUTE_DNS_INFO_S    *pRemoteDnsInfo;
};

int nfs_multipath_parse_options(enum nfs_multi_path_options type,
			char *str, void **enfs_option, struct net *net_ns);
void nfs_multipath_free_options(void **enfs_option);

#endif
