/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */
#ifndef __LINUX_PBHA_H
#define __LINUX_PBHA_H

#include <linux/libfdt.h>
#include <linux/pgtable.h>

#define PBHA_VAL_BIT0 1UL

#define EFI_OEMCONFIG_VARIABLE_GUID                                            \
	EFI_GUID(0x21f3b3c5, 0x946d, 0x41c1, 0x83, 0x8c, 0x19, 0x4e, 0x48,     \
		 0xaa, 0x41, 0xe2)

#define HBM_MODE_MEMORY	0
#define HBM_MODE_CACHE	1

#ifdef CONFIG_ARM64_PBHA
extern bool __ro_after_init pbha_bit0_enabled;
extern void __init early_pbha_bit0_init(void);

static inline bool system_support_pbha_bit0(void)
{
	return pbha_bit0_enabled;
}

static inline pgprot_t pbha_bit0_update_pgprot(pgprot_t prot)
{
	if (!system_support_pbha_bit0())
		return prot;

	return pgprot_pbha(prot, PBHA_VAL_BIT0);
}
#else
static inline bool system_support_pbha_bit0(void) { return false; }
static inline pgprot_t pbha_bit0_update_pgprot(pgprot_t prot) { return prot; }
#endif

#endif
