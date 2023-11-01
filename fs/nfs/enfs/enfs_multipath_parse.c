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
#include <linux/kern_levels.h>
#include <linux/sunrpc/addr.h>
#include "enfs_multipath_parse.h"
#include "enfs_log.h"

#define NFSDBG_FACILITY NFSDBG_CLIENT

void nfs_multipath_parse_ip_ipv6_add(struct sockaddr_in6 *sin6, int add_num)
{
	int i;

	pr_info("NFS:  before %08x%08x%08x%08x  add_num: %d[%s]\n",
		ntohl(sin6->sin6_addr.in6_u.u6_addr32[0]),
		ntohl(sin6->sin6_addr.in6_u.u6_addr32[1]),
		ntohl(sin6->sin6_addr.in6_u.u6_addr32[2]),
		ntohl(sin6->sin6_addr.in6_u.u6_addr32[3]),
		add_num, __func__);
	for (i = 0; i < add_num; i++) {
		sin6->sin6_addr.in6_u.u6_addr32[3] =
			htonl(ntohl(sin6->sin6_addr.in6_u.u6_addr32[3]) + 1);

		if (sin6->sin6_addr.in6_u.u6_addr32[3] != 0)
			continue;

		sin6->sin6_addr.in6_u.u6_addr32[2] =
			htonl(ntohl(sin6->sin6_addr.in6_u.u6_addr32[2]) + 1);

		if (sin6->sin6_addr.in6_u.u6_addr32[2] != 0)
			continue;

		sin6->sin6_addr.in6_u.u6_addr32[1] =
			htonl(ntohl(sin6->sin6_addr.in6_u.u6_addr32[1]) + 1);

		if (sin6->sin6_addr.in6_u.u6_addr32[1] != 0)
			continue;

		sin6->sin6_addr.in6_u.u6_addr32[0] =
			htonl(ntohl(sin6->sin6_addr.in6_u.u6_addr32[0]) + 1);

		if (sin6->sin6_addr.in6_u.u6_addr32[0] != 0)
			continue;
	}

	return;

}

static int nfs_multipath_parse_ip_range(struct net *net_ns, const char *cursor,
	struct nfs_ip_list *ip_list, enum nfs_multi_path_options type)
{
	struct sockaddr_storage addr;
	struct sockaddr_storage tmp_addr;
	int i;
	size_t len;
	int add_num = 1;
	bool duplicate_flag = false;
	bool is_complete = false;
	struct sockaddr_in *sin4;
	struct sockaddr_in6 *sin6;

	pr_info("NFS:   parsing nfs mount option '%s' type: %d[%s]\n",
			cursor, type, __func__);
	len = rpc_pton(net_ns, cursor, strlen(cursor),
				(struct sockaddr *)&addr, sizeof(addr));
	if (!len)
		return -EINVAL;

	if (addr.ss_family != ip_list->address[ip_list->count - 1].ss_family) {
		pr_info("NFS: %s  parsing nfs mount option type: %d fail.\n",
				__func__, type);
		return -EINVAL;
	}

	if (rpc_cmp_addr((const struct sockaddr *)
		&ip_list->address[ip_list->count - 1],
		(const struct sockaddr *)&addr)) {

		pr_info("range ip is same ip.\n");
		return 0;

	}

	while (true) {

		tmp_addr = ip_list->address[ip_list->count - 1];

		switch (addr.ss_family) {
		case AF_INET:
			sin4 = (struct sockaddr_in *)&tmp_addr;

			sin4->sin_addr.s_addr =
			htonl(ntohl(sin4->sin_addr.s_addr) + add_num);

			pr_info("NFS: mount option ip%08x type: %d ipcont %d [%s]\n",
					ntohl(sin4->sin_addr.s_addr),
					type, ip_list->count, __func__);
			break;
		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)&tmp_addr;
			nfs_multipath_parse_ip_ipv6_add(sin6, add_num);
			pr_info("NFS: mount option ip %08x%08x%08x%08x type: %d ipcont %d [%s]\n",
			ntohl(sin6->sin6_addr.in6_u.u6_addr32[0]),
			ntohl(sin6->sin6_addr.in6_u.u6_addr32[1]),
			ntohl(sin6->sin6_addr.in6_u.u6_addr32[2]),
			ntohl(sin6->sin6_addr.in6_u.u6_addr32[3]),
			type, ip_list->count, __func__);
			break;
			// return -EOPNOTSUPP;
		default:
			return -EOPNOTSUPP;
			}

