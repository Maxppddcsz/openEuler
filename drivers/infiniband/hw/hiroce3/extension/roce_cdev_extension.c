/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name	 : roce_cdev_extension.c
 * Version	   : v2.0
 * Created	   : 2021/7/1
 * Last Modified : 2022/7/1
 * Description   : The definition of RoCE Async Event realated functions in kernel space.
 */
#ifndef PANGEA_NOF

#include "roce_cdev_extension.h"

long ioctl_non_bonding_extend(unsigned int cmd, struct roce3_device *rdev, unsigned long arg)
{
	return NOT_SUPOORT_TYPE;
}
#endif