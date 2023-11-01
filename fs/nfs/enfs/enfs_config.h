/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: nfs configuration
 * Create: 2023-07-27
 */

#ifndef ENFS_CONFIG_H
#define ENFS_CONFIG_H

#include <linux/types.h>

enum enfs_multipath_state {
	ENFS_MULTIPATH_ENABLE = 0,
	ENFS_MULTIPATH_DISABLE = 1,
};

enum enfs_loadbalance_mode {
	ENFS_LOADBALANCE_RR,
};


int enfs_get_config_path_detect_interval(void);
int enfs_get_config_path_detect_timeout(void);
int enfs_get_config_multipath_timeout(void);
int enfs_get_config_multipath_state(void);
int enfs_get_config_loadbalance_mode(void);
int enfs_config_load(void);
int enfs_config_timer_init(void);
void enfs_config_timer_exit(void);
#endif // ENFS_CONFIG_H
