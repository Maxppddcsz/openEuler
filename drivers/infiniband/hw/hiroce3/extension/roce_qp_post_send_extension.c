/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name	 : roce_qp_post_send_standard.c
 * Version	   : v1.2
 * Created	   : 2021/8/20
 * Last Modified : 2021/11/23
 * Description   : Define standard kernel post_send realted function prototype.
 */

#include "roce_qp_post_send_extension.h"

int roce3_post_send(struct ib_qp *ibqp, const struct ib_send_wr *wr, const struct ib_send_wr **bad_wr)
{
	return roce3_post_send_standard(ibqp, (const struct ib_send_wr *)wr, (const struct ib_send_wr **)bad_wr);
}
