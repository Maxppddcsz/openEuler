// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017 Josh Poimboeuf <jpoimboe@redhat.com>
 */

#include <stdlib.h>
#include <string.h>

#include <linux/objtool.h>
#include <asm/orc_types.h>

#include "check.h"
#include "warn.h"
#include "orc.h"

int create_orc_sections(struct objtool_file *file)
{
	struct instruction *insn, *prev_insn;
	struct section *sec, *u_sec, *ip_relocsec;
	unsigned int idx;

	sec = find_section_by_name(file->elf, ".orc_unwind");
	if (sec) {
		WARN("file already has .orc_unwind section, skipping");
		return -1;
	}

	/* count the number of needed orcs */
	idx = 0;
	for_each_sec(file, sec) {
		if (!sec->text)
			continue;

		prev_insn = NULL;
		sec_for_each_insn(file, sec, insn) {
			if (!prev_insn ||
			    memcmp(&insn->orc, &prev_insn->orc,
				   sizeof(struct orc_entry))) {
				idx++;
			}
			prev_insn = insn;
		}

		/* section terminator */
		if (prev_insn)
			idx++;
	}
	if (!idx)
		return -1;


	/* create .orc_unwind_ip and .rela.orc_unwind_ip sections */
	sec = elf_create_section(file->elf, ".orc_unwind_ip", 0, sizeof(int), idx);
	if (!sec)
		return -1;

	ip_relocsec = elf_create_reloc_section(file->elf, sec, SHT_RELA);
	if (!ip_relocsec)
		return -1;

	/* create .orc_unwind section */
	u_sec = elf_create_section(file->elf, ".orc_unwind", 0,
				   sizeof(struct orc_entry), idx);

	/* populate sections */
	idx = 0;
	for_each_sec(file, sec) {
		if (!sec->text)
			continue;

		prev_insn = NULL;
		sec_for_each_insn(file, sec, insn) {
			if (!prev_insn || memcmp(&insn->orc, &prev_insn->orc,
						 sizeof(struct orc_entry))) {

				if (arch_create_orc_entry(file->elf, u_sec, ip_relocsec, idx,
						     insn->sec, insn->offset,
						     &insn->orc))
					return -1;

				idx++;
			}
			prev_insn = insn;
		}

		/* section terminator */
		if (prev_insn) {
			if (arch_create_orc_entry_empty(file->elf, u_sec, ip_relocsec, idx,
					     prev_insn->sec,
					     prev_insn->offset + prev_insn->len))
				return -1;

			idx++;
		}
	}

	if (elf_rebuild_reloc_section(file->elf, ip_relocsec))
		return -1;

	return 0;
}