		if (rpc_cmp_addr((const struct sockaddr *)&tmp_addr,
			(const struct sockaddr *)&addr)) {
			is_complete = true;
		}
		// delete duplicate ip, continuosly repeat, skip it
		for (i = 0; i < ip_list->count; i++) {
			duplicate_flag = false;
			if (rpc_cmp_addr((const struct sockaddr *)
				&ip_list->address[i],
				(const struct sockaddr *)&tmp_addr)) {
				add_num++;
				duplicate_flag = true;
				break;
			}
		}

		if (duplicate_flag == false) {
			pr_info("this ip not duplicate;");
			add_num = 1;
			// if not repeat but omit limit return false
		if ((type == LOCALADDR &&
			ip_list->count >= MAX_SUPPORTED_LOCAL_IP_COUNT) ||
			(type == REMOTEADDR &&
			ip_list->count >= MAX_SUPPORTED_REMOTE_IP_COUNT)) {

			pr_info("[MULTIPATH:%s] iplist for type %d reached %d, more than supported limit %d\n",
			__func__, type, ip_list->count,
			type == LOCALADDR ?
			MAX_SUPPORTED_LOCAL_IP_COUNT :
			MAX_SUPPORTED_REMOTE_IP_COUNT);
			ip_list->count = 0;
			return -ENOSPC;
		}
			ip_list->address[ip_list->count] = tmp_addr;

			ip_list->addrlen[ip_list->count] =
			ip_list->addrlen[ip_list->count - 1];

			ip_list->count += 1;
		}
		if (is_complete == true)
			break;
	}
	return 0;
}

int nfs_multipath_parse_ip_list_inter(struct nfs_ip_list *ip_list,
			struct net *net_ns,
			char *cursor, enum nfs_multi_path_options type)
{
	int i = 0;
	struct sockaddr_storage addr;
	struct sockaddr_storage swap;
	int len;

	pr_info("NFS:   parsing nfs mount option '%s' type: %d[%s]\n",
			cursor, type, __func__);

	len = rpc_pton(net_ns, cursor,
				strlen(cursor),
				(struct sockaddr *)&addr, sizeof(addr));
	if (!len)
		return -EINVAL;

	// check repeated ip
	for (i = 0; i < ip_list->count; i++) {
		if (rpc_cmp_addr((const struct sockaddr *)
			&ip_list->address[i],
			(const struct sockaddr *)&addr)) {

			pr_info("NFS: mount option '%s' type:%d index %d same as before index %d [%s]\n",
			cursor, type, ip_list->count, i, __func__);
			// prevent this ip is beginning
			// if repeated take it to the end of list
			swap = ip_list->address[i];

			ip_list->address[i] =
				ip_list->address[ip_list->count-1];

			ip_list->address[ip_list->count-1] = swap;
			return 0;
		}
	}
	// if not repeated, check exceed limit
		if ((type == LOCALADDR &&
			ip_list->count >= MAX_SUPPORTED_LOCAL_IP_COUNT) ||
			(type == REMOTEADDR &&
			ip_list->count >= MAX_SUPPORTED_REMOTE_IP_COUNT)) {

			pr_info("[MULTIPATH:%s] iplist for type %d reached %d, more than supported limit %d\n",
			__func__, type, ip_list->count,
			type == LOCALADDR ?
			MAX_SUPPORTED_LOCAL_IP_COUNT :
			MAX_SUPPORTED_REMOTE_IP_COUNT);

			ip_list->count = 0;
			return -ENOSPC;
		}
	ip_list->address[ip_list->count] = addr;
	ip_list->addrlen[ip_list->count] = len;
	ip_list->count++;

	return 0;
}

