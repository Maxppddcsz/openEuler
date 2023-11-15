// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include "enfs_errcode.h"
#include "enfs_log.h"
#include "enfs_config.h"

#define MAX_FILE_SIZE 8192
#define STRING_BUF_SIZE 128
#define CONFIG_FILE_PATH "/etc/enfs/config.ini"
#define ENFS_NOTIFY_FILE_PERIOD 1000UL

#define MAX_PATH_DETECT_INTERVAL 300
#define MIN_PATH_DETECT_INTERVAL 5
#define MAX_PATH_DETECT_TIMEOUT 60
#define MIN_PATH_DETECT_TIMEOUT 1
#define MAX_MULTIPATH_TIMEOUT 60
#define MIN_MULTIPATH_TIMEOUT 0
#define MAX_MULTIPATH_STATE ENFS_MULTIPATH_DISABLE
#define MIN_MULTIPATH_STATE ENFS_MULTIPATH_ENABLE

#define DEFAULT_PATH_DETECT_INTERVAL 10
#define DEFAULT_PATH_DETECT_TIMEOUT 5
#define DEFAULT_MULTIPATH_TIMEOUT 0
#define DEFAULT_MULTIPATH_STATE ENFS_MULTIPATH_ENABLE
#define DEFAULT_LOADBALANCE_MODE ENFS_LOADBALANCE_RR

typedef int (*check_and_assign_func)(char *, char *, int, int);

struct enfs_config_info {
	int path_detect_interval;
	int path_detect_timeout;
	int multipath_timeout;
	int loadbalance_mode;
	int multipath_state;
};

struct check_and_assign_value {
	char *field_name;
	check_and_assign_func func;
	int min_value;
	int max_value;
};

static struct enfs_config_info g_enfs_config_info;
static struct timespec64 modify_time;
static struct task_struct *thread;

static int enfs_check_config_value(char *value, int min_value, int max_value)
{
	unsigned long num_value;
	int ret;

	ret = kstrtol(value, 10, &num_value);
	if (ret != 0) {
		enfs_log_error("Failed to convert string to int\n");
		return -EINVAL;
	}

	if (num_value < min_value || num_value > max_value)
		return -EINVAL;

	return num_value;
}

static int enfs_check_and_assign_int_value(char *field_name, char *value,
						  int min_value, int max_value)
{
	int int_value = enfs_check_config_value(value, min_value, max_value);

	if (int_value < 0)
		return -EINVAL;

	if (strcmp(field_name, "path_detect_interval") == 0) {
		g_enfs_config_info.path_detect_interval = int_value;
		return ENFS_RET_OK;
	}
	if (strcmp(field_name, "path_detect_timeout") == 0) {
		g_enfs_config_info.path_detect_timeout = int_value;
		return ENFS_RET_OK;
	}
	if (strcmp(field_name, "multipath_timeout") == 0) {
		g_enfs_config_info.multipath_timeout = int_value;
		return ENFS_RET_OK;
	}
	if (strcmp(field_name, "multipath_disable") == 0) {
		g_enfs_config_info.multipath_state = int_value;
		return ENFS_RET_OK;
	}
	return -EINVAL;
}

static int enfs_check_and_assign_loadbalance_mode(char *field_name,
							 char *value,
							 int min_value,
							 int max_value)
{
	if (value == NULL)
		return -EINVAL;

	if (strcmp(field_name, "multipath_select_policy") == 0) {
		if (strcmp(value, "roundrobin") == 0) {
			g_enfs_config_info.loadbalance_mode
				= ENFS_LOADBALANCE_RR;
			return ENFS_RET_OK;
		}
	}
	return -EINVAL;
}

static const struct check_and_assign_value g_check_and_assign_value[] = {
	{"path_detect_interval", enfs_check_and_assign_int_value,
	MIN_PATH_DETECT_INTERVAL, MAX_PATH_DETECT_INTERVAL},
	{"path_detect_timeout", enfs_check_and_assign_int_value,
	MIN_PATH_DETECT_TIMEOUT, MAX_PATH_DETECT_TIMEOUT},
	{"multipath_timeout", enfs_check_and_assign_int_value,
	MIN_MULTIPATH_TIMEOUT, MAX_MULTIPATH_TIMEOUT},
	{"multipath_disable", enfs_check_and_assign_int_value,
	MIN_MULTIPATH_STATE, MAX_MULTIPATH_STATE},
	{"multipath_select_policy", enfs_check_and_assign_loadbalance_mode,
	0, 0},
};

static int enfs_read_config_file(char *buffer, char *file_path)
{
	int ret;
	struct file *filp = NULL;
	loff_t f_pos = 0;
	mm_segment_t fs;


	filp = filp_open(file_path, O_RDONLY, 0);

	if (IS_ERR(filp)) {
		enfs_log_error("Failed to open file %s\n", CONFIG_FILE_PATH);
		ret = -ENOENT;
		return ret;
	}

	fs = get_fs();
	set_fs(get_ds());
	kernel_read(filp, buffer, MAX_FILE_SIZE, &f_pos);
	set_fs(fs);

	ret = filp_close(filp, NULL);
	if (ret) {
		enfs_log_error("Close File:%s failed:%d.\n",
			      CONFIG_FILE_PATH, ret);
		return -EINVAL;
	}
	return ENFS_RET_OK;
}

static int enfs_deal_with_comment_line(char *buffer)
{
	int ret;
	char *pos = strchr(buffer, '\n');

	if (pos != NULL)
		ret = strlen(buffer) - strlen(pos);
	else
		ret = strlen(buffer);

	return ret;
}

