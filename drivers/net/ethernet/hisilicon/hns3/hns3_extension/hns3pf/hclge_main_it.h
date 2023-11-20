/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (c) 2016-2017 Hisilicon Limited. */

#ifndef __HCLGE_MAIN_IT_H
#define __HCLGE_MAIN_IT_H

extern struct hnae3_ae_algo ae_algo;
extern struct hnae3_ae_ops hclge_ops;


/**
 * nic_event_fn_t - nic event handler prototype
 * @netdev:	net device
 * @hnae3_event_type_custom:	nic device event type
 */
typedef void (*nic_event_fn_t) (struct net_device *netdev,
				enum hnae3_event_type_custom);

int hclge_init(void);
#endif
