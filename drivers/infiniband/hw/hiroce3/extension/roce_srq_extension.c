/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name	 : roce_srq_extension.c
 * Version	   : v1.0
 * Created	   : 2021/12/7
 * Last Modified : 2021/12/24
 * Description   : The definition of RoCE SRQ module standard callback functions.
 */

#include "roce_srq_extension.h"

#ifndef ROCE_CHIP_TEST
void roce3_srq_container_init(struct ib_srq_init_attr *init_attr, struct roce3_srq *rsrq, struct roce3_device *rdev)
{
	rsrq->xrc_en = (init_attr->srq_type == IB_SRQT_XRC);

	if (rsrq->xrc_en != 0) {
		rsrq->container_flag = rdev->cfg_info.srq_container_en;
	}

	rsrq->container_mode =
		(rsrq->xrc_en != 0) ? rdev->cfg_info.xrc_srq_container_mode : rdev->cfg_info.srq_container_mode;
	rsrq->container_warn_th = roce3_calculate_cont_th(init_attr->attr.srq_limit);
	rsrq->rqe_cnt_th = rdev->cfg_info.warn_th;
	rsrq->container_size = roce3_get_container_sz(rsrq->container_mode);
}
#endif

#ifndef PANGEA_NOF
void roce3_create_user_srq_update_ext(u32 *cqn, u32 srqn)
{
	return;
}
#endif