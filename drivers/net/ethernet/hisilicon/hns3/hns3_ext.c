// SPDX-License-Identifier: GPL-2.0+
// Copyright (c) 2023 Hisilicon Limited.

#include "hns3_ext.h"

int nic_netdev_match_check(struct net_device *ndev)
{
#define HNS3_DRIVER_NAME_LEN 5

	struct ethtool_drvinfo drv_info;
	struct hnae3_handle *h;

	if (!ndev || !ndev->ethtool_ops ||
	    !ndev->ethtool_ops->get_drvinfo)
		return -EINVAL;

	ndev->ethtool_ops->get_drvinfo(ndev, &drv_info);

	if (strncmp(drv_info.driver, "hns3", HNS3_DRIVER_NAME_LEN))
		return -EINVAL;

	h = hns3_get_handle(ndev);
	if (h->flags & HNAE3_SUPPORT_VF)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(nic_netdev_match_check);

static int nic_invoke_pri_ops(struct net_device *ndev, int opcode,
			      void *data, size_t length)

{
	struct hnae3_handle *h;
	int ret;

	if ((!data && length) || (data && !length)) {
		netdev_err(ndev, "failed to check data and length");
		return -EINVAL;
	}

	if (nic_netdev_match_check(ndev))
		return -ENODEV;

	h = hns3_get_handle(ndev);
	if (!h->ae_algo->ops->priv_ops)
		return -EOPNOTSUPP;

	ret = h->ae_algo->ops->priv_ops(h, opcode, data, length);
	if (ret)
		netdev_err(ndev,
			   "failed to invoke pri ops, opcode = %#x, ret = %d\n",
			   opcode, ret);

	return ret;
}

void nic_chip_recover_handler(struct net_device *ndev,
			      enum hnae3_event_type_custom event_t)
{
	dev_info(&ndev->dev, "reset type is %d!!\n", event_t);

	if (event_t == HNAE3_PPU_POISON_CUSTOM)
		event_t = HNAE3_FUNC_RESET_CUSTOM;

	if (event_t != HNAE3_FUNC_RESET_CUSTOM &&
	    event_t != HNAE3_GLOBAL_RESET_CUSTOM &&
	    event_t != HNAE3_IMP_RESET_CUSTOM) {
		dev_err(&ndev->dev, "reset type err!!\n");
		return;
	}

	nic_invoke_pri_ops(ndev, HNAE3_EXT_OPC_RESET,
			   &event_t, sizeof(event_t));
}
EXPORT_SYMBOL(nic_chip_recover_handler);

int nic_clean_stats64(struct net_device *ndev, struct rtnl_link_stats64 *stats)
{
	struct hnae3_knic_private_info *kinfo;
	struct hns3_enet_ring *ring;
	struct hns3_nic_priv *priv;
	struct hnae3_handle *h;
	int i, ret;

	priv = netdev_priv(ndev);
	h = hns3_get_handle(ndev);
	kinfo = &h->kinfo;

	rtnl_lock();
	if (!test_bit(HNS3_NIC_STATE_INITED, &priv->state) ||
	    test_bit(HNS3_NIC_STATE_RESETTING, &priv->state)) {
		ret = -EBUSY;
		goto end_unlock;
	}

	ret = nic_invoke_pri_ops(ndev, HNAE3_EXT_OPC_CLEAN_STATS64,
				 NULL, 0);
	if (ret)
		goto end_unlock;

	for (i = 0; i < kinfo->num_tqps; i++) {
		ring = &priv->ring[i];
		memset(&ring->stats, 0, sizeof(struct ring_stats));
		ring = &priv->ring[i + kinfo->num_tqps];
		memset(&ring->stats, 0, sizeof(struct ring_stats));
	}

	memset(&ndev->stats, 0, sizeof(struct net_device_stats));
	netdev_info(ndev, "clean stats succ\n");

end_unlock:
	rtnl_unlock();
	return ret;
}
EXPORT_SYMBOL(nic_clean_stats64);

int nic_set_cpu_affinity(struct net_device *ndev, cpumask_t *affinity_mask)
{
	struct hns3_enet_tqp_vector *tqp_vector;
	struct hns3_nic_priv *priv;
	int ret = 0;
	u16 i;

	if (!ndev || !affinity_mask) {
		netdev_err(ndev,
			   "Invalid input param when set ethernet cpu affinity\n");
		return -EINVAL;
	}

	if (nic_netdev_match_check(ndev))
		return -ENODEV;

	priv = netdev_priv(ndev);
	rtnl_lock();
	if (!test_bit(HNS3_NIC_STATE_INITED, &priv->state) ||
	    test_bit(HNS3_NIC_STATE_RESETTING, &priv->state)) {
		ret = -EBUSY;
		goto err_unlock;
	}

	for (i = 0; i < priv->vector_num; i++) {
		tqp_vector = &priv->tqp_vector[i];
		if (tqp_vector->irq_init_flag != HNS3_VECTOR_INITED)
			continue;

		tqp_vector->affinity_mask = *affinity_mask;

		ret = irq_set_affinity_hint(tqp_vector->vector_irq, NULL);
		if (ret) {
			netdev_err(ndev,
				   "failed to reset affinity hint, ret = %d\n", ret);
			goto err_unlock;
		}

		ret = irq_set_affinity_hint(tqp_vector->vector_irq,
					    &tqp_vector->affinity_mask);
		if (ret) {
			netdev_err(ndev,
				   "failed to set affinity hint, ret = %d\n", ret);
			goto err_unlock;
		}
	}

	netdev_info(ndev, "set nic cpu affinity %*pb succeed\n",
		    cpumask_pr_args(affinity_mask));

err_unlock:
	rtnl_unlock();
	return ret;
}
EXPORT_SYMBOL(nic_set_cpu_affinity);
