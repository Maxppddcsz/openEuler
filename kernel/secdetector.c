// SPDX-License-Identifier: GPL-2.0-only

#ifdef CONFIG_SECDETECTOR

#include <linux/secdetector.h>

DEFINE_TRACE(secdetector_chkfsevent,
	     TP_PROTO(struct secdetector_file *file, int flag, int *sec_ret),
	     TP_ARGS(file, flag, sec_ret));
EXPORT_TRACEPOINT_SYMBOL(secdetector_chkfsevent);

DEFINE_TRACE(secdetector_chknetevent,
	     TP_PROTO(struct secdetector_net *net, int flag, int *sec_ret),
	     TP_ARGS(net, flag, sec_ret));
EXPORT_TRACEPOINT_SYMBOL(secdetector_chknetevent);

DEFINE_TRACE(secdetector_chktaskevent,
	     TP_PROTO(struct secdetector_task *task, int flag, int *sec_ret),
	     TP_ARGS(task, flag, sec_ret));
EXPORT_TRACEPOINT_SYMBOL(secdetector_chktaskevent);

DEFINE_TRACE(secdetector_chkapievent,
	     TP_PROTO(struct secdetector_api *api, int flag, int *sec_ret),
	     TP_ARGS(api, flag, sec_ret));
EXPORT_TRACEPOINT_SYMBOL(secdetector_chkapievent);

bool secdetector_enable __ro_after_init;
static int __init secdetector_enable_setup(char *str)
{
	secdetector_enable = true;
	return 1;
}
__setup("secdetector_enable", secdetector_enable_setup);

#endif
