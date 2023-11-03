/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 - 2023, Shanghai Yunsilicon Technology Co., Ltd.
 * All rights reserved.
 */

#ifndef XSC_PPH_H
#define XSC_PPH_H


#define XSC_PPH_HEAD_LEN	64

enum {
	L4_PROTO_NONE	= 0,
	L4_PROTO_TCP	= 1,
	L4_PROTO_UDP	= 2,
	L4_PROTO_ICMP	= 3,
	L4_PROTO_GRE	= 4,
};

enum {
	L3_PROTO_NONE	= 0,
	L3_PROTO_IP	= 2,
	L3_PROTO_IP6	= 3,
};

struct epp_pph {
	uint16_t outer_eth_type;              //2 bytes
	uint16_t inner_eth_type;              //4 bytes

	uint16_t rsv1:1;
	uint16_t outer_vlan_flag:2;
	uint16_t outer_ip_type:2;
	uint16_t outer_ip_ofst:5;
	uint16_t outer_ip_len:6;                //6 bytes

	uint16_t rsv2:1;
	uint16_t outer_tp_type:3;
	uint16_t outer_tp_csum_flag:1;
	uint16_t outer_tp_ofst:7;
	uint16_t ext_tunnel_type:4;              //8 bytes

	uint8_t tunnel_ofst;                     //9 bytes
	uint8_t inner_mac_ofst;                  //10 bytes

	uint32_t rsv3:2;
	uint32_t inner_mac_flag:1;
	uint32_t inner_vlan_flag:2;
	uint32_t inner_ip_type:2;
	uint32_t inner_ip_ofst:8;
	uint32_t inner_ip_len:6;
	uint32_t inner_tp_type:2;
	uint32_t inner_tp_csum_flag:1;
	uint32_t inner_tp_ofst:8;		//14 bytees

	uint16_t rsv4:1;
	uint16_t payload_type:4;
	uint16_t payload_ofst:8;
	uint16_t pkt_type:3;			//16 bytes

	uint16_t rsv5:2;
	uint16_t pri:3;
	uint16_t logical_in_port:11;
	uint16_t vlan_info;
	uint8_t error_bitmap:8;			//21 bytes

	uint8_t rsv6:7;
	uint8_t recirc_id_vld:1;
	uint16_t recirc_id;			//24 bytes

	uint8_t rsv7:7;
	uint8_t recirc_data_vld:1;
	uint32_t recirc_data;			//29 bytes

	uint8_t rsv8:6;
	uint8_t mark_tag_vld:2;
	uint16_t mark_tag;			//32 bytes

	uint8_t rsv9:4;
	uint8_t upa_to_soc:1;
	uint8_t upa_from_soc:1;
	uint8_t upa_re_up_call:1;
	uint8_t upa_pkt_drop:1;			//33 bytes

	uint8_t ucdv;
	uint16_t rsv10:2;
	uint16_t pkt_len:14;			//36 bytes

	uint16_t rsv11:2;
	uint16_t pkt_hdr_ptr:14;		//38 bytes

	uint64_t rsv12:5;
	uint64_t csum_ofst:8;
	uint64_t csum_val:29;
	uint64_t csum_plen:14;
	uint64_t rsv11_0:8;			//46 bytes

	uint64_t rsv11_1;
	uint64_t rsv11_2;
	uint16_t rsv11_3;
};

#define OUTER_L3_BIT	BIT(3)
#define OUTER_L4_BIT	BIT(2)
#define INNER_L3_BIT	BIT(1)
#define INNER_L4_BIT	BIT(0)
#define OUTER_BIT		(OUTER_L3_BIT | OUTER_L4_BIT)
#define INNER_BIT		(INNER_L3_BIT | INNER_L4_BIT)
#define OUTER_AND_INNER	(OUTER_BIT | INNER_BIT)

#define PACKET_UNKNOWN	BIT(4)

#define EPP2SOC_PPH_EXT_TUNNEL_TYPE_OFFSET (6UL)
#define EPP2SOC_PPH_EXT_TUNNEL_TYPE_BIT_MASK (0XF00)
#define EPP2SOC_PPH_EXT_TUNNEL_TYPE_BIT_OFFSET (8)

#define EPP2SOC_PPH_EXT_ERROR_BITMAP_OFFSET (20UL)
#define EPP2SOC_PPH_EXT_ERROR_BITMAP_BIT_MASK (0XFF)
#define EPP2SOC_PPH_EXT_ERROR_BITMAP_BIT_OFFSET (0)

