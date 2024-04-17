/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: bond common command interface define.
 */

#ifndef BOND_COMMON_DEFS_H
#define BOND_COMMON_DEFS_H

#define BOND_NAME_MAX_LEN	   16
#define BOND_PORT_MAX_NUM	   4
#define BOND_ID_INVALID		 0xFFFF
#define OVS_PORT_NUM_MAX		BOND_PORT_MAX_NUM
#define DEFAULT_ROCE_BOND_FUNC  0xFFFFFFFF

#define BOND_ID_IS_VALID(_id)   (((_id) >= BOND_FIRST_ID) && ((_id) <= BOND_MAX_ID))
#define BOND_ID_IS_INVALID(_id) (!(BOND_ID_IS_VALID(_id)))

enum bond_group_id {
	BOND_FIRST_ID = 1,
	BOND_MAX_ID = 4,
	BOND_MAX_NUM,
};

/**
 * OVS bond mode
 */
typedef enum tag_ovs_bond_mode {
	OVS_BOND_MODE_NONE		  = 0, /**< bond disable */
	OVS_BOND_MODE_ACTIVE_BACKUP = 1, /**< 1 for active-backup */
	OVS_BOND_MODE_BALANCE	   = 2, /**< 2 for balance-xor */
	OVS_BOND_MODE_LACP		  = 4, /**< 4 for 802.3ad */
	OVS_BOND_MODE_MAX
} ovs_bond_mode_e;

/**
 * ovs bond hash policy
 */
typedef enum tag_ovs_bond_hash_policy {
	OVS_BOND_HASH_POLICY_L2	   = 0, /**< 0 for layer 2   SMAC_DMAC */
	OVS_BOND_HASH_POLICY_L34	  = 1, /**< 1 for layer 3+4 SIP_DIP_SPORT_DPORT */
	OVS_BOND_HASH_POLICY_L23	  = 2, /**< 2 for layer 2+3 SMAC_DMAC_SIP_DIP */
	OVS_BOND_HASH_POLICY_VFID_SQN = 3, /**< 3 for vfid ^ sqn */
	OVS_BOND_HASH_POLICY_MAX
} ovs_bond_hash_policy_e;

/**
 * bond set attr: bond_name之前变量定义要求与结构体struct bond_attr保持一致
 */
typedef struct tag_ovs_bond_cmd {
	u16 bond_mode;				/**< bond mode:1 for active-backup,2 for balance-xor,4 for 802.3ad */
	u16 bond_id;				  /**< bond id */

	u16 up_delay;				 /**< defualt:200ms */
	u16 down_delay;			   /**< defualt:200ms */

	u32 active_slaves		: 8; /**< active port slaves(bitmaps) */
	u32 slaves			   : 8; /**< bond port id bitmaps */
	u32 lacp_collect_slaves  : 8; /**< bond port id bitmaps */
	u32 xmit_hash_policy	 : 8; /**< xmit hash:0 for layer 2 ,1 for layer 2+3 ,2 for layer 3+4 */
	u32 first_roce_func;		  /* RoCE used */
	u32 bond_pf_bitmap;		   /**< all PFs under the bond */
	u32 user_bitmap;
	u8  bond_name[BOND_NAME_MAX_LEN]; /**< bond name, length must be less than 16 */
} ovs_bond_cmd_s;

#pragma pack(4)
/**
 * bond per port statistics
 */
typedef struct tag_bond_port_stat {
	/** mpu provide */
	u64 rx_pkts;
	u64 rx_bytes;
	u64 rx_drops;
	u64 rx_errors;

	u64 tx_pkts;
	u64 tx_bytes;
	u64 tx_drops;
	u64 tx_errors;
} hinic3_bond_port_stat_s;
#pragma pack()

/**
 * bond port attribute
 */
typedef struct tag_bond_port_attr {
	u8 duplex;
	u8 status;
	u8 rsvd0[2];
	u32 speed;
} hinic3_bond_port_attr_s;

/**
 * Get bond information command struct defination
 * @see OVS_MPU_CMD_BOND_GET_ATTR
 */
typedef struct tag_bond_get {
	u16 bond_id_vld;	/**< bond_id_vld=1: used bond_id get bond info; bond_id_vld=0: used bond_name get bond info */
	u16 bond_id;					  /**< if bond_id_vld=1 input, else output */
	u8  bond_name[BOND_NAME_MAX_LEN]; /**< if bond_id_vld=0 input, else output */

	u16 bond_mode;		   /**< bond mode:1 for active-backup,2 for balance-xor,4 for 802.3ad */
	u8  active_slaves;	   /**< active port slaves(bitmaps) */
	u8  slaves;			  /**< bond port id bitmaps */

	u8  lacp_collect_slaves; /**< bond port id bitmaps */
	u8  xmit_hash_policy;	/**< xmit hash:0 for layer 2 ,1 for layer 2+3 ,2 for layer 3+4 */
	u16 rsvd0;			   /**< in order to 4B alligned */

	hinic3_bond_port_stat_s stat[BOND_PORT_MAX_NUM];
	hinic3_bond_port_attr_s attr[BOND_PORT_MAX_NUM];
} hinic3_bond_get_s;

#endif /** BOND_COMMON_DEFS_H */