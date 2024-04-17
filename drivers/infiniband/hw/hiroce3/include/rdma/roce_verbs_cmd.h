/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 * Description: RDMA cmdq commands.
 * Create: 2021-12-30
 */

#ifndef ROCE_VERBS_CMD_H
#define ROCE_VERBS_CMD_H

#include "rdma_context_format.h"
#include "roce_verbs_pub.h"


/* ************************************************* */
typedef struct tag_roce_verbs_cmd_com {
	union {
		u32 value;

		struct {
			u32 version : 8;
			u32 rsvd : 8;
			u32 cmd_bitmask : 16; // CMD_TYPE_BITMASK_E
		} bs;
	} dw0;

	u32 index; // qpn/cqn/srqn/mpt_index/gid idx
} roce_verbs_cmd_com_s;

typedef struct tag_roce_cmd_gid {
	roce_verbs_cmd_com_s com;

	u32 port;
	u32 rsvd;
	struct roce_gid_context gid_entry;
} roce_cmd_update_gid_s;

typedef struct tag_roce_clear_gid {
	roce_verbs_cmd_com_s com;

	u32 port;
	u32 gid_num;
} roce_cmd_clear_gid_s;

typedef struct tag_roce_qurey_gid {
	roce_verbs_cmd_com_s com;

	u32 port;
	u32 rsvd;
} roce_cmd_query_gid_s;

typedef struct tag_roce_flush_mpt {
	roce_verbs_cmd_com_s com;
} roce_cmd_mpt_s;

typedef struct tag_roce_cmd_flush_mpt {
	roce_verbs_cmd_com_s com;
} roce_cmd_flush_mpt_s;

typedef struct tag_roce_cmd_mpt_query {
	roce_verbs_cmd_com_s com;
} roce_cmd_mpt_query_s;

typedef struct tag_roce_sw2hw_mpt {
	roce_verbs_cmd_com_s com;
	struct roce_mpt_context mpt_entry; /* When creating a MR/MW, you need to enter the content of the MPT Context. */
} roce_cmd_mpt_sw2hw_s;

typedef struct tag_roce_cmd_modify_mpt {
	roce_verbs_cmd_com_s com;

	u32 new_key;

	/* DW2~3 */
	union {
		u64 length; /* Length of mr or mw */

		struct {
			u32 length_hi; /* Length of mr or mw */
			u32 length_lo; /* Length of mr or mw */
		} dw2;
	};

	/* DW4~5 */
	union {
		u64 iova; /* Start address of mr or mw */

		struct {
			u32 iova_hi; /* Upper 32 bits of the start address of mr or mw */
			u32 iova_lo; /* Lower 32 bits of the start address of mr or mw */
		} dw4;
	};
} roce_cmd_modify_mpt_s;

typedef struct tag_roce_cmd_mpt_hw2sw {
	roce_verbs_cmd_com_s com;

	u32 dmtt_flags;
	u32 dmtt_num;
	u32 dmtt_cache_line_start;
	u32 dmtt_cache_line_end;
	u32 dmtt_cache_line_size;
} roce_cmd_mpt_hw2sw_s;

typedef struct tag_roce_cmd_query_mtt {
	roce_verbs_cmd_com_s com;

	u32 mtt_addr_start_hi32;
	u32 mtt_addr_start_lo32;
	u32 mtt_num;
	u32 rsvd;
} roce_cmd_query_mtt_s;

typedef struct tag_roce_cmd_creat_cq {
	roce_verbs_cmd_com_s com;
	roce_cq_context_s cqc;
} roce_cmd_creat_cq_s;

typedef struct tag_roce_cmd_resize_cq {
	roce_verbs_cmd_com_s com;

	u32 rsvd;
	u32 page_size;	 /* Size of the resize buf page. */
	u32 log_cq_size;   /* Cq depth after resize */
	u32 mtt_layer_num; /* Number of mtt levels after resize */
	/* DW4~5 */
	union {
		u64 mtt_base_addr; /* Start address of mr or mw */
		u32 cqc_l0mtt_gpa[2];
	};
	u32 mtt_page_size; /* Size of the mtt page after resize. */
	roce_xq_mtt_info_s mtt_info;
} roce_cmd_resize_cq_s;

