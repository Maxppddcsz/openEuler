// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/xpu_group.h>
#include <linux/rwsem.h>
#include <linux/slab.h>

extern int ucc_rt_nr_running(struct xcu *cu);
static DECLARE_RWSEM(xpu_group_rwsem);

static struct xpu_capability xpu_capability_root;

struct xpu_group __xpu_root = {
	.type = XPU_TYPE_ROOT,
	.capability = &xpu_capability_root,

	.next_layer = IDR_INIT(next_layer),
};

struct xpu_group *xpu_root = &__xpu_root;
EXPORT_SYMBOL(xpu_root);

int __xpu_group_attach(struct xpu_group *new_group,
		       struct xpu_group *previous_group)
{
	int id = new_group->id;

	if (id == -1)
		id = idr_alloc(&previous_group->next_layer, new_group,
			       0, INT_MAX, GFP_KERNEL);
	else
		id = idr_alloc(&previous_group->next_layer, new_group,
			       id, id + 1, GFP_KERNEL);
	if (id < 0)
		return -EEXIST;

	new_group->id = id;
	new_group->previous_layer = previous_group;

	return 0;
}

int xpu_group_attach(struct xpu_group *new_group,
		     struct xpu_group *previous_group)
{
	int ret;

	down_write(&xpu_group_rwsem);
	ret = __xpu_group_attach(new_group, previous_group);
	up_write(&xpu_group_rwsem);
	return ret;
}
EXPORT_SYMBOL(xpu_group_attach);

struct xpu_group *xpu_group_alloc_and_attach(struct xpu_group *previous_group,
					      int id)
{
	struct xpu_group *new = xpu_group_alloc();

	if (!new) {
		pr_err("alloc xpu_group failed\n");
		return NULL;
	}

	new->id = id;

	if (!xpu_group_attach(new, previous_group))
		return NULL;

	return new;
}
EXPORT_SYMBOL(xpu_group_alloc_and_attach);

int __xpu_group_detach(struct xpu_group *group)
{
	idr_remove(&group->previous_layer->next_layer, group->id);
	return 0;
}

int xpu_group_detach(struct xpu_group *group)
{
	int ret;

	down_write(&xpu_group_rwsem);
	ret = __xpu_group_detach(group);
	up_write(&xpu_group_rwsem);
	return ret;
}
EXPORT_SYMBOL(xpu_group_detach);

struct xpu_group *__xpu_group_find(struct xpu_group *group, int id)
{
	return idr_find(&group->next_layer, id);
}

struct xpu_group *xpu_group_find(struct xpu_group *group, int id)
{
	struct xpu_group *p;

	p = xpu_group_alloc();

	down_read(&xpu_group_rwsem);
	p = __xpu_group_find(group, id);
	up_read(&xpu_group_rwsem);

	return p;
}
EXPORT_SYMBOL(xpu_group_find);


struct xpu_group *xpu_idle_group_find(struct xpu_group *group)
{
	struct xpu_group *entry_group;
	int id;

	down_read(&xpu_group_rwsem);
	idr_for_each_entry(&group->next_layer, entry_group, id) {
		if (!entry_group->used) {
			up_read(&xpu_group_rwsem);
			return entry_group;
		}
	}
	up_read(&xpu_group_rwsem);

	return NULL;
}

int xpu_run(struct xpu_group *group, void *para1, void *para2)
{
	int ret = 0;

	if (group->opt && group->opt->run)
		ret = group->opt->run(group, para1, para2);

	return ret;
}

int xpu_finish(struct xpu_group *group, void *para1, void *para2)
{
	if (group->opt && group->opt->finish)
		return group->opt->finish(group, para1, para2);

	return 0;
}

int xpu_wait(struct xpu_group *group, void *para1, void *para2, void *para3)
{
	if (group->opt && group->opt->wait)
		return group->opt->wait(group, para1, para2, para3);

	return 0;
}

int xpu_complete(struct xpu_group *group, void *para1, void *para2, void *para3)
{
	if (group->opt && group->opt->complete)
		return group->opt->complete(group, para1, para2, para3);

	return 0;
}

struct xpu_group *xpu_group_alloc(void)
{
	struct xpu_group *node = kzalloc(sizeof(*node), GFP_KERNEL);

	if (!node)
		return NULL;

	node->type = XPU_TYPE_CUSTOM;
	idr_init(&node->next_layer);

	return node;
}
EXPORT_SYMBOL(xpu_group_alloc);