char *nfs_multipath_parse_ip_list_get_cursor(char **buf_to_parse, bool *single)
{
	char *cursor = NULL;
	const char *single_sep = strchr(*buf_to_parse, '~');
	const char *range_sep = strchr(*buf_to_parse, '-');

	*single = true;
	if (range_sep) {
		if (range_sep > single_sep) { // A-B or A~B-C
			if (single_sep == NULL) {  // A-B
				cursor = strsep(buf_to_parse, "-");
				if (cursor)
					*single = false;
			} else// A~B-C
				cursor = strsep(buf_to_parse, "~");
		} else { // A-B~C
			cursor = strsep(buf_to_parse, "-");
			if (cursor)
				*single = false;
		}
	} else { // A~B~C
		cursor = strsep(buf_to_parse, "~");
	}
	return cursor;
}

bool nfs_multipath_parse_param_check(enum nfs_multi_path_options type,
			struct multipath_mount_options *options)
{
	if (type == REMOUNTREMOTEADDR && options->remote_ip_list->count != 0) {
		memset(options->remote_ip_list, 0, sizeof(struct nfs_ip_list));
		return true;
	}
	if (type == REMOUNTLOCALADDR && options->local_ip_list->count != 0) {
		memset(options->local_ip_list, 0, sizeof(struct nfs_ip_list));
		return true;
	}
	if ((type == REMOTEADDR || type == REMOTEDNSNAME) &&
		options->pRemoteDnsInfo->dnsNameCount != 0) {

		pr_info("[MULTIPATH:%s] parse for %d ,already have dns\n",
		__func__, type);
		return false;
	} else if ((type == REMOTEADDR || type == REMOTEDNSNAME) &&
		options->remote_ip_list->count != 0) {

		pr_info("[MULTIPATH:%s] parse for %d ,already have iplist\n",
		__func__, type);
		return false;
	}
	return true;
}

int nfs_multipath_parse_ip_list(char *buffer, struct net *net_ns,
			struct multipath_mount_options *options,
			enum nfs_multi_path_options type)
{
	char *buf_to_parse = NULL;
	bool  prev_range = false;
	int   ret    = 0;
	char *cursor = NULL;
	bool  single = true;
	struct nfs_ip_list *ip_list_tmp = NULL;

	if (!nfs_multipath_parse_param_check(type, options))
		return -ENOTSUPP;

	if (type == REMOUNTREMOTEADDR)
		type = REMOTEADDR;

	if (type == REMOUNTLOCALADDR)
		type = LOCALADDR;

	if (type == LOCALADDR)
		ip_list_tmp = options->local_ip_list;
	else
		ip_list_tmp = options->remote_ip_list;

	pr_info("NFS:   parsing nfs mount option '%s' type: %d[%s]\n",
	buffer, type, __func__);

	buf_to_parse = buffer;
	while (buf_to_parse != NULL) {
		cursor =
		nfs_multipath_parse_ip_list_get_cursor(&buf_to_parse, &single);
		if (!cursor)
			break;

		if (single == false && prev_range == true) {
			pr_info("NFS: mount option type: %d fail. Multiple Range.[%s]\n",
			type, __func__);

			ret = -EINVAL;
			goto out;
		}

		if (prev_range == false) {
			ret = nfs_multipath_parse_ip_list_inter(ip_list_tmp,
				net_ns, cursor, type);
			if (ret)
				goto out;
			if (single == false)
				prev_range = true;
		} else {
			ret = nfs_multipath_parse_ip_range(net_ns, cursor,
				ip_list_tmp, type);
			if (ret != 0)
				goto out;
			prev_range = false;
		}
	}

out:
	if (ret)
		memset(ip_list_tmp, 0, sizeof(struct nfs_ip_list));

	return ret;
}

int nfs_multipath_parse_dns_list(char *buffer, struct net *net_ns,
			struct multipath_mount_options *options)
{
	struct NFS_ROUTE_DNS_INFO_S *dns_name_list_tmp = NULL;
	char *cursor = NULL;
	char *bufToParse;

