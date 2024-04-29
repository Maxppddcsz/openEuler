/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2022. All rights reserved.
 * Description   : RoCE service ulp common API b/w driver and MPU
 * Author		: /
 * Create		: /
 * Notes		 : /
 * History	   : /
 */

#ifndef ROCE_MPU_ULP_COMMON_H
#define ROCE_MPU_ULP_COMMON_H

#include "roce_mpu_common.h"

#define ROCE_MAX_DD_ID 32
#define ROCE_MAX_DD_SIZE 4096
#define ROCE_SET_DD_SIZE_ONCE 1024

/* ********************************** AA cmd between driver and mpu ********************************** */
typedef struct roce_ulp_aa_ctrl_ready_cmd {
	struct comm_info_head head;

	u16 func_id;
	u8 rsvd[2];
} roce_ulp_aa_ctrl_ready_cmd_s;

typedef struct roce_ulp_aa_clear_act_ctrl_bmp_cmd {
	struct comm_info_head head;

	u16 func_id;
	u8 rsvd[2];
} roce_ulp_aa_clear_act_ctrl_bmp_cmd_s;

typedef struct roce_ulp_aa_set_dd_cfg_cmd {
	struct comm_info_head head;

	u8 dd_id;
	u8 offset;
	u16 data_len; // max:4096

	u8 data[1024];
} roce_ulp_aa_set_dd_cfg_cmd_s;

typedef struct roce_ulp_aa_switch_io_cmd {
	struct comm_info_head head;
	u16 func_id;
	u8 rsvd[2];
} roce_ulp_aa_switch_io_cmd_s;

typedef struct roce_ulp_aa_set_fake_data_addr_cmd {
	struct comm_info_head head;
	u32 fake_data_gpa_h;
	u32 fake_data_gpa_l;
} roce_ulp_aa_set_fake_data_addr_cmd_s;

#endif /* ROCE_MPU_ULP_COMMON_H */
