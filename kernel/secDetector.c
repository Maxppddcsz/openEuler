#ifdef CONFIG_SECDETECTOR

#include <linux/secDetector.h>

DEFINE_TRACE(secDetector_chkfsevent,
		TP_PROTO(struct secDetector_file *file, int flag, int *sec_ret),
		TP_ARGS(file, flag, sec_ret));
EXPORT_TRACEPOINT_SYMBOL(secDetector_chkfsevent);

DEFINE_TRACE(secDetector_chknetevent,
		TP_PROTO(struct secDetector_net *net, int flag, int *sec_ret),
		TP_ARGS(net, flag, sec_ret));
EXPORT_TRACEPOINT_SYMBOL(secDetector_chknetevent);

DEFINE_TRACE(secDetector_chktaskevent,
		TP_PROTO(struct secDetector_task *task, int flag, int *sec_ret),
		TP_ARGS(task, flag, sec_ret));
EXPORT_TRACEPOINT_SYMBOL(secDetector_chktaskevent);

DEFINE_TRACE(secDetector_chkapievent,
		TP_PROTO(struct secDetector_api *api, int flag, int *sec_ret),
		TP_ARGS(api, flag, sec_ret));
EXPORT_TRACEPOINT_SYMBOL(secDetector_chkapievent);

bool secDetector_enable __ro_after_init;
static int __init secDetector_enable_setup(char *str)
{
	secDetector_enable = true;
	return 1;
}
__setup("secDetector_enable", secDetector_enable_setup);

#endif
