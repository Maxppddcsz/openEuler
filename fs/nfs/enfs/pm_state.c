// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: path state file
 * Create: 2023-08-12
 */
#include "pm_state.h"
#include <linux/sunrpc/xprt.h>

#include "enfs.h"
#include "enfs_log.h"

enum pm_path_state pm_get_path_state(struct rpc_xprt *xprt)
{
	struct enfs_xprt_context *ctx = NULL;
	enum pm_path_state state;

	if (xprt == NULL) {
		enfs_log_error("The xprt is not valid.\n");
		return PM_STATE_UNDEFINED;
	}

	xprt_get(xprt);

	ctx = (struct enfs_xprt_context *)xprt->multipath_context;
	if (ctx == NULL) {
		enfs_log_error("The xprt multipath ctx is not valid.\n");
		xprt_put(xprt);
		return PM_STATE_UNDEFINED;
	}

	state = atomic_read(&ctx->path_state);

	xprt_put(xprt);

	return state;
}

void pm_set_path_state(struct rpc_xprt *xprt, enum pm_path_state state)
{
	struct enfs_xprt_context *ctx = NULL;
	enum pm_path_state cur_state;

	if (xprt == NULL) {
		enfs_log_error("The xprt is not valid.\n");
		return;
	}

	xprt_get(xprt);

	ctx = (struct enfs_xprt_context *)xprt->multipath_context;
	if (ctx == NULL) {
		enfs_log_error("The xprt multipath ctx is not valid.\n");
		xprt_put(xprt);
		return;
	}

	cur_state = atomic_read(&ctx->path_state);
	if (cur_state == state) {
		enfs_log_debug("The xprt is already {%d}.\n", state);
		xprt_put(xprt);
		return;
	}

	atomic_set(&ctx->path_state, state);
	enfs_log_info("The xprt {%p} path state change from {%d} to {%d}.\n",
		xprt, cur_state, state);

	xprt_put(xprt);
}

void pm_get_path_state_desc(struct rpc_xprt *xprt, char *buf, int len)
{
	enum pm_path_state state;

	if (xprt == NULL) {
		enfs_log_error("The xprt is not valid.\n");
		return;
	}

	if ((buf == NULL) || (len <= 0)) {
		enfs_log_error("Buffer is not valid, len=%d.\n", len);
		return;
	}

	state = pm_get_path_state(xprt);

	switch (state) {
	case PM_STATE_INIT:
		(void)snprintf(buf, len, "Init");
		break;
	case PM_STATE_NORMAL:
		(void)snprintf(buf, len, "Normal");
		break;
	case PM_STATE_FAULT:
		(void)snprintf(buf, len, "Fault");
		break;
	default:
		(void)snprintf(buf, len, "Unknown");
		break;
	}
}

void pm_get_xprt_state_desc(struct rpc_xprt *xprt, char *buf, int len)
{
	int i;
	unsigned long state;
	static unsigned long xprt_mask[] = {
		XPRT_LOCKED, XPRT_CONNECTED,
		XPRT_CONNECTING, XPRT_CLOSE_WAIT,
		XPRT_BOUND, XPRT_BINDING, XPRT_CLOSING,
		XPRT_CONGESTED};

	static const char *const xprt_state_desc[] = {
		"LOCKED", "CONNECTED", "CONNECTING",
		"CLOSE_WAIT", "BOUND", "BINDING",
		"CLOSING", "CONGESTED"};
	int pos = 0;
	int ret = 0;

	if (xprt == NULL) {
		enfs_log_error("The xprt is not valid.\n");
		return;
	}

	if ((buf == NULL) || (len <= 0)) {
		enfs_log_error(
			"Xprt state buffer is not valid, len=%d.\n",
			len);
		return;
	}

	xprt_get(xprt);
	state = READ_ONCE(xprt->state);
	xprt_put(xprt);

	for (i = 0; i < ARRAY_SIZE(xprt_mask); ++i) {
		if (pos >= len)
			break;

		if (!test_bit(xprt_mask[i], &state))
			continue;

		if (pos == 0)
			ret = snprintf(buf, len, "%s", xprt_state_desc[i]);
		else
			ret = snprintf(buf + pos, len - pos, "|%s",
					xprt_state_desc[i]);

		if (ret < 0) {
			enfs_log_error("format state failed, ret %d.\n", ret);
			break;
		}

		pos += ret;
	}
}
