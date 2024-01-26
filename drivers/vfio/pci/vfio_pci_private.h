/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Red Hat, Inc.  All rights reserved.
 *     Author: Alex Williamson <alex.williamson@redhat.com>
 *
 * Derived from original vfio:
 * Copyright 2010 Cisco Systems, Inc.  All rights reserved.
 * Author: Tom Lyon, pugs@cisco.com
 */

#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/irqbypass.h>
#include <linux/types.h>
#include <linux/uuid.h>
#include <linux/notifier.h>

#ifndef VFIO_PCI_PRIVATE_H
#define VFIO_PCI_PRIVATE_H

#define VFIO_PCI_OFFSET_SHIFT   40

#define VFIO_PCI_OFFSET_TO_INDEX(off)	(off >> VFIO_PCI_OFFSET_SHIFT)
#define VFIO_PCI_INDEX_TO_OFFSET(index)	((u64)(index) << VFIO_PCI_OFFSET_SHIFT)
#define VFIO_PCI_OFFSET_MASK	(((u64)(1) << VFIO_PCI_OFFSET_SHIFT) - 1)

/* Special capability IDs predefined access */
#define PCI_CAP_ID_INVALID		0xFF	/* default raw access */
#define PCI_CAP_ID_INVALID_VIRT		0xFE	/* default virt access */

/* Cap maximum number of ioeventfds per device (arbitrary) */
#define VFIO_PCI_IOEVENTFD_MAX		1000

struct vfio_pci_ioeventfd {
	struct list_head	next;
	struct vfio_pci_device	*vdev;
	struct virqfd		*virqfd;
	void __iomem		*addr;
	uint64_t		data;
	loff_t			pos;
	int			bar;
	int			count;
	bool			test_mem;
};

struct vfio_pci_irq_ctx {
	struct eventfd_ctx	*trigger;
	struct virqfd		*unmask;
	struct virqfd		*mask;
	char			*name;
	bool			masked;
	struct irq_bypass_producer	producer;
};

struct vfio_pci_device;
struct vfio_pci_region;

struct vfio_pci_regops {
	size_t	(*rw)(struct vfio_pci_device *vdev, char __user *buf,
		      size_t count, loff_t *ppos, bool iswrite);
	void	(*release)(struct vfio_pci_device *vdev,
			   struct vfio_pci_region *region);
	int	(*mmap)(struct vfio_pci_device *vdev,
			struct vfio_pci_region *region,
			struct vm_area_struct *vma);
	int	(*add_capability)(struct vfio_pci_device *vdev,
				  struct vfio_pci_region *region,
				  struct vfio_info_cap *caps);
};

struct vfio_pci_region {
	u32				type;
	u32				subtype;
	const struct vfio_pci_regops	*ops;
	void				*data;
	size_t				size;
	u32				flags;
};

struct vfio_pci_dummy_resource {
	struct resource		resource;
	int			index;
	struct list_head	res_next;
};

struct vfio_pci_reflck {
	struct kref		kref;
	struct mutex		lock;
};

struct vfio_pci_vf_token {
	struct mutex		lock;
	uuid_t			uuid;
	int			users;
};

struct vfio_pci_mmap_vma {
	struct vm_area_struct	*vma;
	struct list_head	vma_next;
};

struct vfio_pci_vendor_driver {
	const struct vfio_pci_vendor_driver_ops *ops;
	struct list_head                        next;
};

#ifdef CONFIG_VFIO_PCI_IOV
#define VFIO_PCI_NUM_BARS	(PCI_STD_NUM_BARS + 1 + PCI_SRIOV_NUM_BARS)
#else
#define VFIO_PCI_NUM_BARS	PCI_STD_NUM_BARS
#endif

