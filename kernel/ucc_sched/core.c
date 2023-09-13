// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * Author: Huawei OS Kernel Lab
 * Create: Tue Jan 17 22:19:17 2023
 */

#include <uapi/linux/sched/types.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/ucc_sched.h>

#include "ucc_sched.h"
#include "../sched/sched.h"
#define CREATE_TRACE_POINTS
#include <trace/events/ucc_sched.h>

#define MAX_XCU_NUM	(100)
#define TS_SQ_TRANS_TASK_THRESHOLD	(20)

static struct xcu xcu_manager[MAX_XCU_NUM];
static int num_active_xcu;
raw_spinlock_t xcu_mgr_lock;
int sysctl_ucc_sched_rcv_timeout_ms = 10;

static struct task_struct vstream_idle_task;
static struct vstream_info vstream_idle = {
	.vstreamId = UINT_MAX,
	.p = &vstream_idle_task,
};

struct sched_args {
	int cu_id;
};

static inline int is_xcu_offline(struct xcu *cu)
{
	return cu->state == XCU_INACTIVE;
}

void ucc_set_vstream_state(struct vstream_info *vinfo, int state)
{
	vinfo->se.state = state;
}

static inline int should_se_run(struct ucc_se *se)
{
	return se->state != SE_BLOCK && se->state != SE_DEAD;
}

static inline void update_stats_run_start(struct xcu *cu,
					  struct ucc_se *se)
{
	u64 start;

	if (!schedstat_enabled())
		return;

	start = ktime_get_boot_ns();
	__schedstat_set(se->statistics.run_start, start);
}

static inline void update_stats_run_end(struct xcu *cu,
					struct ucc_se *se)
{

	struct vstream_info *vinfo;
	u64 delta;

	if (!schedstat_enabled())
		return;

	delta = ktime_get_boot_ns() - schedstat_val(se->statistics.run_start);
	vinfo = container_of(se, struct vstream_info, se);
	trace_ucc_sched_stat_run(vinfo, delta, se->is_timeout);

	__schedstat_set(se->statistics.run_max,
		      max(schedstat_val(se->statistics.run_max), delta));
	__schedstat_inc(se->statistics.run_count);
	__schedstat_add(se->statistics.run_sum, delta);
	__schedstat_set(se->statistics.run_start, 0);
}

static inline void update_stats_preempt_start(struct xcu *cu,
					      struct ucc_se *se)
{
	u64 wait_start;

	if (!schedstat_enabled())
		return;

	wait_start = ktime_get_boot_ns();
	__schedstat_set(se->statistics.preempt_start, wait_start);
}

static inline void update_stats_wait_start(struct xcu *cu, struct ucc_se *se)
{
	u64 wait_start;

	if (!schedstat_enabled())
		return;

	wait_start = ktime_get_boot_ns();
	__schedstat_set(se->statistics.wait_start, wait_start);
}


static inline void update_stats_wait_end(struct xcu *cu, struct ucc_se *se)
{
	struct vstream_info *vinfo;
	u64 delta, preempt_delta;

	if (!schedstat_enabled())
		return;

	delta = ktime_get_boot_ns() - schedstat_val(se->statistics.wait_start);
	vinfo = container_of(se, struct vstream_info, se);
	trace_ucc_sched_stat_wait(vinfo, delta);

	__schedstat_set(se->statistics.wait_max,
		      max(schedstat_val(se->statistics.wait_max), delta));
	__schedstat_inc(se->statistics.wait_count);
	__schedstat_add(se->statistics.wait_sum, delta);
	__schedstat_set(se->statistics.wait_start, 0);

	if (se->statistics.preempt_start) {
		preempt_delta = ktime_get_boot_ns() -
				schedstat_val(se->statistics.preempt_start);
		trace_ucc_sched_stat_preempt(vinfo, preempt_delta);

		__schedstat_set(se->statistics.preempt_max,
				max(schedstat_val(se->statistics.preempt_max),
				preempt_delta));
		__schedstat_inc(se->statistics.preempt_count);
		__schedstat_add(se->statistics.preempt_sum, preempt_delta);
		__schedstat_set(se->statistics.preempt_start, 0);
	}
}

