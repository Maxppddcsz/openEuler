/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (c) 2023 Hisilicon Limited. */

#ifndef __HNS3_EXT_H
#define __HNS3_EXT_H
#include <linux/types.h>
#include "hns3_enet.h"
#include "hnae3_ext.h"

int nic_netdev_match_check(struct net_device *netdev);
void nic_chip_recover_handler(struct net_device *ndev,
			      enum hnae3_event_type_custom event_t);
int nic_clean_stats64(struct net_device *ndev, struct rtnl_link_stats64 *stats);
int nic_set_cpu_affinity(struct net_device *ndev, cpumask_t *affinity_mask);
int nic_set_notify_pkt_param(struct net_device *ndev,
			     struct hnae3_notify_pkt_param *param);
int nic_set_notify_pkt_start(struct net_device *ndev);
#endif