struct vfio_pci_device {
	struct pci_dev		*pdev;
	void __iomem		*barmap[VFIO_PCI_NUM_BARS];
	bool			bar_mmap_supported[VFIO_PCI_NUM_BARS];
	u8			*pci_config_map;
	u8			*vconfig;
	struct perm_bits	*msi_perm;
	spinlock_t		irqlock;
	struct mutex		igate;
	struct vfio_pci_irq_ctx	*ctx;
	int			num_ctx;
	int			irq_type;
	int			num_regions;
	int			num_vendor_regions;
	int			num_vendor_irqs;
	struct vfio_pci_region	*region;
	u8			msi_qmax;
	u8			msix_bar;
	u16			msix_size;
	u32			msix_offset;
	u32			rbar[7];
	bool			pci_2_3;
	bool			virq_disabled;
	bool			reset_works;
	bool			extended_caps;
	bool			bardirty;
	bool			has_vga;
	bool			needs_reset;
	bool			nointx;
	bool			needs_pm_restore;
	struct pci_saved_state	*pci_saved_state;
	struct pci_saved_state	*pm_save;
	struct vfio_pci_reflck	*reflck;
	int			refcnt;
	int			ioeventfds_nr;
	struct eventfd_ctx	*err_trigger;
	struct eventfd_ctx	*req_trigger;
	struct list_head	dummy_resources_list;
	struct mutex		ioeventfds_lock;
	struct list_head	ioeventfds_list;
	struct vfio_pci_vf_token	*vf_token;
	struct notifier_block	nb;
	struct mutex		vma_lock;
	struct list_head	vma_list;
	struct rw_semaphore	memory_lock;
	void			*vendor_data;
	struct vfio_pci_vendor_driver	*vendor_driver;
};

#define is_intx(vdev) (vdev->irq_type == VFIO_PCI_INTX_IRQ_INDEX)
#define is_msi(vdev) (vdev->irq_type == VFIO_PCI_MSI_IRQ_INDEX)
#define is_msix(vdev) (vdev->irq_type == VFIO_PCI_MSIX_IRQ_INDEX)
#define is_irq_none(vdev) (!(is_intx(vdev) || is_msi(vdev) || is_msix(vdev)))
#define irq_is(vdev, type) (vdev->irq_type == type)

extern void vfio_pci_intx_mask(struct vfio_pci_device *vdev);
extern void vfio_pci_intx_unmask(struct vfio_pci_device *vdev);

extern int vfio_pci_set_irqs_ioctl(struct vfio_pci_device *vdev,
				   uint32_t flags, unsigned index,
				   unsigned start, unsigned count, void *data);

extern int vfio_default_config_read(struct vfio_pci_device *vdev, int pos,
				    int count, struct perm_bits *perm,
				    int offset, __le32 *val);
extern int vfio_default_config_write(struct vfio_pci_device *vdev, int pos,
				     int count, struct perm_bits *perm,
				     int offset, __le32 val);
extern ssize_t vfio_pci_config_rw(struct vfio_pci_device *vdev,
				  char __user *buf, size_t count,
				  loff_t *ppos, bool iswrite);

extern ssize_t vfio_pci_bar_rw(struct vfio_pci_device *vdev, char __user *buf,
			       size_t count, loff_t *ppos, bool iswrite);

extern ssize_t vfio_pci_vga_rw(struct vfio_pci_device *vdev, char __user *buf,
			       size_t count, loff_t *ppos, bool iswrite);

extern long vfio_pci_ioeventfd(struct vfio_pci_device *vdev, loff_t offset,
			       uint64_t data, int count, int fd);

/*
 * Read/Write Permission Bits - one bit for each bit in capability
 * Any field can be read if it exists, but what is read depends on
 * whether the field is 'virtualized', or just pass thru to the
 * hardware.  Any virtualized field is also virtualized for writes.
 * Writes are only permitted if they have a 1 bit here.
 */
struct perm_bits {
	u8	*virt;		/* read/write virtual data, not hw */
	u8	*write;		/* writeable bits */
	int	(*readfn)(struct vfio_pci_device *vdev, int pos, int count,
			  struct perm_bits *perm, int offset, __le32 *val);
	int	(*writefn)(struct vfio_pci_device *vdev, int pos, int count,
			   struct perm_bits *perm, int offset, __le32 val);
};

#define	NO_VIRT		0
#define	ALL_VIRT	0xFFFFFFFFU
#define	NO_WRITE	0
#define	ALL_WRITE	0xFFFFFFFFU

/*
 * Helper functions for filling in permission tables
 */
static inline void p_setb(struct perm_bits *p, int off, u8 virt, u8 write)
{
	p->virt[off] = virt;
	p->write[off] = write;
}

