// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#define pr_fmt(fmt) "hisi_l3t: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/mm.h>

#include "hisi_l3t.h"

static bool hisi_l3t_test_slot_empty(struct hisi_l3t *l3t, int slot_idx)
{
	unsigned long addr;
	int size;

	if (slot_idx < 0 || slot_idx >= L3T_REG_NUM) {
		pr_err("slot index is invalid: %d\n", slot_idx);
		return false;
	}

	hisi_l3t_read(l3t, slot_idx, &addr, &size);

	return addr == 0;
}

static int hisi_l3t_lock_sccl(struct hisi_sccl *sccl, unsigned long addr,
			      int size)
{
	int ccl_cnt = sccl->ccl_cnt;
	int i, slot_idx;
	bool empty;
	struct hisi_l3t *l3t;

	if (!sccl->l3t || !sccl->l3t[0])
		return -ENODEV;

	l3t = sccl->l3t[0];

	mutex_lock(&l3t_mutex);
	for (slot_idx = 0; slot_idx < L3T_REG_NUM; slot_idx++) {
		empty = hisi_l3t_test_slot_empty(l3t, slot_idx);
		if (empty)
			break;
	}

	if (slot_idx >= L3T_REG_NUM) {
		mutex_unlock(&l3t_mutex);
		return -ENOMEM;
	}

	for (i = 0; i < ccl_cnt; i++) {
		l3t = sccl->l3t[i];
		if (!l3t)
			continue;

		hisi_l3t_lock(l3t, slot_idx, addr, size);
	}
	mutex_unlock(&l3t_mutex);

	return 0;
}

int l3t_shared_lock(int nid, unsigned long pfn, unsigned long size)
{
	struct hisi_sccl *sccl;

	sccl = hisi_l3t_get_sccl(nid);
	if (!sccl || !sccl->ccl_cnt)
		return -ENODEV;

	return hisi_l3t_lock_sccl(sccl, pfn << PAGE_SHIFT, size);
}
EXPORT_SYMBOL_GPL(l3t_shared_lock);

static bool hisi_l3t_test_equal(struct hisi_l3t *l3t, int slot_idx,
				unsigned long addr, int size)
{
	unsigned long addr_inner;
	int size_inner;

	hisi_l3t_read(l3t, slot_idx, &addr_inner, &size_inner);

	return (addr_inner == addr && size_inner == size);
}

static int hisi_l3t_unlock_sccl(struct hisi_sccl *sccl, unsigned long addr,
				int size)
{
	int ccl_cnt = sccl->ccl_cnt;
	int slot_idx, i;
	bool equal;
	struct hisi_l3t *l3t;

	if (!sccl->l3t || !sccl->l3t[0])
		return -ENODEV;

	l3t = sccl->l3t[0];

	mutex_lock(&l3t_mutex);
	for (slot_idx = 0; slot_idx < L3T_REG_NUM; slot_idx++) {
		equal = hisi_l3t_test_equal(l3t, slot_idx, addr, size);
		if (equal)
			break;
	}

	if (slot_idx >= L3T_REG_NUM) {
		mutex_unlock(&l3t_mutex);
		return -EINVAL;
	}

	for (i = 0; i < ccl_cnt; i++) {
		l3t = sccl->l3t[i];
		if (!l3t)
			continue;

		hisi_l3t_unlock(l3t, slot_idx);
	}
	mutex_unlock(&l3t_mutex);

	return 0;
}

int l3t_shared_unlock(int nid, unsigned long pfn, unsigned long size)
{
	struct hisi_sccl *sccl;

	sccl = hisi_l3t_get_sccl(nid);
	if (!sccl || !sccl->ccl_cnt)
		return -ENODEV;

	return hisi_l3t_unlock_sccl(sccl, pfn << PAGE_SHIFT, size);
}
EXPORT_SYMBOL_GPL(l3t_shared_unlock);
