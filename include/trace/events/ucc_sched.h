/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ucc_sched

#if !defined(_TRACE_UCC_SCHED_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_UCC_SCHED_H

#include <linux/tracepoint.h>
#include <linux/binfmts.h>

/*
 * XXX the below ucc_sched_stat tracepoints only apply to SCHED_OTHER/BATCH/IDLE
 *     adding ucc_sched_stat support to SCHED_FIFO/RR would be welcome.
 */
DECLARE_EVENT_CLASS(ucc_sched_stat_template,

	TP_PROTO(struct vstream_info *vinfo, u64 delay),

	TP_ARGS(vinfo, delay),

	TP_STRUCT__entry(
		__array(char,	comm,	TASK_COMM_LEN)
		__field(pid_t,	pid)
		__field(int,	cu_id)
		__field(u32,	vstreamId)
		__field(u32,	prio)
		__field(u64,	delay)
	),

	TP_fast_assign(
		memcpy(__entry->comm, vinfo->p->comm, TASK_COMM_LEN);
		__entry->pid	= vinfo->p->pid;
		__entry->cu_id = vinfo->cu_id;
		__entry->vstreamId	= vinfo->vstreamId;
		__entry->prio = vinfo->p->ucc_priority;
		__entry->delay	= delay;
	),

	TP_printk("comm=%s pid=%d cu_id=%d vstreamId %u prio %u, delay=%llu [ns]",
			__entry->comm, __entry->pid,
			__entry->cu_id, __entry->vstreamId, __entry->prio,
			(unsigned long long)__entry->delay)
);

DECLARE_EVENT_CLASS(ucc_sched_stat_template_1,

	TP_PROTO(struct vstream_info *vinfo, u64 delay, int is_timeout),

	TP_ARGS(vinfo, delay, is_timeout),

	TP_STRUCT__entry(
		__array(char,	comm,	TASK_COMM_LEN)
		__field(pid_t,	pid)
		__field(int,	cu_id)
		__field(u32,	vstreamId)
		__field(u64,	delay)
		__field(int,	is_timeout)
	),

	TP_fast_assign(
		memcpy(__entry->comm, vinfo->p->comm, TASK_COMM_LEN);
		__entry->pid	= vinfo->p->pid;
		__entry->cu_id = vinfo->cu_id;
		__entry->vstreamId	= vinfo->vstreamId;
		__entry->delay	= delay;
		__entry->is_timeout = is_timeout;
	),

	TP_printk("comm=%s pid=%d cu_id=%d vstreamId %u, delay=%llu [ns]:%d",
			__entry->comm, __entry->pid,
			__entry->cu_id, __entry->vstreamId,
			(unsigned long long)__entry->delay,
			__entry->is_timeout)
);
/*
 * Tracepoint for accounting wait time (time the task is runnable
 * but not actually running due to scheduler contention).
 */
DEFINE_EVENT(ucc_sched_stat_template, ucc_sched_stat_wait,
	     TP_PROTO(struct vstream_info *vinfo, u64 delay),
	     TP_ARGS(vinfo, delay));

DEFINE_EVENT(ucc_sched_stat_template, ucc_sched_stat_preempt,
	     TP_PROTO(struct vstream_info *vinfo, u64 delay),
	     TP_ARGS(vinfo, delay));

DEFINE_EVENT(ucc_sched_stat_template_1, ucc_sched_stat_run,
	     TP_PROTO(struct vstream_info *vinfo, u64 delay, int is_timeout),
	     TP_ARGS(vinfo, delay, is_timeout));

TRACE_EVENT(ucc_sched_switch,

	TP_PROTO(int preempt,
		 struct vstream_info *next),

	TP_ARGS(preempt, next),

	TP_STRUCT__entry(
		__field(int,	cu_id)
		__field(u32,	next_vstreamId)
		__field(u32,	next_prio)
		__field(int,	preempt)
	),

	TP_fast_assign(
		__entry->cu_id = next->cu_id;
		__entry->next_vstreamId	= next->vstreamId;
		__entry->next_prio = next->p->ucc_priority;
		__entry->preempt = preempt;
	),

	TP_printk("cu_id=%d next_vstreamId %u next_prio %u preempt[%d]",
			__entry->cu_id,
			__entry->next_vstreamId, __entry->next_prio,
			__entry->preempt)
);
#endif /* _TRACE_UCC_SCHED_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
