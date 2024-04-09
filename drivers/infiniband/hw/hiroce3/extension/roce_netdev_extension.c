/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name	 : roce_netdev_extension.c
 * Version	   : v1.1
 * Created	   : 2021/10/18
 * Last Modified : 2022/1/26
 * Description   : The definition of RoCE net devices realated extended functions.
 */

#include "roce_netdev_extension.h"
#include "roce_mpu_common.h"

#ifdef ROCE_BONDING_EN
#include "roce_bond.h"
#endif

#ifdef ROCE_VROCE_EN
static int roce3_get_vni_by_func_id(void *hwdev, u16 func_id, u32 *vni)
{
	int ret;
	vroce_mac_cfg_vni_info_s mac_vni_info;
	u16 out_size = (u16)sizeof(vroce_mac_cfg_vni_info_s);

	if (!hwdev || !vni) {
		pr_err("[ROCE, ERR] %s: null pointer hwdev %p vni %p\n", __func__, hwdev, vni);
		return -EINVAL;
	}

	memset(&mac_vni_info, 0, sizeof(mac_vni_info));

	mac_vni_info.func_id = func_id;
	ret = roce3_msg_to_mgmt_sync(hwdev, ROCE_MPU_CMD_GET_MAC_VNI, &mac_vni_info, sizeof(mac_vni_info), &mac_vni_info,
		&out_size);
	if ((ret != 0) || (out_size == 0) || (mac_vni_info.head.status != 0)) {
		pr_err("[ROCE, ERR] %s: Failed to get mac vni, err(%d), status(0x%x), out size(0x%x)\n", __func__, ret,
			mac_vni_info.head.status, out_size);
		return -EINVAL;
	}

	*vni = (mac_vni_info.vni_en) ? mac_vni_info.vlan_vni : 0;

	return ret;
}
#endif

#ifndef PANGEA_NOF
int roce3_add_real_device_mac(struct roce3_device *rdev, struct net_device *netdev)
{
	int ret;
	u32 vlan_id = 0;

#ifndef ROCE_VROCE_EN
	/* no need to configure IPSURX vf table for vroce here, this action has been done in vroce driver */
	roce3_add_ipsu_tbl_mac_entry(rdev->hwdev, (u8 *)netdev->dev_addr, vlan_id, rdev->glb_func_id,
		hinic3_er_id(rdev->hwdev));
#else
	/* vroce uses vni to filt invalid packets */
	ret = roce3_get_vni_by_func_id(rdev->hwdev, rdev->glb_func_id, &vlan_id);
	if (ret != 0) {
		pr_err("[ROCE, ERR] %s: get roce3_get_vni_by_func_id failed, vlan_id(0x%x) ret(%d)\n", __func__, vlan_id, ret);
	}

	ret =
		roce3_add_mac_tbl_mac_entry(rdev->hwdev, (u8 *)netdev->dev_addr, vlan_id, rdev->glb_func_id, rdev->glb_func_id);
	if (ret != 0) {
		pr_err("[ROCE, ERR] %s: Failed to add mac_vlan entry, ret(%d)\n", __func__, ret);
		return ret;
	}
#endif

#ifdef ROCE_BONDING_EN
	if (roce3_bond_is_active(rdev)) {
		ret = roce3_add_bond_real_slave_mac(rdev, (u8 *)netdev->dev_addr);
		if (ret != 0) {
			dev_err(rdev->hwdev_hdl, "[ROCE, ERR] %s: Failed to add bond read device ipsu mac, func_id(%d)\n", __func__,
				rdev->glb_func_id);
			roce3_del_ipsu_tbl_mac_entry(rdev->hwdev, (u8 *)netdev->dev_addr, 0, rdev->glb_func_id,
				hinic3_er_id(rdev->hwdev));
			return ret;
		}
	}
#endif

	memcpy(rdev->mac, netdev->dev_addr, sizeof(rdev->mac));

	return 0;
}

