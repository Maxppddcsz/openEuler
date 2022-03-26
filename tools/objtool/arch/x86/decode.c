// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2015 Josh Poimboeuf <jpoimboe@redhat.com>
 */

#include <stdio.h>
#include <stdlib.h>

#define unlikely(cond) (cond)
#include <asm/insn.h>
#include <asm/orc_types.h>
#include <linux/static_call_types.h>

#include "../../../arch/x86/lib/inat.c"
#include "../../../arch/x86/lib/insn.c"

#include "../../check.h"
#include "../../elf.h"
#include "../../arch.h"
#include "../../warn.h"

static unsigned char op_to_cfi_reg[][2] = {
	{CFI_AX, CFI_R8},
	{CFI_CX, CFI_R9},
	{CFI_DX, CFI_R10},
	{CFI_BX, CFI_R11},
	{CFI_SP, CFI_R12},
	{CFI_BP, CFI_R13},
	{CFI_SI, CFI_R14},
	{CFI_DI, CFI_R15},
};

static int is_x86_64(const struct elf *elf)
{
	switch (elf->ehdr.e_machine) {
	case EM_X86_64:
		return 1;
	case EM_386:
		return 0;
	default:
		WARN("unexpected ELF machine type %d", elf->ehdr.e_machine);
		return -1;
	}
}

static int update_cfi_state_regs(struct instruction *insn,
				  struct cfi_state *cfi,
				  struct stack_op *op)
{
	struct cfi_reg *cfa = &cfi->cfa;

	if (cfa->base != CFI_SP && cfa->base != CFI_SP_INDIRECT)
		return 0;

	/* push */
	if (op->dest.type == OP_DEST_PUSH || op->dest.type == OP_DEST_PUSHF)
		cfa->offset += 8;

	/* pop */
	if (op->src.type == OP_SRC_POP || op->src.type == OP_SRC_POPF)
		cfa->offset -= 8;

	/* add immediate to sp */
	if (op->dest.type == OP_DEST_REG && op->src.type == OP_SRC_ADD &&
	    op->dest.reg == CFI_SP && op->src.reg == CFI_SP)
		cfa->offset -= op->src.offset;

	return 0;
}

/*
 * A note about DRAP stack alignment:
 *
 * GCC has the concept of a DRAP register, which is used to help keep track of
 * the stack pointer when aligning the stack.  r10 or r13 is used as the DRAP
 * register.  The typical DRAP pattern is:
 *
 *   4c 8d 54 24 08		lea    0x8(%rsp),%r10
 *   48 83 e4 c0		and    $0xffffffffffffffc0,%rsp
 *   41 ff 72 f8		pushq  -0x8(%r10)
 *   55				push   %rbp
 *   48 89 e5			mov    %rsp,%rbp
 *				(more pushes)
 *   41 52			push   %r10
 *				...
 *   41 5a			pop    %r10
 *				(more pops)
 *   5d				pop    %rbp
 *   49 8d 62 f8		lea    -0x8(%r10),%rsp
 *   c3				retq
 *
 * There are some variations in the epilogues, like:
 *
 *   5b				pop    %rbx
 *   41 5a			pop    %r10
 *   41 5c			pop    %r12
 *   41 5d			pop    %r13
 *   41 5e			pop    %r14
 *   c9				leaveq
 *   49 8d 62 f8		lea    -0x8(%r10),%rsp
 *   c3				retq
 *
 * and:
 *
 *   4c 8b 55 e8		mov    -0x18(%rbp),%r10
 *   48 8b 5d e0		mov    -0x20(%rbp),%rbx
 *   4c 8b 65 f0		mov    -0x10(%rbp),%r12
 *   4c 8b 6d f8		mov    -0x8(%rbp),%r13
 *   c9				leaveq
 *   49 8d 62 f8		lea    -0x8(%r10),%rsp
 *   c3				retq
 *
 * Sometimes r13 is used as the DRAP register, in which case it's saved and
 * restored beforehand:
 *
 *   41 55			push   %r13
 *   4c 8d 6c 24 10		lea    0x10(%rsp),%r13
 *   48 83 e4 f0		and    $0xfffffffffffffff0,%rsp
 *				...
 *   49 8d 65 f0		lea    -0x10(%r13),%rsp
 *   41 5d			pop    %r13
 *   c3				retq
 */
static int update_cfi_state(struct instruction *insn, struct cfi_state *cfi,
			     struct stack_op *op)
{
	struct cfi_reg *cfa = &cfi->cfa;
	struct cfi_reg *regs = cfi->regs;

