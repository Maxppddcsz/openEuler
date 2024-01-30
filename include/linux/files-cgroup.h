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

#ifndef __FILES_CGROUP_H_
#define __FILES_CGROUP_H_

#include <linux/fdtable.h>

extern int files_cgroup_alloc_fd(struct files_struct *files, u64 n);
extern void files_cgroup_unalloc_fd(struct files_struct *files, u64 n);

extern void files_cgroup_assign(struct files_struct *files);
extern void files_cgroup_remove(struct files_struct *files);

extern int files_cgroup_dup_fds(struct files_struct *newf);
extern void files_cgroup_put_fd(struct files_struct *files, unsigned int fd);

#endif /* __FILES_CGROUP_H_ */
