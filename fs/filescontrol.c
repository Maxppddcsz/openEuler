/* SPDX-License-Identifier: GPL-2.0-or-later OR BSD-2-Clause */
/*
 * Copyright (c) 2024-2024, Huawei Tech. Co., Ltd.
 *
 * Author: Ridong Chen <chenridong@huawei.com>
 */

#include <linux/fdtable.h>
#include <linux/filescontrol.h>
#include <linux/files-cgroup.h>
#include <linux/misc-fd.h>

struct static_key_false misc_fd_enable_key;

static inline bool file_cg_misc_enabled(void)
{
	return static_branch_likely(&misc_fd_enable_key);
}

u64 file_cg_count_fds(struct files_struct *files)
{
	int i;
	struct fdtable *fdt;
	int retval = 0;

	fdt = files_fdtable(files);
	for (i = 0; i < DIV_ROUND_UP(fdt->max_fds, BITS_PER_LONG); i++)
		retval += hweight64((__u64)fdt->open_fds[i]);
	return retval;
}

int files_cg_alloc_fd(struct files_struct *files, u64 n)
{
	if (file_cg_misc_enabled())
		return misc_fd_alloc_fd(files, n);

	return files_cgroup_alloc_fd(files, n);
}

void files_cg_unalloc_fd(struct files_struct *files, u64 n)
{
	if (file_cg_misc_enabled())
		return misc_fd_unalloc_fd(files, n);

	return files_cgroup_unalloc_fd(files, n);
}

void files_cg_assign(struct files_struct *files)
{
	if (file_cg_misc_enabled())
		return misc_fd_assign(files);

	return files_cgroup_assign(files);
}

void files_cg_remove(struct files_struct *files)
{
	if (file_cg_misc_enabled())
		return misc_fd_remove(files);

	return files_cgroup_remove(files);
}

int files_cg_dup_fds(struct files_struct *newf)
{
	if (file_cg_misc_enabled())
		return misc_fd_dup_fds(newf);

	return files_cgroup_dup_fds(newf);
}

void files_cg_put_fd(struct files_struct *files, unsigned int fd)
{
	if (file_cg_misc_enabled())
		return misc_fd_put_fd(files, fd);

	return files_cgroup_put_fd(files, fd);
}

static int __init enable_misc_fd(char *s)
{
	static_branch_enable(&misc_fd_enable_key);
	pr_info("file_cg  enable misc to control fd\n");

	return 1;
}
__setup("file_cg=misc", enable_misc_fd);

