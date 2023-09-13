/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _ASCEND_VSTREAM_H
#define _ASCEND_VSTREAM_H

int ascend_vstream_alloc(struct vstream_args *arg);
int ascend_vstream_free(struct vstream_args *arg);
int ascend_vstream_kick(struct vstream_args *arg);
int ascend_callback_vstream_wait(struct vstream_args *arg);
int ascend_callback_vstream_kick(struct vstream_args *arg);
int ascend_vstream_get_head(struct vstream_args *arg);

#endif /* _ASCEND_VSTREAM_H */