#define XSC_GET_EPP2SOC_PPH_EXT_TUNNEL_TYPE(PPH_BASE_ADDR)	\
	((*(uint16_t *)((uint8_t *)PPH_BASE_ADDR + EPP2SOC_PPH_EXT_TUNNEL_TYPE_OFFSET) & \
	EPP2SOC_PPH_EXT_TUNNEL_TYPE_BIT_MASK) >> EPP2SOC_PPH_EXT_TUNNEL_TYPE_BIT_OFFSET)

#define XSC_GET_EPP2SOC_PPH_ERROR_BITMAP(PPH_BASE_ADDR)		\
	((*(uint8_t *)((uint8_t *)PPH_BASE_ADDR + EPP2SOC_PPH_EXT_ERROR_BITMAP_OFFSET) & \
	EPP2SOC_PPH_EXT_ERROR_BITMAP_BIT_MASK) >> EPP2SOC_PPH_EXT_ERROR_BITMAP_BIT_OFFSET)


#define PPH_OUTER_IP_TYPE_OFF		(4UL)
#define PPH_OUTER_IP_TYPE_MASK		(0x3)
#define PPH_OUTER_IP_TYPE_SHIFT		(11)
#define PPH_OUTER_IP_TYPE(base)		\
	((ntohs(*(uint16_t *)((uint8_t *)base + PPH_OUTER_IP_TYPE_OFF)) >> \
	PPH_OUTER_IP_TYPE_SHIFT) & PPH_OUTER_IP_TYPE_MASK)

#define PPH_OUTER_IP_OFST_OFF		(4UL)
#define PPH_OUTER_IP_OFST_MASK		(0x1f)
#define PPH_OUTER_IP_OFST_SHIFT		(6)
#define PPH_OUTER_IP_OFST(base)		 \
	((ntohs(*(uint16_t *)((uint8_t *)base + PPH_OUTER_IP_OFST_OFF)) >> \
	PPH_OUTER_IP_OFST_SHIFT) & PPH_OUTER_IP_OFST_MASK)

#define PPH_OUTER_IP_LEN_OFF		(4UL)
#define PPH_OUTER_IP_LEN_MASK		(0x3f)
#define PPH_OUTER_IP_LEN_SHIFT		(0)
#define PPH_OUTER_IP_LEN(base)		\
	((ntohs(*(uint16_t *)((uint8_t *)base + PPH_OUTER_IP_LEN_OFF)) >> \
	PPH_OUTER_IP_LEN_SHIFT) & PPH_OUTER_IP_LEN_MASK)

#define PPH_OUTER_TP_TYPE_OFF		(6UL)
#define PPH_OUTER_TP_TYPE_MASK		(0x7)
#define PPH_OUTER_TP_TYPE_SHIFT		(12)
#define PPH_OUTER_TP_TYPE(base)		\
	((ntohs(*(uint16_t *)((uint8_t *)base + PPH_OUTER_TP_TYPE_OFF)) >> \
	PPH_OUTER_TP_TYPE_SHIFT) & PPH_OUTER_TP_TYPE_MASK)

#define PPH_PAYLOAD_OFST_OFF		(14UL)
#define PPH_PAYLOAD_OFST_MASK		(0xff)
#define PPH_PAYLOAD_OFST_SHIFT		(3)
#define PPH_PAYLOAD_OFST(base)		\
	((ntohs(*(uint16_t *)((uint8_t *)base + PPH_PAYLOAD_OFST_OFF)) >> \
	PPH_PAYLOAD_OFST_SHIFT) & PPH_PAYLOAD_OFST_MASK)

#define PPH_CSUM_OFST_OFF		(38UL)
#define PPH_CSUM_OFST_MASK		(0xff)
#define PPH_CSUM_OFST_SHIFT		(51)
#define PPH_CSUM_OFST(base)		\
	((be64_to_cpu(*(uint64_t *)((uint8_t *)base + PPH_CSUM_OFST_OFF)) >> \
	PPH_CSUM_OFST_SHIFT) & PPH_CSUM_OFST_MASK)

#define PPH_CSUM_VAL_OFF		(38UL)
#define PPH_CSUM_VAL_MASK		(0xeffffff)
#define PPH_CSUM_VAL_SHIFT		(22)
#define PPH_CSUM_VAL(base)		\
	((be64_to_cpu(*(uint64_t *)((uint8_t *)base + PPH_CSUM_VAL_OFF)) >> \
	PPH_CSUM_VAL_SHIFT) & PPH_CSUM_VAL_MASK)
#endif /* XSC_TBM_H */

