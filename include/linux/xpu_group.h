/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __XPU_GROUP_H__
#define __XPU_GROUP_H__
#include <linux/idr.h>

struct xpu_group;
struct xcu;

enum xpu_type {
	XPU_TYPE_ROOT,
	XPU_TYPE_TASK_QUEUE,
	XPU_TYPE_NPU_310,
	XPU_TYPE_CUSTOM,
};

enum xpu_capability_type {
	TYPE_1,
	XPU_CAPABILITY_TYPE_NR,
};

struct xpu_capability {
	unsigned long capacities[XPU_CAPABILITY_TYPE_NR];
};

struct xpu_operation {
	int (*run)(struct xpu_group *group, void *para1, void *para2);
	int (*finish)(struct xpu_group *group, void *para1, void *para2);
	int (*wait)(struct xpu_group *group, void *para1, void *para2,
		    void *para3);
	int (*complete)(struct xpu_group *group, void *para1, void *para2,
			void *para3);
};

struct xpu_group {
	int id;
	enum xpu_type type;
	struct xpu_capability *capability;

	struct xpu_group *previous_layer;
	struct idr next_layer;

	struct xpu_operation *opt;

	int used;

	void *data;
};

extern struct xpu_group *xpu_root;

#ifdef CONFIG_XPU_SCHEDULE
int xpu_group_attach(struct xpu_group *new_group,
		     struct xpu_group *previous_group);
int xpu_group_detach(struct xpu_group *group);
struct xpu_group *xpu_group_find(struct xpu_group *group, int id);
struct xpu_group *xpu_idle_group_find(struct xpu_group *group);
struct xpu_group *xpu_group_alloc(void);
struct xpu_group *xpu_group_alloc_and_attach(struct xpu_group *previous_group,
					     int id);
int xpu_run(struct xpu_group *group, void *para1, void *para2);
int xpu_finish(struct xpu_group *group, void *para1, void *para2);
int xpu_wait(struct xpu_group *group, void *para1, void *para2, void *para3);
#endif

#endif