	if (!nfs_multipath_parse_param_check(REMOTEDNSNAME, options))
		return -ENOTSUPP;

	pr_info("[MULTIPATH:%s] buffer %s\n", __func__, buffer);
	// freed in nfs_free_parsed_mount_data
	dns_name_list_tmp = kmalloc(sizeof(struct NFS_ROUTE_DNS_INFO_S),
							GFP_KERNEL);
	if (!dns_name_list_tmp)
		return -ENOMEM;

	dns_name_list_tmp->dnsNameCount = 0;
	bufToParse = buffer;
	while (bufToParse) {
		if (dns_name_list_tmp->dnsNameCount >= MAX_DNS_SUPPORTED) {
			pr_err("%s: dnsname for %s reached %d,more than supported limit %d\n",
				__func__, cursor,
				dns_name_list_tmp->dnsNameCount,
				MAX_DNS_SUPPORTED);
			dns_name_list_tmp->dnsNameCount = 0;
			return -ENOSPC;
		}
		cursor = strsep(&bufToParse, "~");
		if (!cursor)
			break;

		strcpy(dns_name_list_tmp->routeRemoteDnsList
		[dns_name_list_tmp->dnsNameCount].dnsname,
		cursor);
		dns_name_list_tmp->dnsNameCount++;
	}
	if (dns_name_list_tmp->dnsNameCount == 0)
		return -EINVAL;
	options->pRemoteDnsInfo = dns_name_list_tmp;
	return 0;
}

int nfs_multipath_parse_options_check_ipv4_valid(struct sockaddr_in *addr)
{
	if (addr->sin_addr.s_addr == 0 || addr->sin_addr.s_addr == 0xffffffff)
		return -EINVAL;
	return 0;
}

int nfs_multipath_parse_options_check_ipv6_valid(struct sockaddr_in6 *addr)
{
	if (addr->sin6_addr.in6_u.u6_addr32[0] == 0 &&
		addr->sin6_addr.in6_u.u6_addr32[1] == 0 &&
		addr->sin6_addr.in6_u.u6_addr32[2] == 0 &&
		addr->sin6_addr.in6_u.u6_addr32[3] == 0)
		return -EINVAL;

	if (addr->sin6_addr.in6_u.u6_addr32[0] == 0xffffffff &&
		addr->sin6_addr.in6_u.u6_addr32[1] == 0xffffffff &&
		addr->sin6_addr.in6_u.u6_addr32[2] == 0xffffffff &&
		addr->sin6_addr.in6_u.u6_addr32[3] == 0xffffffff)
		return -EINVAL;
	return 0;
}

int nfs_multipath_parse_options_check_ip_valid(struct sockaddr_storage *address)
{
	int rc = 0;

	if (address->ss_family == AF_INET)
		rc = nfs_multipath_parse_options_check_ipv4_valid(
			(struct sockaddr_in *)address);
	else if (address->ss_family == AF_INET6)
		rc = nfs_multipath_parse_options_check_ipv6_valid(
			(struct sockaddr_in6 *)address);
	else
		rc = -EINVAL;

	return rc;
}

int nfs_multipath_parse_options_check_valid(
			struct multipath_mount_options *options)
{
	int rc;
	int i;

	if (options == NULL)
		return 0;

	for (i = 0; i < options->local_ip_list->count; i++) {
		rc = nfs_multipath_parse_options_check_ip_valid(
			&options->local_ip_list->address[i]);
		if (rc != 0)
			return rc;
	}

	for (i = 0; i < options->remote_ip_list->count; i++) {
		rc = nfs_multipath_parse_options_check_ip_valid(
			&options->remote_ip_list->address[i]);
		if (rc != 0)
			return rc;
	}

	return 0;
}
int nfs_multipath_parse_options_check_duplicate(
			struct multipath_mount_options *options)
{
	int i;
	int j;

	if (options == NULL ||
		options->local_ip_list->count == 0 ||
		options->remote_ip_list->count == 0)

