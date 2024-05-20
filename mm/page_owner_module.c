// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 *
 * page_owner_module core file
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/ctype.h>

#include "page_owner.h"

#define PAGE_OWNER_FILTER_BUF_SIZE 16
#define PAGE_OWNER_NONE_FILTER 0
#define PAGE_OWNER_MODULE_FILTER 1

static unsigned int page_owner_filter = PAGE_OWNER_NONE_FILTER;

void po_find_module_name_with_update(depot_stack_handle_t handle, char *mod_name, size_t size)
{
	int i;
	struct module *mod = NULL;
	unsigned long *entries;
	unsigned int nr_entries;

	if (unlikely(!mod_name))
		return;

	nr_entries = stack_depot_fetch(handle, &entries);
	if (!in_task())
		nr_entries = filter_irq_stacks(entries, nr_entries);
	for (i = 0; i < nr_entries; i++) {
		if (core_kernel_text(entries[i]))
			continue;

		preempt_disable();
		mod = __module_address(entries[i]);
		preempt_enable();

		if (!mod)
			continue;

		strscpy(mod_name, mod->name, size);
		return;
	}
}

void po_set_module_name(struct page_owner *page_owner, char *mod_name)
{
	if (unlikely(!page_owner || !mod_name))
		return;

	if (strlen(mod_name) != 0)
		strscpy(page_owner->module_name, mod_name, MODULE_NAME_LEN);
	else
		memset(page_owner->module_name, 0, MODULE_NAME_LEN);
}

static inline bool po_is_module(struct page_owner *page_owner)
{
	return strlen(page_owner->module_name) != 0;
}

int po_module_name_snprint(struct page_owner *page_owner,
		char *kbuf, size_t size)
{
	if (unlikely(!page_owner || !kbuf))
		return 0;

	if (po_is_module(page_owner))
		return scnprintf(kbuf, size, "Page allocated by module %s\n",
					page_owner->module_name);

	return 0;
}

static ssize_t read_page_owner_filter(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	char kbuf[PAGE_OWNER_FILTER_BUF_SIZE];
	int kcount;

	if (page_owner_filter & PAGE_OWNER_MODULE_FILTER)
		kcount = snprintf(kbuf, sizeof(kbuf), "module\n");
	else
		kcount = snprintf(kbuf, sizeof(kbuf), "none\n");

	return simple_read_from_buffer(user_buf, count, ppos, kbuf, kcount);
}

static ssize_t write_page_owner_filter(struct file *file,
	       const char __user *user_buf, size_t count, loff_t *ppos)
{
	char kbuf[PAGE_OWNER_FILTER_BUF_SIZE];
	char *p_kbuf;
	size_t kbuf_size;

	kbuf_size = min(count, sizeof(kbuf) - 1);
	if (copy_from_user(kbuf, user_buf, kbuf_size))
		return -EFAULT;

	kbuf[kbuf_size] = '\0';
	p_kbuf = strstrip(kbuf);

	if (!strcmp(p_kbuf, "module"))
		page_owner_filter = PAGE_OWNER_MODULE_FILTER;
	else if (!strcmp(p_kbuf, "none"))
		page_owner_filter = PAGE_OWNER_NONE_FILTER;
	else
		return -EINVAL;

	return count;
}

static const struct file_operations page_owner_filter_ops = {
	.read =		read_page_owner_filter,
	.write =	write_page_owner_filter,
	.llseek =	default_llseek,
};

bool po_is_filtered(struct page_owner *page_owner)
{
	if (unlikely(!page_owner))
		return false;

	if (page_owner_filter & PAGE_OWNER_MODULE_FILTER &&
		!po_is_module(page_owner))
		return true;

	return false;
}

void po_module_stat_init(void)
{
	debugfs_create_file("page_owner_filter", 0600, NULL, NULL,
			&page_owner_filter_ops);
}
