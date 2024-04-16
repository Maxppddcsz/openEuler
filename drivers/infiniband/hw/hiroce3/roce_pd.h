/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name	 : roce_pd.h
 * Version	   : v2.0
 * Created	   : 2021/3/12
 * Last Modified : 2021/12/7
 * Description   : struct related to pd
 */

#ifndef ROCE_PD_H
#define ROCE_PD_H

#include <rdma/ib_verbs.h>

#include "roce.h"

#define PD_RESP_SIZE (2 * sizeof(u32))
struct roce3_pd {
	struct ib_pd ibpd;
	u32 pdn;
	u16 func_id;
	u16 rsvd;
};

static inline struct roce3_pd *to_roce3_pd(const struct ib_pd *ibpd)
{
	return container_of(ibpd, struct roce3_pd, ibpd);
}

#endif // ROCE_PD_H
