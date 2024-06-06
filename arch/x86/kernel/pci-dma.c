// SPDX-License-Identifier: GPL-2.0
#include <linux/dma-direct.h>
#include <linux/dma-debug.h>
#include <linux/dmar.h>
#include <linux/export.h>
#include <linux/bootmem.h>
#include <linux/gfp.h>
#include <linux/pci.h>

#include <asm/proto.h>
#include <asm/dma.h>
#include <asm/iommu.h>
#include <asm/gart.h>
#include <asm/calgary.h>
#include <asm/x86_init.h>
#include <asm/iommu_table.h>

static bool disable_dac_quirk __read_mostly;

const struct dma_map_ops *dma_ops = &dma_direct_ops;
EXPORT_SYMBOL(dma_ops);

#ifdef CONFIG_IOMMU_DEBUG
int panic_on_overflow __read_mostly = 1;
int force_iommu __read_mostly = 1;
#else
int panic_on_overflow __read_mostly = 0;
int force_iommu __read_mostly = 0;
#endif

int iommu_merge __read_mostly = 0;

int no_iommu __read_mostly;
/* Set this to 1 if there is a HW IOMMU in the system */
int iommu_detected __read_mostly = 0;

/*
 * This variable becomes 1 if iommu=pt is passed on the kernel command line.
 * If this variable is 1, IOMMU implementations do no DMA translation for
 * devices and allow every device to access to whole physical memory. This is
 * useful if a user wants to use an IOMMU only for KVM device assignment to
 * guests and not for driver dma translation.
 * It is also possible to disable by default in kernel config, and enable with
 * iommu=nopt at boot time.
 */
#ifdef CONFIG_IOMMU_DEFAULT_PASSTHROUGH
int iommu_pass_through __read_mostly = 1;
#else
int iommu_pass_through __read_mostly;
#endif

extern struct iommu_table_entry __iommu_table[], __iommu_table_end[];

/* Dummy device used for NULL arguments (normally ISA). */
struct device x86_dma_fallback_dev = {
	.init_name = "fallback device",
	.coherent_dma_mask = ISA_DMA_BIT_MASK,
	.dma_mask = &x86_dma_fallback_dev.coherent_dma_mask,
};
EXPORT_SYMBOL(x86_dma_fallback_dev);

void __init pci_iommu_alloc(void)
{
	struct iommu_table_entry *p;

	sort_iommu_table(__iommu_table, __iommu_table_end);
	check_iommu_entries(__iommu_table, __iommu_table_end);

	for (p = __iommu_table; p < __iommu_table_end; p++) {
		if (p && p->detect && p->detect() > 0) {
			p->flags |= IOMMU_DETECTED;
			if (p->early_init)
				p->early_init();
			if (p->flags & IOMMU_FINISH_IF_DETECTED)
				break;
		}
	}
}

bool arch_dma_alloc_attrs(struct device **dev)
{
	if (!*dev)
		*dev = &x86_dma_fallback_dev;

	if (!is_device_dma_capable(*dev))
		return false;
	return true;

}
EXPORT_SYMBOL(arch_dma_alloc_attrs);

/*
 * See <Documentation/x86/x86_64/boot-options.txt> for the iommu kernel
 * parameter documentation.
 */