		return 0;

	for (i = 0; i < options->local_ip_list->count; i++) {
		for (j = 0; j < options->remote_ip_list->count; j++) {
			if (rpc_cmp_addr((const struct sockaddr *)
				&options->local_ip_list->address[i],
				(const struct sockaddr *)
				&options->remote_ip_list->address[j]))
				return -ENOTSUPP;
		}
	}
	return 0;
}

int nfs_multipath_parse_options_check(struct multipath_mount_options *options)
{
	int rc = 0;

	rc = nfs_multipath_parse_options_check_valid(options);

	if (rc != 0) {
		pr_err("has invaild ip.\n");
		return rc;
	}

	rc = nfs_multipath_parse_options_check_duplicate(options);
	if (rc != 0)
		return rc;
	return rc;
}

int nfs_multipath_alloc_options(void **enfs_option)
{
	struct multipath_mount_options *options = NULL;

	options = kzalloc(sizeof(struct multipath_mount_options), GFP_KERNEL);

	if (options == NULL)
		return -ENOMEM;

	options->local_ip_list =
	kzalloc(sizeof(struct nfs_ip_list), GFP_KERNEL);
	if (options->local_ip_list == NULL) {
		kfree(options);
		return -ENOMEM;
	}

	options->remote_ip_list =
	kzalloc(sizeof(struct nfs_ip_list), GFP_KERNEL);
	if (options->remote_ip_list == NULL) {
		kfree(options->local_ip_list);
		kfree(options);
		return -ENOMEM;
	}

	options->pRemoteDnsInfo = kzalloc(sizeof(struct NFS_ROUTE_DNS_INFO_S),
				GFP_KERNEL);
	if (options->pRemoteDnsInfo == NULL) {
		kfree(options->remote_ip_list);
		kfree(options->local_ip_list);
		kfree(options);
		return -ENOMEM;
	}

	*enfs_option = options;
	return 0;
}

int nfs_multipath_parse_options(enum nfs_multi_path_options type,
			char *str, void **enfs_option, struct net *net_ns)
{
	int rc;
	struct multipath_mount_options *options = NULL;

	if ((str == NULL) || (enfs_option == NULL) || (net_ns == NULL))
		return -EINVAL;

	if (*enfs_option == NULL) {
		rc = nfs_multipath_alloc_options(enfs_option);
		if (rc != 0) {
			enfs_log_error(
				"alloc enfs_options failed! errno:%d\n", rc);
			return rc;
		}
	}

	options = (struct multipath_mount_options *)*enfs_option;

	if (type == LOCALADDR  || type == REMOUNTLOCALADDR ||
		type == REMOTEADDR || type == REMOUNTREMOTEADDR) {
		rc = nfs_multipath_parse_ip_list(str, net_ns, options, type);
	} else if (type == REMOTEDNSNAME) {
		/* alloc and release need to modify */
		rc = nfs_multipath_parse_dns_list(str, net_ns, options);
	}  else {
		rc = -EOPNOTSUPP;
	}

	// after parsing cmd, need checking local and remote
	// IP is same. if not means illegal cmd
	if (rc == 0)
		rc = nfs_multipath_parse_options_check_duplicate(options);

	if (rc == 0)
		rc = nfs_multipath_parse_options_check(options);

	return rc;
}

void nfs_multipath_free_options(void **enfs_option)
{
	struct multipath_mount_options *options;

	if (enfs_option == NULL || *enfs_option == NULL)
		return;

	options = (struct multipath_mount_options *)*enfs_option;

	if (options->remote_ip_list != NULL) {
		kfree(options->remote_ip_list);
		options->remote_ip_list = NULL;
	}

	if (options->local_ip_list != NULL) {
		kfree(options->local_ip_list);
		options->local_ip_list = NULL;
	}

	if (options->pRemoteDnsInfo != NULL) {
		kfree(options->pRemoteDnsInfo);
		options->pRemoteDnsInfo = NULL;
	}

	kfree(options);
	*enfs_option = NULL;
}
