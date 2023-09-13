/* SPDX-License-Identifier: GPL-2.0 */

#ifndef TS_H
#define TS_H

#include <linux/file.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#define DEVDRV_MAX_SQ_DEPTH		(1024)
#define DEVDRV_SQ_SLOT_SIZE		(64)

#define DEVDRV_MAX_SQ_NUM		(512 - 1)
#define DEVDRV_MAX_CQ_NUM		(352 - 1)

#define DEVDRV_MAX_TS_NUM		(1)

#define REMAP_ALIGN_SIZE		(64 * 1024)
#define REMAP_ALIGN_MASK		(~(REMAP_ALIGN_SIZE - 1))
#define REMAP_ALIGN(x)			(((x) + REMAP_ALIGN_SIZE - 1) & \
					REMAP_ALIGN_MASK)

#define DEVDRV_DB_SPACE_SIZE		(1024 * 4096)

#define SQCQ_RTS_INFO_LENGTH		5
#define SQCQ_RESV_LENGTH		8

#define DEVDRV_CBCQ_MAX_GID		128

enum phy_sqcq_type {
	NORMAL_SQCQ_TYPE = 0,
	CALLBACK_SQCQ_TYPE,
	LOGIC_SQCQ_TYPE,
	SHM_SQCQ_TYPE,
	DFX_SQCQ_TYPE,
	TS_SQCQ_TYPE,
	KERNEL_SQCQ_TYPE,
};

struct notifier_operations {
	int (*notifier_call)(struct file *file_op, unsigned long mode);
};

#define MAX_DEVICE_COUNT 64

struct davinci_intf_stru {
	atomic_t count;
	struct mutex dmutex;
	struct cdev cdev;
	struct device *device;
	struct list_head process_list;
	struct list_head module_list;
	unsigned int device_status[MAX_DEVICE_COUNT];
	cpumask_var_t cpumask;
};

#define DAVINIC_MODULE_NAME_MAX		256
struct davinci_intf_private_stru {
	char module_name[DAVINIC_MODULE_NAME_MAX];
	unsigned int device_id;
	pid_t owner_pid;
	int close_flag;
	atomic_t work_count;
	int release_status;
	struct mutex fmutex;
	const struct file_operations fops;
	struct notifier_operations notifier;
	struct davinci_intf_stru *device_cb;
	struct file priv_filep;
	unsigned int free_type;
};

enum sqcq_alloc_status {
	SQCQ_INACTIVE = 0,
	SQCQ_ACTIVE
};

struct devdrv_ts_sq_info {
	enum phy_sqcq_type type;
	pid_t tgid;
	u32 head;
	u32 tail;
	u32 credit;
	u32 index;
	int uio_fd;

	u8 *uio_addr;
	int uio_size;

	enum sqcq_alloc_status alloc_status;
	u64 send_count;

	void *sq_sub;
};

struct devdrv_ts_cq_info {
	enum phy_sqcq_type type;
	pid_t tgid;
	u32 vfid;

	u32 head;
	u32 tail;
	u32 release_head;  /* runtime read cq head value */
	u32 index;
	u32 phase;
	u32 int_flag;

	int uio_fd;

	u8 *uio_addr;
	int uio_size;

	enum sqcq_alloc_status alloc_status;
	u64 receive_count;

	void *cq_sub;

	void (*complete_handle)(struct devdrv_ts_cq_info *cq_info);

	u8 slot_size;
};

#define DEVDRV_SQ_INFO_OCCUPY_SIZE \
	(sizeof(struct devdrv_ts_sq_info) * DEVDRV_MAX_SQ_NUM)
#define DEVDRV_CQ_INFO_OCCUPY_SIZE \
	(sizeof(struct devdrv_ts_cq_info) * DEVDRV_MAX_CQ_NUM)

#define DEVDRV_MAX_INFO_SIZE	\
	(DEVDRV_SQ_INFO_OCCUPY_SIZE + DEVDRV_CQ_INFO_OCCUPY_SIZE)
#define DEVDRV_VM_SQ_MEM_OFFSET		0
#define DEVDRV_VM_SQ_SLOT_SIZE	\
	REMAP_ALIGN(DEVDRV_MAX_SQ_DEPTH * DEVDRV_SQ_SLOT_SIZE)