	/* stack operations don't make sense with an undefined CFA */
	if (cfa->base == CFI_UNDEFINED) {
		if (insn->func) {
			WARN_FUNC("undefined stack state", insn->sec, insn->offset);
			return -1;
		}
		return 0;
	}

	if (cfi->type == UNWIND_HINT_TYPE_REGS ||
	    cfi->type == UNWIND_HINT_TYPE_REGS_PARTIAL)
		return update_cfi_state_regs(insn, cfi, op);

	switch (op->dest.type) {

	case OP_DEST_REG:
		switch (op->src.type) {

		case OP_SRC_REG:
			if (op->src.reg == CFI_SP && op->dest.reg == CFI_BP &&
			    cfa->base == CFI_SP &&
			    regs[CFI_BP].base == CFI_CFA &&
			    regs[CFI_BP].offset == -cfa->offset) {

				/* mov %rsp, %rbp */
				cfa->base = op->dest.reg;
				cfi->bp_scratch = false;
			}

			else if (op->src.reg == CFI_SP &&
				 op->dest.reg == CFI_BP && cfi->drap) {

				/* drap: mov %rsp, %rbp */
				regs[CFI_BP].base = CFI_BP;
				regs[CFI_BP].offset = -cfi->stack_size;
				cfi->bp_scratch = false;
			}

			else if (op->src.reg == CFI_SP && cfa->base == CFI_SP) {

				/*
				 * mov %rsp, %reg
				 *
				 * This is needed for the rare case where GCC
				 * does:
				 *
				 *   mov    %rsp, %rax
				 *   ...
				 *   mov    %rax, %rsp
				 */
				cfi->vals[op->dest.reg].base = CFI_CFA;
				cfi->vals[op->dest.reg].offset = -cfi->stack_size;
			}

			else if (op->src.reg == CFI_BP && op->dest.reg == CFI_SP &&
				 cfa->base == CFI_BP) {

				/*
				 * mov %rbp, %rsp
				 *
				 * Restore the original stack pointer (Clang).
				 */
				cfi->stack_size = -cfi->regs[CFI_BP].offset;
			}

			else if (op->dest.reg == cfa->base) {

				/* mov %reg, %rsp */
				if (cfa->base == CFI_SP &&
				    cfi->vals[op->src.reg].base == CFI_CFA) {

					/*
					 * This is needed for the rare case
					 * where GCC does something dumb like:
					 *
					 *   lea    0x8(%rsp), %rcx
					 *   ...
					 *   mov    %rcx, %rsp
					 */
					cfa->offset = -cfi->vals[op->src.reg].offset;
					cfi->stack_size = cfa->offset;

				} else {
					cfa->base = CFI_UNDEFINED;
					cfa->offset = 0;
				}
			}

			break;

		case OP_SRC_ADD:
			if (op->dest.reg == CFI_SP && op->src.reg == CFI_SP) {

				/* add imm, %rsp */
				cfi->stack_size -= op->src.offset;
				if (cfa->base == CFI_SP)
					cfa->offset -= op->src.offset;
				break;
			}

			if (op->dest.reg == CFI_SP && op->src.reg == CFI_BP) {

				/* lea disp(%rbp), %rsp */
				cfi->stack_size = -(op->src.offset + regs[CFI_BP].offset);
				break;
			}

			if (op->src.reg == CFI_SP && cfa->base == CFI_SP) {

				/* drap: lea disp(%rsp), %drap */
				cfi->drap_reg = op->dest.reg;

				/*
				 * lea disp(%rsp), %reg
				 *
				 * This is needed for the rare case where GCC
				 * does something dumb like:
				 *
				 *   lea    0x8(%rsp), %rcx
				 *   ...
				 *   mov    %rcx, %rsp
				 */
				cfi->vals[op->dest.reg].base = CFI_CFA;
				cfi->vals[op->dest.reg].offset = \
					-cfi->stack_size + op->src.offset;

				break;
			}

			if (cfi->drap && op->dest.reg == CFI_SP &&
			    op->src.reg == cfi->drap_reg) {

				 /* drap: lea disp(%drap), %rsp */
				cfa->base = CFI_SP;
				cfa->offset = cfi->stack_size = -op->src.offset;
				cfi->drap_reg = CFI_UNDEFINED;
				cfi->drap = false;
				break;
			}

			if (op->dest.reg == cfi->cfa.base) {
				WARN_FUNC("unsupported stack register modification",
					  insn->sec, insn->offset);
				return -1;
			}

			break;

		case OP_SRC_AND:
			if (op->dest.reg != CFI_SP ||
			    (cfi->drap_reg != CFI_UNDEFINED && cfa->base != CFI_SP) ||
			    (cfi->drap_reg == CFI_UNDEFINED && cfa->base != CFI_BP)) {
				WARN_FUNC("unsupported stack pointer realignment",
					  insn->sec, insn->offset);
				return -1;
			}

			if (cfi->drap_reg != CFI_UNDEFINED) {
				/* drap: and imm, %rsp */
				cfa->base = cfi->drap_reg;
				cfa->offset = cfi->stack_size = 0;
				cfi->drap = true;
			}

			/*
			 * Older versions of GCC (4.8ish) realign the stack
			 * without DRAP, with a frame pointer.
			 */

			break;

		case OP_SRC_POP:
		case OP_SRC_POPF:
			if (!cfi->drap && op->dest.reg == cfa->base) {

				/* pop %rbp */
				cfa->base = CFI_SP;
			}

			if (cfi->drap && cfa->base == CFI_BP_INDIRECT &&
			    op->dest.reg == cfi->drap_reg &&
			    cfi->drap_offset == -cfi->stack_size) {

				/* drap: pop %drap */
				cfa->base = cfi->drap_reg;
				cfa->offset = 0;
				cfi->drap_offset = -1;

			} else if (regs[op->dest.reg].offset == -cfi->stack_size) {

				/* pop %reg */
				restore_reg(cfi, op->dest.reg);
			}

			cfi->stack_size -= 8;
			if (cfa->base == CFI_SP)
				cfa->offset -= 8;

			break;

		case OP_SRC_REG_INDIRECT:
			if (cfi->drap && op->src.reg == CFI_BP &&
			    op->src.offset == cfi->drap_offset) {

				/* drap: mov disp(%rbp), %drap */
				cfa->base = cfi->drap_reg;
				cfa->offset = 0;
				cfi->drap_offset = -1;
			}

			if (cfi->drap && op->src.reg == CFI_BP &&
			    op->src.offset == regs[op->dest.reg].offset) {

				/* drap: mov disp(%rbp), %reg */
				restore_reg(cfi, op->dest.reg);

			} else if (op->src.reg == cfa->base &&
			    op->src.offset == regs[op->dest.reg].offset + cfa->offset) {

				/* mov disp(%rbp), %reg */
				/* mov disp(%rsp), %reg */
				restore_reg(cfi, op->dest.reg);
			}

			break;

		default:
			WARN_FUNC("unknown stack-related instruction",
				  insn->sec, insn->offset);
			return -1;
		}

		break;

	case OP_DEST_PUSH:
	case OP_DEST_PUSHF:
		cfi->stack_size += 8;
		if (cfa->base == CFI_SP)
			cfa->offset += 8;

		if (op->src.type != OP_SRC_REG)
			break;

		if (cfi->drap) {
			if (op->src.reg == cfa->base && op->src.reg == cfi->drap_reg) {

				/* drap: push %drap */
				cfa->base = CFI_BP_INDIRECT;
				cfa->offset = -cfi->stack_size;

				/* save drap so we know when to restore it */
				cfi->drap_offset = -cfi->stack_size;

			} else if (op->src.reg == CFI_BP && cfa->base == cfi->drap_reg) {

				/* drap: push %rbp */
				cfi->stack_size = 0;

			} else {

				/* drap: push %reg */
				save_reg(cfi, op->src.reg, CFI_BP, -cfi->stack_size);
			}

		} else {

			/* push %reg */
			save_reg(cfi, op->src.reg, CFI_CFA, -cfi->stack_size);
		}

		/* detect when asm code uses rbp as a scratch register */
		if (!no_fp && insn->func && op->src.reg == CFI_BP &&
		    cfa->base != CFI_BP)
			cfi->bp_scratch = true;
		break;

	case OP_DEST_REG_INDIRECT:

		if (cfi->drap) {
			if (op->src.reg == cfa->base && op->src.reg == cfi->drap_reg) {

				/* drap: mov %drap, disp(%rbp) */
				cfa->base = CFI_BP_INDIRECT;
				cfa->offset = op->dest.offset;

				/* save drap offset so we know when to restore it */
				cfi->drap_offset = op->dest.offset;
			} else {

				/* drap: mov reg, disp(%rbp) */
				save_reg(cfi, op->src.reg, CFI_BP, op->dest.offset);
			}

		} else if (op->dest.reg == cfa->base) {

			/* mov reg, disp(%rbp) */
			/* mov reg, disp(%rsp) */
			save_reg(cfi, op->src.reg, CFI_CFA,
				 op->dest.offset - cfi->cfa.offset);
		}

		break;

	case OP_DEST_LEAVE:
		if ((!cfi->drap && cfa->base != CFI_BP) ||
		    (cfi->drap && cfa->base != cfi->drap_reg)) {
			WARN_FUNC("leave instruction with modified stack frame",
				  insn->sec, insn->offset);
			return -1;
		}

		/* leave (mov %rbp, %rsp; pop %rbp) */

		cfi->stack_size = -cfi->regs[CFI_BP].offset - 8;
		restore_reg(cfi, CFI_BP);

		if (!cfi->drap) {
			cfa->base = CFI_SP;
			cfa->offset -= 8;
		}

		break;

	case OP_DEST_MEM:
		if (op->src.type != OP_SRC_POP && op->src.type != OP_SRC_POPF) {
			WARN_FUNC("unknown stack-related memory operation",
				  insn->sec, insn->offset);
			return -1;
		}

		/* pop mem */
		cfi->stack_size -= 8;
		if (cfa->base == CFI_SP)
			cfa->offset -= 8;

		break;

	default:
		WARN_FUNC("unknown stack-related instruction",
			  insn->sec, insn->offset);
		return -1;
	}

