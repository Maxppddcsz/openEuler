/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __LINUX_DYNAMIC_POOL_H
#define __LINUX_DYNAMIC_POOL_H

#include <linux/memcontrol.h>
#include <linux/hugetlb.h>

#ifdef CONFIG_DYNAMIC_POOL

DECLARE_STATIC_KEY_FALSE(dynamic_pool_key);
#define dpool_enabled (static_branch_unlikely(&dynamic_pool_key))

enum pages_pool_type {
	PAGES_POOL_1G,
	PAGES_POOL_2M,
	PAGES_POOL_4K,
	PAGES_POOL_MAX,
};

struct pages_pool {
	unsigned long free_pages;
	unsigned long used_pages;
	struct list_head freelist;
};

struct dynamic_pool_ops;

struct dynamic_pool {
	refcount_t refcnt;
	bool online;
	struct mem_cgroup *memcg;
	struct dynamic_pool_ops *ops;

	spinlock_t lock;
	struct pages_pool pool[PAGES_POOL_MAX];

	/* Used for dynamic hugetlb */
	int nid;
	unsigned long total_pages;
};

void dynamic_pool_inherit(struct mem_cgroup *parent, struct mem_cgroup *memcg);
int dynamic_pool_destroy(struct cgroup *cgrp, bool *clear_css_online);

bool dynamic_pool_hide_files(struct cftype *cft);
int dynamic_pool_add_memory(struct mem_cgroup *memcg, int nid,
			    unsigned long size);
void dynamic_pool_show(struct mem_cgroup *memcg, struct seq_file *m);

#else
struct dynamic_pool {};

static inline void dynamic_pool_inherit(struct mem_cgroup *parent,
					struct mem_cgroup *memcg)
{
}

static inline int dynamic_pool_destroy(struct cgroup *cgrp,
				       bool *clear_css_online)
{
	return 0;
}

#ifdef CONFIG_CGROUPS
static inline bool dynamic_pool_hide_files(struct cftype *cft)
{
	return false;
}
#endif
#endif /* CONFIG_DYNAMIC_POOL */
#endif /* __LINUX_DYNAMIC_POOL_H */