#define DEVDRV_VM_SQ_MEM_SIZE	\
	(DEVDRV_VM_SQ_SLOT_SIZE * DEVDRV_MAX_SQ_NUM)

#define DEVDRV_VM_INFO_MEM_OFFSET	\
	(DEVDRV_VM_SQ_MEM_OFFSET + DEVDRV_VM_SQ_MEM_SIZE)
#define DEVDRV_VM_INFO_MEM_SIZE		REMAP_ALIGN(DEVDRV_MAX_INFO_SIZE)

#define DEVDRV_VM_DB_MEM_OFFSET	\
	(DEVDRV_VM_INFO_MEM_OFFSET + DEVDRV_VM_INFO_MEM_SIZE)
#define DEVDRV_VM_DB_MEM_SIZE		REMAP_ALIGN(DEVDRV_DB_SPACE_SIZE)

#define DEVDRV_VM_CQ_MEM_OFFSET	\
	(DEVDRV_VM_DB_MEM_OFFSET + DEVDRV_VM_DB_MEM_SIZE)

enum tsdrv_id_type {
	TSDRV_STREAM_ID,
	TSDRV_NOTIFY_ID,
	TSDRV_MODEL_ID,
	TSDRV_EVENT_SW_ID, /* should use for event alloc/free/inquiry res_num*/
	TSDRV_EVENT_HW_ID,
	TSDRV_IPC_EVENT_ID,
	TSDRV_SQ_ID,
	TSDRV_CQ_ID,
	TSDRV_PCQ_ID,
	TSDRV_MAX_ID,
};

#define TSDRV_CQ_REUSE 0x00000001
#define TSDRV_SQ_REUSE 0x00000002

struct normal_alloc_sqcq_para {
	uint32_t fd;
	uint32_t tsId;
	uint32_t devId;
	uint32_t sqeSize;
	uint32_t cqeSize;
	uint32_t sqeDepth;
	uint32_t cqeDepth;
	uint32_t grpId;
	uint32_t flag;
	uint32_t sqId;
	uint32_t cqId;
	uint32_t priority;
	uint32_t info[SQCQ_RTS_INFO_LENGTH];
	uint32_t res[SQCQ_RESV_LENGTH];
};

struct normal_free_sqcq_para {
	uint32_t tsId;
	uint32_t flag;
	uint32_t sqId;
	uint32_t cqId;
	uint32_t res[SQCQ_RESV_LENGTH];
};

struct tsdrv_sqcq_data_para {
	uint32_t id;
	uint32_t val;
};

struct devdrv_report_para {
	int timeout;
	u32 cq_tail;
	u32 cq_id;
};

struct tsdrv_ts_id_ctx {
	u32 id_num;
	struct list_head id_list;
	spinlock_t id_lock;
};
struct tsdrv_ts_ctx {
	u32 tsid;
	atomic_t status;
	u32 send_count;
	u64 receive_count;

	int32_t cq_tail_updated;
	wait_queue_head_t report_wait;

	struct work_struct recycle_work;

	wait_queue_head_t cbcq_wait[DEVDRV_CBCQ_MAX_GID];

	void *shm_sqcq_ctx;
	void *logic_sqcq_ctx;
	void *sync_cb_sqcq_ctx; // mini callback

	struct tsdrv_ts_id_ctx id_ctx[TSDRV_MAX_ID];

	/* only used by vm */
	u32 vcqid;
	u32 wait_queue_inited;
	u32 cq_report_status;
	int32_t cq_tail;
	spinlock_t ctx_lock;

	u32 recycle_cbsqcq_num; // min callback
};

//Context Delivers
struct tsdrv_ctx {
	u32 ctx_index;
	atomic_t status;
	atomic_t type;
	pid_t tgid;
	pid_t pid;
	int32_t ssid;
	u32 thread_bind_irq_num;
	u32 mirror_ctx_status;
	struct rb_node node;
	struct list_head list;
	struct vm_area_struct *vma[DEVDRV_MAX_TS_NUM];
	spinlock_t ctx_lock;
	struct mutex mutex_lock;
	struct tsdrv_ts_ctx ts_ctx[DEVDRV_MAX_TS_NUM];

	u64 unique_id; /* mark unique processes for vm */
};

#endif