void ucc_dump_statistics_info(struct ucc_se *se)
{
	struct vstream_info *vinfo = container_of(se, struct vstream_info, se);

	pr_info("comm %s pid %d vstreamId %d kernel_sum %llu wait_count %llu wait_max %llu[ns] wait_sum %llu[ns] preempt_count %llu preempt_max %llu[ns] preempt_sum %llu[ns]\n",
		 vinfo->p->comm,
		 vinfo->p->pid,
		 vinfo->vstreamId,
		 vinfo->se.statistics.kernel_sum,
		 vinfo->se.statistics.wait_count,
		 vinfo->se.statistics.wait_max,
		 vinfo->se.statistics.wait_sum,
		 vinfo->se.statistics.preempt_count,
		 vinfo->se.statistics.preempt_max,
		 vinfo->se.statistics.preempt_sum);
}

static void put_prev_entity(struct xcu *cu, struct ucc_se *prev)
{
	if (!prev)
		return;

	if (prev->on_cu)
		update_stats_wait_start(cu, prev);

	prev->state = SE_READY;
	cu->curr_se->state = SE_RUNNING;
}

static void set_next_entity(struct xcu *cu, struct ucc_se *se)
{
	if (se->on_cu && se != cu->curr_se)
		update_stats_wait_end(cu, se);

	cu->curr_se = se;
}

static void dequeue_ucc_se(struct ucc_se *se, struct xcu *cu)
{
	raw_spin_lock(&cu->xcu_lock);
	if (!se->on_cu) {
		raw_spin_unlock(&cu->xcu_lock);
		return;
	}

	se->on_cu = 0;

	list_del_init(&se->run_list);

	if (list_empty(cu->queue + se->prio))
		__clear_bit(se->prio, cu->bitmap);
	cu->rt_nr_running--;

	if (se != cu->curr_se)
		update_stats_wait_end(cu, se);

	if (cu->curr_se == se)
		cu->curr_se = NULL;

	raw_spin_unlock(&cu->xcu_lock);
}

static void enqueue_ucc_se(struct ucc_se *se, struct xcu *cu)
{
	struct list_head *queue = cu->queue + se->prio;

	raw_spin_lock(&cu->xcu_lock);
	if (se->on_cu) {
		raw_spin_unlock(&cu->xcu_lock);
		return;
	}
	se->on_cu = 1;
	se->is_timeout = 0;
	list_add_tail(&se->run_list, queue);
	__set_bit(se->prio, cu->bitmap);
	cu->rt_nr_running++;

	update_stats_wait_start(cu, se);

	raw_spin_unlock(&cu->xcu_lock);
}

static struct xcu *ucc_select_cu(struct ucc_se *se)
{
	struct vstream_info *vstream_info;
	int min_nr_running = INT_MAX;
	struct xcu *cu;
	int select_cu = 0;
	int cu_id;

	vstream_info = container_of(se, struct vstream_info, se);
	for (cu_id = 0; cu_id < num_active_xcu; cu_id++) {
		cu = &xcu_manager[cu_id];

		if (vstream_info->devId != cu->dev_id ||
		    vstream_info->tsId != cu->ts_id)
			continue;

		if (cu->rt_nr_running < min_nr_running) {
			min_nr_running = cu->rt_nr_running;
			select_cu = cu_id;
		}
	}

	vstream_info->cu_id = select_cu;
	return &xcu_manager[select_cu];
}

static int ucc_check_preempt(struct ucc_se *se, struct xcu *cu)
{
	struct vstream_info *vinfo_curr, *vinfo;
	struct ucc_se *curr_se;

	curr_se = cu->curr_se;
	if (!curr_se)
		return 1;

	vinfo = container_of(se, struct vstream_info, se);
	vinfo_curr = container_of(curr_se, struct vstream_info, se);
	if (vinfo_curr->p->ucc_priority > vinfo->p->ucc_priority) {
		update_stats_preempt_start(cu, se);
		curr_se->flag = UCC_TIF_PREEMPT;
		return 1;
	}

	return 0;
}

static inline void ucc_wakeup_idle_worker(struct xcu *cu)
{
	wake_up_state(cu->worker, TASK_INTERRUPTIBLE);
}

static inline void ucc_wakeup_running_worker(struct xcu *cu)
{
	wake_up_state(cu->worker, TASK_UNINTERRUPTIBLE);
}

int ucc_schedule(int cu_id)
{
	struct xcu *cu;

	cu = &xcu_manager[cu_id];
	cu->is_wake = 1;
	ucc_wakeup_running_worker(cu);

	return 0;
}
EXPORT_SYMBOL(ucc_schedule);

