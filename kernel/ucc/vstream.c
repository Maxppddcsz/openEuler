// SPDX-License-Identifier: GPL-2.0

#include <linux/syscalls.h>
#include <linux/vstream.h>

#include "ascend_vstream.h"

static int amdgpu_vstream_alloc(struct vstream_args *arg)
{
	return 0;
}
static int amdgpu_vstream_free(struct vstream_args *arg)
{
	return 0;
}
static int amdgpu_vstream_kick(struct vstream_args *arg)
{
	return 0;
}
static int amdgpu_vstream_update(struct vstream_args *arg)
{
	return 0;
}

/*
 * vstream_manage_cmd table
 */
static vstream_manage_t (*vstream_command_table[AMDGPU_MAX_COMMAND + 1]) = {
	ascend_vstream_alloc,			// ASCEND_VSTREAM_ALLOC
	ascend_vstream_free,			// ASCEND_VSTREAM_FREE
	ascend_vstream_kick,			// ASCEND_VSTREAM_KICK
	ascend_callback_vstream_wait,		// ASCEND_CALLBACK_VSTREAM_WAIT
	ascend_callback_vstream_kick,		// ASCEND_CALLBACK_VSTREAM_KICK
	ascend_vstream_get_head,		// ASCEND_VSTREAM_GET_HEAD
	NULL,					// ASCEND_MAX_COMMAND
	amdgpu_vstream_alloc,			// AMDGPU_VSTREAM_ALLOC
	amdgpu_vstream_free,			// AMDGPU_VSTREAM_FREE
	amdgpu_vstream_kick,			// AMDGPU_VSTREAM_KICK
	amdgpu_vstream_update,			// AMDGPU_VSTREAM_UPDATE
	NULL					// AMDGPU_MAX_COMMAND
};

SYSCALL_DEFINE2(vstream_manage, struct vstream_args __user *, arg, int, cmd)
{
	int res = 0;
	struct vstream_args vstream_arg;

	if (cmd > AMDGPU_MAX_COMMAND)
		return -EINVAL;

	if (copy_from_user(&vstream_arg, arg, sizeof(struct vstream_args))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}
	res = vstream_command_table[cmd](&vstream_arg);
	if (copy_to_user(arg, &vstream_arg, sizeof(struct vstream_args))) {
		pr_err("copy_to_user failed\n");
		return -EFAULT;
	}

	return res;
}
