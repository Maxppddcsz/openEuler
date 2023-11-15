// SPDX-License-Identifier: GPL-2.0
/*
 * Client-side ENFS adapter.
 *
 * Copyright (c) 2023. Huawei Technologies Co., Ltd. All rights reserved.
 */
#include <linux/types.h>
#include <linux/nfs.h>
#include <linux/nfs4.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_fs_sb.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/addr.h>
#include "enfs_multipath_client.h"
#include "enfs_multipath_parse.h"

int
nfs_multipath_client_mount_info_init(struct multipath_client_info *client_info,
	const struct nfs_client_initdata *client_init_data)
{
	struct multipath_mount_options *mount_options =
		(struct multipath_mount_options *)client_init_data->enfs_option;

	if (mount_options->local_ip_list) {
		client_info->local_ip_list =
		kzalloc(sizeof(struct nfs_ip_list), GFP_KERNEL);

		if (!client_info->local_ip_list)
			return -ENOMEM;

		memcpy(client_info->local_ip_list, mount_options->local_ip_list,
			sizeof(struct nfs_ip_list));
	}

	if (mount_options->remote_ip_list) {

		client_info->remote_ip_list =
		kzalloc(sizeof(struct nfs_ip_list), GFP_KERNEL);

		if (!client_info->remote_ip_list) {
			kfree(client_info->local_ip_list);
			client_info->local_ip_list = NULL;
			return -ENOMEM;
		}
		memcpy(client_info->remote_ip_list,
			mount_options->remote_ip_list,
			sizeof(struct nfs_ip_list));
	}

	if (mount_options->pRemoteDnsInfo) {
		client_info->pRemoteDnsInfo =
		kzalloc(sizeof(struct NFS_ROUTE_DNS_INFO_S), GFP_KERNEL);

		if (!client_info->pRemoteDnsInfo) {
			kfree(client_info->local_ip_list);
			client_info->local_ip_list = NULL;
			kfree(client_info->remote_ip_list);
			client_info->remote_ip_list = NULL;
			return -ENOMEM;
		}
		memcpy(client_info->pRemoteDnsInfo,
		mount_options->pRemoteDnsInfo,
		sizeof(struct NFS_ROUTE_DNS_INFO_S));
	}
	return 0;
}

void nfs_multipath_client_info_free_work(struct work_struct *work)
{

	struct multipath_client_info *clp_info;

	if (work == NULL)
		return;

	clp_info = container_of(work, struct multipath_client_info, work);

	if (clp_info->local_ip_list != NULL) {
		kfree(clp_info->local_ip_list);
		clp_info->local_ip_list = NULL;
	}
	if (clp_info->remote_ip_list != NULL) {
		kfree(clp_info->remote_ip_list);
		clp_info->remote_ip_list = NULL;
	}
	kfree(clp_info);
}

void nfs_multipath_client_info_free(void *data)
{
	struct multipath_client_info *clp_info =
		(struct multipath_client_info *)data;

	if (clp_info == NULL)
		return;
	pr_info("free client info %p.\n", clp_info);
	INIT_WORK(&clp_info->work, nfs_multipath_client_info_free_work);
	schedule_work(&clp_info->work);
}

int nfs_multipath_client_info_init(void **data,
			const struct nfs_client_initdata *cl_init)
{
	int rc;
	struct multipath_client_info *info;
	struct multipath_client_info **enfs_info;
    /* no multi path info, no need do multipath init */
	if (cl_init->enfs_option == NULL)
		return 0;
	enfs_info = (struct multipath_client_info **)data;
	if (enfs_info == NULL)
		return -EINVAL;

	if (*enfs_info == NULL)
		*enfs_info = kzalloc(sizeof(struct multipath_client_info),
						GFP_KERNEL);

	if (*enfs_info == NULL)
		return -ENOMEM;

	info = (struct multipath_client_info *)*enfs_info;
	pr_info("init client info %p.\n", info);
	rc = nfs_multipath_client_mount_info_init(info, cl_init);
	if (rc) {
		nfs_multipath_client_info_free((void *)info);
		return rc;
	}
	return rc;
}

bool nfs_multipath_ip_list_info_match(const struct nfs_ip_list *ip_list_src,
	const struct nfs_ip_list *ip_list_dst)
{
	int i;
	int j;
	bool is_find;
	/* if both are equal or NULL, then return true. */
	if (ip_list_src == ip_list_dst)
		return true;

	if ((ip_list_src == NULL || ip_list_dst == NULL))
		return false;

	if (ip_list_src->count != ip_list_dst->count)
		return false;

	for (i = 0; i < ip_list_src->count; i++) {
		is_find = false;
		for (j = 0; j < ip_list_src->count; j++) {
			if (rpc_cmp_addr_port(
				(const struct sockaddr *)
				&ip_list_src->address[i],
				(const struct sockaddr *)
				&ip_list_dst->address[j])
				) {
				is_find = true;
				break;
			}
		}
		if (is_find == false)
			return false;
	}
	return true;
}

