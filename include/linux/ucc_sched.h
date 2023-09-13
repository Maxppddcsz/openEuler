/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __LINUX_UCC_SCHED_H__
#define __LINUX_UCC_SCHED_H__

#include <linux/list.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/hash.h>
#include <linux/rculist.h>
#include <linux/idr.h>
#include <linux/xpu_group.h>
#include <linux/hashtable.h>
#include <linux/vstream.h>
#include <linux/slab.h>
#include <linux/sched.h>

#define VRTSQ_RTSQ_HASH_ORDER  6

#ifdef CONFIG_XPU_SCHEDULE
int ucc_process_task(struct vstream_info *vsqcq_info, struct tsdrv_ctx *ctx,
		     int *sqenum);
int ucc_free_task(struct vstream_info *vsqcq_info, struct tsdrv_ctx *ctx);
int ucc_wait_cq(struct vstream_info *vsqcq_info, struct tsdrv_ctx *ctx,
		struct devdrv_report_para *arg, int *sqenum);
struct xpu_group *select_sq(struct vstream_info *vstream_info);
int ucc_sched_register_xcu(int dev_id, int ts_id, int cu_num);
void ucc_set_vstream_state(struct vstream_info *vinfo, int state);
void ucc_dequeue_task(struct vstream_info *vInfo);
int ucc_rt_nr_running(struct xcu *cu);
struct xcu *ucc_get_xcu_by_id(int cu_id);
int ucc_xcu_is_sched(int cu_id);
void ucc_dump_statistics_info(struct ucc_se *se);
#endif

#endif
