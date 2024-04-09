/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name     : roce_mr_extension.h
 * Version       : v1.0
 * Created       : 2021/1/27
 * Last Modified : 2022/1/27
 * Description   : The definition of RoCE MR module standard callback function prototypes, macros etc.,
 */

#ifndef ROCE_MR_EXTEND_H
#define ROCE_MR_EXTEND_H

#include <rdma/ib_verbs.h>

#include "hinic3_rdma.h"

#include "roce.h"

int roce3_check_alloc_mr_type(enum ib_mr_type mr_type);
enum rdma_mr_type roce3_get_mrtype(enum ib_mr_type ib_mr_type);

#endif

