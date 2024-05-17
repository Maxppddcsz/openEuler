// SPDX-License-Identifier: GPL-2.0-only
#include <linux/jump_label.h>
#include <linux/memblock.h>
#include <linux/swiotlb.h>
#include <linux/cc_platform.h>
#include <asm/tsi_tmm.h>
#include <asm/tsi_cmds.h>
#include <asm/tsi.h>

unsigned long prot_ns_shared;
EXPORT_SYMBOL(prot_ns_shared);

unsigned int phys_mask_shift = CONFIG_ARM64_PA_BITS;

DEFINE_STATIC_KEY_FALSE_RO(tsi_present);

static bool tsi_version_matches(void)
{
	unsigned long ver = tsi_get_version();

	if (ver == SMCCC_RET_NOT_SUPPORTED)
		return false;

	pr_info("RME: TSI version %lu.%lu advertised\n",
		TSI_ABI_VERSION_GET_MAJOR(ver),
		TSI_ABI_VERSION_GET_MINOR(ver));

	return (ver >= TSI_ABI_VERSION &&
		TSI_ABI_VERSION_GET_MAJOR(ver) == TSI_ABI_VERSION_MAJOR);
}


void arm64_setup_memory(void)
{
	if (!static_branch_unlikely(&tsi_present))
		return;
}

void __init arm64_tsi_init(void)
{
	if (!tsi_version_matches())
		return;

	static_branch_enable(&tsi_present);
}