	return 0;
}

bool arch_callee_saved_reg(unsigned char reg)
{
	switch (reg) {
	case CFI_BP:
	case CFI_BX:
	case CFI_R12:
	case CFI_R13:
	case CFI_R14:
	case CFI_R15:
		return true;

	case CFI_AX:
	case CFI_CX:
	case CFI_DX:
	case CFI_SI:
	case CFI_DI:
	case CFI_SP:
	case CFI_R8:
	case CFI_R9:
	case CFI_R10:
	case CFI_R11:
	case CFI_RA:
	default:
		return false;
	}
}

unsigned long arch_dest_reloc_offset(int addend)
{
	return addend + 4;
}

unsigned long arch_jump_destination(struct instruction *insn)
{
	return insn->offset + insn->len + insn->immediate;
}

#define ADD_OP(op) \
	if (!(op = calloc(1, sizeof(*op)))) \
		return -1; \
	else for (list_add_tail(&op->list, ops_list); op; op = NULL)

int arch_decode_instruction(const struct elf *elf, const struct section *sec,
			    unsigned long offset, unsigned int maxlen,
			    unsigned int *len, enum insn_type *type,
			    unsigned long *immediate,
			    struct list_head *ops_list)
{
	struct insn insn;
	int x86_64, sign;
	unsigned char op1, op2, rex = 0, rex_b = 0, rex_r = 0, rex_w = 0,
		      rex_x = 0, modrm = 0, modrm_mod = 0, modrm_rm = 0,
		      modrm_reg = 0, sib = 0;
	struct stack_op *op = NULL;
	struct symbol *sym;

