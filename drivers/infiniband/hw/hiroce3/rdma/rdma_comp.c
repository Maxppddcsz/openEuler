/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name	 : rdma_comp.c
 * Version	   : v2.0
 * Created	   : 2021/3/10
 * Last Modified : 2021/12/23
 * Description   : implement the management of PDID, XRCD, MPT, MTT, RDMARC,
 * GID and GUID
 */

#include <linux/module.h>
#include <linux/netdevice.h>

#include "hinic3_hw.h"
#include "rdma_comp.h"


struct rdma_comp_priv *get_rdma_comp_priv(void *hwdev)
{
	struct rdma_comp_priv *comp_private = NULL;
	comp_private = (struct rdma_comp_priv *)hinic3_get_service_adapter(hwdev, SERVICE_T_ROCE);
	return comp_private;
}

void rdma_cleanup_pd_table(struct rdma_comp_priv *comp_priv)
{
	rdma_bitmap_cleanup(&comp_priv->pd_bitmap);
}
