// SPDX-License-Identifier: GPL-2.0-or-later OR BSD-2-Clause
/*
 * Copyright (c) 2024-2024, Huawei Tech. Co., Ltd.
 *
 * Author: Ridong Chen <chenridong@huawei.com>
 */

#include <linux/misc_cgroup.h>
#include <linux/fdtable.h>
#include <linux/filescontrol.h>
#include <linux/misc-fd.h>

/*
 * If first time to alloc,it has to init capacity
 */
int misc_fd_alloc_fd(struct files_struct *files, u64 n)
{
	if (files != &init_files)
		return misc_cg_try_charge(MISC_CG_RES_FD, files->misc_cg, n);
	return 0;
}

void misc_fd_unalloc_fd(struct files_struct *files, u64 n)
{
	if (files != &init_files)
		return misc_cg_uncharge(MISC_CG_RES_FD, files->misc_cg, n);
}

void misc_fd_assign(struct files_struct *files)
{
	struct cgroup_subsys_state *css;

	if (files == NULL || files == &init_files)
		return;

	css = task_get_css(current, misc_cgrp_id);
	files->misc_cg = (css ? container_of(css, struct misc_cg, css) : NULL);
}

void misc_fd_remove(struct files_struct *files)
{
	struct task_struct *tsk = current;

	if (files == &init_files)
		return;

	task_lock(tsk);
	spin_lock(&files->file_lock);
	if (files->misc_cg != NULL)
		css_put(&files->misc_cg->css);
	spin_unlock(&files->file_lock);
	task_unlock(tsk);
}

int misc_fd_dup_fds(struct files_struct *newf)
{
	int err;

	if (newf == &init_files)
		return 0;

	spin_lock(&newf->file_lock);
	err = misc_fd_alloc_fd(newf, file_cg_count_fds(newf));
	spin_unlock(&newf->file_lock);
	return err;
}

void misc_fd_put_fd(struct files_struct *files, unsigned int fd)
{
	struct fdtable *fdt = files_fdtable(files);

	if (files == &init_files)
		return;

	if (test_bit(fd, fdt->open_fds))
		return misc_fd_unalloc_fd(files, 1);
}
