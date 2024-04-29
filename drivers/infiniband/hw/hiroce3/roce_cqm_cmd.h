/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name	 : roce_cpm_cmd.h
 * Created	   : 2021/3/10
 * Description   : The definition of roce cqm cmd management
 */

#ifndef ROCE_CQM_CMD_H
#define ROCE_CQM_CMD_H

#include "hinic3_cqm.h"

#include "roce.h"

void roce3_cqm_cmd_free_inoutbuf(void *ex_handle, cqm_cmd_buf_s *cqm_cmd_inbuf, cqm_cmd_buf_s *cqm_cmd_outbuf);
int roce3_cqm_cmd_zalloc_inoutbuf(void *ex_handle, cqm_cmd_buf_s **cqm_cmd_inbuf, u16 inbuf_size,
	cqm_cmd_buf_s **cqm_cmd_outbuf, u16 outbuf_size);

#endif // ROCE_CQM_CMD_H
