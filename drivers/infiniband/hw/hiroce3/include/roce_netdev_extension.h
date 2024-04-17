/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name     : roce_netdev_extension.h
 * Version       : v1.1
 * Created       : 2021/10/18
 * Last Modified : 2022/1/26
 * Description   : The definition of RoCE net devices realated extended function prototypes, macros etc,.
 */

#ifndef ROCE_NETDEV_EXTENSION_H
#define ROCE_NETDEV_EXTENSION_H

#include "roce_netdev.h"

int roce3_add_real_device_mac(struct roce3_device *rdev, struct net_device *netdev);

int roce3_add_vlan_device_mac(struct roce3_device *rdev, struct net_device *netdev);

void roce3_del_real_device_mac(struct roce3_device *rdev);

void roce3_del_vlan_device_mac(struct roce3_device *rdev, struct roce3_vlan_dev_list *old_list);

void roce3_event_up_extend(struct roce3_device *rdev);

#endif /* ROCE_NETDEV_EXTENSION_H */
