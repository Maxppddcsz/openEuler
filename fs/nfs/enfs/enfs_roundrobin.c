// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kref.h>
#include <linux/rculist.h>
#include <linux/types.h>
#include <linux/sunrpc/xprt.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/xprtmultipath.h>
#include "enfs_roundrobin.h"

#include "enfs.h"
#include "enfs_config.h"
#include "pm_state.h"

typedef struct rpc_xprt *(*enfs_xprt_switch_find_xprt_t)(
	struct rpc_xprt_switch *xps, const struct rpc_xprt *cur);
static const struct rpc_xprt_iter_ops enfs_xprt_iter_roundrobin;
static const struct rpc_xprt_iter_ops enfs_xprt_iter_singular;

static bool enfs_xprt_is_active(struct rpc_xprt *xprt)
{
	enum pm_path_state state;

	if (kref_read(&xprt->kref) <= 0)
		return false;

	state = pm_get_path_state(xprt);
	if (state == PM_STATE_NORMAL)
		return true;

	return false;
}

static struct rpc_xprt *enfs_lb_set_cursor_xprt(
	struct rpc_xprt_switch *xps, struct rpc_xprt **cursor,
	enfs_xprt_switch_find_xprt_t find_next)
{
	struct rpc_xprt *pos;
	struct rpc_xprt *old;

	old = smp_load_acquire(cursor); /* read latest cursor */
	pos = find_next(xps, old);
	smp_store_release(cursor, pos); /* let cursor point to pos */
	return pos;
}

static
struct rpc_xprt *enfs_lb_find_next_entry_roundrobin(
		struct rpc_xprt_switch *xps, const struct rpc_xprt *cur)
{
	struct rpc_xprt *pos;
	struct rpc_xprt *prev = NULL;
	bool found = false;
	struct rpc_xprt *min_queuelen_xprt = NULL;
	unsigned long pos_xprt_queuelen;
	unsigned long min_xprt_queuelen = 0;

	unsigned long xps_queuelen = atomic_long_read(&xps->xps_queuelen);
	// delete origin xprt
	unsigned int multipath_nactive = READ_ONCE(xps->xps_nactive) - 1;

	list_for_each_entry_rcu(pos, &xps->xps_xprt_list, xprt_switch) {
		if (enfs_is_main_xprt(pos) || !enfs_xprt_is_active(pos)) {
			prev = pos;
			continue;
		}

		pos_xprt_queuelen = atomic_long_read(&pos->queuelen);
		if (min_queuelen_xprt == NULL ||
			pos_xprt_queuelen < min_xprt_queuelen) {

			min_queuelen_xprt = pos;
			min_xprt_queuelen = pos_xprt_queuelen;
		}

		if (cur == prev)
			found = true;

		if (found && pos_xprt_queuelen *
			multipath_nactive <= xps_queuelen)
			return pos;
		prev = pos;
	};

	return min_queuelen_xprt;
}

struct rpc_xprt *enfs_lb_switch_find_first_active_xprt(
			struct rpc_xprt_switch *xps)
{
	struct rpc_xprt *pos;

	list_for_each_entry_rcu(pos, &xps->xps_xprt_list, xprt_switch) {
		if (enfs_xprt_is_active(pos))
			return pos;
	};
	return NULL;
}

struct rpc_xprt *enfs_lb_switch_get_main_xprt(struct rpc_xprt_switch *xps)
{
	return list_first_or_null_rcu(&xps->xps_xprt_list,
				struct rpc_xprt, xprt_switch);
}

static struct rpc_xprt *enfs_lb_switch_get_next_xprt_roundrobin(
			struct rpc_xprt_switch *xps, const struct rpc_xprt *cur)
{
	struct rpc_xprt *xprt;

	// disable multipath
	if (enfs_get_config_multipath_state())
		return enfs_lb_switch_get_main_xprt(xps);

	xprt = enfs_lb_find_next_entry_roundrobin(xps, cur);
	if (xprt != NULL)
		return xprt;

	return enfs_lb_switch_get_main_xprt(xps);
}

static
struct rpc_xprt *enfs_lb_iter_next_entry_roundrobin(struct rpc_xprt_iter *xpi)
{
	struct rpc_xprt_switch *xps = rcu_dereference(xpi->xpi_xpswitch);

