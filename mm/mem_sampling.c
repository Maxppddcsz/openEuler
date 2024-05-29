// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) "mem_sampling: " fmt

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/vmstat.h>
#include <linux/mem_sampling.h>

struct mem_sampling_ops_struct mem_sampling_ops;

static int mem_sampling_override __initdata;

enum mem_sampling_saved_state_e mem_sampling_saved_state = MEM_SAMPLING_STATE_EMPTY;

struct mem_sampling_record_cb_list_entry {
	struct list_head list;
	mem_sampling_record_cb_type cb;
};
LIST_HEAD(mem_sampling_record_cb_list);

void mem_sampling_record_cb_register(mem_sampling_record_cb_type cb)
{
	struct mem_sampling_record_cb_list_entry *cb_entry, *tmp;

	list_for_each_entry_safe(cb_entry, tmp, &mem_sampling_record_cb_list, list) {
		if (cb_entry->cb == cb) {
			pr_info("mem_sampling record cb already registered\n");
			return;
		}
	}

	cb_entry = NULL;
	cb_entry = kmalloc(sizeof(struct mem_sampling_record_cb_list_entry), GFP_KERNEL);
	if (!cb_entry) {
		pr_info("mem_sampling record cb entry alloc memory failed\n");
		return;
	}

	cb_entry->cb = cb;
	list_add(&(cb_entry->list), &mem_sampling_record_cb_list);
}

void mem_sampling_record_cb_unregister(mem_sampling_record_cb_type cb)
{
	struct mem_sampling_record_cb_list_entry *cb_entry, *tmp;

	list_for_each_entry_safe(cb_entry, tmp, &mem_sampling_record_cb_list, list) {
		if (cb_entry->cb == cb) {
			list_del(&cb_entry->list);
			kfree(cb_entry);
			return;
		}
	}
}

void mem_sampling_sched_in(struct task_struct *prev, struct task_struct *curr)
{
	if (!static_branch_unlikely(&mem_sampling_access_hints))
		return;

	if (!mem_sampling_ops.sampling_start)
		return;

	if (!curr->mm)
		goto out;

	mem_sampling_ops.sampling_start();

	return;

out:
	mem_sampling_ops.sampling_stop();
}

void mem_sampling_process(struct mem_sampling_record *record_base, int nr_records)
{
	int i;
	struct mem_sampling_record *record;
	struct mem_sampling_record_cb_list_entry *cb_entry, *tmp;

	if (list_empty(&mem_sampling_record_cb_list))
		goto out;

	for (i = 0; i < nr_records; i++) {
		record = record_base + i;
		list_for_each_entry_safe(cb_entry, tmp, &mem_sampling_record_cb_list, list) {
			cb_entry->cb(record);
		}
	}
out:
	/* if mem_sampling_access_hints is set to false, stop sampling */
	if (static_branch_unlikely(&mem_sampling_access_hints))
		mem_sampling_ops.sampling_continue();
	else
		mem_sampling_ops.sampling_stop();
}

static inline enum mem_sampling_type_enum mem_sampling_get_type(void)
{
#ifdef CONFIG_ARM_SPE
	return MEM_SAMPLING_ARM_SPE;
#else
	return MEM_SAMPLING_UNSUPPORTED;
#endif
}

void mem_sampling_user_switch_process(enum user_switch_type type)
{
	bool state;

	if (type > USER_SWITCH_BACK_TO_MEM_SAMPLING) {
		pr_err("user switch type error.\n");
		return;
	}

	if (type == USER_SWITCH_AWAY_FROM_MEM_SAMPLING) {
		/* save state only the status when leave mem_sampling for the first time */
		if (mem_sampling_saved_state != MEM_SAMPLING_STATE_EMPTY)
			return;

		if (static_branch_unlikely(&mem_sampling_access_hints))
			mem_sampling_saved_state = MEM_SAMPLING_STATE_ENABLE;
		else
			mem_sampling_saved_state = MEM_SAMPLING_STATE_DISABLE;

		pr_debug("user switch away from mem_sampling, %s is saved, set to disable.\n",
				mem_sampling_saved_state ? "disabled" : "enabled");

		set_mem_sampling_state(false);
	} else {
		/* If the state is not backed up, do not restore it */
		if (mem_sampling_saved_state == MEM_SAMPLING_STATE_EMPTY)
			return;

		state = (mem_sampling_saved_state == MEM_SAMPLING_STATE_ENABLE) ? true : false;
		set_mem_sampling_state(state);
		mem_sampling_saved_state = MEM_SAMPLING_STATE_EMPTY;

		pr_debug("user switch back to mem_sampling, set to saved %s.\n",
				state ? "enalbe" : "disable");
	}
}

static void __init check_mem_sampling_enable(void)
{
	bool mem_sampling_default = false;

	/* Parsed by setup_mem_sampling. override == 1 enables, -1 disables */
	if (mem_sampling_override)
		set_mem_sampling_state(mem_sampling_override == 1);
	else
		set_mem_sampling_state(mem_sampling_default);
}

static int __init mem_sampling_init(void)
{
	enum mem_sampling_type_enum mem_sampling_type = mem_sampling_get_type();

	switch (mem_sampling_type) {
	case MEM_SAMPLING_ARM_SPE:
		if (!arm_spe_enabled()) {
			set_mem_sampling_state(false);
			return -ENODEV;
		}
		mem_sampling_ops.sampling_start	= arm_spe_start,
		mem_sampling_ops.sampling_stop	= arm_spe_stop,
		mem_sampling_ops.sampling_continue	= arm_spe_continue,

		arm_spe_record_capture_callback_register(mem_sampling_process);
		arm_spe_user_switch_callback_register(mem_sampling_user_switch_process);
		break;

	default:
		pr_info("unsupport hardware pmu type(%d), disable access hint!\n",
			mem_sampling_type);
		set_mem_sampling_state(false);
		return -ENODEV;
	}
	check_mem_sampling_enable();

	pr_info("mem_sampling layer access profiling setup for NUMA Balancing and DAMON etc.\n");
	return 0;
}
late_initcall(mem_sampling_init);
