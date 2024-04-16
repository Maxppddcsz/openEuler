/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 * Description: RDMA cmdq extended attributes.
 * Create: 2021-12-30
 */

#ifndef ROCE_VERBS_EXT_ATTR_H
#define ROCE_VERBS_EXT_ATTR_H

#include "roce_verbs_pub.h"

#ifndef BIG_ENDIAN
#define BIG_ENDIAN	0x4321
#endif

enum VERBS_ATTR_EXT_TYPE_E {
	VERBS_ATTR_EXT_TYPE_NONE = 0,
	VERBS_ATTR_EXT_TYPE_SQP,
	VERBS_ATTR_EXT_TYPE_RSVD = 0xff
};

#define ROCE_VERBS_SQP_WQE_SIZE (2)

#pragma pack(4)

typedef struct tag_roce_verbs_sqp_attr {
	/* DW0 */
	roce_verbs_segment_hdr_s seg_hdr;

	/* DW1 */
	union {
		struct {
#if defined(BYTE_ORDER) && defined(BIG_ENDIAN) && ((BYTE_ORDER == BIG_ENDIAN))
			u32 rsvd0 : 26;
			u32 sqp_wqecnt_lth : 4;
			u32 sqp_wqecnt_rctl_en : 1;
			u32 sqp_ci_on_chip : 1;
#else
			u32 sqp_ci_on_chip : 1;
			u32 sqp_wqecnt_rctl_en : 1;
			u32 sqp_wqecnt_lth : 4;
			u32 rsvd0 : 26;
#endif
		} bs;
		u32 value;
	} dw1;
} roce_verbs_sqp_attr_s;

#pragma pack()

#endif /* ROCE_VERBS_EXT_ATTR_H */