	if (xps == NULL)
		return NULL;

	return enfs_lb_set_cursor_xprt(xps, &xpi->xpi_cursor,
				enfs_lb_switch_get_next_xprt_roundrobin);
}

static
struct rpc_xprt *enfs_lb_switch_find_singular_entry(
			struct rpc_xprt_switch *xps, const struct rpc_xprt *cur)
{
	struct rpc_xprt *pos;
	bool found = false;

	list_for_each_entry_rcu(pos, &xps->xps_xprt_list, xprt_switch) {
		if (cur == pos)
			found = true;

		if (found && enfs_xprt_is_active(pos))
			return pos;
	}
	return NULL;
}

struct rpc_xprt *enfs_lb_get_singular_xprt(
			struct rpc_xprt_switch *xps, const struct rpc_xprt *cur)
{
	struct rpc_xprt *xprt;

	if (xps == NULL)
		return NULL;

	// disable multipath
	if (enfs_get_config_multipath_state())
		return enfs_lb_switch_get_main_xprt(xps);

	if (cur == NULL || xps->xps_nxprts < 2)
		return enfs_lb_switch_find_first_active_xprt(xps);

	xprt = enfs_lb_switch_find_singular_entry(xps, cur);
	if (!xprt)
		return enfs_lb_switch_get_main_xprt(xps);

	return xprt;
}

static
struct rpc_xprt *enfs_lb_iter_next_entry_sigular(struct rpc_xprt_iter *xpi)
{
	struct rpc_xprt_switch *xps = rcu_dereference(xpi->xpi_xpswitch);

	if (xps == NULL)
		return NULL;

	return enfs_lb_set_cursor_xprt(xps, &xpi->xpi_cursor,
				enfs_lb_get_singular_xprt);
}

static void enfs_lb_iter_default_rewind(struct rpc_xprt_iter *xpi)
{
	WRITE_ONCE(xpi->xpi_cursor, NULL);
}

static void enfs_lb_switch_set_roundrobin(struct rpc_clnt *clnt)
{
	struct rpc_xprt_switch *xps;

	rcu_read_lock();
	xps = rcu_dereference(clnt->cl_xpi.xpi_xpswitch);
	rcu_read_unlock();
	if (clnt->cl_vers == 3) {

		if (READ_ONCE(xps->xps_iter_ops) != &enfs_xprt_iter_roundrobin)
			WRITE_ONCE(xps->xps_iter_ops,
			&enfs_xprt_iter_roundrobin);

		return;
	}
	if (READ_ONCE(xps->xps_iter_ops) != &enfs_xprt_iter_singular)
		WRITE_ONCE(xps->xps_iter_ops, &enfs_xprt_iter_singular);
}

static
struct rpc_xprt *enfs_lb_switch_find_current(struct list_head *head,
			const struct rpc_xprt *cur)
{
	struct rpc_xprt *pos;

	list_for_each_entry_rcu(pos, head, xprt_switch) {
		if (cur == pos)
			return pos;
	}
	return NULL;
}

static struct rpc_xprt *enfs_lb_iter_current_entry(struct rpc_xprt_iter *xpi)
{
	struct rpc_xprt_switch *xps = rcu_dereference(xpi->xpi_xpswitch);
	struct list_head *head;

	if (xps == NULL)
		return NULL;
	head = &xps->xps_xprt_list;
	if (xpi->xpi_cursor == NULL || xps->xps_nxprts < 2)
		return enfs_lb_switch_get_main_xprt(xps);
	return enfs_lb_switch_find_current(head, xpi->xpi_cursor);
}

void enfs_lb_set_policy(struct rpc_clnt *clnt)
{
	enfs_lb_switch_set_roundrobin(clnt);
}

static const struct rpc_xprt_iter_ops enfs_xprt_iter_roundrobin = {
	.xpi_rewind = enfs_lb_iter_default_rewind,
	.xpi_xprt = enfs_lb_iter_current_entry,
	.xpi_next = enfs_lb_iter_next_entry_roundrobin,
};

static const struct rpc_xprt_iter_ops enfs_xprt_iter_singular = {
	.xpi_rewind = enfs_lb_iter_default_rewind,
	.xpi_xprt = enfs_lb_iter_current_entry,
	.xpi_next = enfs_lb_iter_next_entry_sigular,
};
