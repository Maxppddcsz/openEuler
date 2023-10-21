#ifdef CONFIG_SECDETECTOR
#ifndef _SECDETECTOR_H
#define _SECDETECTOR_H

#include <linux/tracepoint.h>
#include <linux/binfmts.h>
#include <linux/ctype.h>

struct secDetector_file {
	struct file *file;
	struct dentry *dentry;
	struct dentry *new_dentry;	
	unsigned int rename_flag;
	int opflag;
	const char *name;
	const void *value;
	size_t value_len;
	struct inode *oldinode;
	struct inode *newinode;
	struct inode *dir;
	const struct path *path;
	umode_t mode;
	kuid_t uid;
	kgid_t gid;
	const char __user *buf;
	size_t buf_len;
	int flags;	
};

struct secDetector_net {
	struct socket *sock;
	struct socket *newsock;
	struct sockaddr *address;
	int addrlen;
	struct msghdr *msg;
	int size;
	int backlog;
};

struct secDetector_task {
	struct task_struct *task;
	const struct cred *old_cred;
	const struct cred *new_cred;
	const struct linux_binprm *bprm;
};

typedef u32 compat_uptr_t;

struct sec_user_arg_ptr {
#ifdef CONFIG_COMPAT
	bool is_compat;
#endif
	const char __user *const __user *native;
#ifdef CONFIG_COMPAT
	const compat_uptr_t __user *compat;
#endif
};

struct secDetector_api {
	const char *api_name;
	struct task_struct *cur_task;
	struct task_struct *arg_task;
	struct filename *exec_filename;
	struct sec_user_arg_ptr argv;
	struct sec_user_arg_ptr envp;
	int dfd;
	const char __user *pipe_name;
	umode_t mode;
	unsigned int dev;
};

enum SECDETECTOR_FILE_EVENT {
	SECDETECTOR_FILE_WRITE = 1, 
	SECDETECTOR_FILE_UNLINK_NFS = 2, 
	SECDETECTOR_FILE_UNLINK = 3, 
	SECDETECTOR_FILE_OPEN = 4, 
	SECDETECTOR_FILE_RENAME = 5, 
	SECDETECTOR_FILE_SETXATTR = 6, 
	SECDETECTOR_FILE_SETXATTR2 = 7,
	SECDETECTOR_FILE_REMOVEXATTR = 8,
	SECDETECTOR_FILE_READ = 9,
	SECDETECTOR_FILE_CHMOD = 10,
	SECDETECTOR_FILE_CHOWN = 11,
	SECDETECTOR_FILE_UTIMES = 12,
	SECDETECTOR_FILE_RENAME_PRE = 15,
	SECDETECTOR_FILE_WRITE_PRE = 14,
	SECDETECTOR_FILE_READ_PRE = 13,
	SECDETECTOR_FILE_UNLINK_PRE = 16, 
	SECDETECTOR_FILE_SETXATTR_PRE = 17, 
	SECDETECTOR_FILE_REMOVEXATTR_PRE = 18, 
	SECDETECTOR_FILE_CHMOD_PRE = 19, 
	SECDETECTOR_FILE_CHOWN_PRE = 20, 
	SECDETECTOR_FILE_UTIMES_PRE = 21, 
	SECDETECTOR_FILE_NUM,
};

enum SECDETECTOR_TASK_EVENT {
	SECDETECTOR_TASK_BPRM_CHECK = 1, 
	SECDETECTOR_TASK_CRED_CHANGE = 2, 
	SECDETECTOR_TASK_NUM,
};

enum SECDETECTOR_API_EVENT {
	SECDETECTOR_API_PTRACE = 1,
	SECDETECTOR_API_MEMFD = 2,
	SECDETECTOR_API_EXECVE = 3,
	SECDETECTOR_API_PIPE = 4,
	SECDETECTOR_API_MKNOD = 5,
	SECDETECTOR_API_NUM,
};

extern bool secDetector_enable;

DECLARE_TRACE(secDetector_chkfsevent,
		TP_PROTO(struct secDetector_file *file, int flag, int *sec_ret),
		TP_ARGS(file, flag, sec_ret));

DECLARE_TRACE(secDetector_chknetevent,
		TP_PROTO(struct secDetector_net *net, int flag, int *sec_ret),
		TP_ARGS(net, flag, sec_ret));

DECLARE_TRACE(secDetector_chktaskevent,
		TP_PROTO(struct secDetector_task *task, int flag, int *sec_ret),
		TP_ARGS(task, flag, sec_ret));

DECLARE_TRACE(secDetector_chkapievent,
		TP_PROTO(struct secDetector_api *api, int flag, int *sec_ret),
		TP_ARGS(api, flag, sec_ret));
#endif
#endif


