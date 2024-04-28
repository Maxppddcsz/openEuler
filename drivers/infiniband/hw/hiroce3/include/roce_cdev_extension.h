/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 *
 * File Name     : roce_cdev_extension.h
 * Version       : v1.0
 * Created       : 2022/1/27
 * Last Modified : 2022/1/27
 * Description   : The definition of RoCE async event module standard callback function prototypes, macros etc.,
 */

#ifndef ROCE_CDEV_EXTENSION_H
#define ROCE_CDEV_EXTENSION_H

#include "roce.h"

#define NOT_SUPOORT_TYPE 0xFFFFFFFF

long ioctl_non_bonding_extend(unsigned int cmd, struct roce3_device *rdev, unsigned long arg);

#endif