/* Handle endian-ness - pci and tables are little-endian */
static inline void p_setw(struct perm_bits *p, int off, u16 virt, u16 write)
{
	*(__le16 *)(&p->virt[off]) = cpu_to_le16(virt);
	*(__le16 *)(&p->write[off]) = cpu_to_le16(write);
}

/* Handle endian-ness - pci and tables are little-endian */
static inline void p_setd(struct perm_bits *p, int off, u32 virt, u32 write)
{
	*(__le32 *)(&p->virt[off]) = cpu_to_le32(virt);
	*(__le32 *)(&p->write[off]) = cpu_to_le32(write);
}

extern void free_perm_bits(struct perm_bits *perm);
extern int alloc_perm_bits(struct perm_bits *perm, int size);

extern int vfio_pci_init_perm_bits(void);
extern void vfio_pci_uninit_perm_bits(void);

extern __le32 vfio_generate_bar_flags(struct pci_dev *pdev, int bar);
extern int vfio_find_cap_start(struct vfio_pci_device *vdev, int pos);

extern int vfio_config_init(struct vfio_pci_device *vdev);
extern void vfio_config_free(struct vfio_pci_device *vdev);

extern void vfio_pci_release_region(struct vfio_pci_device *vdev,
				    unsigned int index);

extern int vfio_pci_register_dev_region(struct vfio_pci_device *vdev,
					unsigned int type, unsigned int subtype,
					const struct vfio_pci_regops *ops,
					size_t size, u32 flags, void *data);

extern int vfio_pci_set_power_state(struct vfio_pci_device *vdev,
				    pci_power_t state);

extern bool __vfio_pci_memory_enabled(struct vfio_pci_device *vdev);
extern void vfio_pci_zap_and_down_write_memory_lock(struct vfio_pci_device
						    *vdev);
extern u16 vfio_pci_memory_lock_and_enable(struct vfio_pci_device *vdev);
extern void vfio_pci_memory_unlock_and_restore(struct vfio_pci_device *vdev,
					       u16 cmd);

extern void vfio_pci_probe_one_mmap(struct vfio_pci_device *vdev, int bar);
extern int vfio_pci_mmap_region(struct vfio_pci_device *vdev, unsigned int index,
				struct vm_area_struct *vma);

#ifdef CONFIG_VFIO_PCI_IOV
extern void vfio_pci_probe_vf_bar_mmaps(struct vfio_pci_device *vdev);
extern int vfio_pci_sriov_region_init(struct vfio_pci_device *vdev);
extern int __init init_pci_ext_cap_sriov_perm(struct perm_bits *perm);
extern void vfio_pci_uninit_sriov_perm(struct perm_bits *perms);
#else
static inline void vfio_pci_probe_vf_bar_mmaps(struct vfio_pci_device *vdev)
{
}

static inline int vfio_pci_sriov_region_init(struct vfio_pci_device *vdev)
{
	return -ENODEV;
}

static inline int init_pci_ext_cap_sriov_perm(struct perm_bits *perm)
{
	return 0;
}

static inline void vfio_pci_uninit_sriov_perm(struct perm_bits *perms)
{
}
#endif

#ifdef CONFIG_VFIO_PCI_IGD
extern int vfio_pci_igd_init(struct vfio_pci_device *vdev);
#else
static inline int vfio_pci_igd_init(struct vfio_pci_device *vdev)
{
	return -ENODEV;
}
#endif
#ifdef CONFIG_VFIO_PCI_NVLINK2
extern int vfio_pci_nvdia_v100_nvlink2_init(struct vfio_pci_device *vdev);
extern int vfio_pci_ibm_npu2_init(struct vfio_pci_device *vdev);
#else
static inline int vfio_pci_nvdia_v100_nvlink2_init(struct vfio_pci_device *vdev)
{
	return -ENODEV;
}

static inline int vfio_pci_ibm_npu2_init(struct vfio_pci_device *vdev)
{
	return -ENODEV;
}
#endif

#ifdef CONFIG_VFIO_PCI_ZDEV
extern int vfio_pci_info_zdev_add_caps(struct vfio_pci_device *vdev,
				       struct vfio_info_cap *caps);
#else
static inline int vfio_pci_info_zdev_add_caps(struct vfio_pci_device *vdev,
					      struct vfio_info_cap *caps)
{
	return -ENODEV;
}
#endif

#endif /* VFIO_PCI_PRIVATE_H */
