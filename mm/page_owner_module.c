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