static __init int iommu_setup(char *p)
{
	iommu_merge = 1;

	if (!p)
		return -EINVAL;

	while (*p) {
		if (!strncmp(p, "off", 3))
			no_iommu = 1;
		/* gart_parse_options has more force support */
		if (!strncmp(p, "force", 5))
			force_iommu = 1;
		if (!strncmp(p, "noforce", 7)) {
			iommu_merge = 0;
			force_iommu = 0;
		}

		if (!strncmp(p, "biomerge", 8)) {
			iommu_merge = 1;
			force_iommu = 1;
		}
		if (!strncmp(p, "panic", 5))
			panic_on_overflow = 1;
		if (!strncmp(p, "nopanic", 7))
			panic_on_overflow = 0;
		if (!strncmp(p, "merge", 5)) {
			iommu_merge = 1;
			force_iommu = 1;
		}
		if (!strncmp(p, "nomerge", 7))
			iommu_merge = 0;
		if (!strncmp(p, "forcesac", 8))
			pr_warn("forcesac option ignored.\n");
		if (!strncmp(p, "allowdac", 8))
			pr_warn("allowdac option ignored.\n");
		if (!strncmp(p, "nodac", 5))
			pr_warn("nodac option ignored.\n");
		if (!strncmp(p, "usedac", 6)) {
			disable_dac_quirk = true;
			return 1;
		}
#ifdef CONFIG_SWIOTLB
		if (!strncmp(p, "soft", 4))
			swiotlb = 1;
#endif
		if (!strncmp(p, "pt", 2))
			iommu_pass_through = 1;
		if (!strncmp(p, "nopt", 4))
			iommu_pass_through = 0;

		gart_parse_options(p);

#ifdef CONFIG_CALGARY_IOMMU
		if (!strncmp(p, "calgary", 7))
			use_calgary = 1;
#endif /* CONFIG_CALGARY_IOMMU */

		p += strcspn(p, ",");
		if (*p == ',')
			++p;
	}
	return 0;
}
early_param("iommu", iommu_setup);

static int __init pci_iommu_init(void)
{
	struct iommu_table_entry *p;

	x86_init.iommu.iommu_init();

	for (p = __iommu_table; p < __iommu_table_end; p++) {
		if (p && (p->flags & IOMMU_DETECTED) && p->late_init)
			p->late_init();
	}

	return 0;
}
/* Must execute after PCI subsystem */
rootfs_initcall(pci_iommu_init);

#ifdef CONFIG_PCI
/***
 * usage:
 *  set "zx_p2cw_filter_state=0|1" in cmdline
 * value description:
 *  bit 0: enable(1) node check or not(0). default 1
 */
u8 zx_p2cw_patch_filter = ZX_P2CW_PARAMS_DEFAULT;
static int __init zx_p2cw_patch_filter_setup(char *str)
{
	u8 filter_state = *((u8 *)str) - 0x30;

	pr_info("kh-40000 p2cw patch: node check is %s\n",
		(filter_state & 1) ? "enabled" : "disabled");
	zx_p2cw_patch_filter = filter_state;

	return 1;
}
__setup("zx_p2cw_filter_state=", zx_p2cw_patch_filter_setup);

static struct pci_dev *kh40000_get_pci_dev(struct device *dev)
{
	if (dev_is_pci(dev))
		return to_pci_dev(dev);

	if (dev->parent)
		return kh40000_get_pci_dev(dev->parent);

	return NULL;
}

void kh40000_sync_single_dma_for_cpu(struct device *dev, dma_addr_t paddr,
		enum dma_data_direction dir, bool is_iommu)
{
	u8 vid;
	struct pci_dev *pci;
	u64 dma_mask = *dev->dma_mask;

	/* check direction */
	if ((dir != DMA_FROM_DEVICE) && (dir != DMA_BIDIRECTIONAL))
		return;

	/* check dma capability */
	if (dma_mask <= DMA_BIT_MASK(32))
		return;

	/* check device type */
	pci = kh40000_get_pci_dev(dev);
	if (pci == NULL)
		return;

	/* get real physical address */
	if (is_iommu)
		paddr = kh40000_iommu_iova_to_phys(dev, paddr);

	/* check node or not */
	if ((zx_p2cw_patch_filter & ZX_P2CW_PARAM_NODE_CHECK)) {
		unsigned long pfn = PFN_DOWN(paddr);

		if (pfn_to_nid(pfn) == dev_to_node(dev))
			return;
	}

	pci_read_config_byte(pci, PCI_VENDOR_ID, &vid);
}

/* Many VIA bridges seem to corrupt data for DAC. Disable it here */

static int via_no_dac_cb(struct pci_dev *pdev, void *data)
{
	pdev->dev.bus_dma_mask = DMA_BIT_MASK(32);
	return 0;
}

static void via_no_dac(struct pci_dev *dev)
{
	if (!disable_dac_quirk) {
		dev_info(&dev->dev, "disabling DAC on VIA PCI bridge\n");
		pci_walk_bus(dev->subordinate, via_no_dac_cb, NULL);
	}
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_VIA, PCI_ANY_ID,
				PCI_CLASS_BRIDGE_PCI, 8, via_no_dac);
#endif