int
nfs_multipath_dns_list_info_match(
			const struct NFS_ROUTE_DNS_INFO_S *pRemoteDnsInfoSrc,
			const struct NFS_ROUTE_DNS_INFO_S *pRemoteDnsInfoDst)
{
	int i;

	/* if both are equal or NULL, then return true. */
	if (pRemoteDnsInfoSrc == pRemoteDnsInfoDst)
		return true;

	if ((pRemoteDnsInfoSrc == NULL || pRemoteDnsInfoDst == NULL))
		return false;

	if (pRemoteDnsInfoSrc->dnsNameCount != pRemoteDnsInfoDst->dnsNameCount)
		return false;

	for (i = 0; i < pRemoteDnsInfoSrc->dnsNameCount; i++) {
		if (!strcmp(pRemoteDnsInfoSrc->routeRemoteDnsList[i].dnsname,
		pRemoteDnsInfoDst->routeRemoteDnsList[i].dnsname))
			return false;
	}
	return true;
}

int nfs_multipath_client_info_match(void *src, void *dst)
{
	int ret = true;

	struct multipath_client_info *src_info;
	struct multipath_mount_options *dst_info;

	src_info = (struct multipath_client_info *)src;
	dst_info = (struct multipath_mount_options *)dst;
	pr_info("try match client .\n");
	ret = nfs_multipath_ip_list_info_match(src_info->local_ip_list,
				dst_info->local_ip_list);
	if (ret == false) {
		pr_err("local_ip not match.\n");
		return ret;
	}

	ret = nfs_multipath_ip_list_info_match(src_info->remote_ip_list,
			dst_info->remote_ip_list);
	if (ret == false) {
		pr_err("remote_ip not match.\n");
		return ret;
	}

	ret = nfs_multipath_dns_list_info_match(src_info->pRemoteDnsInfo,
			dst_info->pRemoteDnsInfo);
	if (ret == false) {
		pr_err("dns not match.\n");
		return ret;
	}
	pr_info("try match client ret %d.\n", ret);
	return ret;
}

void nfs_multipath_print_ip_info(struct seq_file *mount_option,
			struct nfs_ip_list *ip_list,
			const char *type)
{
	char buf[IP_ADDRESS_LEN_MAX + 1];
	int len = 0;
	int i = 0;

	seq_printf(mount_option, ",%s=", type);
	for (i = 0; i < ip_list->count; i++) {
		len = rpc_ntop((struct sockaddr *)&ip_list->address[i],
				buf, IP_ADDRESS_LEN_MAX);
		if (len > 0 && len < IP_ADDRESS_LEN_MAX)
			buf[len] = '\0';

		if (i == 0)
			seq_printf(mount_option, "%s", buf);
		else
			seq_printf(mount_option, "~%s", buf);
		dfprintk(MOUNT,
			"NFS:   show nfs mount option type:%s %s [%s]\n",
			type, buf, __func__);
	}
}

void nfs_multipath_print_dns_info(struct seq_file *mount_option,
			struct NFS_ROUTE_DNS_INFO_S *pRemoteDnsInfo,
			const char *type)
{
	int i = 0;

	seq_printf(mount_option, ",%s=", type);
	for (i = 0; i < pRemoteDnsInfo->dnsNameCount; i++) {
		if (i == 0)
			seq_printf(mount_option,
			"[%s", pRemoteDnsInfo->routeRemoteDnsList[i].dnsname);
		else if (i == pRemoteDnsInfo->dnsNameCount - 1)
			seq_printf(mount_option, ",%s]",
			pRemoteDnsInfo->routeRemoteDnsList[i].dnsname);
		else
			seq_printf(mount_option,
			",%s", pRemoteDnsInfo->routeRemoteDnsList[i].dnsname);
	}
}


static void multipath_print_sockaddr(struct seq_file *seq,
			struct sockaddr *addr)
{
	switch (addr->sa_family) {
	case AF_INET: {
		struct sockaddr_in *sin = (struct sockaddr_in *)addr;

		seq_printf(seq, "%pI4", &sin->sin_addr);
		return;
	}
	case AF_INET6: {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;

		seq_printf(seq, "%pI6", &sin6->sin6_addr);
		return;
	}
	default:
		break;
	}
	pr_err("unsupport family:%d\n", addr->sa_family);
}

static void multipath_print_enfs_info(struct seq_file *seq,
			struct nfs_server *server)
{
	struct sockaddr_storage peeraddr;
	struct rpc_clnt *next = server->client;

	rpc_peeraddr(server->client,
			(struct sockaddr *)&peeraddr, sizeof(peeraddr));
	seq_puts(seq, ",enfs_info=");
	multipath_print_sockaddr(seq, (struct sockaddr *)&peeraddr);

	while (next->cl_parent) {
		if (next == next->cl_parent)
			break;
		next = next->cl_parent;
	}
	seq_printf(seq, "_%u", next->cl_clid);
}

void nfs_multipath_client_info_show(struct seq_file *mount_option, void *data)
{
	struct nfs_server *server = data;
	struct multipath_client_info *client_info =
			server->nfs_client->cl_multipath_data;

	dfprintk(MOUNT, "NFS:   show nfs mount option[%s]\n", __func__);
	if ((client_info->remote_ip_list) &&
		(client_info->remote_ip_list->count > 0))
		nfs_multipath_print_ip_info(mount_option,
			client_info->remote_ip_list,
			"remoteaddrs");

	if ((client_info->local_ip_list) &&
		(client_info->local_ip_list->count > 0))
		nfs_multipath_print_ip_info(mount_option,
			client_info->local_ip_list,
			"localaddrs");

	if ((client_info->pRemoteDnsInfo) &&
		(client_info->pRemoteDnsInfo->dnsNameCount > 0))
		nfs_multipath_print_dns_info(mount_option,
			client_info->pRemoteDnsInfo,
			"remotednsname");

	multipath_print_enfs_info(mount_option, server);
}
