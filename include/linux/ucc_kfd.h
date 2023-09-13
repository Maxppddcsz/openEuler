/* SPDX-License-Identifier: GPL-2.0 */

#ifndef KFD_PRIV_H_INCLUDED
#define KFD_PRIV_H_INCLUDED

#include <linux/mmu_notifier.h>
#include <linux/types.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/mmu_notifier.h>
#include <linux/idr.h>
#include <linux/dma-fence.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

struct process_queue_manager;
struct kfd_process;
struct kfd_signal_page;

struct process_queue_manager {
	struct kfd_process	*process;
	struct list_head	queues;
	unsigned long		*queue_slot_bitmap;
};

struct kfd_signal_page {
	uint64_t *kernel_address;
	uint64_t __user *user_address;
	bool need_to_free_pages;
};

/* Process data */
struct kfd_process {
	struct hlist_node kfd_processes;
	void *mm;
	struct kref ref;
	struct work_struct release_work;
	struct mutex mutex;
	struct task_struct *lead_thread;
	struct mmu_notifier mmu_notifier;
/* TODO: check if use right branch */
	struct rcu_head	rcu;
	uint16_t pasid;
	struct list_head per_device_data;
	struct process_queue_manager pqm;
	bool is_32bit_user_mode;
	struct mutex event_mutex;
	struct idr event_idr;
	struct kfd_signal_page *signal_page;
	size_t signal_mapped_size;
	size_t signal_event_count;
	bool signal_event_limit_reached;
/* TODO: check if use right branch */
	struct rb_root bo_interval_tree;
	void *kgd_process_info;
	struct dma_fence *ef;
	struct delayed_work eviction_work;
	struct delayed_work restore_work;
	unsigned int last_eviction_seqno;
	unsigned long last_restore_timestamp;
	unsigned long last_evict_timestamp;
	bool debug_trap_enabled;
	uint32_t trap_debug_wave_launch_mode;
	struct file *dbg_ev_file;
	uint32_t allocated_debug_watch_point_bitmask;
	struct kobject *kobj;
	struct kobject *kobj_queues;
	struct attribute attr_pasid;
	bool has_cwsr;
	uint64_t exception_enable_mask;
	uint64_t exception_status;
};

struct kfd_ioctl_create_queue_args {
	__u64 ring_base_address;	/* to KFD */
	__u64 write_pointer_address;	/* from KFD */
	__u64 read_pointer_address;	/* from KFD */
	__u64 doorbell_offset;	/* from KFD */

	__u32 ring_size;		/* to KFD */
	__u32 gpu_id;		/* to KFD */
	__u32 queue_type;		/* to KFD */
	__u32 queue_percentage;	/* to KFD */
	__u32 queue_priority;	/* to KFD */
	__u32 queue_id;		/* from KFD */

	__u64 eop_buffer_address;	/* to KFD */
	__u64 eop_buffer_size;	/* to KFD */
	__u64 ctx_save_restore_address; /* to KFD */
	__u32 ctx_save_restore_size;	/* to KFD */
	__u32 ctl_stack_size;		/* to KFD */
};

struct kfd_ioctl_destroy_queue_args {
	__u32 queue_id;		/* to KFD */
	__u32 pad;
};

struct kfd_ioctl_update_queue_args {
	__u64 ring_base_address;	/* to KFD */

	__u32 queue_id;		/* to KFD */
	__u32 ring_size;		/* to KFD */
	__u32 queue_percentage;	/* to KFD */
	__u32 queue_priority;	/* to KFD */
};
#endif
