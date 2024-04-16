/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 * Description: RDMA cmdq command format.
 * Create: 2021-12-30
 */

#ifndef ROCE_VERBS_FORMAT_H
#define ROCE_VERBS_FORMAT_H

#include "roce_verbs_pub.h"
#include "roce_verbs_attr.h"


/* ********************************************************************************** */
/* * verbs struct */
typedef struct tag_roce_uni_cmd_gid {
	roce_verbs_cmd_header_s com;
	roce_verbs_gid_attr_s gid_attr;
} roce_uni_cmd_update_gid_s;

typedef struct tag_roce_uni_cmd_clear_gid {
	roce_verbs_cmd_header_s com;
	roce_verbs_clear_gid_info_s gid_clear;
} roce_uni_cmd_clear_gid_s;

typedef struct tag_roce_uni_cmd_qurey_gid {
	roce_verbs_cmd_header_s com;
} roce_uni_cmd_query_gid_s;

typedef struct tag_roce_uni_cmd_flush_mpt {
	roce_verbs_cmd_header_s com;
} roce_uni_cmd_flush_mpt_s;

typedef struct tag_roce_uni_cmd_mpt_query {
	roce_verbs_cmd_header_s com;
} roce_uni_cmd_mpt_query_s;

typedef struct tag_roce_uni_cmd_sw2hw_mpt {
	roce_verbs_cmd_header_s com;
	roce_verbs_mr_attr_s mr_attr; /* When creating a MR/MW, you need to enter the content of the MPT Context. */
} roce_uni_cmd_mpt_sw2hw_s;

typedef struct tag_roce_uni_cmd_modify_mpt {
	roce_verbs_cmd_header_s com;
	roce_verbs_mr_sge_s mr_sge;
} roce_uni_cmd_modify_mpt_s;

typedef struct tag_roce_uni_cmd_mpt_hw2sw {
	roce_verbs_cmd_header_s com;
	roce_verbs_mtt_cacheout_info_s dmtt_cache;
} roce_uni_cmd_mpt_hw2sw_s;

typedef struct tag_roce_uni_cmd_query_mtt {
	roce_verbs_cmd_header_s com;
	roce_verbs_query_mtt_info_s mtt_query;
} roce_uni_cmd_query_mtt_s;

typedef struct tag_roce_uni_cmd_creat_cq {
	roce_verbs_cmd_header_s com;
	roce_verbs_cq_attr_s cq_attr;
} roce_uni_cmd_creat_cq_s;

typedef struct tag_roce_uni_cmd_resize_cq {
	roce_verbs_cmd_header_s com;
	roce_verbs_cq_resize_info_s cq_resize;
} roce_uni_cmd_resize_cq_s;

typedef struct tag_roce_uni_cmd_modify_cq {
	roce_verbs_cmd_header_s com;
	roce_verbs_modify_cq_info_s cq_modify;
} roce_uni_cmd_modify_cq_s;

typedef struct tag_roce_uni_cmd_cq_hw2sw {
	roce_verbs_cmd_header_s com;
	roce_verbs_mtt_cacheout_info_s cmtt_cache;
} roce_uni_cmd_cq_hw2sw_s;

typedef struct tag_roce_uni_cmd_roce_cq_query {
	roce_verbs_cmd_header_s com;
} roce_uni_cmd_cq_query_s;

typedef struct tag_roce_uni_cmd_creat_srq {
	roce_verbs_cmd_header_s com;
	roce_verbs_srq_attr_s srq_attr;
} roce_uni_cmd_creat_srq_s;

typedef struct tag_roce_uni_cmd_srq_arm {
	roce_verbs_cmd_header_s com;
	roce_verbs_arm_srq_info_u srq_arm;
} roce_uni_cmd_srq_arm_s;

typedef struct tag_roce_uni_cmd_srq_hw2sw {
	roce_verbs_cmd_header_s com;
	roce_verbs_srq_hw2sw_info_s srq_cache;
} roce_uni_cmd_srq_hw2sw_s;

typedef struct tag_roce_uni_cmd_srq_query {
	roce_verbs_cmd_header_s com;
} roce_uni_cmd_srq_query_s;

typedef struct tag_roce_uni_cmd_modify_qpc {
	roce_verbs_cmd_header_s com;
	roce_verbs_qp_attr_s qp_attr;
} roce_uni_cmd_modify_qpc_s;

typedef struct tag_roce_uni_cmd_qp_modify2rst {
	roce_verbs_cmd_header_s com;
} roce_uni_cmd_qp_modify2rst_s;

typedef struct tag_roce_uni_cmd_qp_modify_rts2sqd {
	roce_verbs_cmd_header_s com;
	u32 sqd_event_en;
} roce_uni_cmd_qp_modify_rts2sqd_s;

typedef struct tag_roce_uni_cmd_qp_query {
	roce_verbs_cmd_header_s com;
} roce_uni_cmd_qp_query_s;

typedef struct tag_roce_uni_cmd_qp_cache_invalid {
	roce_verbs_cmd_header_s com;
	roce_verbs_qp_hw2sw_info_s qp_cache;
} roce_uni_cmd_qp_cache_invalid_s;

typedef struct tag_roce_uni_cmd_modify_ctx {
	roce_verbs_cmd_header_s com;
	roce_verbs_modify_ctx_info_s ctx_modify;
} roce_uni_cmd_modify_ctx_s;

typedef struct tag_roce_uni_cmd_cap_pkt {
	roce_verbs_cmd_header_s com;
} roce_uni_cmd_cap_pkt_s;


#endif /* ROCE_VERBS_FORMAT_H */
