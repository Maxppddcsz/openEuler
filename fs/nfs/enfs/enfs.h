/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Client-side ENFS multipath adapt header.
 *
 *  Copyright (c) 2023. Huawei Technologies Co., Ltd. All rights reserved.
 */

#ifndef _ENFS_H_
#define _ENFS_H_
#include <linux/atomic.h>
#include <linux/nfs.h>
#include <linux/nfs4.h>
#include <linux/nfs3.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_fs_sb.h>
#include "../enfs_adapter.h"

#define IP_ADDRESS_LEN_MAX 64
#define MAX_IP_PAIR_PER_MOUNT 8
#define MAX_IP_INDEX (MAX_IP_PAIR_PER_MOUNT)
#define MAX_SUPPORTED_LOCAL_IP_COUNT 8
#define MAX_SUPPORTED_REMOTE_IP_COUNT 32
#define MAX_DNS_NAME_LEN 512
#define MAX_DNS_SUPPORTED 2
#define EXTEND_CMD_MAX_BUF_LEN 65356


struct nfs_ip_list {
	int count;
	struct sockaddr_storage address[MAX_SUPPORTED_REMOTE_IP_COUNT];
	size_t addrlen[MAX_SUPPORTED_REMOTE_IP_COUNT];
};

struct NFS_ROUTE_DNS_S {
	char dnsname[MAX_DNS_NAME_LEN];  // valid only if dnsExist is true
};

struct NFS_ROUTE_DNS_INFO_S {
	int dnsNameCount; // Count of DNS name in the list
	// valid only if dnsExist is true
	struct NFS_ROUTE_DNS_S routeRemoteDnsList[MAX_DNS_SUPPORTED];
};

struct rpc_iostats;
struct enfs_xprt_context {
	struct sockaddr_storage	srcaddr;
	struct rpc_iostats *stats;
	bool main;
	atomic_t path_state;
	atomic_t path_check_state;
};

static inline bool enfs_is_main_xprt(struct rpc_xprt *xprt)
{
	struct enfs_xprt_context *ctx = xprt->multipath_context;

	if (!ctx)
		return false;
	return ctx->main;
}

#endif