typedef struct tag_roce_cmd_modify_cq {
	roce_verbs_cmd_com_s com;
	u32 max_cnt;
	u32 timeout;
} roce_cmd_modify_cq_s;

typedef struct tag_roce_cmd_cq_hw2sw {
	roce_verbs_cmd_com_s com;
} roce_cmd_cq_hw2sw_s;

typedef struct tag_roce_cmd_cq_cache_invalidate {
	roce_verbs_cmd_com_s com;
	roce_xq_mtt_info_s mtt_info;
} roce_cmd_cq_cache_invalidate_s;

typedef struct tag_roce_cmd_roce_cq_query {
	roce_verbs_cmd_com_s com;
} roce_cmd_cq_query_s;

typedef struct tag_roce_cmd_creat_srq {
	roce_verbs_cmd_com_s com;
	roce_srq_context_s srqc;
} roce_cmd_creat_srq_s;

typedef struct tag_roce_cmd_srq_arm {
	roce_verbs_cmd_com_s com;
	union {
		u32 limitwater;
		struct {
#if defined(BYTE_ORDER) && defined(BIG_ENDIAN) && ((BYTE_ORDER == BIG_ENDIAN))
			u32 lwm : 16;
			u32 warth : 4;
			u32 th_up_en : 1;
			u32 cont_en : 1;
			u32 rsvd : 10;
#else
			u32 rsvd : 10;
			u32 cont_en : 1;
			u32 th_up_en : 1;
			u32 warth : 4;
			u32 lwm : 16;
#endif
		} bs;
	};
} roce_cmd_srq_arm_s;

typedef struct tag_roce_cmd_srq_hw2sw {
	roce_verbs_cmd_com_s com;
	roce_xq_mtt_info_s mtt_info;
	u32 srq_buf_len;
	u32 wqe_cache_line_start;
	u32 wqe_cache_line_end;
	u32 wqe_cache_line_size;
} roce_cmd_srq_hw2sw_s;

typedef struct tag_roce_cmd_srq_query {
	roce_verbs_cmd_com_s com;
} roce_cmd_srq_query_s;

typedef struct tag_roce_cmd_modify_qpc {
	roce_verbs_cmd_com_s com;

	u32 opt;
	u32 rsvd[3];
	roce_qp_context_s qpc;
} roce_cmd_modify_qpc_s;

typedef struct tag_roce_cmd_qp_modify2rst {
	roce_verbs_cmd_com_s com;
} roce_cmd_qp_modify2rst_s;

typedef struct tag_roce_cmd_qp_modify_rts2sqd {
	roce_verbs_cmd_com_s com;

	u32 sqd_event_en;
	u32 rsvd;
} roce_cmd_qp_modify_rts2sqd_s;

typedef struct tag_roce_cmd_qp_query {
	roce_verbs_cmd_com_s com;
} roce_cmd_qp_query_s;

typedef struct tag_roce_cmd_modify_ctx {
	roce_verbs_cmd_com_s com;
	u32 ctx_type;
	u32 offset;
	u32 value;
	u32 mask;
} roce_cmd_modify_ctx_s;

typedef struct tag_roce_cmd_cap_pkt {
	roce_verbs_cmd_com_s com;
} roce_cmd_cap_pkt_s;

typedef struct tag_roce_modify_hash_value {
	roce_verbs_cmd_com_s com;
	u32 hash_value;
} roce_modify_hash_value_s;

typedef struct tag_roce_modify_udp_src_port {
	roce_verbs_cmd_com_s com;
	u32 udp_src_port;
} roce_modify_udp_src_port_s;

typedef struct roce_get_qp_udp_src_port {
	roce_verbs_cmd_com_s com;
} roce_get_qp_udp_src_port_s;

typedef struct tag_roce_get_qp_rx_port {
	roce_verbs_cmd_com_s com;
} roce_get_qp_rx_port_s;

typedef struct tag_roce_get_qp_func_table {
	roce_verbs_cmd_com_s com;
} roce_get_qp_func_table_s;

#endif /* ROCE_VERBS_CMD_H */
