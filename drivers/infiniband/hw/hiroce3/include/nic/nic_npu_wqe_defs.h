/*
 * Copyright (C), 2001-2011, Huawei Tech. Co., Ltd.
 * File Name	 : nic_npu_wqe_define.h
 * Version	   : Initial Draft
 * Created	   : 2019/4/25
 * Last Modified :
 * Description   : NIC wqe define between Driver and NPU
 * Function List :
 */

#ifndef NIC_NPU_WQE_DEFINE_H
#define NIC_NPU_WQE_DEFINE_H

#include "typedef.h"

/* Inner/External IP type */
typedef enum {
	/*
	 * 00 - non ip packet or packet type is not defined by software
	 * 01 - ipv6 packet
	 * 10 - ipv4 packet with no ip checksum offload
	 * 11 - ipv4 packet with ip checksum offload
	 */
	NON_IP_TYPE = 0,
	TYPE_IPV6,
	TYPE_IPV4,
	TYPE_IPV4_CS_OFF
} qsf_ip_type_e;

typedef enum {
	/*
	 * 0 - no udp / gre tunneling / no tunnel
	 * 1 - udp tunneling header with no cs
	 * 2 - udp tunneling header with cs
	 * 3 - gre tunneling header
	 */
	L4_TUNNEL_NO_TUNNEL = 0,
	L4_TUNNEL_UDP_NO_CS,
	L4_TUNNEL_UDP_CS,
	L4_TUNNEL_GRE
} qsf_tunnel_type_e;

typedef enum {
	/*
	 * 00b - unknown / fragmented packet
	 * 01b - tcp
	 * 10b - sctp
	 * 11b - udp
	 */

	L4_TYPE_UNKNOWN = 0,
	L4_TYPE_TCP,
	L4_TYPE_SCTP,
	L4_TYPE_UDP
} qsf_l4_offload_type_e;

/* *
 * Struct name:	  l2nic_rx_cqe_s.
 * @brief: L2nic_rx_cqe_s data structure.
 * Description:
 */
typedef struct tag_l2nic_rx_cqe {
	union {
		struct {
#if (BYTE_ORDER == BIG_ENDIAN)
			u32 rx_done : 1;
			u32 bp_en : 1;
			u32 decry_pkt : 1; /* decrypt packet flag */
			u32 flush : 1;
			u32 spec_flags : 3;
			u32 rsvd0 : 1;
			u32 lro_num : 8;
			u32 checksum_err : 16;
#else
			u32 checksum_err : 16;
			u32 lro_num : 8;
			u32 rsvd0 : 1;
			u32 spec_flags : 3;
			u32 flush : 1;
			u32 decry_pkt : 1;
			u32 bp_en : 1;
			u32 rx_done : 1;
#endif
		} bs;
		u32 value;
	} dw0;

	union {
		struct {
#if (BYTE_ORDER == BIG_ENDIAN)
			u32 length : 16;
			u32 vlan : 16;
#else
			u32 vlan : 16;
			u32 length : 16;
#endif
		} bs;
		u32 value;
	} dw1;

	union {
		struct {
#if (BYTE_ORDER == BIG_ENDIAN)
			u32 rss_type : 8;
			u32 rsvd0 : 2;
			u32 vlan_offload_en : 1;
			u32 umbcast : 2;
			u32 rsvd1 : 7;
			u32 pkt_types : 12;
#else
			u32 pkt_types : 12;
			u32 rsvd1 : 7;
			u32 umbcast : 2;
			u32 vlan_offload_en : 1;
			u32 rsvd0 : 2;
			u32 rss_type : 8;
#endif
		} bs;
		u32 value;
	} dw2;

	union {
		struct {
			u32 rss_hash_value;
		} bs;
		u32 value;
	} dw3;

	/* dw4~dw7 field for nic/ovs multipexing */
	union {
		struct { /* for nic */
#if (BYTE_ORDER == BIG_ENDIAN)
			u32 if_1588 : 1;
			u32 if_tx_ts : 1;
			u32 if_rx_ts : 1;
			u32 rsvd : 1;
			u32 msg_1588_type : 4;
			u32 msg_1588_offset : 8;
			u32 tx_ts_seq : 16;
#else
			u32 tx_ts_seq : 16;
			u32 msg_1588_offset : 8;
			u32 msg_1588_type : 4;
			u32 rsvd : 1;
			u32 if_rx_ts : 1;
			u32 if_tx_ts : 1;
			u32 if_1588 : 1;
#endif
		} bs;

		struct { /* for ovs */
#if (BYTE_ORDER == BIG_ENDIAN)
			u32 rsvd	 : 16;
			u32 crypt_en : 1;
			u32 sa_index : 15;
#else
			u32 sa_index : 15;
			u32 crypt_en : 1;
			u32 rsvd	 : 16;
#endif
		} ovs_bs;

		struct {
			u32 xid;
		} crypt_bs;

		u32 value;
	} dw4;

	union {
		struct { /* for nic */
			u32 msg_1588_ts;
		} bs;

		struct { /* for ovs */
#if (BYTE_ORDER == BIG_ENDIAN)
			u32 mac_type : 2; /* for ovs. mac_type */
			u32 l3_type : 3;  /* for ovs. l3_type */
			u32 l4_type : 3;  /* for ovs. l4_type */
			u32 rsvd0 : 2;
			u32 traffic_type : 6;  /* for ovs. traffic type: 0-default l2nic pkt, 1-fallback traffic, 2-miss upcall
									  traffic, 2-command */
			u32 traffic_from : 16; /* for ovs. traffic from: vf_id, only support traffic_type=0(default l2nic) or 2(miss
									  upcall) */
#else
			u32 traffic_from : 16;
			u32 traffic_type : 6;
			u32 rsvd0 : 2;
			u32 l4_type : 3;
			u32 l3_type : 3;
			u32 mac_type : 2;
#endif
		} ovs_bs;

		struct { /* for crypt */
#if (BYTE_ORDER == BIG_ENDIAN)
			u32 rsvd : 16;
			u32 decrypt_status : 8;
			u32 esp_next_head : 8;
#else
			u32 esp_next_head : 8;
			u32 decrypt_status : 8;
			u32 rsvd : 16;
#endif
		} crypt_bs;

		u32 value;
	} dw5;

	union {
		struct { /* for nic */
			u32 lro_ts;
		} bs;

		struct { /* for ovs */
			u32 reserved;
		} ovs_bs;

		u32 value;
	} dw6;

	union {
		struct { /* for nic */
			// u32 reserved;
			u32 first_len : 13;   /* Datalen of the first or middle pkt size. */
			u32 last_len : 13;	/* Data len of the last pkt size. */
			u32 pkt_num : 5;	  /* the number of packet. */
			u32 super_cqe_en : 1; /* only this bit = 1, other fileds in this DW is valid. */
		} bs;

		struct { /* for ovs */
			u32 localtag;
		} ovs_bs;

		u32 value;
	} dw7;
} l2nic_rx_cqe_s;


#endif