	x86_64 = is_x86_64(elf);
	if (x86_64 == -1)
		return -1;

	insn_init(&insn, sec->data->d_buf + offset, maxlen, x86_64);
	insn_get_length(&insn);

	if (!insn_complete(&insn)) {
		WARN("can't decode instruction at %s:0x%lx", sec->name, offset);
		return -1;
	}

	*len = insn.length;
	*type = INSN_OTHER;

	if (insn.vex_prefix.nbytes)
		return 0;

	op1 = insn.opcode.bytes[0];
	op2 = insn.opcode.bytes[1];

	if (insn.rex_prefix.nbytes) {
		rex = insn.rex_prefix.bytes[0];
		rex_w = X86_REX_W(rex) >> 3;
		rex_r = X86_REX_R(rex) >> 2;
		rex_x = X86_REX_X(rex) >> 1;
		rex_b = X86_REX_B(rex);
	}

	if (insn.modrm.nbytes) {
		modrm = insn.modrm.bytes[0];
		modrm_mod = X86_MODRM_MOD(modrm);
		modrm_reg = X86_MODRM_REG(modrm);
		modrm_rm = X86_MODRM_RM(modrm);
	}

	if (insn.sib.nbytes)
		sib = insn.sib.bytes[0];

	switch (op1) {

	case 0x1:
	case 0x29:
		if (rex_w && !rex_b && modrm_mod == 3 && modrm_rm == 4) {

			/* add/sub reg, %rsp */
			ADD_OP(op) {
				op->src.type = OP_SRC_ADD;
				op->src.reg = op_to_cfi_reg[modrm_reg][rex_r];
				op->dest.type = OP_DEST_REG;
				op->dest.reg = CFI_SP;
			}
		}
		break;

	case 0x50 ... 0x57:

		/* push reg */
		ADD_OP(op) {
			op->src.type = OP_SRC_REG;
			op->src.reg = op_to_cfi_reg[op1 & 0x7][rex_b];
			op->dest.type = OP_DEST_PUSH;
		}

		break;

	case 0x58 ... 0x5f:

		/* pop reg */
		ADD_OP(op) {
			op->src.type = OP_SRC_POP;
			op->dest.type = OP_DEST_REG;
			op->dest.reg = op_to_cfi_reg[op1 & 0x7][rex_b];
		}

		break;

	case 0x68:
	case 0x6a:
		/* push immediate */
		ADD_OP(op) {
			op->src.type = OP_SRC_CONST;
			op->dest.type = OP_DEST_PUSH;
		}
		break;

	case 0x70 ... 0x7f:
		*type = INSN_JUMP_CONDITIONAL;
		break;

	case 0x81:
	case 0x83:
		if (rex != 0x48)
			break;

		if (modrm == 0xe4) {
			/* and imm, %rsp */
			ADD_OP(op) {
				op->src.type = OP_SRC_AND;
				op->src.reg = CFI_SP;
				op->src.offset = insn.immediate.value;
				op->dest.type = OP_DEST_REG;
				op->dest.reg = CFI_SP;
			}
			break;
		}

		if (modrm == 0xc4)
			sign = 1;
		else if (modrm == 0xec)
			sign = -1;
		else
			break;

		/* add/sub imm, %rsp */
		ADD_OP(op) {
			op->src.type = OP_SRC_ADD;
			op->src.reg = CFI_SP;
			op->src.offset = insn.immediate.value * sign;
			op->dest.type = OP_DEST_REG;
			op->dest.reg = CFI_SP;
		}
		break;

	case 0x89:
		if (rex_w && !rex_r && modrm_mod == 3 && modrm_reg == 4) {

			/* mov %rsp, reg */
			ADD_OP(op) {
				op->src.type = OP_SRC_REG;
				op->src.reg = CFI_SP;
				op->dest.type = OP_DEST_REG;
				op->dest.reg = op_to_cfi_reg[modrm_rm][rex_b];
			}
			break;
		}

		if (rex_w && !rex_b && modrm_mod == 3 && modrm_rm == 4) {

			/* mov reg, %rsp */
			ADD_OP(op) {
				op->src.type = OP_SRC_REG;
				op->src.reg = op_to_cfi_reg[modrm_reg][rex_r];
				op->dest.type = OP_DEST_REG;
				op->dest.reg = CFI_SP;
			}
			break;
		}

		/* fallthrough */
	case 0x88:
		if (!rex_b &&
		    (modrm_mod == 1 || modrm_mod == 2) && modrm_rm == 5) {

			/* mov reg, disp(%rbp) */
			ADD_OP(op) {
				op->src.type = OP_SRC_REG;
				op->src.reg = op_to_cfi_reg[modrm_reg][rex_r];
				op->dest.type = OP_DEST_REG_INDIRECT;
				op->dest.reg = CFI_BP;
				op->dest.offset = insn.displacement.value;
			}

		} else if (rex_w && !rex_b && modrm_rm == 4 && sib == 0x24) {

			/* mov reg, disp(%rsp) */
			ADD_OP(op) {
				op->src.type = OP_SRC_REG;
				op->src.reg = op_to_cfi_reg[modrm_reg][rex_r];
				op->dest.type = OP_DEST_REG_INDIRECT;
				op->dest.reg = CFI_SP;
				op->dest.offset = insn.displacement.value;
			}
		}

		break;

	case 0x8b:
		if (rex_w && !rex_b && modrm_mod == 1 && modrm_rm == 5) {

			/* mov disp(%rbp), reg */
			ADD_OP(op) {
				op->src.type = OP_SRC_REG_INDIRECT;
				op->src.reg = CFI_BP;
				op->src.offset = insn.displacement.value;
				op->dest.type = OP_DEST_REG;
				op->dest.reg = op_to_cfi_reg[modrm_reg][rex_r];
			}

		} else if (rex_w && !rex_b && sib == 0x24 &&
			   modrm_mod != 3 && modrm_rm == 4) {

			/* mov disp(%rsp), reg */
			ADD_OP(op) {
				op->src.type = OP_SRC_REG_INDIRECT;
				op->src.reg = CFI_SP;
				op->src.offset = insn.displacement.value;
				op->dest.type = OP_DEST_REG;
				op->dest.reg = op_to_cfi_reg[modrm_reg][rex_r];
			}
		}

		break;

	case 0x8d:
		if (sib == 0x24 && rex_w && !rex_b && !rex_x) {

			ADD_OP(op) {
				if (!insn.displacement.value) {
					/* lea (%rsp), reg */
					op->src.type = OP_SRC_REG;
				} else {
					/* lea disp(%rsp), reg */
					op->src.type = OP_SRC_ADD;
					op->src.offset = insn.displacement.value;
				}
				op->src.reg = CFI_SP;
				op->dest.type = OP_DEST_REG;
				op->dest.reg = op_to_cfi_reg[modrm_reg][rex_r];
			}

		} else if (rex == 0x48 && modrm == 0x65) {

			/* lea disp(%rbp), %rsp */
			ADD_OP(op) {
				op->src.type = OP_SRC_ADD;
				op->src.reg = CFI_BP;
				op->src.offset = insn.displacement.value;
				op->dest.type = OP_DEST_REG;
				op->dest.reg = CFI_SP;
			}

		} else if (rex == 0x49 && modrm == 0x62 &&
			   insn.displacement.value == -8) {

			/*
			 * lea -0x8(%r10), %rsp
			 *
			 * Restoring rsp back to its original value after a
			 * stack realignment.
			 */
			ADD_OP(op) {
				op->src.type = OP_SRC_ADD;
				op->src.reg = CFI_R10;
				op->src.offset = -8;
				op->dest.type = OP_DEST_REG;
				op->dest.reg = CFI_SP;
			}

		} else if (rex == 0x49 && modrm == 0x65 &&
			   insn.displacement.value == -16) {

			/*
			 * lea -0x10(%r13), %rsp
			 *
			 * Restoring rsp back to its original value after a
			 * stack realignment.
			 */
			ADD_OP(op) {
				op->src.type = OP_SRC_ADD;
				op->src.reg = CFI_R13;
				op->src.offset = -16;
				op->dest.type = OP_DEST_REG;
				op->dest.reg = CFI_SP;
			}
		}

		break;

	case 0x8f:
		/* pop to mem */
		ADD_OP(op) {
			op->src.type = OP_SRC_POP;
			op->dest.type = OP_DEST_MEM;
		}
		break;

	case 0x90:
		*type = INSN_NOP;
		break;

	case 0x9c:
		/* pushf */
		ADD_OP(op) {
			op->src.type = OP_SRC_CONST;
			op->dest.type = OP_DEST_PUSHF;
		}
		break;

	case 0x9d:
		/* popf */
		ADD_OP(op) {
			op->src.type = OP_SRC_POPF;
			op->dest.type = OP_DEST_MEM;
		}
		break;

	case 0x0f:

		if (op2 == 0x01) {

			if (modrm == 0xca)
				*type = INSN_CLAC;
			else if (modrm == 0xcb)
				*type = INSN_STAC;

		} else if (op2 >= 0x80 && op2 <= 0x8f) {

			*type = INSN_JUMP_CONDITIONAL;

		} else if (op2 == 0x05 || op2 == 0x07 || op2 == 0x34 ||
			   op2 == 0x35) {

			/* sysenter, sysret */
			*type = INSN_CONTEXT_SWITCH;

		} else if (op2 == 0x0b || op2 == 0xb9) {

			/* ud2 */
			*type = INSN_BUG;

		} else if (op2 == 0x0d || op2 == 0x1f) {

			/* nopl/nopw */
			*type = INSN_NOP;

		} else if (op2 == 0xa0 || op2 == 0xa8) {

			/* push fs/gs */
			ADD_OP(op) {
				op->src.type = OP_SRC_CONST;
				op->dest.type = OP_DEST_PUSH;
			}

		} else if (op2 == 0xa1 || op2 == 0xa9) {

			/* pop fs/gs */
			ADD_OP(op) {
				op->src.type = OP_SRC_POP;
				op->dest.type = OP_DEST_MEM;
			}
		}

		break;

	case 0xc9:
		/*
		 * leave
		 *
		 * equivalent to:
		 * mov bp, sp
		 * pop bp
		 */
		ADD_OP(op)
			op->dest.type = OP_DEST_LEAVE;

		break;

	case 0xe3:
		/* jecxz/jrcxz */
		*type = INSN_JUMP_CONDITIONAL;
		break;

	case 0xe9:
	case 0xeb:
		*type = INSN_JUMP_UNCONDITIONAL;
		break;

	case 0xc2:
	case 0xc3:
		*type = INSN_RETURN;
		break;

	case 0xcf: /* iret */
		/*
		 * Handle sync_core(), which has an IRET to self.
		 * All other IRET are in STT_NONE entry code.
		 */
		sym = find_symbol_containing(sec, offset);
		if (sym && sym->type == STT_FUNC) {
			ADD_OP(op) {
				/* add $40, %rsp */
				op->src.type = OP_SRC_ADD;
				op->src.reg = CFI_SP;
				op->src.offset = 5*8;
				op->dest.type = OP_DEST_REG;
				op->dest.reg = CFI_SP;
			}
			break;
		}

		/* fallthrough */

	case 0xca: /* retf */
	case 0xcb: /* retf */
		*type = INSN_CONTEXT_SWITCH;
		break;

	case 0xe8:
		*type = INSN_CALL;
		/*
		 * For the impact on the stack, a CALL behaves like
		 * a PUSH of an immediate value (the return address).
		 */
		ADD_OP(op) {
			op->src.type = OP_SRC_CONST;
			op->dest.type = OP_DEST_PUSH;
		}
		break;

	case 0xfc:
		*type = INSN_CLD;
		break;

	case 0xfd:
		*type = INSN_STD;
		break;

	case 0xff:
		if (modrm_reg == 2 || modrm_reg == 3)

			*type = INSN_CALL_DYNAMIC;

		else if (modrm_reg == 4)

			*type = INSN_JUMP_DYNAMIC;

		else if (modrm_reg == 5)

			/* jmpf */
			*type = INSN_CONTEXT_SWITCH;

		else if (modrm_reg == 6) {

			/* push from mem */
			ADD_OP(op) {
				op->src.type = OP_SRC_CONST;
				op->dest.type = OP_DEST_PUSH;
			}
		}

		break;

	default:
		break;
	}

