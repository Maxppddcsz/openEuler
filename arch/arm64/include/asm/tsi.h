/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_TSI_H_
#define __ASM_TSI_H_

#include <linux/jump_label.h>

extern struct static_key_false tsi_present;

void arm64_setup_memory(void);

void __init arm64_tsi_init(void);

static inline bool is_cvm_world(void)
{
	return static_branch_unlikely(&tsi_present);
}

#endif /* __ASM_TSI_H_ */
