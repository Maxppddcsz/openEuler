/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PKRAM_H
#define _LINUX_PKRAM_H

#include <linux/types.h>
#include <linux/mm_types.h>

struct pkram_node_page;

struct pkram_stream {
	struct pkram_node_page *node;
	u64 current_data_pfn;
	u64 current_data_offset;
};

#define PKRAM_NAME_MAX		256	/* including nul */

int pkram_prepare_save(struct pkram_stream *ps, const char *name);
int pkram_prepare_save_obj(struct pkram_stream *ps);
void pkram_finish_save(struct pkram_stream *ps);
void pkram_finish_save_obj(struct pkram_stream *ps);
void pkram_discard_save(struct pkram_stream *ps);

int pkram_prepare_load(struct pkram_stream *ps, const char *name);
int pkram_prepare_load_obj(struct pkram_stream *ps);
void pkram_finish_load(struct pkram_stream *ps);
void pkram_finish_load_obj(struct pkram_stream *ps);

int pkram_save_page(struct pkram_stream *ps, struct page *page);
struct page *pkram_load_page(struct pkram_stream *ps);

int pkram_save_chunk(struct pkram_stream *ps, const void *buf, size_t size);
int pkram_load_chunk(struct pkram_stream *ps, void *buf, size_t size);

struct page *pkram_alloc_pages(unsigned int order);
struct page *pkram_alloc_page(void);
void pkram_free_pages(struct page *page, unsigned int order);
void pkram_free_page(struct page *page);
void pkram_get_page(struct page *page);
void pkram_put_page(struct page *page);

bool pkram_available(void);

int pkram_init_sb(void);

#endif /* _LINUX_PKRAM_H */