	*immediate = insn.immediate.nbytes ? insn.immediate.value : 0;

	return 0;
}

void arch_initial_func_cfi_state(struct cfi_init_state *state)
{
	int i;

	for (i = 0; i < CFI_NUM_REGS; i++) {
		state->regs[i].base = CFI_UNDEFINED;
		state->regs[i].offset = 0;
	}

	/* initial CFA (call frame address) */
	state->cfa.base = CFI_SP;
	state->cfa.offset = 8;

	/* initial RA (return address) */
	state->regs[16].base = CFI_CFA;
	state->regs[16].offset = -8;
}

bool arch_has_valid_stack_frame(struct insn_state *state)
{
	struct cfi_state *cfi = &state->cfi;

	if (cfi->cfa.base == CFI_BP && cfi->regs[CFI_BP].base == CFI_CFA &&
	    cfi->regs[CFI_BP].offset == -16)
		return true;

	if (cfi->drap && cfi->regs[CFI_BP].base == CFI_BP)
		return true;

	return false;
}

int arch_handle_insn_ops(struct instruction *insn, struct insn_state *state)
{
	struct stack_op *op;

	list_for_each_entry(op, &insn->stack_ops, list) {
		struct cfi_state old_cfi = state->cfi;
		int res;

		res = update_cfi_state(insn, &state->cfi, op);
		if (res)
			return res;

		if (insn->alt_group && memcmp(&state->cfi, &old_cfi, sizeof(struct cfi_state))) {
			WARN_FUNC("alternative modifies stack", insn->sec, insn->offset);
			return -1;
		}

		if (op->dest.type == OP_DEST_PUSHF) {
			if (!state->uaccess_stack) {
				state->uaccess_stack = 1;
			} else if (state->uaccess_stack >> 31) {
				WARN_FUNC("PUSHF stack exhausted",
					  insn->sec, insn->offset);
				return 1;
			}
			state->uaccess_stack <<= 1;
			state->uaccess_stack  |= state->uaccess;
		}

		if (op->src.type == OP_SRC_POPF) {
			if (state->uaccess_stack) {
				state->uaccess = state->uaccess_stack & 1;
				state->uaccess_stack >>= 1;
				if (state->uaccess_stack == 1)
					state->uaccess_stack = 0;
			}
		}
	}

	return 0;
}

