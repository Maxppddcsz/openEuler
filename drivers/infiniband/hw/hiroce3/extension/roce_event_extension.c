/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 *
 * File Name	 : roce_event_extension.c
 * Version	   : v1.0
 * Created	   : 2022/1/27
 * Last Modified : 2022/1/27
 * Description   : The definition of RoCE async event module standard callback functions.
 */

#include "roce_event_extension.h"

#ifndef PANGEA_NOF
void roce3_event_report_extend(const struct roce3_device *rdev, int event_str_index)
{
	/*lint -e160 -e522*/
	roce3_pr_err_once("[ROCE] %s: [non ofed event type] Invalid extend event type error. function id: %u\n", __func__,
		rdev->glb_func_id);
	/*lint +e160 +e522*/
}

int roce3_async_event_handle_extend(u8 event_type, u8 *val, struct roce3_device *rdev)
{
	int event_str_index = 0;

	event_str_index = to_unkown_event_str_index(event_type, val);

	return event_str_index;
}
#endif