int ucc_wake_up(struct ucc_se *se)
{
	struct xcu *cu;

	raw_spin_lock(&se->se_lock);
	if (se->on_cu) {
		raw_spin_unlock(&se->se_lock);
		return 0;
	}

	if (se->state == SE_BLOCK)
		se->state = SE_READY;

	cu = ucc_select_cu(se);
	if (!cu) {
		raw_spin_unlock(&se->se_lock);
		return -1;
	}

	enqueue_ucc_se(se, cu);
	if (ucc_check_preempt(se, cu))
		ucc_wakeup_idle_worker(cu);

	raw_spin_unlock(&se->se_lock);

	return 0;
}

static struct ucc_se *pick_next_ucc_se(struct xcu *cu)
{
	struct ucc_se *se;
	struct list_head *queue;
	int idx;

	if (!cu->rt_nr_running)
		return NULL;

	idx = sched_find_first_bit(cu->bitmap);
	BUG_ON(idx >= MAX_UCC_PRIO);

	queue = cu->queue + idx;
	se = list_entry(queue->next, struct ucc_se, run_list);

	return se;
}

static int ucc_submit_kernel(struct xcu *cu, struct ucc_se *se)
{
	struct vstream_info *vstream_info;
	struct xpu_group *group;
	struct tsdrv_ctx *ctx;
	int kernel_num, left;

	vstream_info = container_of(se, struct vstream_info, se);
	ctx = vstream_info->privdata;
	left = (vstream_info->vsqNode->tail - vstream_info->vsqNode->head +
	       MAX_VSTREAM_SIZE) % MAX_VSTREAM_SIZE;

	group = vstream_info->group;

	kernel_num = xpu_run(group, vstream_info, ctx);
	if (kernel_num <= 0)
		return kernel_num;

	//update vstream info head and tail;
	update_vstream_head(vstream_info, kernel_num);

	left -= kernel_num;

	return kernel_num;
}

static inline void ucc_wait_idle(struct xcu *cu)
{
	cu->state = XCU_IDLE;

	do {
		schedule_timeout_interruptible(1);
	} while (cu->rt_nr_running == 0);

	cu->state = XCU_BUSY;
}

static inline void ucc_wait_running(struct xcu *cu, struct ucc_se *se)
{
	int cnt = 1;

	do {
		schedule_timeout_uninterruptible(
			msecs_to_jiffies(sysctl_ucc_sched_rcv_timeout_ms));
	} while (cu->is_wake == 0 && --cnt > 0);

	if (cnt == 0) {
		__schedstat_inc(se->statistics.timeout_count);
		se->is_timeout = 1;
	}
}

static inline void clear_se_flag(struct ucc_se *se)
{
	if (se)
		se->flag = UCC_TIF_NONE;
}

void ucc_dequeue_task(struct vstream_info *vInfo)
{
	struct xcu *cu = &xcu_manager[vInfo->cu_id];
	struct ucc_se *se = &vInfo->se;

	raw_spin_lock(&se->se_lock);
	dequeue_ucc_se(se, cu);
	raw_spin_unlock(&se->se_lock);
}

/*
 * dynamic padding: select kernels with no QoS confilcts to current ucc_se
 * to fill cu;
 */
static void dynamic_padding(struct xcu *cu, struct ucc_se *se)
{
}

