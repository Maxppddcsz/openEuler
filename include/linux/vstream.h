/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_VSTREAM_H
#define _LINUX_VSTREAM_H

#include <linux/ucc_kfd.h>
#include <linux/ucc_sched/ucc_sched.h>
#include <linux/ucc_ts.h>

#define MAX_VSTREAM_SIZE	1024
#define MAX_VSTREAM_SLOT_SIZE	64
#define MAX_CQ_SLOT_SIZE	12

/*
 * XXX_VSTREAM_ALLOC: alloc a vstream, buffer for tasks
 * XXX_VSTREAM_FREE: free a vstream
 * XXX_VSTREAM_KICK: there are tasks to be executed in the vstream
 * XXX_VSTREAM_UPDATE: update information for an existing vstream
 * XXX_CALLBACK_VSTREAM_WAIT: waiting for callback tasks
 * XXX_CALLBACK_VSTREAM_KICK: callback tasks have been executed
 *
 * NOTE: Callback vstream is only for Ascend now. We do not need
 * CALLBACK_VSTREAM_ALLOC because the callback vstream will be
 * alloced with vstream on Ascend.
 */
enum VSTREAM_COMMAND {
	/* vstream command for Ascend */
	ASCEND_VSTREAM_ALLOC = 0,
	ASCEND_VSTREAM_FREE,
	ASCEND_VSTREAM_KICK,
	ASCEND_CALLBACK_VSTREAM_WAIT,
	ASCEND_CALLBACK_VSTREAM_KICK,
	ASCEND_VSTREAM_GET_HEAD,
	ASCEND_MAX_COMMAND,

	/* vstream command for amdgpu */
	AMDGPU_VSTREAM_ALLOC = ASCEND_MAX_COMMAND + 1,
	AMDGPU_VSTREAM_FREE,
	AMDGPU_VSTREAM_KICK,
	AMDGPU_VSTREAM_UPDATE,
	AMDGPU_MAX_COMMAND,
};

struct vstream_alloc_args {
	union {
		/* For Ascend */
		struct normal_alloc_sqcq_para ascend;
		/* For amdgpu */
		struct kfd_ioctl_create_queue_args amdgpu;
	};
};

struct vstream_free_args {
	union {
		/* For Ascend */
		struct normal_free_sqcq_para ascend;
		/* For amdgpu */
		struct kfd_ioctl_destroy_queue_args amdgpu;
	};
};

struct vstream_kick_args {
	union {
		/* For Ascend */
		struct tsdrv_sqcq_data_para ascend;
		/* For amdgpu */
	};
};

struct vstream_args {
	union {
		struct vstream_alloc_args va_args;
		struct vstream_free_args vf_args;
		struct vstream_kick_args vk_args;
		struct kfd_ioctl_update_queue_args vu_args;
		struct tsdrv_sqcq_data_para vh_args;
		struct devdrv_report_para cvw_args;
		struct tsdrv_sqcq_data_para cvk_args;
	};
};

struct vstream_node {
	uint32_t id;
	uint32_t head;
	uint32_t tail;
	uint32_t credit;
	void *vstreamData;
	raw_spinlock_t spin_lock;
};

struct vstream_id {
	uint32_t vstreamId;
	struct list_head list;
};

struct vcq_map_table {
	uint32_t vcqId;
	struct vstream_node *vcqNode;
	struct list_head vstreamId_list;
};

struct vstream_info {
	uint32_t vstreamId; //key
	uint32_t vcqId;
	uint32_t devId;
	uint32_t tsId;
	struct ucc_se se;
	//TODO::check name
	struct vstream_node *vsqNode;
	struct vstream_node *vcqNode;
	void *privdata;
	uint32_t info[SQCQ_RTS_INFO_LENGTH];
	int cu_id;
	struct xpu_group *group;
	int send_cnt;
	struct task_struct *p;
};

typedef int vstream_manage_t(struct vstream_args *arg);
int update_vstream_head(struct vstream_info *vstream_info, int num);
struct vstream_info *vstream_get_info(uint32_t id);
bool vstream_have_kernel(struct ucc_se *se);

#endif /* _LINUX_VSTREAM_H */
