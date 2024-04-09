/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024, The Linux Foundation. All rights reserved.
 */
#ifndef __ASM_CVM_GUEST_H
#define __ASM_CVM_GUEST_H

#ifdef CONFIG_CVM_GUEST
static inline bool cvm_mem_encrypt_active(void)
{
	return false;
}

int set_cvm_memory_encrypted(unsigned long addr, int numpages);

int set_cvm_memory_decrypted(unsigned long addr, int numpages);

bool is_cvm_world(void);

#endif
#endif
