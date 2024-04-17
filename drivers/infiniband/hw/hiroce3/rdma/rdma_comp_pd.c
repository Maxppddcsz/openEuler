/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name	 : rdma_comp_pds.c
 * Version	   : v2.0
 * Created	   : 2021/3/10
 * Last Modified : 2021/12/23
 * Description   : implement the management of PD
 */

#include <linux/module.h>
#include <linux/netdevice.h>

#include "hinic3_hw.h"
#include "rdma_comp.h"

int roce3_rdma_pd_alloc(void *hwdev, u32 *pdn)
{
	struct rdma_comp_priv *comp_priv = NULL;

	if ((hwdev == NULL) || (pdn == NULL)) {
		pr_err("%s: Hwdev or pdn is null\n", __FUNCTION__);
		return -EINVAL;
	}

	comp_priv = get_rdma_comp_priv(hwdev);
	if (comp_priv == NULL) {
		pr_err("%s: Comp_priv is null\n", __FUNCTION__);
		return -EINVAL;
	}

	*pdn = rdma_bitmap_alloc(&comp_priv->pd_bitmap);
	if (*pdn == RDMA_INVALID_INDEX) {
		pr_err("%s: Can't get valid pdn, err(%d)\n", __FUNCTION__, -ENOMEM);
		return -ENOMEM;
	}

	return 0;
}

void roce3_rdma_pd_free(void *hwdev, u32 pdn)
{
	struct rdma_comp_priv *comp_priv = NULL;

	if (hwdev == NULL) {
		pr_err("%s: Hwdev is null\n", __FUNCTION__);
		return;
	}

	comp_priv = get_rdma_comp_priv(hwdev);
	if (comp_priv == NULL) {
		pr_err("%s: Comp_priv is null\n", __FUNCTION__);
		return;
	}

	rdma_bitmap_free(&comp_priv->pd_bitmap, pdn);
}
