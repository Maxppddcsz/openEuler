/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: remount ip header file
 * Create: 2023-08-12
 */
#ifndef _ENFS_REMOUNT_
#define _ENFS_REMOUNT_
#include <linux/string.h>
#include "enfs.h"

int enfs_remount_iplist(struct nfs_client *nfs_client, void *enfs_option);

#endif
