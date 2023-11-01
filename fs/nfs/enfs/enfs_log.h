/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: enfs log
 * Create: 2023-07-31
 */
#ifndef ENFS_LOG_H
#define ENFS_LOG_H

#include <linux/printk.h>

#define enfs_log_info(fmt, ...) \
	pr_info("enfs:[%s]" pr_fmt(fmt), \
	__func__, ##__VA_ARGS__)

#define enfs_log_error(fmt, ...) \
	pr_err("enfs:[%s]" pr_fmt(fmt), \
	__func__, ##__VA_ARGS__)

#define enfs_log_debug(fmt, ...) \
	pr_debug("enfs:[%s]" pr_fmt(fmt), \
	__func__, ##__VA_ARGS__)

#endif  // ENFS_ERRCODE_H
