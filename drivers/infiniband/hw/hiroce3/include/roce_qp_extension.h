/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name	 : roce_qp_extension.h
 * Version	   : v1.0
 * Created	   : 2021/12/20
 * Last Modified : 2021/12/20
 * Description   : Define extended kernel QP realted function prototype.
 */

#ifndef ROCE_QP_EXTENSION_H
#define ROCE_QP_EXTENSION_H

#include <rdma/ib_verbs.h>

#include "roce_qp.h"
#include "roce_qp_exp.h"

int to_roce3_qp_type(enum ib_qp_type qp_type);

bool roce3_check_qp_modify_ok(enum ib_qp_state cur_state, enum ib_qp_state next_state, enum ib_qp_type type,
	enum ib_qp_attr_mask mask, enum rdma_link_layer ll);

int roce3_create_qp_pre_ext(struct roce3_device *rdev, struct roce3_qp *rqp, struct ib_qp_init_attr *init_attr);

int roce3_create_qp_user_pre_ext(struct ib_qp_init_attr *init_attr, struct roce3_qp *rqp, u32 *qpn);

int roce3_create_qp_user_post_ext(struct ib_pd *ibpd, struct roce3_device *rdev, struct roce3_qp *rqp,
	struct ib_qp_init_attr *init_attr);

int roce3_qp_modify_cmd_ext(cqm_cmd_buf_s *cqm_cmd_inbuf, struct roce3_qp *rqp, roce_verbs_qp_attr_s *qp_attr,
	u32 optpar);

bool roce3_need_qpn_lb1_consistent_srqn(const struct roce3_qp *rqp, const struct roce3_device *rdev,
	const struct ib_qp_init_attr *init_attr);

int roce3_is_qp_normal(struct roce3_qp *rqp, struct ib_qp_init_attr *init_attr);

void roce3_set_qp_dif_attr(struct roce3_qp *rqp, const struct ib_qp_init_attr *init_attr,
	const struct roce3_device *rdev);

int roce3_qp_modify_pre_extend(struct roce3_qp *rqp, struct ib_qp_attr *attr, int attr_mask, struct ib_udata *udata);

#ifdef ROCE_EXTEND

#define QPC_ROCE_VBS_QPC_OFFSET_FOR_SQPC 204800 // 200K

#define ROCE_QP_VBS_FLAG (1U << 22)

struct roce3_set_qp_ext_attr_cmd {
	u32 qpn;
	u32 attr_mask;
};

struct roce3_modify_qp_vbs_cmd {
	u32 qpn;
	u64 ci_record_addr;
};

struct roce3_vbs_qp {
	cqm_qpc_mpt_s *vbs_sqpc_info; /* vbs private data */
	struct roce3_db db;		   /* to record ci_record_pa */
};

long roce3_set_qp_ext_attr(struct roce3_device *rdev, void *buf);
long roce3_vbs_create_sqpc(struct roce3_device *rdev, void *buf);
#endif

#endif /* ROCE_QP_EXTENSION_H */
