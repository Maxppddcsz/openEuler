// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/pci.h>
#include <linux/vfio.h>

#include "vfio_pci_private.h"

void vfio_pci_probe_vf_bar_mmaps(struct vfio_pci_device *vdev)
{
	int i;

	if (!dev_is_pf(&vdev->pdev->dev))
		return;

	for (i = 0; i < PCI_SRIOV_NUM_BARS; i++) {
		int bar = i + PCI_IOV_RESOURCES;

		vfio_pci_probe_one_mmap(vdev, bar);
	}
}

static size_t vfio_pci_vf_bar_rw(struct vfio_pci_device *vdev, char __user *buf,
				 size_t count, loff_t *ppos, bool iswrite)
{
	return vfio_pci_bar_rw(vdev, buf, count, ppos, iswrite);
}

static void vfio_pci_vf_bar_release(struct vfio_pci_device *vdev,
				    struct vfio_pci_region *region)
{
	unsigned int index = (unsigned int)(unsigned long)region->data;

	vfio_pci_release_region(vdev, index);
}

static int vfio_pci_vf_bar_mmap(struct vfio_pci_device *vdev,
				struct vfio_pci_region *region,
				struct vm_area_struct *vma)
{
	unsigned int index = (unsigned int)(unsigned long)region->data;

	return vfio_pci_mmap_region(vdev, index, vma);
}

static const struct vfio_pci_regops vfio_pci_sriov_regops = {
	.rw		= vfio_pci_vf_bar_rw,
	.release	= vfio_pci_vf_bar_release,
	.mmap		= vfio_pci_vf_bar_mmap,
};

int vfio_pci_sriov_region_init(struct vfio_pci_device *vdev)
{
	struct pci_dev *pdev = vdev->pdev;
	unsigned int i;
	int ret;

	if (!dev_is_pf(&pdev->dev))
		return -ENODEV;

	for (i = 0; i < PCI_SRIOV_NUM_BARS; i++) {
		size_t size;
		u32 flags;

		size = pci_resource_len(pdev, i + PCI_IOV_RESOURCES);
		if (!size)
			flags = 0;
		else
			flags = VFIO_REGION_INFO_FLAG_READ |
				VFIO_REGION_INFO_FLAG_WRITE |
				VFIO_REGION_INFO_FLAG_MMAP;

		ret = vfio_pci_register_dev_region(vdev,
			VFIO_REGION_TYPE_SRIOV_VF_BAR, i,
			&vfio_pci_sriov_regops, size, flags,
			(void *)(unsigned long)(i + PCI_IOV_RESOURCES));
		if (ret)
			return ret;
	}

	return 0;
}

static void vfio_sriov_bar_fixup(struct vfio_pci_device *vdev,
				 int sriov_cap_start)
{
	struct pci_dev *pdev = vdev->pdev;
	int i;
	__le32 *bar;
	u64 mask;

	bar = (__le32 *)&vdev->vconfig[sriov_cap_start + PCI_SRIOV_BAR];
	for (i = PCI_IOV_RESOURCES; i <= PCI_IOV_RESOURCE_END; i++, bar++) {
		if (!pci_resource_start(pdev, i)) {
			*bar = 0; /* Unmapped by host = unimplemented to user */
			continue;
		}

		mask = ~(pci_iov_resource_size(pdev, i) - 1);

		*bar &= cpu_to_le32((u32)mask);
		*bar |= vfio_generate_bar_flags(pdev, i);

		if (*bar & cpu_to_le32(PCI_BASE_ADDRESS_MEM_TYPE_64)) {
			bar++;
			*bar &= cpu_to_le32((u32)(mask >> 32));
			i++;
		}
	}
}

static int vfio_sriov_cap_config_read(struct vfio_pci_device *vdev, int pos,
				      int count, struct perm_bits *perm,
				      int offset, __le32 *val)
{
	int cap_start = vfio_find_cap_start(vdev, pos);

	vfio_sriov_bar_fixup(vdev, cap_start);
	return vfio_default_config_read(vdev, pos, count, perm, offset, val);
}

