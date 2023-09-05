/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2014-2023, Huawei.
 * Copyright (C) 2023, ISCAS.
 *	Author: Li Bin <huawei.libin@huawei.com>
 *	Author: Cheng Jian <cj.chengjian@huawei.com>
 *	Author: laokz <zhangkai@iscas.ac.cn>
 *
 * livepatch.h - riscv64-specific Kernel Live Patching Core
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ASM_RISCV64_LIVEPATCH_H
#define _ASM_RISCV64_LIVEPATCH_H

#include <linux/module.h>
#include <linux/livepatch.h>
#include <asm/cacheflush.h>

#define klp_smp_isb()  local_flush_icache_all()

/*
 * For arbitrary kernel address(PC offset might over/underflow +-2G), we
 * need store the new address and then use at least 3 instructors to jump
 * to. And with the alignment, we need 6 u32 space. Here C extension can't
 * help because it lacks scratch registers.
 * 	 0. 0x00000f97 // auipc   t6,0x0
 * 	 1. 0x010fbf83 // ld      t6,16(t6)  # t6 --> 4,5 new_addr
 * 	 2. 0x000f8067 // jr      t6
 * 	 3. not used
 * 	 4,5. new_addr
 */
#define LJMP_INSN_SIZE 6
struct arch_klp_data {
	u32 old_insns[LJMP_INSN_SIZE];
};

#define KLP_MAX_REPLACE_SIZE sizeof_field(struct arch_klp_data, old_insns)

#endif /* _ASM_RISCV64_LIVEPATCH_H */
