/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * Author: Huawei OS Kernel Lab
 * Create: Tue Jan 17 22:27:22 2023
 */
#ifndef __UCC_SCHED_USCHED_H__
#define __UCC_SCHED_USCHED_H__

#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/types.h>
#include <linux/vstream.h>

//For simplicity, we set this parameter to 2.
#define MAX_UCC_PRIO	(2)

enum xcu_state {
	XCU_INACTIVE,
	XCU_IDLE,
	XCU_BUSY,
	XCU_SUBMIT,
};

/*
 * This is the abstraction object of the xpu computing unit.
 */
struct xcu {
	int is_sched;
	int cu_id;
	int dev_id;
	int ts_id;
	int rt_nr_running;
	int is_wake;
	struct task_struct *worker;
	DECLARE_BITMAP(bitmap, MAX_UCC_PRIO);
	struct list_head queue[MAX_UCC_PRIO];
	enum xcu_state state;
	struct ucc_se *curr_se;
	raw_spinlock_t xcu_lock;
};

#endif
