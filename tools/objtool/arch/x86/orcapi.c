// SPDX-License-Identifier: GPL-2.0-or-later
#include <string.h>
#include <stdlib.h>

#include "../../orc.h"
#include "../../warn.h"

static	struct orc_entry empty = {
	.sp_reg = ORC_REG_UNDEFINED,
	.bp_reg = ORC_REG_UNDEFINED,
	.type   = UNWIND_HINT_TYPE_CALL,
};

int create_orc(struct objtool_file *file)
{
	struct instruction *insn;

	for_each_insn(file, insn) {
		struct orc_entry *orc = &insn->orc;
		struct cfi_reg *cfa = &insn->cfi.cfa;
		struct cfi_reg *bp = &insn->cfi.regs[CFI_BP];

		if (!insn->sec->text)
			continue;

		orc->end = insn->cfi.end;

		if (cfa->base == CFI_UNDEFINED) {
			orc->sp_reg = ORC_REG_UNDEFINED;
			continue;
		}

		switch (cfa->base) {
		case CFI_SP:
			orc->sp_reg = ORC_REG_SP;
			break;
		case CFI_SP_INDIRECT:
			orc->sp_reg = ORC_REG_SP_INDIRECT;
			break;
		case CFI_BP:
			orc->sp_reg = ORC_REG_BP;
			break;
		case CFI_BP_INDIRECT:
			orc->sp_reg = ORC_REG_BP_INDIRECT;
			break;
		case CFI_R10:
			orc->sp_reg = ORC_REG_R10;
			break;
		case CFI_R13:
			orc->sp_reg = ORC_REG_R13;
			break;
		case CFI_DI:
			orc->sp_reg = ORC_REG_DI;
			break;
		case CFI_DX:
			orc->sp_reg = ORC_REG_DX;
			break;
		default:
			WARN_FUNC("unknown CFA base reg %d",
				  insn->sec, insn->offset, cfa->base);
			return -1;
		}

		switch (bp->base) {
		case CFI_UNDEFINED:
			orc->bp_reg = ORC_REG_UNDEFINED;
			break;
		case CFI_CFA:
			orc->bp_reg = ORC_REG_PREV_SP;
			break;
		case CFI_BP:
			orc->bp_reg = ORC_REG_BP;
			break;
		default:
			WARN_FUNC("unknown BP base reg %d",
				  insn->sec, insn->offset, bp->base);
			return -1;
		}

		orc->sp_offset = cfa->offset;
		orc->bp_offset = bp->offset;
		orc->type = insn->cfi.type;
	}

	return 0;
}

int arch_create_orc_entry(struct elf *elf, struct section *u_sec, struct section *ip_relocsec,
				unsigned int idx, struct section *insn_sec,
				unsigned long insn_off, struct orc_entry *o)
{
	struct orc_entry *orc;
	struct reloc *reloc;

	/* populate ORC data */
	orc = (struct orc_entry *)u_sec->data->d_buf + idx;
	memcpy(orc, o, sizeof(*orc));

	/* populate reloc for ip */
	reloc = malloc(sizeof(*reloc));
	if (!reloc) {
		perror("malloc");
		return -1;
	}
	memset(reloc, 0, sizeof(*reloc));

	insn_to_reloc_sym_addend(insn_sec, insn_off, reloc);
	if (!reloc->sym) {
		WARN("missing symbol for insn at offset 0x%lx",
		     insn_off);
		return -1;
	}

	reloc->type = R_X86_64_PC32;
	reloc->offset = idx * sizeof(int);
	reloc->sec = ip_relocsec;

	elf_add_reloc(elf, reloc);

	return 0;
}

int arch_create_orc_entry_empty(struct elf *elf, struct section *u_sec,
				struct section *ip_relasec, unsigned int idx,
				struct section *insn_sec, unsigned long insn_off)
{
	return arch_create_orc_entry(elf, u_sec, ip_relasec, idx,
					insn_sec, insn_off, &empty);
}

static const char *reg_name(unsigned int reg)
{
	switch (reg) {
	case ORC_REG_PREV_SP:
		return "prevsp";
	case ORC_REG_DX:
		return "dx";
	case ORC_REG_DI:
		return "di";
	case ORC_REG_BP:
		return "bp";
	case ORC_REG_SP:
		return "sp";
	case ORC_REG_R10:
		return "r10";
	case ORC_REG_R13:
		return "r13";
	case ORC_REG_BP_INDIRECT:
		return "bp(ind)";
	case ORC_REG_SP_INDIRECT:
		return "sp(ind)";
	default:
		return "?";
	}
}

static const char *orc_type_name(unsigned int type)
{
	switch (type) {
	case UNWIND_HINT_TYPE_CALL:
		return "call";
	case UNWIND_HINT_TYPE_REGS:
		return "regs";
	case UNWIND_HINT_TYPE_REGS_PARTIAL:
		return "regs (partial)";
	default:
		return "?";
	}
}

static void print_reg(unsigned int reg, int offset)
{
	if (reg == ORC_REG_BP_INDIRECT)
		printf("(bp%+d)", offset);
	else if (reg == ORC_REG_SP_INDIRECT)
		printf("(sp%+d)", offset);
	else if (reg == ORC_REG_UNDEFINED)
		printf("(und)");
	else
		printf("%s%+d", reg_name(reg), offset);
}

void arch_print_reg(struct orc_entry orc)
{
	printf(" sp:");

	print_reg(orc.sp_reg, orc.sp_offset);

	printf(" bp:");

	print_reg(orc.bp_reg, orc.bp_offset);

	printf(" type:%s end:%d\n",
	       orc_type_name(orc.type), orc.end);
}
