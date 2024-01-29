/* SPDX-License-Identifier: GPL-2.0 */
/* filescontrol.h - Files Controller
 *
 * Copyright 2014 Google Inc.
 * Author: Brian Makin <merimus@google.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_FILESCONTROL_H
#define _LINUX_FILESCONTROL_H

#include <linux/fdtable.h>
#include <linux/files-cgroup.h>
#include <linux/misc-fd.h>

static inline u64 fc_count_fds(struct files_struct *files)
{
	int i;
	struct fdtable *fdt;
	int retval = 0;

	fdt = files_fdtable(files);
	for (i = 0; i < DIV_ROUND_UP(fdt->max_fds, BITS_PER_LONG); i++)
		retval += hweight64((__u64)fdt->open_fds[i]);
	return retval;
}

#ifdef CONFIG_CGROUP_FILES
#ifdef CONFIG_CGROUP_MISC_FD
static inline int fc_alloc_fd(struct files_struct *files, u64 n)
{
	return misc_fd_alloc_fd(files, n);
}
static inline void fc_unalloc_fd(struct files_struct *files, u64 n)
{
	return misc_fd_unalloc_fd(files, n);
}

static inline void fc_assign(struct files_struct *files)
{
	return misc_fd_assign(files);
}

static inline void fc_remove(struct files_struct *files)
{
	return misc_fd_assign(files);
}

static inline int fc_dup_fds(struct files_struct *newf)
{
	return misc_fd_dup_fds(newf);
}

static inline void fc_put_fd(struct files_struct *files, unsigned int fd)
{
	return misc_fd_put_fd(files, fd);
}
#else /* no CONFIG_CGROUP_MISC */
static inline int fc_alloc_fd(struct files_struct *files, u64 n)
{
	return files_cgroup_alloc_fd(files, n);
}

static inline void fc_unalloc_fd(struct files_struct *files, u64 n)
{
	return files_cgroup_unalloc_fd(files, n);
}

static inline void fc_assign(struct files_struct *files)
{
	return files_cgroup_assign(files);
}

static inline void fc_remove(struct files_struct *files)
{
	return files_cgroup_assign(files);
}

static inline int fc_dup_fds(struct files_struct *newf)
{
	return files_cgroup_dup_fds(newf);
}

static inline void fc_put_fd(struct files_struct *files, unsigned int fd)
{
	return files_cgroup_put_fd(files, fd);
}

#endif /* CONFIG_CGROUP_MISC */
#else /* no CONFIG_CGROUP_FILES */
static inline int fc_alloc_fd(struct files_struct *files, u64 n)
{
	return 0;
};
static inline void fc_unalloc_fd(struct files_struct *files, u64 n) {};

static inline void fc_assign(struct files_struct *files) {};
static inline void fc_remove(struct files_struct *files) {};

static inline int fc_dup_fds(struct files_struct *newf)
{
	return 0;
};
static inline void fc_put_fd(struct files_struct *files, unsigned int fd) {};
#endif /* CONFIG_CGROUP_FILES */
#endif /* _LINUX_FILESCONTROL_H */