int arch_create_static_call_sections(struct objtool_file *file)
{
	struct section *sec, *reloc_sec;
	struct reloc *reloc;
	struct static_call_site *site;
	struct instruction *insn;
	struct symbol *key_sym;
	char *key_name, *tmp;
	int idx;

	sec = find_section_by_name(file->elf, ".static_call_sites");
	if (sec) {
		INIT_LIST_HEAD(&file->static_call_list);
		WARN("file already has .static_call_sites section, skipping");
		return 0;
	}

	if (list_empty(&file->static_call_list))
		return 0;

	idx = 0;
	list_for_each_entry(insn, &file->static_call_list, static_call_node)
		idx++;

	sec = elf_create_section(file->elf, ".static_call_sites", SHF_WRITE,
				 sizeof(struct static_call_site), idx);
	if (!sec)
		return -1;

	reloc_sec = elf_create_reloc_section(file->elf, sec, SHT_RELA);
	if (!reloc_sec)
		return -1;

	idx = 0;
	list_for_each_entry(insn, &file->static_call_list, static_call_node) {

		site = (struct static_call_site *)sec->data->d_buf + idx;
		memset(site, 0, sizeof(struct static_call_site));

		/* populate reloc for 'addr' */
		reloc = malloc(sizeof(*reloc));

		if (!reloc) {
			perror("malloc");
			return -1;
		}
		memset(reloc, 0, sizeof(*reloc));

		insn_to_reloc_sym_addend(insn->sec, insn->offset, reloc);
		if (!reloc->sym) {
			WARN_FUNC("static call tramp: missing containing symbol",
				  insn->sec, insn->offset);
			return -1;
		}

		reloc->type = R_X86_64_PC32;
		reloc->offset = idx * sizeof(struct static_call_site);
		reloc->sec = reloc_sec;
		elf_add_reloc(file->elf, reloc);

		/* find key symbol */
		key_name = strdup(insn->call_dest->name);
		if (!key_name) {
			perror("strdup");
			return -1;
		}
		if (strncmp(key_name, STATIC_CALL_TRAMP_PREFIX_STR,
			    STATIC_CALL_TRAMP_PREFIX_LEN)) {
			WARN("static_call: trampoline name malformed: %s", key_name);
			return -1;
		}
		tmp = key_name + STATIC_CALL_TRAMP_PREFIX_LEN - STATIC_CALL_KEY_PREFIX_LEN;
		memcpy(tmp, STATIC_CALL_KEY_PREFIX_STR, STATIC_CALL_KEY_PREFIX_LEN);

		key_sym = find_symbol_by_name(file->elf, tmp);
		if (!key_sym) {
			if (!module) {
				WARN("static_call: can't find static_call_key symbol: %s", tmp);
				return -1;
			}

			/*
			 * For modules(), the key might not be exported, which
			 * means the module can make static calls but isn't
			 * allowed to change them.
			 *
			 * In that case we temporarily set the key to be the
			 * trampoline address.  This is fixed up in
			 * static_call_add_module().
			 */
			key_sym = insn->call_dest;
		}
		free(key_name);

		/* populate reloc for 'key' */
		reloc = malloc(sizeof(*reloc));
		if (!reloc) {
			perror("malloc");
			return -1;
		}
		memset(reloc, 0, sizeof(*reloc));
		reloc->sym = key_sym;
		reloc->addend = is_sibling_call(insn) ? STATIC_CALL_SITE_TAIL : 0;
		reloc->type = R_X86_64_PC32;
		reloc->offset = idx * sizeof(struct static_call_site) + 4;
		reloc->sec = reloc_sec;
		elf_add_reloc(file->elf, reloc);

		idx++;
	}

	if (elf_rebuild_reloc_section(file->elf, reloc_sec))
		return -1;

	return 0;
}

