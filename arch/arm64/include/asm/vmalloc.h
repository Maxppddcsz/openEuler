#ifndef _ASM_ARM64_VMALLOC_H
#define _ASM_ARM64_VMALLOC_H

#include <asm/page.h>
#include <asm/pgtable.h>

#ifdef CONFIG_HAVE_ARCH_HUGE_VMAP

#define arch_vmap_pud_supported arch_vmap_pud_supported
static inline bool arch_vmap_pud_supported(pgprot_t prot)
{
	/*
	 * SW table walks can't handle removal of intermediate entries.
	 */
	return pud_sect_supported() &&
	       !IS_ENABLED(CONFIG_PTDUMP_DEBUGFS);
}

#define arch_vmap_pmd_supported arch_vmap_pmd_supported
static inline bool arch_vmap_pmd_supported(pgprot_t prot)
{
	/* See arch_vmap_pud_supported() */
	return !IS_ENABLED(CONFIG_PTDUMP_DEBUGFS);
}

#endif

#define arch_vmap_pgprot_tagged arch_vmap_pgprot_tagged
static inline pgprot_t arch_vmap_pgprot_tagged(pgprot_t prot)
{
	return pgprot_tagged(prot);
}

#ifdef CONFIG_RANDOMIZE_BASE
extern u64 module_alloc_base;
#define arch_vmap_skip_module_region arch_vmap_skip_module_region
static inline void arch_vmap_skip_module_region(unsigned long *addr,
						unsigned long vstart,
						unsigned long size,
						unsigned long align)
{
	u64 module_alloc_end = module_alloc_base + MODULES_VSIZE;

	if (vstart == module_alloc_base)
		return;

	if (IS_ENABLED(CONFIG_KASAN_GENERIC) ||
	    IS_ENABLED(CONFIG_KASAN_SW_TAGS))
		/* don't exceed the static module region - see module_alloc() */
		module_alloc_end = MODULES_END;

	if ((module_alloc_base >= *addr + size) ||
	    (module_alloc_end <= *addr))
		return;

	*addr = ALIGN(module_alloc_end, align);
}
#endif

#endif /* _ASM_ARM64_VMALLOC_H */
