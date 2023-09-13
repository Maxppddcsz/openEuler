/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2019. All rights reserved.
 * Author: Huawei OS Kernel Lab
 * Create: Mon Jan 30 14:29:19 2023
 */

#ifndef __LINUX_UCC_SCHED_USCHED_H__
#define __LINUX_UCC_SCHED_USCHED_H__

enum ucc_se_state {
	SE_PREPARE,
	SE_READY,
	SE_RUNNING,
	SE_BLOCK,
	SE_DEAD,
};

enum ucc_se_flag {
	UCC_TIF_NONE,
	UCC_TIF_PREEMPT,
	UCC_TIF_BALANCE,
};

enum ucc_se_prio {
	UCC_PRIO_HIGH,
	UCC_PRIO_LOW,
};

enum ucc_se_step {
	UCC_STEP_SLOW = 1,
	UCC_STEP_FAST = 10,
};

struct ucc_statistics {
	u64	wait_start;
	u64	wait_max;
	u64	wait_count;
	u64	wait_sum;

	u64	preempt_start;
	u64	preempt_max;
	u64	preempt_count;
	u64	preempt_sum;

	u64	kernel_sum;
	u64	timeout_count;

	u64	run_start;
	u64	run_max;
	u64	run_count;
	u64	run_sum;
};

struct ucc_se {
	int on_cu;
	struct list_head run_list;
	enum ucc_se_state state;
	enum ucc_se_flag flag;
	enum ucc_se_prio prio;
	enum ucc_se_step step;
	raw_spinlock_t se_lock;
	struct ucc_statistics statistics;
	int is_timeout;
};

int ucc_sched_init(void);
int ucc_schedule(int cu_id);
int ucc_wake_up(struct ucc_se *se);

#endif
