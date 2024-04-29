/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name	 : roce_cpm_cmd.c
 * Created	   : 2021/3/10
 * Description   : implement of roce cqm cmd management
 */

#include "roce_cqm_cmd.h"

void roce3_cqm_cmd_free_inoutbuf(void *ex_handle, cqm_cmd_buf_s *cqm_cmd_inbuf, cqm_cmd_buf_s *cqm_cmd_outbuf)
{
	if (cqm_cmd_outbuf != NULL) {
		cqm_cmd_free(ex_handle, cqm_cmd_outbuf);
	}

	if (cqm_cmd_inbuf != NULL) {
		cqm_cmd_free(ex_handle, cqm_cmd_inbuf);
	}
}

int roce3_cqm_cmd_zalloc_inoutbuf(void *ex_handle, cqm_cmd_buf_s **cqm_cmd_inbuf, u16 inbuf_size,
	cqm_cmd_buf_s **cqm_cmd_outbuf, u16 outbuf_size)
{
	int ret;

	if (cqm_cmd_inbuf != NULL) {
		*cqm_cmd_inbuf = cqm_cmd_alloc(ex_handle);

		if (*cqm_cmd_inbuf == NULL) {
			ret = -ENOMEM;
			goto err;
		}
		(*cqm_cmd_inbuf)->size = inbuf_size;

		memset((*cqm_cmd_inbuf)->buf, 0, inbuf_size);
	}

	if (cqm_cmd_outbuf != NULL) {
		*cqm_cmd_outbuf = cqm_cmd_alloc(ex_handle);

		if (*cqm_cmd_outbuf == NULL) {
			ret = -ENOMEM;
			goto err;
		}
		(*cqm_cmd_outbuf)->size = outbuf_size;

		memset((*cqm_cmd_outbuf)->buf, 0, outbuf_size);
	}

	return 0;
err:
	roce3_cqm_cmd_free_inoutbuf(ex_handle, *cqm_cmd_inbuf, *cqm_cmd_outbuf);
	*cqm_cmd_inbuf = NULL;
	*cqm_cmd_outbuf = NULL;
	return ret;
}
