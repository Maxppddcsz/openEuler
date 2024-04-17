/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name     : roce_qp_post_send_extend.h
 * Version       : v1.1
 * Created       : 2021/8/20
 * Last Modified : 2021/9/22
 * Description   : Define extended kernel post_send realted function prototype.
 */

#ifndef ROCE_QP_POST_SEND_EXTEND_H
#define ROCE_QP_POST_SEND_EXTEND_H

#include <rdma/ib_verbs.h>
#include "roce_qp.h"

int roce3_post_send(struct ib_qp *ibqp, const struct ib_send_wr *wr, const struct ib_send_wr **bad_wr);

#endif // ROCE_QP_POST_SEND_EXTEND_H
