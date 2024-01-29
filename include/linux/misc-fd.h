/* SPDX-License-Identifier: GPL-2.0-or-later OR BSD-2-Clause */
/*
 * Copyright (c) 2024-2024, Huawei Tech. Co., Ltd.
 *
 * Author: Ridong Chen <chenridong@huawei.com>
 */

#ifndef __FD_MISC_H_
#define __FD_MISC_H_

#include <linux/fdtable.h>

extern int misc_fd_alloc_fd(struct files_struct *files, u64 n);
extern void misc_fd_unalloc_fd(struct files_struct *files, u64 n);

extern void misc_fd_assign(struct files_struct *files);
extern void misc_fd_remove(struct files_struct *files);

extern int misc_fd_dup_fds(struct files_struct *newf);
extern void misc_fd_put_fd(struct files_struct *files, unsigned int fd);

#endif