static int enfs_parse_key_value_from_config(char *buffer, char *key,
					       char *value, int keyLen,
					       int valueLen)
{
	char *line;
	char *tokenPtr;
	int len;
	char *tem;
	char *pos = strchr(buffer, '\n');

	if (pos != NULL)
		len = strlen(buffer) - strlen(pos);
	else
		len = strlen(buffer);

	line = kmalloc(len + 1, GFP_KERNEL);
	if (!line) {
		enfs_log_error("Failed to allocate memory.\n");
		return -ENOMEM;
	}
	line[len] = '\0';
	strncpy(line, buffer, len);

	tem = line;
	tokenPtr = strsep(&tem, "=");
	if (tokenPtr == NULL || tem == NULL) {
		kfree(line);
		return len;
	}
	strncpy(key, strim(tokenPtr), keyLen);
	strncpy(value, strim(tem), valueLen);

	kfree(line);
	return len;
}

static int enfs_get_value_from_config_file(char *buffer, char *field_name,
					      char *value, int valueLen)
{
	int ret;
	char key[STRING_BUF_SIZE + 1] = {0};
	char val[STRING_BUF_SIZE + 1] = {0};

	while (buffer[0] != '\0') {
		if (buffer[0] == '\n') {
			buffer++;
		} else if (buffer[0] == '#') {
			ret = enfs_deal_with_comment_line(buffer);
			if (ret > 0)
				buffer += ret;
		} else {
			ret = enfs_parse_key_value_from_config(buffer, key, val,
							      STRING_BUF_SIZE,
							      STRING_BUF_SIZE);
			if (ret < 0) {
				enfs_log_error("failed parse key value, %d\n"
					, ret);
				return ret;
			}
			key[STRING_BUF_SIZE] = '\0';
			val[STRING_BUF_SIZE] = '\0';

			buffer += ret;

			if (strcmp(field_name, key) == 0) {
				strncpy(value, val, valueLen);
				return ENFS_RET_OK;
			}
		}
	}
	enfs_log_error("can not find value which matched field_name: %s.\n",
			  field_name);
	return -EINVAL;
}

int enfs_config_load(void)
{
	char value[STRING_BUF_SIZE + 1];
	int ret;
	int table_len;
	int min;
	int max;
	int i;
	char *buffer;

	buffer = kmalloc(MAX_FILE_SIZE, GFP_KERNEL);
	if (!buffer) {
		enfs_log_error("Failed to allocate memory.\n");
		return -ENOMEM;
	}
	memset(buffer, 0, MAX_FILE_SIZE);

	g_enfs_config_info.path_detect_interval = DEFAULT_PATH_DETECT_INTERVAL;
	g_enfs_config_info.path_detect_timeout = DEFAULT_PATH_DETECT_TIMEOUT;
	g_enfs_config_info.multipath_timeout = DEFAULT_MULTIPATH_TIMEOUT;
	g_enfs_config_info.multipath_state = DEFAULT_MULTIPATH_STATE;
	g_enfs_config_info.loadbalance_mode = DEFAULT_LOADBALANCE_MODE;

	table_len = sizeof(g_check_and_assign_value) /
				sizeof(g_check_and_assign_value[0]);

	ret = enfs_read_config_file(buffer, CONFIG_FILE_PATH);
	if (ret != 0) {
		kfree(buffer);
		return ret;
	}

	for (i = 0; i < table_len; i++) {
		ret = enfs_get_value_from_config_file(buffer,
				g_check_and_assign_value[i].field_name,
				value, STRING_BUF_SIZE);
		if (ret < 0)
			continue;

		value[STRING_BUF_SIZE] = '\0';
		min = g_check_and_assign_value[i].min_value;
		max = g_check_and_assign_value[i].max_value;
		if (g_check_and_assign_value[i].func != NULL)
			(*g_check_and_assign_value[i].func)(
				g_check_and_assign_value[i].field_name,
				value, min, max);
	}

	kfree(buffer);
	return ENFS_RET_OK;
}

int enfs_get_config_path_detect_interval(void)
{
	return g_enfs_config_info.path_detect_interval;
}

int enfs_get_config_path_detect_timeout(void)
{
	return g_enfs_config_info.path_detect_timeout;
}

int enfs_get_config_multipath_timeout(void)
{
	return g_enfs_config_info.multipath_timeout;
}

int enfs_get_config_multipath_state(void)
{
	return g_enfs_config_info.multipath_state;
}

int enfs_get_config_loadbalance_mode(void)
{
	return g_enfs_config_info.loadbalance_mode;
}

static bool enfs_file_changed(const char *filename)
{
	int err;
	struct kstat file_stat;

	err = vfs_stat(filename, &file_stat);
	if (err) {
		pr_err("failed to open file:%s err:%d\n", filename, err);
		return false;
	}

	if (timespec64_compare(&modify_time, &file_stat.mtime) == -1) {
		modify_time = file_stat.mtime;
		pr_info("file change: %lld %lld\n", modify_time.tv_sec,
			   file_stat.mtime.tv_sec);
		return true;
	}

	return false;
}

static int enfs_thread_func(void *data)
{
	while (!kthread_should_stop()) {
		if (enfs_file_changed(CONFIG_FILE_PATH))
			enfs_config_load();

		msleep(ENFS_NOTIFY_FILE_PERIOD);
	}
	return 0;
}

int enfs_config_timer_init(void)
{
	thread = kthread_run(enfs_thread_func, NULL, "enfs_notiy_file_thread");
	if (IS_ERR(thread)) {
		pr_err("Failed to create kernel thread\n");
		return PTR_ERR(thread);
	}
	return 0;
}

void enfs_config_timer_exit(void)
{
	pr_info("enfs_notify_file_exit\n");
	if (thread)
		kthread_stop(thread);
}
