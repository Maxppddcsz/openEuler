// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright 2022 Huawei LLC.
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <errno.h>

#ifndef NULL
#define NULL 0
#endif

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, __u64);
} array SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, __u64);
} hash SEC(".maps");

char _license[] SEC("license") = "GPL";

unsigned long tgidpid;
unsigned long cgid;

unsigned long tick_tgidpid_ret;
unsigned int tick_cgid_ret;
unsigned int tick_cgid_pid_ret;
unsigned long tick_cgid_se_to_cgid_ret;

unsigned long wakeup_tgidpid_ret;
unsigned int wakeup_cgid_ret;
unsigned int wakeup_cgid_pid_ret;
unsigned long wakeup_cgid_se_to_cgid_ret;

unsigned long entity_tgidpid_ret;
unsigned int entity_cgid_ret;
unsigned int entity_cgid_pid_ret;
unsigned long entity_cgid_se_to_cgid_ret;

// #define debug(args...) bpf_printk(args)
#define debug(args...)

SEC("sched/cfs_check_preempt_tick")
int BPF_PROG(test_check_preempt_tick, struct sched_entity *curr, unsigned long delta_exec)
{
	if (curr == NULL)
		return 0;

	if (tgidpid) {
		unsigned long curr_tgidpid;

		curr_tgidpid = bpf_sched_entity_to_tgidpid(curr);

		if (curr_tgidpid == tgidpid) {
			tick_tgidpid_ret = curr_tgidpid;
			debug("tick tgid %d pid %d tick_tgidpid_ret %lu", curr_tgidpid >> 32,
				curr_tgidpid & 0xFFFFFFFF, tick_tgidpid_ret);
		}
	} else if (cgid) {
		if (bpf_sched_entity_belongs_to_cgrp(curr, cgid)) {
			unsigned long curr_tgidpid;

			tick_cgid_ret = 1;

			if (!curr->my_q) {
				curr_tgidpid = bpf_sched_entity_to_tgidpid(curr);
				tick_cgid_pid_ret = curr_tgidpid & 0xFFFFFFFF;
				debug("tick tick_cgid_ret %d curr_pid %d", tick_cgid_ret,
					tick_cgid_pid_ret);
			}

			if (curr->my_q) {
				tick_cgid_se_to_cgid_ret = bpf_sched_entity_to_cgrpid(curr);
				debug("tick tick_cgid_ret %d curr_se_to_cgid %d", tick_cgid_ret,
					tick_cgid_se_to_cgid_ret);
			}

		}
	}
	return 0;
}

SEC("sched/cfs_check_preempt_wakeup")
int BPF_PROG(test_check_preempt_wakeup, struct task_struct *curr, struct task_struct *p)
{
	__u64 *value = NULL;
	__u32 key = 0;

	if (curr == NULL || p == NULL)
		return 0;

	value = bpf_map_lookup_elem(&array, &key);
	if (value)
		*value = 0;
	value = bpf_map_lookup_elem(&hash, &key);
	if (value)
		*value = 0;

	if (tgidpid) {
		unsigned long curr_tgidpid, p_tgidpid;

		curr_tgidpid = bpf_sched_entity_to_tgidpid(&curr->se);
		p_tgidpid = bpf_sched_entity_to_tgidpid(&p->se);

		if (curr_tgidpid == tgidpid) {
			wakeup_tgidpid_ret = curr_tgidpid;

			debug("wakeup curr_tgid %d curr_pid %d wakeup_tgidpid_ret %lu",
				curr_tgidpid >> 32, curr_tgidpid & 0xFFFFFFFF, wakeup_tgidpid_ret);
		} else if (p_tgidpid == tgidpid) {
			wakeup_tgidpid_ret = p_tgidpid;

			debug("wakeup p_tgid %d p_pid %d wakeup_tgidpid_ret %lu", p_tgidpid >> 32,
				p_tgidpid & 0xFFFFFFFF, wakeup_tgidpid_ret);
		}
	} else if (cgid) {
		if (bpf_sched_entity_belongs_to_cgrp(&curr->se, cgid)) {
			wakeup_cgid_ret = 1;
			wakeup_cgid_pid_ret = curr->pid;

			debug("wakeup wakeup_cgid_ret %d curr_pid %d", wakeup_cgid_ret,
				wakeup_cgid_pid_ret);
		} else if (bpf_sched_entity_belongs_to_cgrp(&p->se, cgid)) {
			wakeup_cgid_ret = 1;
			wakeup_cgid_pid_ret = p->pid;

			debug("wakeup wakeup_cgid_ret %d p_pid %d", wakeup_cgid_ret,
				wakeup_cgid_pid_ret);
		}
	}
	return 0;
}

SEC("sched/cfs_wakeup_preempt_entity")
int BPF_PROG(test_wakeup_preempt_entity, struct sched_entity *curr, struct sched_entity *se)
{
	if (curr == NULL || se == NULL)
		return 0;

	if (tgidpid) {
		unsigned long curr_tgidpid, se_tgidpid;

		curr_tgidpid = bpf_sched_entity_to_tgidpid(curr);
		se_tgidpid = bpf_sched_entity_to_tgidpid(se);

		if (curr_tgidpid == tgidpid) {
			entity_tgidpid_ret = curr_tgidpid;
			debug("entity curr_tgid %d curr_pid %d entity_tgidpid_ret %lu",
				curr_tgidpid >> 32, curr_tgidpid & 0xFFFFFFFF, entity_tgidpid_ret);
		} else if (se_tgidpid == tgidpid) {
			entity_tgidpid_ret = se_tgidpid;
			debug("entity se_tgid %d se_pid %d entity_tgidpid_ret %lu",
				se_tgidpid >> 32, se_tgidpid & 0xFFFFFFFF, entity_tgidpid_ret);
		}
	} else if (cgid) {
		if (bpf_sched_entity_belongs_to_cgrp(curr, cgid)) {
			unsigned long curr_tgidpid;

			entity_cgid_ret = 1;

			if (!curr->my_q) {
				curr_tgidpid = bpf_sched_entity_to_tgidpid(curr);
				entity_cgid_pid_ret = curr_tgidpid & 0xFFFFFFFF;
				debug("entity entity_cgid_ret %d curr_pid %d",
				entity_cgid_ret, entity_cgid_pid_ret);
			}

			if (curr->my_q) {
				entity_cgid_se_to_cgid_ret = bpf_sched_entity_to_cgrpid(curr);
				debug("entity entity_cgid_ret %d curr_se_to_cgid %d",
					entity_cgid_ret, entity_cgid_se_to_cgid_ret);
			}
		} else if (bpf_sched_entity_belongs_to_cgrp(se, cgid)) {
			unsigned long se_tgidpid;

			entity_cgid_ret = 1;

			if (!se->my_q) {
				se_tgidpid = bpf_sched_entity_to_tgidpid(se);
				entity_cgid_pid_ret = se_tgidpid & 0xFFFFFFFF;
				debug("entity entity_cgid_ret %d se_pid %d", entity_cgid_ret,
					entity_cgid_pid_ret);
			}

			if (se->my_q) {
				entity_cgid_se_to_cgid_ret = bpf_sched_entity_to_cgrpid(se);
				debug("entity entity_cgid_ret %d se_se_to_cgid %d", entity_cgid_ret,
					entity_cgid_se_to_cgid_ret);
			}
		}
	}
	return 0;
}
