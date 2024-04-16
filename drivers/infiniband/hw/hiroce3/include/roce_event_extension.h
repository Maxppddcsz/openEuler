/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 *
 * File Name     : roce_event_extension.h
 * Version       : v1.0
 * Created       : 2022/1/27
 * Last Modified : 2022/1/27
 * Description   : The definition of RoCE async event module standard callback function prototypes, macros etc.,
 */

#ifndef ROCE_EVENT_EXTENSION_H
#define ROCE_EVENT_EXTENSION_H

#include "roce_event.h"

void roce3_event_report_extend(const struct roce3_device *rdev, int event_str_index);

int roce3_async_event_handle_extend(u8 event_type, u8 *val, struct roce3_device *rdev);

#endif /* ROCE_EVENT_EXTENSION_H */