static int vfio_sriov_cap_config_write(struct vfio_pci_device *vdev, int pos,
				       int count, struct perm_bits *perm,
				       int offset, __le32 val)
{
	int cap_start = vfio_find_cap_start(vdev, pos);
	u16 sriov_ctrl = *(u16 *)(vdev->vconfig + cap_start + PCI_SRIOV_CTRL);
	int ret;
	bool cur_vf_enabled = sriov_ctrl & PCI_SRIOV_CTRL_VFE;
	bool vf_enabled;

	switch (offset) {
	case  PCI_SRIOV_NUM_VF:
		/*
		 * Per SR-IOV spec sec 3.3.10 and 3.3.11, First VF Offset
		 * and VF Stride may change when NumVFs changes.
		 *
		 * Therefore we should pass valid writes to the hardware.
		 *
		 * Per SR-IOV spec sec 3.3.7
		 * The results are undefined if NumVFs is set to a value greater
		 * than TotalVFs.
		 * NumVFs may only be written while VF Enable is Clear.
		 * If NumVFs is written when VF Enable is Set, the results
		 * are undefined.

		 * Avoid passing such writes to the Hardware just in case.
		 */
		device_lock(&vdev->pdev->dev);
		if (pci_num_vf(vdev->pdev) ||
		    val > pci_sriov_get_totalvfs(vdev->pdev)) {
			device_unlock(&vdev->pdev->dev);
			return count;
		}
		pci_iov_set_numvfs(vdev->pdev, val);
		device_unlock(&vdev->pdev->dev);
		break;
	case PCI_SRIOV_CTRL:
		vf_enabled = val & PCI_SRIOV_CTRL_VFE;
		ret = 0;

		if (!cur_vf_enabled && vf_enabled) {
			u16 num_vfs = *(u16 *)(vdev->vconfig +
					cap_start  + PCI_SRIOV_NUM_VF);
			device_lock(&vdev->pdev->dev);
			ret = pci_enable_sriov(vdev->pdev, num_vfs);
			device_unlock(&vdev->pdev->dev);
		} else if (cur_vf_enabled && !vf_enabled) {
			device_lock(&vdev->pdev->dev);
			pci_disable_sriov(vdev->pdev);
			device_unlock(&vdev->pdev->dev);
		}
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	return vfio_default_config_write(vdev, pos, count, perm,
					 offset, val);
}

int __init init_pci_ext_cap_sriov_perm(struct perm_bits *perm)
{
	int i;

	if (alloc_perm_bits(perm, PCI_EXT_CAP_SRIOV_SIZEOF))
		return -ENOMEM;

	/*
	 * Virtualize the first dword of all express capabilities
	 * because it includes the next pointer.  This lets us later
	 * remove capabilities from the chain if we need to.
	 */
	p_setd(perm, 0, ALL_VIRT, NO_WRITE);

	/*
	 * VF Enable - Virtualized and writable
	 * Memory Space Enable - Non-virtualized and writable
	 */
	p_setw(perm, PCI_SRIOV_CTRL, PCI_SRIOV_CTRL_VFE,
	       PCI_SRIOV_CTRL_VFE | PCI_SRIOV_CTRL_MSE);

	p_setw(perm, PCI_SRIOV_NUM_VF, (u16)ALL_VIRT, (u16)ALL_WRITE);
	p_setw(perm, PCI_SRIOV_SUP_PGSIZE, (u16)ALL_VIRT, NO_WRITE);

	/*
	 * We cannot let user space application change the page size
	 * so we mark it as read only and trust the user application
	 * (e.g. qemu) to virtualize this correctly for the guest
	 */
	p_setw(perm, PCI_SRIOV_SYS_PGSIZE, (u16)ALL_VIRT, NO_WRITE);

	for (i = 0; i < PCI_SRIOV_NUM_BARS; i++)
		p_setd(perm, PCI_SRIOV_BAR + 4 * i, ALL_VIRT, ALL_WRITE);

	perm->readfn = vfio_sriov_cap_config_read;
	perm->writefn = vfio_sriov_cap_config_write;

	return 0;
}

void vfio_pci_uninit_sriov_perm(struct perm_bits *perm)
{
	free_perm_bits(perm);
}
