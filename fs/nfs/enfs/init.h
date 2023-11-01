/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: nfs client init
 * Create: 2023-07-31
 */

#ifndef ENFS_INIT_H
#define ENFS_INIT_H

#include <linux/types.h>

int32_t enfs_init(void);
void enfs_fini(void);

#endif