static int __ucc_schedule(void *args)
{
	struct sched_args *sargs = (struct sched_args *)args;
	int cu_id = sargs->cu_id;
	struct xcu *cu = &xcu_manager[cu_id];
	struct ucc_se *se = NULL, *curr_se = NULL;
	struct ucc_se *prev_se = NULL;
	struct vstream_info *vinfo;
	int send_cnt = 0;
	int kernel_num, preempt;

	while (!is_xcu_offline(cu)) {
		raw_spin_lock(&cu->xcu_lock);
		cu->is_sched = 0;
		prev_se = cu->curr_se;

		preempt = 0;
		if (prev_se) {
			if (prev_se->flag != UCC_TIF_PREEMPT)
				goto submit_kernel;

			vinfo = container_of(prev_se, struct vstream_info, se);
			if (send_cnt < vinfo->p->ucc_step)
				goto submit_kernel;

			preempt = 1;
		}

		clear_se_flag(prev_se);
		se = pick_next_ucc_se(cu);
		if (!se) {
			cu->is_sched = 1;
			raw_spin_unlock(&cu->xcu_lock);
			trace_ucc_sched_switch(0, &vstream_idle);
			ucc_wait_idle(cu);
			continue;
		}

		set_next_entity(cu, se);
		if (se != prev_se) {
			put_prev_entity(cu, prev_se);
			vinfo = container_of(se, struct vstream_info, se);
			trace_ucc_sched_switch(preempt, vinfo);
		}
		send_cnt = 0;
submit_kernel:
		curr_se = cu->curr_se;
		dynamic_padding(cu, curr_se);
		raw_spin_unlock(&cu->xcu_lock);

		curr_se->is_timeout = 0;
		kernel_num = ucc_submit_kernel(cu, curr_se);
		//has no more kernels to submit.
		if (kernel_num <= 0 && !vstream_have_kernel(curr_se)) {
			raw_spin_lock(&curr_se->se_lock);
			curr_se->state = SE_BLOCK;
			dequeue_ucc_se(curr_se, cu);
			raw_spin_unlock(&curr_se->se_lock);
			cu->is_sched = 1;
			continue;
		}
		cu->is_sched = 1;

		vinfo = container_of(curr_se, struct vstream_info, se);
		if (vinfo->send_cnt > TS_SQ_TRANS_TASK_THRESHOLD) {
			update_stats_run_start(cu, curr_se);
			/* kernel has not finish */
			if (!cu->is_wake)
				ucc_wait_running(cu, curr_se);

			update_stats_run_end(cu, curr_se);
			cu->is_wake = 0;
			vinfo->send_cnt = 0;
		}

		send_cnt += kernel_num;
		schedstat_add(se->statistics.kernel_sum, kernel_num);
	}

	return 0;
}

static void init_xcu_rq(struct xcu *cu)
{
	int i;

	for (i = 0; i < MAX_UCC_PRIO; i++) {
		INIT_LIST_HEAD(cu->queue + i);
		__clear_bit(i, cu->bitmap);
	}

	/* delimiter for bitsearch: */
	__set_bit(MAX_UCC_PRIO, cu->bitmap);
	cu->rt_nr_running = 0;
	raw_spin_lock_init(&cu->xcu_lock);
}

static int alloc_cu_id(void)
{
	int cu_id = -1;

	raw_spin_lock(&xcu_mgr_lock);
	if (num_active_xcu >= MAX_XCU_NUM) {
		raw_spin_unlock(&xcu_mgr_lock);
		return cu_id;
	}

	cu_id = num_active_xcu;
	num_active_xcu++;
	raw_spin_unlock(&xcu_mgr_lock);

	return cu_id;
}

int ucc_sched_register_xcu(int dev_id, int ts_id, int cu_num)
{
	int cu_id;
	struct xcu *cu;
	struct sched_args *args;
	struct sched_param param = { .sched_priority = 1 };
	char id_buf[16];
	int i;

	for (i = 0; i < cu_num; i++) {
		cu_id = alloc_cu_id();
		if (cu_id < 0) {
			pr_err("alloc cu id failed\n");
			return -1;
		}

		cu = &xcu_manager[cu_id];
		cu->cu_id = cu_id;
		cu->state = XCU_IDLE;
		cu->curr_se = NULL;
		cu->dev_id = dev_id;
		cu->ts_id = ts_id;
		cu->is_wake = 0;
		init_xcu_rq(cu);

		args = kzalloc(sizeof(struct sched_args), GFP_KERNEL);
		if (!args)
			return -1;

		args->cu_id = cu->cu_id;
		snprintf(id_buf, sizeof(id_buf), "%d:%d:%d",
			 cu->cu_id, cu->dev_id, cu->ts_id);
		cu->worker = kthread_create_on_node(__ucc_schedule,
						    (void *)args, NUMA_NO_NODE,
						    "u_sched/%s", id_buf);
		sched_setscheduler_nocheck(cu->worker, SCHED_FIFO, &param);
		wake_up_process(cu->worker);
	}

	return 0;
}
EXPORT_SYMBOL(ucc_sched_register_xcu);

int ucc_sched_init(void)
{
	raw_spin_lock_init(&xcu_mgr_lock);
	return 0;
}

int ucc_rt_nr_running(struct xcu *cu)
{
	return cu->rt_nr_running;
}
EXPORT_SYMBOL(ucc_rt_nr_running);

struct xcu *ucc_get_xcu_by_id(int cu_id)
{
	return &xcu_manager[cu_id];
}
EXPORT_SYMBOL(ucc_get_xcu_by_id);

int ucc_xcu_is_sched(int cu_id)
{
	return xcu_manager[cu_id].is_sched;
}
EXPORT_SYMBOL(ucc_xcu_is_sched);
