/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name     : roce_srq_extension.h
 * Version       : v1.0
 * Created       : 2021/12/7
 * Last Modified : 2021/12/7
 * Description   : The definition of RoCE SRQ module standard callback function prototypes, macros etc.,
 */

#ifndef ROCE_SRQ_EXTENSION_H
#define ROCE_SRQ_EXTENSION_H

#include "roce_srq.h"

void roce3_srq_container_init(struct ib_srq_init_attr *init_attr, struct roce3_srq *rsrq, struct roce3_device *rdev);

void roce3_create_user_srq_update_ext(u32 *cqn, u32 srqn);

#endif /* ROCE_SRQ_EXTENSION_H */