int roce3_add_vlan_device_mac(struct roce3_device *rdev, struct net_device *netdev)
{
	int ret = 0;
	u32 vlan_id = 0;

	vlan_id = ROCE_GID_SET_VLAN_32BIT_VLAID(((u32)rdma_vlan_dev_vlan_id(netdev)));
	dev_info(rdev->hwdev_hdl, "[ROCE] %s: enter roce3_add_vlan_device_mac, vlan_id(0x%x), func_id(%d)\n", __func__,
		vlan_id, rdev->glb_func_id);
	ret =
		roce3_add_mac_tbl_mac_entry(rdev->hwdev, (u8 *)netdev->dev_addr, vlan_id, rdev->glb_func_id, rdev->glb_func_id);
	if (ret != 0) {
		dev_err(rdev->hwdev_hdl, "[ROCE, ERR] %s: Failed to set vlan device mac, vlan_id(0x%x), func_id(%d)\n",
			__func__, vlan_id, rdev->glb_func_id);
		return ret;
	}

	roce3_add_ipsu_tbl_mac_entry(rdev->hwdev, (u8 *)netdev->dev_addr, vlan_id, rdev->glb_func_id,
		hinic3_er_id(rdev->hwdev));

#ifdef ROCE_BONDING_EN
	if (roce3_bond_is_active(rdev)) {
		ret = roce3_add_bond_vlan_slave_mac(rdev, (u8 *)netdev->dev_addr, (u16)vlan_id);
		if (ret != 0) {
			dev_err(rdev->hwdev_hdl, "[ROCE, ERR] %s: Failed to set bond vlan slave mac, vlan_id(0x%x), func_id(%d)\n",
				__func__, vlan_id, rdev->glb_func_id);
			goto err_add_bond_vlan_slave_mac;
		}
	}
#endif

	return ret;

#ifdef ROCE_BONDING_EN
err_add_bond_vlan_slave_mac:
	roce3_del_ipsu_tbl_mac_entry(rdev->hwdev, (u8 *)netdev->dev_addr, vlan_id, rdev->glb_func_id,
		hinic3_er_id(rdev->hwdev));

	(void)hinic3_del_mac(rdev->hwdev, (u8 *)netdev->dev_addr, (u16)vlan_id, rdev->glb_func_id, HINIC3_CHANNEL_ROCE);

	return ret;
#endif
}

void roce3_del_real_device_mac(struct roce3_device *rdev)
{
	u32 vlan_id = 0;
#ifdef ROCE_BONDING_EN
	if (roce3_bond_is_active(rdev)) {
		roce3_del_bond_real_slave_mac(rdev);
	}
#endif

#ifndef ROCE_VROCE_EN
	roce3_del_ipsu_tbl_mac_entry(rdev->hwdev, rdev->mac, vlan_id, rdev->glb_func_id, hinic3_er_id(rdev->hwdev));
#else
	/* vroce uses vni to filt invalid packets */
	(void)roce3_get_vni_by_func_id(rdev->hwdev, rdev->glb_func_id, &vlan_id);

	(void)roce3_del_mac_tbl_mac_entry(rdev->hwdev, rdev->mac, vlan_id, rdev->glb_func_id, rdev->glb_func_id);
#endif
}

void roce3_del_vlan_device_mac(struct roce3_device *rdev, struct roce3_vlan_dev_list *old_list)
{
#ifdef ROCE_BONDING_EN
	if (roce3_bond_is_active(rdev)) {
		roce3_del_bond_vlan_slave_mac(rdev, old_list->mac, (u16)old_list->vlan_id);
	}
#endif

	roce3_del_ipsu_tbl_mac_entry(rdev->hwdev, old_list->mac, old_list->vlan_id, rdev->glb_func_id,
		hinic3_er_id(rdev->hwdev));

	(void)roce3_del_mac_tbl_mac_entry(rdev->hwdev, old_list->mac, old_list->vlan_id, rdev->glb_func_id,
		rdev->glb_func_id);
}

void roce3_event_up_extend(struct roce3_device *rdev)
{
	if (test_and_set_bit(ROCE3_PORT_EVENT, &rdev->status) == 0) {
		roce3_ifconfig_up_down_event_report(rdev, IB_EVENT_PORT_ACTIVE);
	}
}

#endif /* PANGEA_NOF */
