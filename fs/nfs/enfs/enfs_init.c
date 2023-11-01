// SPDX-License-Identifier: GPL-2.0
/*
 * Client-side ENFS adapter.
 *
 * Copyright (c) 2023. Huawei Technologies Co., Ltd. All rights reserved.
 */
#include <linux/module.h>
#include <linux/sunrpc/sched.h>
#include <linux/sunrpc/clnt.h>
#include <linux/nfs.h>
#include <linux/nfs4.h>
#include <linux/nfs3.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_fs_sb.h>
#include "enfs.h"
#include "enfs_multipath_parse.h"
#include "enfs_multipath_client.h"
#include "enfs_remount.h"
#include "init.h"
#include "enfs_log.h"
#include "enfs_multipath.h"
#include "mgmt_init.h"

struct enfs_adapter_ops enfs_adapter = {
	.name				= "enfs",
	.owner				= THIS_MODULE,
	.parse_mount_options     = nfs_multipath_parse_options,
	.free_mount_options      = nfs_multipath_free_options,
	.client_info_init        = nfs_multipath_client_info_init,
	.client_info_free        = nfs_multipath_client_info_free,
	.client_info_match       = nfs_multipath_client_info_match,
	.client_info_show        = nfs_multipath_client_info_show,
	.remount_ip_list         = enfs_remount_iplist,
};

int32_t enfs_init(void)
{
	int err;

	err = enfs_multipath_init();
	if (err) {
		enfs_log_error("init multipath failed.\n");
		goto out;
	}

	err = mgmt_init();
	if (err != 0) {
		enfs_log_error("init mgmt failed.\n");
		goto out_tp_exit;
	}

	return 0;

out_tp_exit:
	enfs_multipath_exit();
out:
	return err;
}

void enfs_fini(void)
{
	mgmt_fini();

	enfs_multipath_exit();
}

static int __init init_enfs(void)
{
	int ret;

	ret = enfs_adapter_register(&enfs_adapter);
	if (ret) {
		pr_err("regist enfs_adapter fail. ret %d\n", ret);
		return -1;
	}

		ret = enfs_init();
		if (ret) {
			enfs_adapter_unregister(&enfs_adapter);
		return -1;
		}

	return 0;
}

static void __exit exit_enfs(void)
{
	enfs_fini();
	enfs_adapter_unregister(&enfs_adapter);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_DESCRIPTION("Nfs client router");
MODULE_VERSION("1.0");

module_init(init_enfs);
module_exit(exit_enfs);
