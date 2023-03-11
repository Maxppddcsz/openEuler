/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Huawei Technologies Co., Ltd. All rights reserved.
 */

#ifndef VFIO_PCI_MIGRATION_H
#define VFIO_PCI_MIGRATION_H

#include <linux/types.h>
#include <linux/pci.h>

#define VFIO_REGION_TYPE_MIGRATION (3)
/* sub-types for VFIO_REGION_TYPE_MIGRATION */
#define VFIO_REGION_SUBTYPE_MIGRATION (1)

#define VFIO_MIGRATION_BUFFER_MAX_SIZE SZ_256K
#define VFIO_MIGRATION_REGION_DATA_OFFSET \
	(sizeof(struct vfio_device_migration_info))
#define VFIO_DEVICE_MIGRATION_OFFSET(x) \
	offsetof(struct vfio_device_migration_info, x)

struct vfio_device_migration_info {
	__u32 device_state; /* VFIO device state */
#define VFIO_DEVICE_STATE_STOP (0)
#define VFIO_DEVICE_STATE_RUNNING (1 << 0)
#define VFIO_DEVICE_STATE_SAVING (1 << 1)
#define VFIO_DEVICE_STATE_RESUMING (1 << 2)
#define VFIO_DEVICE_STATE_MASK (VFIO_DEVICE_STATE_RUNNING | \
	VFIO_DEVICE_STATE_SAVING | VFIO_DEVICE_STATE_RESUMING)
	__u32 reserved;

	__u32 device_cmd;
	__u32 version_id;

	__u64 pending_bytes;
	__u64 data_offset;
	__u64 data_size;
};

enum {
	VFIO_DEVICE_STOP = 0xffff0001,
	VFIO_DEVICE_CONTINUE,
	VFIO_DEVICE_MIGRATION_CANCEL,
};

struct vfio_log_buf_sge {
	__u64 len;
	__u64 addr;
};

struct vfio_log_buf_info {
	__u32 uuid;
	__u64 buffer_size;
	__u64 addrs_size;
	__u64 frag_size;
	struct vfio_log_buf_sge *sgevec;
};

struct vfio_log_buf_ctl {
	__u32 argsz;
	__u32 flags;
	#define VFIO_DEVICE_LOG_BUF_FLAG_SETUP (1 << 0)
	#define VFIO_DEVICE_LOG_BUF_FLAG_RELEASE (1 << 1)
	#define VFIO_DEVICE_LOG_BUF_FLAG_START	(1 << 2)
	#define VFIO_DEVICE_LOG_BUF_FLAG_STOP (1 << 3)
	#define VFIO_DEVICE_LOG_BUF_FLAG_STATUS_QUERY (1 << 4)
	void *data;
};
#define VFIO_LOG_BUF_CTL _IO(VFIO_TYPE, VFIO_BASE + 21)
#define VFIO_GET_LOG_BUF_FD _IO(VFIO_TYPE, VFIO_BASE + 22)
#define VFIO_DEVICE_LOG_BUF_CTL _IO(VFIO_TYPE, VFIO_BASE + 23)

struct vf_migration_log_info {
	__u32 dom_uuid;
	__u64 buffer_size;
	__u64 sge_len;
	__u64 sge_num;
	struct vfio_log_buf_sge *sgevec;
};

struct vfio_device_migration_ops {
	/* Get device information */
	int (*get_info)(struct pci_dev *pdev,
		struct vfio_device_migration_info *info);
	/* Enable a vf device */
	int (*enable)(struct pci_dev *pdev);
	/* Disable a vf device */
	int (*disable)(struct pci_dev *pdev);
	/* Save a vf device */
	int (*save)(struct pci_dev *pdev, void *base,
		uint64_t off, uint64_t count);
	/* Resuming a vf device */
	int (*restore)(struct pci_dev *pdev, void *base,
		uint64_t off, uint64_t count);
	/* Log start a vf device */
	int (*log_start)(struct pci_dev *pdev,
		struct vf_migration_log_info *log_info);
	/* Log stop a vf device */
	int (*log_stop)(struct pci_dev *pdev, uint32_t uuid);
	/* Get vf device log status */
	int (*get_log_status)(struct pci_dev *pdev);
	/* Pre enable a vf device(load_setup, before restore a vf) */
	int (*pre_enable)(struct pci_dev *pdev);
	/* Cancel a vf device when live migration failed (rollback) */
	int (*cancel)(struct pci_dev *pdev);
	/* Init a vf device */
	int (*init)(struct pci_dev *pdev);
	/* Uninit a vf device */
	void (*uninit)(struct pci_dev *pdev);
	/* Release a vf device */
	void (*release)(struct pci_dev *pdev);
};

struct vfio_pci_vendor_mig_driver {
	struct pci_dev *pdev;
	unsigned char bus_num;
	struct vfio_device_migration_ops *dev_mig_ops;
	struct module *owner;
	atomic_t count;
	struct list_head list;
};

struct vfio_pci_migration_data {
	u64 state_size;
	struct pci_dev *vf_dev;
	struct vfio_pci_vendor_mig_driver *mig_driver;
	struct vfio_device_migration_info *mig_ctl;
	void *vf_data;
};

int vfio_pci_register_migration_ops(struct vfio_device_migration_ops *ops,
	struct module *mod, struct pci_dev *pdev);
void vfio_pci_unregister_migration_ops(struct module *mod,
	struct pci_dev *pdev);

#endif /* VFIO_PCI_MIGRATION_H */