int arch_read_static_call_tramps(struct objtool_file *file)
{
	struct section *sec;
	struct symbol *func;

	for_each_sec(file, sec) {
		list_for_each_entry(func, &sec->symbol_list, list) {
			if (func->bind == STB_GLOBAL &&
			    !strncmp(func->name, STATIC_CALL_TRAMP_PREFIX_STR,
				     strlen(STATIC_CALL_TRAMP_PREFIX_STR)))
				func->static_call_tramp = true;
		}
	}

	return 0;
}

const char *arch_nop_insn(int len)
{
	static const char nops[5][5] = {
		/* 1 */ { 0x90 },
		/* 2 */ { 0x66, 0x90 },
		/* 3 */ { 0x0f, 0x1f, 0x00 },
		/* 4 */ { 0x0f, 0x1f, 0x40, 0x00 },
		/* 5 */ { 0x0f, 0x1f, 0x44, 0x00, 0x00 },
	};

	if (len < 1 || len > 5) {
		WARN("invalid NOP size: %d\n", len);
		return NULL;
	}

	return nops[len-1];
}

int arch_decode_hint_reg(struct instruction *insn, u8 sp_reg)
{
	struct cfi_reg *cfa = &insn->cfi.cfa;

	switch (sp_reg) {
	case ORC_REG_UNDEFINED:
		cfa->base = CFI_UNDEFINED;
		break;
	case ORC_REG_SP:
		cfa->base = CFI_SP;
		break;
	case ORC_REG_BP:
		cfa->base = CFI_BP;
		break;
	case ORC_REG_SP_INDIRECT:
		cfa->base = CFI_SP_INDIRECT;
		break;
	case ORC_REG_R10:
		cfa->base = CFI_R10;
		break;
	case ORC_REG_R13:
		cfa->base = CFI_R13;
		break;
	case ORC_REG_DI:
		cfa->base = CFI_DI;
		break;
	case ORC_REG_DX:
		cfa->base = CFI_DX;
		break;
	default:
		return -1;
	}

	return 0;
}

void arch_try_find_call(struct list_head *p_orbit_list, struct objtool_file *file,
			struct symbol *func, struct instruction *insn)
{
}
