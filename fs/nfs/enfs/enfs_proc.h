/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Client-side ENFS PROC.
 *
 * Copyright (c) 2023. Huawei Technologies Co., Ltd. All rights reserved.
 */
#ifndef ENFS_PROC_H
#define ENFS_PROC_H

struct rpc_clnt;
struct rpc_task;
struct proc_dir_entry;

int enfs_proc_init(void);
void enfs_proc_exit(void);
struct proc_dir_entry *enfs_get_proc_parent(void);
int enfs_proc_create_clnt(struct rpc_clnt *clnt);
void enfs_proc_delete_clnt(struct rpc_clnt *clnt);
void enfs_count_iostat(struct rpc_task *task);

#endif
