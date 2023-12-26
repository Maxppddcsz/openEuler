// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2013, 2014 Linaro Ltd;  <roy.franz@linaro.org>
 *
 * This file implements the EFI boot stub for the arm64 kernel.
 * Adapted from ARM version by Mark Salter <msalter@redhat.com>
 */


#include <linux/efi.h>
#include <asm/efi.h>
#include <asm/memory.h>
#include <asm/sections.h>

#include "efistub.h"

#if defined CONFIG_UEFI_KASLR_SKIP_MEMMAP || defined(CONFIG_NOKASLR_MEM_RANGE)
enum mem_avoid_index {
	MEM_AVOID_MAX,
};

struct mem_vector {
	unsigned long long start;
	unsigned long long size;
};

static struct mem_vector mem_avoid[MEM_AVOID_MAX];

static bool mem_overlaps(struct mem_vector *one, struct mem_vector *two)
{
	if (one->start + one->size <= two->start)
		return false;
	if (one->start >= two->start + two->size)
		return false;
	return true;
}

static bool mem_avoid_overlap(struct mem_vector *region, struct mem_vector *overlap)
{
	int i;
	u64 earliest = region->start + region->size;
	bool is_overlapping = false;

	for (i = 0; i < MEM_AVOID_MAX; i++) {
		if (mem_overlaps(region, &mem_avoid[i]) &&
		    mem_avoid[i].start < earliest) {
			*overlap = mem_avoid[i];
			earliest = overlap->start;
			is_overlapping = true;
		}
	}
	return is_overlapping;
}

unsigned long cal_slots_avoid_overlap(efi_memory_desc_t *md, unsigned long size, u8 cal_type,
					  unsigned long align_shift, unsigned long target)
{
	struct mem_vector region, overlap;
	unsigned long region_end, first, last;
	unsigned long align = 1UL << align_shift;
	unsigned long total_slots = 0, slots;

	region.start = md->phys_addr;
	region_end = min(md->phys_addr + md->num_pages * EFI_PAGE_SIZE - 1, (u64)ULONG_MAX);

	while (region.start < region_end) {
		first = round_up(region.start, align);
		last = round_down(region_end - size + 1, align);

		if (first > last)
			break;

		region.size = region_end - region.start + 1;

		if (!mem_avoid_overlap(&region, &overlap)) {
			slots = ((last - first) >> align_shift) + 1;
			total_slots += slots;

			if (cal_type == CAL_SLOTS_PHYADDR)
				return first + target * align;

			break;
		}

		if (overlap.start >= region.start + size) {
			slots = ((round_up(overlap.start - size + 1, align) - first) >>
				align_shift) + 1;
			total_slots += slots;

			if (cal_type == CAL_SLOTS_PHYADDR) {
				if (target > slots)
					target -= slots;
				else
					return first + target * align;
			}
		}

		/* Clip off the overlapping region and start over. */
		region.start = overlap.start + overlap.size;
	}

	return total_slots;
}
#endif

efi_status_t handle_kernel_image(unsigned long *image_addr,
				 unsigned long *image_size,
				 unsigned long *reserve_addr,
				 unsigned long *reserve_size,
				 efi_loaded_image_t *image,
				 efi_handle_t image_handle)
{
	efi_status_t status;
	unsigned long kernel_size, kernel_codesize, kernel_memsize;

	if (image->image_base != _text) {
		efi_err("FIRMWARE BUG: efi_loaded_image_t::image_base has bogus value\n");
		image->image_base = _text;
	}

	if (!IS_ALIGNED((u64)_text, SEGMENT_ALIGN))
		efi_err("FIRMWARE BUG: kernel image not aligned on %dk boundary\n",
			SEGMENT_ALIGN >> 10);

	kernel_size = _edata - _text;
	kernel_codesize = __inittext_end - _text;
	kernel_memsize = kernel_size + (_end - _edata);
	*reserve_size = kernel_memsize;
	*image_addr = (unsigned long)_text;

	status = efi_kaslr_relocate_kernel(image_addr,
					   reserve_addr, reserve_size,
					   kernel_size, kernel_codesize,
					   kernel_memsize,
					   efi_kaslr_get_phys_seed(image_handle));
	if (status != EFI_SUCCESS)
		return status;

	return EFI_SUCCESS;
}

asmlinkage void primary_entry(void);

unsigned long primary_entry_offset(void)
{
	/*
	 * When built as part of the kernel, the EFI stub cannot branch to the
	 * kernel proper via the image header, as the PE/COFF header is
	 * strictly not part of the in-memory presentation of the image, only
	 * of the file representation. So instead, we need to jump to the
	 * actual entrypoint in the .text region of the image.
	 */
	return (char *)primary_entry - _text;
}

void efi_icache_sync(unsigned long start, unsigned long end)
{
	caches_clean_inval_pou(start, end);
}
