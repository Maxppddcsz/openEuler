/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name	 : roce_xrc.h
 * Version	   : v2.0
 * Created	   : 2021/3/12
 * Last Modified : 2021/12/7
 * Description   : The definition of data structures of RoCE XRC features.
 */

#ifndef ROCE_XRC_H
#define ROCE_XRC_H

#include <rdma/ib_verbs.h>

#include "roce.h"

struct roce3_xrcd {
	struct ib_xrcd ibxrcd;
	u32 xrcdn;
	struct ib_pd *pd;
	struct ib_cq *cq;
};

static inline struct roce3_xrcd *to_roce3_xrcd(const struct ib_xrcd *ibxrcd)
{
	return container_of(ibxrcd, struct roce3_xrcd, ibxrcd);
}

#endif // ROCE_XRC_H
