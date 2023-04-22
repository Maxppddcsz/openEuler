// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2021 3snic Technologies Co., Ltd */

#define pr_fmt(fmt) KBUILD_MODNAME ": [NIC]" fmt

#include <net/xfrm.h>
#include <linux/netdevice.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/tcp.h>
#include <linux/sctp.h>
#include <linux/dma-mapping.h>
#include <linux/types.h>
#include <linux/u64_stats_sync.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

#include "sss_kernel.h"
#include "sss_hw.h"
#include "sss_nic_io.h"
#include "sss_nic_cfg.h"
#include "sss_nic_vf_cfg.h"
#include "sss_nic_mag_cfg.h"
#include "sss_nic_rss_cfg.h"
#include "sss_nic_dev_define.h"
#include "sss_nic_tx.h"

#define SSSNIC_DEFAULT_MSS				0x3E00
#define SSSNIC_MIN_MSS					0x50
#define SSSNIC_SKB_LEN_MIN				32
#define SSSNIC_SKB_LEN_MAX				16383
#define SSSNIC_PAYLOAD_OFFSET_MAX		221

#define SSSNIC_IPV4_VERSION					4
#define SSSNIC_IPV6_VERSION					6
#define SSSNIC_TCP_DOFF_TO_BYTES(doff)		((doff) << 2)
#define SSSNIC_VXLAN_OFFLOAD_PORT			46354

#define SSSNIC_TRANSPORT_OFFSET(hdr, skb)	((u32)((hdr) - (skb)->data))

// SQE CTRL
#define SSSNIC_SQE_CTRL_SECT_BD0_LEN_SHIFT 0
#define SSSNIC_SQE_CTRL_SECT_RSVD_SHIFT 18
#define SSSNIC_SQE_CTRL_SECT_BUFDESC_NUM_SHIFT 19
#define SSSNIC_SQE_CTRL_SECT_TASKSECT_LEN_SHIFT 27
#define SSSNIC_SQE_CTRL_SECT_DATA_FORMAT_SHIFT 28
#define SSSNIC_SQE_CTRL_SECT_DIRECT_SHIFT 29
#define SSSNIC_SQE_CTRL_SECT_EXTENDED_SHIFT 30
#define SSSNIC_SQE_CTRL_SECT_OWNER_SHIFT 31

#define SSSNIC_SQE_CTRL_SECT_BD0_LEN_MASK 0x3FFFFU
#define SSSNIC_SQE_CTRL_SECT_RSVD_MASK 0x1U
#define SSSNIC_SQE_CTRL_SECT_BUFDESC_NUM_MASK 0xFFU
#define SSSNIC_SQE_CTRL_SECT_TASKSECT_LEN_MASK 0x1U
#define SSSNIC_SQE_CTRL_SECT_DATA_FORMAT_MASK 0x1U
#define SSSNIC_SQE_CTRL_SECT_DIRECT_MASK 0x1U
#define SSSNIC_SQE_CTRL_SECT_EXTENDED_MASK 0x1U
#define SSSNIC_SQE_CTRL_SECT_OWNER_MASK 0x1U

#define SSSNIC_SQE_CTRL_SECT_SET(val, member) \
(((u32)(val) & SSSNIC_SQE_CTRL_SECT_##member##_MASK) << SSSNIC_SQE_CTRL_SECT_##member##_SHIFT)

// SQ CTRL QINFO
#define SSSNIC_SQE_CTRL_SECT_QINFO_PKT_TYPE_SHIFT 0
#define SSSNIC_SQE_CTRL_SECT_QINFO_PLDOFF_SHIFT 2
#define SSSNIC_SQE_CTRL_SECT_QINFO_UFO_SHIFT 10
#define SSSNIC_SQE_CTRL_SECT_QINFO_TSO_SHIFT 11
#define SSSNIC_SQE_CTRL_SECT_QINFO_TCPUDP_CS_SHIFT 12
#define SSSNIC_SQE_CTRL_SECT_QINFO_MSS_SHIFT 13
#define SSSNIC_SQE_CTRL_SECT_QINFO_SCTP_SHIFT 27
#define SSSNIC_SQE_CTRL_SECT_QINFO_UC_SHIFT 28
#define SSSNIC_SQE_CTRL_SECT_QINFO_PRI_SHIFT 29

#define SSSNIC_SQE_CTRL_SECT_QINFO_PKT_TYPE_MASK 0x3U
#define SSSNIC_SQE_CTRL_SECT_QINFO_PLDOFF_MASK 0xFFU
#define SSSNIC_SQE_CTRL_SECT_QINFO_UFO_MASK 0x1U
#define SSSNIC_SQE_CTRL_SECT_QINFO_TSO_MASK 0x1U
#define SSSNIC_SQE_CTRL_SECT_QINFO_TCPUDP_CS_MASK 0x1U
#define SSSNIC_SQE_CTRL_SECT_QINFO_MSS_MASK 0x3FFFU
#define SSSNIC_SQE_CTRL_SECT_QINFO_SCTP_MASK 0x1U
#define SSSNIC_SQE_CTRL_SECT_QINFO_UC_MASK 0x1U
#define SSSNIC_SQE_CTRL_SECT_QINFO_PRI_MASK 0x7U

#define SSSNIC_SQE_CTRL_SECT_QINFO_SET(val, member) \
	(((u32)(val) & SSSNIC_SQE_CTRL_SECT_QINFO_##member##_MASK) << \
	 SSSNIC_SQE_CTRL_SECT_QINFO_##member##_SHIFT)

#define SSSNIC_SQE_CTRL_SECT_QINFO_GET(val, member) \
	(((val) >> SSSNIC_SQE_CTRL_SECT_QINFO_##member##_SHIFT) & \
	 SSSNIC_SQE_CTRL_SECT_QINFO_##member##_MASK)

#define SSSNIC_SQE_CTRL_SECT_QINFO_CLEAR(val, member) \
	((val) & (~(SSSNIC_SQE_CTRL_SECT_QINFO_##member##_MASK << \
		    SSSNIC_SQE_CTRL_SECT_QINFO_##member##_SHIFT)))

// SQ TASK
#define SSSNIC_SQE_TASK_SECT_VALUE0_TUNNEL_FLAG_SHIFT 19
#define SSSNIC_SQE_TASK_SECT_VALUE0_ESP_NEXT_PROTO_SHIFT 22
#define SSSNIC_SQE_TASK_SECT_VALUE0_INNER_L4_EN_SHIFT 24
#define SSSNIC_SQE_TASK_SECT_VALUE0_INNER_L3_EN_SHIFT 25
#define SSSNIC_SQE_TASK_SECT_VALUE0_INNER_L4_PSEUDO_SHIFT 26
#define SSSNIC_SQE_TASK_SECT_VALUE0_OUT_L4_EN_SHIFT 27
#define SSSNIC_SQE_TASK_SECT_VALUE0_OUT_L3_EN_SHIFT 28
#define SSSNIC_SQE_TASK_SECT_VALUE0_OUT_L4_PSEUDO_SHIFT 29
#define SSSNIC_SQE_TASK_SECT_VALUE0_ESP_OFFLOAD_SHIFT 30
#define SSSNIC_SQE_TASK_SECT_VALUE0_IPSEC_PROTO_SHIFT 31

#define SSSNIC_SQE_TASK_SECT_VALUE0_TUNNEL_FLAG_MASK 0x1U
#define SSSNIC_SQE_TASK_SECT_VALUE0_ESP_NEXT_PROTO_MASK 0x3U
#define SSSNIC_SQE_TASK_SECT_VALUE0_INNER_L4_EN_MASK 0x1U
#define SSSNIC_SQE_TASK_SECT_VALUE0_INNER_L3_EN_MASK 0x1U
#define SSSNIC_SQE_TASK_SECT_VALUE0_INNER_L4_PSEUDO_MASK 0x1U
#define SSSNIC_SQE_TASK_SECT_VALUE0_OUT_L4_EN_MASK 0x1U
#define SSSNIC_SQE_TASK_SECT_VALUE0_OUT_L3_EN_MASK 0x1U
#define SSSNIC_SQE_TASK_SECT_VALUE0_OUT_L4_PSEUDO_MASK 0x1U
#define SSSNIC_SQE_TASK_SECT_VALUE0_ESP_OFFLOAD_MASK 0x1U
#define SSSNIC_SQE_TASK_SECT_VALUE0_IPSEC_PROTO_MASK 0x1U

#define SSSNIC_SQE_TASK_SECT_VALUE0_SET(val, member) \
	(((u32)(val) & SSSNIC_SQE_TASK_SECT_VALUE0_##member##_MASK) << \
	 SSSNIC_SQE_TASK_SECT_VALUE0_##member##_SHIFT)

#define SSSNIC_SQE_TASK_SECT_VALUE3_VLAN_TAG_SHIFT 0
#define SSSNIC_SQE_TASK_SECT_VALUE3_VLAN_TYPE_SHIFT 16
#define SSSNIC_SQE_TASK_SECT_VALUE3_VLAN_TAG_VALID_SHIFT 19

#define SSSNIC_SQE_TASK_SECT_VALUE3_VLAN_TAG_MASK 0xFFFFU
#define SSSNIC_SQE_TASK_SECT_VALUE3_VLAN_TYPE_MASK 0x7U
#define SSSNIC_SQE_TASK_SECT_VALUE3_VLAN_TAG_VALID_MASK 0x1U

#define SSSNIC_SQE_TASK_SECT_VALUE3_SET(val, member) \
	(((val) & SSSNIC_SQE_TASK_SECT_VALUE3_##member##_MASK) << \
	 SSSNIC_SQE_TASK_SECT_VALUE3_##member##_SHIFT)

#define SSSNIC_VLAN_INSERT_MODE_MAX 5
#define SSSNIC_TSO_CS_EN 1
#define SSSNIC_DEF_PKT_CNT 1

#define SSSNIC_SQ_STATS_INC(sq_desc, field)	\
do { \
	u64_stats_update_begin(&(sq_desc)->stats.stats_sync); \
	(sq_desc)->stats.field++; \
	u64_stats_update_end(&(sq_desc)->stats.stats_sync); \
} while (0)

enum sss_nic_check_tx_offload_type {
	SSSNIC_TX_OFFLOAD_TSO = BIT(0),
	SSSNIC_TX_OFFLOAD_CSUM = BIT(1),
	SSSNIC_TX_OFFLOAD_VLAN = BIT(2),
	SSSNIC_TX_OFFLOAD_INVALID = BIT(3),
	SSSNIC_TX_OFFLOAD_ESP = BIT(4),
};

union sss_nic_ip {
	struct iphdr *v4;
	struct ipv6hdr *v6;
	unsigned char *hdr;
};

struct sss_nic_sqe_ctrl_section {
	u32 sect_len;
	u32 qinfo;
	u32 addr_high;
	u32 addr_low;
};

/* Engine only pass first 12B TS field directly to uCode through metadata
 * vlan_offoad is used for hardware when vlan insert in tx
 */
struct sss_nic_sqe_task_section {
	u32 value[4];
};

struct sss_nic_sqe_bd_section {
	u32 len; /* 31-bits Length, L2NIC only use length[17:0] */
	u32 rsvd;
	u32 addr_high;
	u32 addr_low;
};

/* use section pointer for support non continuous wqe */
struct sss_nic_sqe {
	struct sss_nic_sqe_ctrl_section *ctrl_sect;
	struct sss_nic_sqe_task_section *task_sect;
	struct sss_nic_sqe_bd_section *bd_sect0;
	struct sss_nic_sqe_bd_section *bd_sect1;
	u16 first_bds_num;
	u32 wqe_type;
	u32 task_type;
};

/* ************* SQ_CTRL ************** */
enum sss_nic_sqe_data_format {
	SSSNIC_NORMAL_SQE = 0,
};

enum sss_nic_sqe_type {
	SSSNIC_SQE_COMPACT_TYPE = 0,
	SSSNIC_SQE_EXTENDED_TYPE = 1,
};

enum sss_nic_sqe_task_len {
	SSSNIC_SQE_TASK_LEN_46BITS = 0,
	SSSNIC_SQE_TASK_LEN_128BITS = 1,
};

union sss_nic_transport_header {
	struct tcphdr *tcp;
	struct udphdr *udp;
	unsigned char *hdr;
};

enum sss_nic_sq_l3_proto_type {
	SSSNIC_UNSUPPORT_L3_PORTO_TYPE = 0,
	SSSNIC_IPV6_PKT = 1,
	SSSNIC_IPV4_PKT_NO_CSO = 2,
	SSSNIC_IPV4_PKT_WITH_CSO = 3,
};

enum sss_nic_sq_l4_offload_type {
	SSSNIC_DISABLE_OFFLOAD = 0,
	SSSNIC_TCP_OFFLOAD = 1,
	SSSNIC_SCTP_OFFLOAD = 2,
	SSSNIC_UDP_OFFLOAD = 3,
};

static inline __sum16 sss_nic_csum_magic(union sss_nic_ip *ip,
					 unsigned short proto)
{
	return (ip->v4->version == SSSNIC_IPV4_VERSION) ?
	       csum_tcpudp_magic(ip->v4->saddr, ip->v4->daddr, 0, proto, 0) :
	       csum_ipv6_magic(&ip->v6->saddr, &ip->v6->daddr, 0, proto, 0);
}

static inline void sss_nic_set_vlan_tx_offload(struct sss_nic_sqe_task_section *task_sect,
					       u16 vlan_tag, u8 vlan_type)
{
	task_sect->value[3] = SSSNIC_SQE_TASK_SECT_VALUE3_SET(vlan_tag, VLAN_TAG) |
			      SSSNIC_SQE_TASK_SECT_VALUE3_SET(vlan_type, VLAN_TYPE) |
			      SSSNIC_SQE_TASK_SECT_VALUE3_SET(1U, VLAN_TAG_VALID);
}

void sss_nic_get_sq_stats(struct sss_nic_sq_desc *sq_desc,
			  struct sss_nic_sq_stats *stats)
{
	struct sss_nic_sq_stats *sq_stats = &sq_desc->stats;
	unsigned int begin;

	u64_stats_update_begin(&stats->stats_sync);
	do {
		begin = u64_stats_fetch_begin(&sq_stats->stats_sync);
		stats->tx_bytes = sq_stats->tx_bytes;
		stats->tx_packets = sq_stats->tx_packets;
		stats->tx_busy = sq_stats->tx_busy;
		stats->wake = sq_stats->wake;
		stats->tx_dropped = sq_stats->tx_dropped;
	} while (u64_stats_fetch_retry(&sq_stats->stats_sync, begin));
	u64_stats_update_end(&stats->stats_sync);
}

static inline void sss_nic_init_bd_sect(struct sss_nic_sqe_bd_section *bd_sect,
					dma_addr_t addr, u32 len)
{
	bd_sect->addr_high = sss_hw_be32(upper_32_bits(addr));
	bd_sect->addr_low = sss_hw_be32(lower_32_bits(addr));
	bd_sect->len  = sss_hw_be32(len);
}

static inline void sss_nic_unmap_dma_page(struct sss_nic_dev *nic_dev,
					  u16 nr_frags, struct sss_nic_dma_info *dma_group)
{
	struct pci_dev *pdev = nic_dev->pdev;
	int flag;

	for (flag = 1; flag < nr_frags + 1; flag++)
		dma_unmap_page(&pdev->dev, dma_group[flag].dma,
			       dma_group[flag].len, DMA_TO_DEVICE);
	dma_unmap_single(&pdev->dev, dma_group[0].dma, dma_group[0].len,
			 DMA_TO_DEVICE);
}

static int sss_nic_map_dma_page(struct sss_nic_dev *nic_dev,
				struct sk_buff *skb, u16 valid_nr_frag,
				struct sss_nic_sq_desc *sq_desc,
				struct sss_nic_tx_desc *tx_desc,
				struct sss_nic_sqe *sqe)
{
	struct sss_nic_sqe_ctrl_section *ctrl_sect = sqe->ctrl_sect;
	struct sss_nic_sqe_bd_section *bd_sect = sqe->bd_sect0;
	struct sss_nic_dma_info *dma_group = tx_desc->dma_group;
	struct pci_dev *pdev = nic_dev->pdev;
	skb_frag_t *frag = NULL;
	u32 flag;
	int ret;

	dma_group[0].dma = dma_map_single(&pdev->dev, skb->data,
					  skb_headlen(skb), DMA_TO_DEVICE);
	if (dma_mapping_error(&pdev->dev, dma_group[0].dma)) {
		SSSNIC_SQ_STATS_INC(sq_desc, dma_map_err);
		return -EFAULT;
	}

	dma_group[0].len = skb_headlen(skb);

	ctrl_sect->addr_high = sss_hw_be32(upper_32_bits(dma_group[0].dma));
	ctrl_sect->addr_low = sss_hw_be32(lower_32_bits(dma_group[0].dma));
	ctrl_sect->sect_len = dma_group[0].len;

	for (flag = 0; flag < valid_nr_frag;) {
		frag = &(skb_shinfo(skb)->frags[flag]);
		if (unlikely(flag == sqe->first_bds_num))
			bd_sect = sqe->bd_sect1;

		flag++;
		dma_group[flag].dma = skb_frag_dma_map(&pdev->dev, frag, 0,
						       skb_frag_size(frag),
						       DMA_TO_DEVICE);
		if (dma_mapping_error(&pdev->dev, dma_group[flag].dma)) {
			SSSNIC_SQ_STATS_INC(sq_desc, dma_map_err);
			flag--;
			ret = -EFAULT;
			goto frag_map_err;
		}
		dma_group[flag].len = skb_frag_size(frag);

		sss_nic_init_bd_sect(bd_sect, dma_group[flag].dma,
				     dma_group[flag].len);
		bd_sect++;
	}
	return 0;

frag_map_err:
	sss_nic_unmap_dma_page(nic_dev, flag, dma_group);
	return ret;
}

static void sss_nic_get_inner_transport_info(struct sk_buff *skb,
					     union sss_nic_transport_header *l4,
		u8 l4_proto, u32 *offset,
		enum sss_nic_sq_l4_offload_type *l4_offload)
{
	if (l4_proto == IPPROTO_TCP) {
		*l4_offload = SSSNIC_TCP_OFFLOAD;
		*offset = SSSNIC_TCP_DOFF_TO_BYTES(l4->tcp->doff) +
			  SSSNIC_TRANSPORT_OFFSET(l4->hdr, skb);
	} else if (l4_proto == IPPROTO_UDP) {
		*l4_offload = SSSNIC_UDP_OFFLOAD;
		*offset = SSSNIC_TRANSPORT_OFFSET(l4->hdr, skb);
	}
}

static int sss_nic_check_tx_csum(struct sss_nic_sq_desc *sq_desc,
				 struct sss_nic_sqe_task_section *task_sect,
				 struct sk_buff *skb)
{
	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return 0;

	if (skb->encapsulation) {
		union sss_nic_ip ip;
		u8 l4_proto;

		task_sect->value[0] |= SSSNIC_SQE_TASK_SECT_VALUE0_SET(1U, TUNNEL_FLAG);
		ip.hdr = skb_network_header(skb);
		if (ip.v4->version == SSSNIC_IPV4_VERSION) {
			l4_proto = ip.v4->protocol;
		} else if (ip.v4->version == SSSNIC_IPV6_VERSION) {
			union sss_nic_transport_header l4;
			unsigned char *exthdr;
			__be16 frag_off;

			exthdr = ip.hdr + sizeof(*ip.v6);
			l4_proto = ip.v6->nexthdr;
			l4.hdr = skb_transport_header(skb);
			if (l4.hdr != exthdr)
				ipv6_skip_exthdr(skb, exthdr - skb->data,  &l4_proto, &frag_off);
		} else {
			l4_proto = IPPROTO_RAW;
		}
		if (((struct udphdr *)skb_transport_header(skb))->dest !=
		    SSSNIC_VXLAN_OFFLOAD_PORT ||
		    l4_proto != IPPROTO_UDP) {
			SSSNIC_SQ_STATS_INC(sq_desc, unknown_tunnel_proto);
			/* disable checksum offload */
			skb_checksum_help(skb);
			return 0;
		}
	}
	task_sect->value[0] |= SSSNIC_SQE_TASK_SECT_VALUE0_SET(1U, INNER_L4_EN);

	return SSSNIC_TSO_CS_EN;
}

static void sss_nic_get_inner_proto_type(struct sk_buff *skb,
					 union sss_nic_ip *ip, union sss_nic_transport_header *l4,
		enum sss_nic_sq_l3_proto_type *l3_type, u8 *l4_proto)
{
	unsigned char *ext_hdr = NULL;

	if (ip->v4->version == SSSNIC_IPV4_VERSION) {
		*l3_type = SSSNIC_IPV4_PKT_WITH_CSO;
		*l4_proto = ip->v4->protocol;
	} else if (ip->v4->version == SSSNIC_IPV6_VERSION) {
		*l3_type = SSSNIC_IPV6_PKT;
		ext_hdr = ip->hdr + sizeof(*ip->v6);
		*l4_proto = ip->v6->nexthdr;
		if (ext_hdr != l4->hdr) {
			__be16 frag_off = 0;

			ipv6_skip_exthdr(skb, (int)(ext_hdr - skb->data),
					 l4_proto, &frag_off);
		}
	} else {
		*l3_type = SSSNIC_UNSUPPORT_L3_PORTO_TYPE;
		*l4_proto = 0;
	}
}

static void sss_nic_set_tso_info(struct sss_nic_sqe_task_section *task_sect,
				 u32 *qinfo, enum sss_nic_sq_l4_offload_type l4_offload,
				 u32 offset, u32 mss)
{
	if (l4_offload == SSSNIC_TCP_OFFLOAD) {
		*qinfo |= SSSNIC_SQE_CTRL_SECT_QINFO_SET(1U, TSO);
		task_sect->value[0] |= SSSNIC_SQE_TASK_SECT_VALUE0_SET(1U, INNER_L4_EN);
	} else if (l4_offload == SSSNIC_UDP_OFFLOAD) {
		*qinfo |= SSSNIC_SQE_CTRL_SECT_QINFO_SET(1U, UFO);
		task_sect->value[0] |= SSSNIC_SQE_TASK_SECT_VALUE0_SET(1U, INNER_L4_EN);
	}
	/* Default enable L3 calculation */
	task_sect->value[0] |= SSSNIC_SQE_TASK_SECT_VALUE0_SET(1U, INNER_L3_EN);
	*qinfo |= SSSNIC_SQE_CTRL_SECT_QINFO_SET(offset >> 1, PLDOFF);
	/* set MSS value */
	*qinfo = SSSNIC_SQE_CTRL_SECT_QINFO_CLEAR(*qinfo, MSS);
	*qinfo |= SSSNIC_SQE_CTRL_SECT_QINFO_SET(mss, MSS);
}

static void sss_nic_get_proto_hdr(struct sss_nic_sqe_task_section *task_sect,
				  struct sk_buff *skb,
				  union sss_nic_ip *ip, union sss_nic_transport_header *l4)
{
	if (skb->encapsulation) {
		u32 gso_type = skb_shinfo(skb)->gso_type;
		/* L3 checksum always enable */
		task_sect->value[0] |= SSSNIC_SQE_TASK_SECT_VALUE0_SET(1U, OUT_L3_EN);
		task_sect->value[0] |= SSSNIC_SQE_TASK_SECT_VALUE0_SET(1U, TUNNEL_FLAG);

		l4->hdr = skb_transport_header(skb);
		ip->hdr = skb_network_header(skb);

		if (gso_type & SKB_GSO_UDP_TUNNEL_CSUM) {
			l4->udp->check = ~sss_nic_csum_magic(ip, IPPROTO_UDP);
			task_sect->value[0] |= SSSNIC_SQE_TASK_SECT_VALUE0_SET(1U, OUT_L4_EN);
		}

		ip->hdr = skb_inner_network_header(skb);
		l4->hdr = skb_inner_transport_header(skb);
	} else {
		ip->hdr = skb_network_header(skb);
		l4->hdr = skb_transport_header(skb);
	}
}

static int sss_nic_check_tso(struct sss_nic_sqe_task_section *task_sect,
			     u32 *qinfo, struct sk_buff *skb)
{
	enum sss_nic_sq_l4_offload_type l4_offload = SSSNIC_DISABLE_OFFLOAD;
	enum sss_nic_sq_l3_proto_type l3_type;
	union sss_nic_ip ip;
	union sss_nic_transport_header l4;
	u32 offset = 0;
	u8 l4_proto;
	int ret;

	if (skb_is_gso(skb) == 0)
		return 0;

	ret = skb_cow_head(skb, 0);
	if (ret < 0)
		return ret;

	sss_nic_get_proto_hdr(task_sect, skb, &ip, &l4);
	sss_nic_get_inner_proto_type(skb, &ip, &l4, &l3_type, &l4_proto);

	if (l4_proto == IPPROTO_TCP)
		l4.tcp->check = ~sss_nic_csum_magic(&ip, IPPROTO_TCP);
#ifdef HAVE_IP6_FRAG_ID_ENABLE_UFO
	else if (l4_proto == IPPROTO_UDP && ip.v4->version == 6)
		task_sect->value[1] = be32_to_cpu(skb_shinfo(skb)->ip6_frag_id);
#endif

	sss_nic_get_inner_transport_info(skb, &l4, l4_proto, &offset, &l4_offload);

	sss_nic_set_tso_info(task_sect, qinfo, l4_offload, offset,
			     skb_shinfo(skb)->gso_size);
	return SSSNIC_TSO_CS_EN;
}

static u32 sss_nic_check_tx_offload(struct sk_buff *skb,
				    struct sss_nic_sqe_task_section *task_sect,
				    u32 *qinfo, struct sss_nic_sq_desc *sq_desc)
{
	u32 offload = 0;
	int tso_cs_en;

	memset(task_sect->value, 0, sizeof(task_sect->value));

	tso_cs_en = sss_nic_check_tso(task_sect, qinfo, skb);
	if (tso_cs_en < 0 ||
	    unlikely(SSSNIC_SQE_CTRL_SECT_QINFO_GET(*qinfo, PLDOFF) >
		     SSSNIC_PAYLOAD_OFFSET_MAX)) {
		offload = SSSNIC_TX_OFFLOAD_INVALID;
		return offload;
	} else if (tso_cs_en != 0) {
		offload |= SSSNIC_TX_OFFLOAD_TSO;
	} else {
		tso_cs_en = sss_nic_check_tx_csum(sq_desc, task_sect, skb);
		if (tso_cs_en != 0)
			offload |= SSSNIC_TX_OFFLOAD_CSUM;
	}

	if (unlikely(skb_vlan_tag_present(skb))) {
		sss_nic_set_vlan_tx_offload(task_sect, skb_vlan_tag_get(skb),
					    sq_desc->qid % SSSNIC_VLAN_INSERT_MODE_MAX);
		offload |= SSSNIC_TX_OFFLOAD_VLAN;
	}

	return offload;
}

static void sss_nic_get_pkt_stats(struct sss_nic_tx_desc *tx_desc,
				  struct sk_buff *skb)
{
	u32 ihs;
	u32 hdr_len;

	if (skb_is_gso(skb)) {
#if (defined(HAVE_SKB_INNER_TRANSPORT_HEADER) && defined(HAVE_SK_BUFF_ENCAPSULATION))
		if (skb->encapsulation) {
#ifdef HAVE_SKB_INNER_TRANSPORT_OFFSET
			ihs = skb_inner_transport_offset(skb) + inner_tcp_hdrlen(skb);
#else
			ihs = (skb_inner_transport_header(skb) - skb->data) + inner_tcp_hdrlen(skb);
#endif
		} else {
#endif
			ihs = skb_transport_offset(skb) + tcp_hdrlen(skb);
#if (defined(HAVE_SKB_INNER_TRANSPORT_HEADER) && defined(HAVE_SK_BUFF_ENCAPSULATION))
		}
#endif
		hdr_len = (skb_shinfo(skb)->gso_segs - 1) * ihs;
		tx_desc->bytes = skb->len + (u64)hdr_len;
	} else {
		tx_desc->bytes = skb->len > ETH_ZLEN ? skb->len : ETH_ZLEN;
	}

	tx_desc->nr_pkt_cnt = SSSNIC_DEF_PKT_CNT;
}

static inline int sss_nic_check_tx_stop(struct sss_nic_sq_desc *sq_desc,
					u16 wqebb_cnt)
{
	if (likely(sss_nic_get_sq_free_wqebbs(sq_desc->sq) >= wqebb_cnt))
		return 0;

	/* We need to check again in a case another CPU has just
	 * made room available.
	 */
	netif_stop_subqueue(sq_desc->netdev, sq_desc->qid);

	if (likely(sss_nic_get_sq_free_wqebbs(sq_desc->sq) < wqebb_cnt))
		return -EBUSY;

	/* there have enough wqebbs after queue is wake up */
	netif_start_subqueue(sq_desc->netdev, sq_desc->qid);

	return 0;
}

static u16 sss_nic_combo_sqe(struct sss_nic_io_queue *sq,
			     struct sss_nic_sqe *sqe,
			     struct sss_nic_sqe_task_section *task_sect,
			     u32 offload, u16 sge_cnt, u16 *curr_pi)
{
	void *wqebb = NULL;
	void *second_part_wqebbs_addr = NULL;
	u16 tmp_pi;
	u16 first_part_wqebbs_num;
	int i;

	sqe->ctrl_sect = sss_nic_get_sq_one_wqebb(sq, curr_pi);
	if (offload == 0 && sge_cnt == 1) {
		sqe->wqe_type = SSSNIC_SQE_COMPACT_TYPE;
		return sss_nic_get_and_update_sq_owner(sq, *curr_pi, 1);
	}

	sqe->wqe_type = SSSNIC_SQE_EXTENDED_TYPE;

	if (offload != 0) {
		sqe->task_sect = sss_nic_get_sq_one_wqebb(sq, &tmp_pi);
		sqe->task_type = SSSNIC_SQE_TASK_LEN_128BITS;

		for (i = 0; i < ARRAY_LEN(sqe->task_sect->value); i++)
			sqe->task_sect->value[i] = sss_hw_be32(task_sect->value[i]);

	} else {
		sqe->task_type = SSSNIC_SQE_TASK_LEN_46BITS;
	}

	if (sge_cnt > 1) {
		/* first wqebb contain bd0, so use weqbb_cnt(sge_num-1) */
		wqebb = sss_nic_get_sq_multi_wqebbs(sq, sge_cnt - 1, &tmp_pi,
						    &second_part_wqebbs_addr,
						    &first_part_wqebbs_num);
		sqe->first_bds_num = first_part_wqebbs_num;
		sqe->bd_sect1 = second_part_wqebbs_addr;
		sqe->bd_sect0 = wqebb;
	}

	return sss_nic_get_and_update_sq_owner(sq, *curr_pi,
					       sge_cnt + (u16)!!offload);
}

static void sss_nic_init_sq_ctrl(struct sss_nic_sqe *sqe,
				 u32 qinfo, int buf_desc, u16 owner)
{
	struct sss_nic_sqe_ctrl_section *ctrl_sect = sqe->ctrl_sect;

	if (sqe->wqe_type == SSSNIC_SQE_COMPACT_TYPE) {
		ctrl_sect->sect_len |=
			SSSNIC_SQE_CTRL_SECT_SET(owner, OWNER) |
			SSSNIC_SQE_CTRL_SECT_SET(sqe->wqe_type, EXTENDED) |
			SSSNIC_SQE_CTRL_SECT_SET(SSSNIC_NORMAL_SQE, DATA_FORMAT);
		ctrl_sect->sect_len = sss_hw_be32(ctrl_sect->sect_len);
		ctrl_sect->qinfo = 0;
		return;
	}

	ctrl_sect->sect_len |= SSSNIC_SQE_CTRL_SECT_SET(buf_desc, BUFDESC_NUM) |
			       SSSNIC_SQE_CTRL_SECT_SET(owner, OWNER) |
			       SSSNIC_SQE_CTRL_SECT_SET(sqe->task_type, TASKSECT_LEN) |
			       SSSNIC_SQE_CTRL_SECT_SET(sqe->wqe_type, EXTENDED) |
			       SSSNIC_SQE_CTRL_SECT_SET(SSSNIC_NORMAL_SQE, DATA_FORMAT);

	ctrl_sect->sect_len = sss_hw_be32(ctrl_sect->sect_len);
	ctrl_sect->qinfo = qinfo;
	ctrl_sect->qinfo |= SSSNIC_SQE_CTRL_SECT_QINFO_SET(1U, UC);

	if (!SSSNIC_SQE_CTRL_SECT_QINFO_GET(ctrl_sect->qinfo, MSS)) {
		ctrl_sect->qinfo |= SSSNIC_SQE_CTRL_SECT_QINFO_SET(SSSNIC_DEFAULT_MSS, MSS);
	} else if (SSSNIC_SQE_CTRL_SECT_QINFO_GET(ctrl_sect->qinfo, MSS) < SSSNIC_MIN_MSS) {
		/* mss should not less than 80 */
		ctrl_sect->qinfo = SSSNIC_SQE_CTRL_SECT_QINFO_CLEAR(ctrl_sect->qinfo, MSS);
		ctrl_sect->qinfo |= SSSNIC_SQE_CTRL_SECT_QINFO_SET(SSSNIC_MIN_MSS, MSS);
	}
	ctrl_sect->qinfo = sss_hw_be32(ctrl_sect->qinfo);
}

static netdev_tx_t sss_nic_send_one_skb(struct sk_buff *skb,
					struct net_device *netdev,
					struct sss_nic_sq_desc *sq_desc)
{
	struct sss_nic_dev *nic_dev = netdev_priv(netdev);
	struct sss_nic_tx_desc *tx_desc = NULL;
	struct sss_nic_sqe sqe = {0};
	struct sss_nic_sqe_task_section task_sect;
	u32 qinfo = 0;
	u32 offload;
	u16 pi = 0;
	u16 owner;
	u16 sge_cnt;
	u16 nr_frags;
	u16 wqebb_cnt;
	bool find_zero_sge_len = false;
	int ret;
	int frag;

	if (unlikely(skb->len < SSSNIC_SKB_LEN_MIN)) {
		if (skb_pad(skb, (int)(SSSNIC_SKB_LEN_MIN - skb->len))) {
			SSSNIC_SQ_STATS_INC(sq_desc, skb_pad_err);
			goto tx_drop_pad_err;
		}

		skb->len = SSSNIC_SKB_LEN_MIN;
	}

	nr_frags = 0;
	for (frag = 0; frag < skb_shinfo(skb)->nr_frags; frag++) {
		if (skb_frag_size(&skb_shinfo(skb)->frags[frag]) == 0) {
			find_zero_sge_len = true;
			continue;
		} else if (find_zero_sge_len) {
			SSSNIC_SQ_STATS_INC(sq_desc, frag_size_zero);
			goto tx_drop_pkts;
		}
		nr_frags++;
	}
	sge_cnt = nr_frags + 1;
	/* task info need 1 wqebb */
	wqebb_cnt = sge_cnt + 1;

	if (unlikely(sss_nic_check_tx_stop(sq_desc, wqebb_cnt))) {
		SSSNIC_SQ_STATS_INC(sq_desc, tx_busy);
		return NETDEV_TX_BUSY;
	}

	offload = sss_nic_check_tx_offload(skb, &task_sect, &qinfo, sq_desc);
	if (unlikely(offload == SSSNIC_TX_OFFLOAD_INVALID)) {
		SSSNIC_SQ_STATS_INC(sq_desc, offload_err);
		goto tx_drop_pkts;
	} else if (offload == 0) {
		/* no TS in current wqe */
		wqebb_cnt -= 1;
		if (unlikely(sge_cnt == 1 && skb->len > SSSNIC_SKB_LEN_MAX))
			goto tx_drop_pkts;
	}
	owner = sss_nic_combo_sqe(sq_desc->sq, &sqe, &task_sect,
				  offload, sge_cnt, &pi);

	tx_desc = &sq_desc->tx_desc_group[pi];
	tx_desc->nr_frags = nr_frags;
	tx_desc->wqebb_cnt = wqebb_cnt;
	tx_desc->skb = skb;
	ret = sss_nic_map_dma_page(nic_dev, skb, nr_frags, sq_desc, tx_desc, &sqe);
	if (ret != 0) {
		sss_nic_rollback_sq_wqebbs(sq_desc->sq, wqebb_cnt, owner);
		goto tx_drop_pkts;
	}

	sss_nic_get_pkt_stats(tx_desc, skb);
	sss_nic_init_sq_ctrl(&sqe, qinfo, sge_cnt, owner);
	sss_nic_write_db(sq_desc->sq, sq_desc->cos, SQ_CFLAG_DP,
			 sss_nic_get_sq_local_pi(sq_desc->sq));
	return NETDEV_TX_OK;

tx_drop_pkts:
	dev_kfree_skb_any(skb);
tx_drop_pad_err:
	SSSNIC_SQ_STATS_INC(sq_desc, tx_dropped);
	return NETDEV_TX_OK;
}

netdev_tx_t sss_nic_loop_start_xmit(struct sk_buff *skb,
				    struct net_device *netdev)
{
	struct sss_nic_dev *nic_dev = netdev_priv(netdev);
	u16 qid = skb_get_queue_mapping(skb);
	struct sss_nic_sq_desc *sq_desc = &nic_dev->sq_desc_group[qid];

	return sss_nic_send_one_skb(skb, netdev, sq_desc);
}

netdev_tx_t sss_nic_ndo_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct sss_nic_sq_desc *sq_desc = NULL;
	struct sss_nic_dev *nic_dev = netdev_priv(netdev);
	u16 qid = skb_get_queue_mapping(skb);

	if (unlikely(!netif_carrier_ok(netdev))) {
		SSSNIC_NIC_STATS_INC(nic_dev, tx_drop);
		goto tx_drop_carrier_off;
	}

	if (unlikely(qid >= nic_dev->qp_res.qp_num)) {
		SSSNIC_NIC_STATS_INC(nic_dev, tx_invalid_qid);
		goto tx_drop_invalid;
	}
	sq_desc = &nic_dev->sq_desc_group[qid];
	return sss_nic_send_one_skb(skb, netdev, sq_desc);

tx_drop_carrier_off:
	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;

tx_drop_invalid:
	dev_kfree_skb_any(skb);
	sq_desc = &nic_dev->sq_desc_group[0];
	SSSNIC_SQ_STATS_INC(sq_desc, tx_dropped);
	return NETDEV_TX_OK;
}

static inline void sss_nic_tx_free_skb(struct sss_nic_dev *nic_dev,
				       struct sss_nic_tx_desc *tx_desc)
{
	sss_nic_unmap_dma_page(nic_dev, tx_desc->nr_frags, tx_desc->dma_group);
	dev_kfree_skb_any(tx_desc->skb);
	tx_desc->skb = NULL;
}

void sss_nic_free_all_skb(struct sss_nic_dev *nic_dev, u32 sq_depth,
			  struct sss_nic_tx_desc *tx_desc_group)
{
	struct sss_nic_tx_desc *tx_desc = NULL;
	u32 i;

	for (i = 0; i < sq_depth; i++) {
		tx_desc = &tx_desc_group[i];
		if (tx_desc->skb)
			sss_nic_tx_free_skb(nic_dev, tx_desc);
	}
}

static u64 sss_nic_stop_subqueue(struct sss_nic_dev *nic_dev,
				 struct sss_nic_sq_desc *sq_desc)
{
	u16 qid = sq_desc->sq->qid;
	u64 wake = 0;
	struct netdev_queue *netdev_txq;

	if (unlikely(__netif_subqueue_stopped(nic_dev->netdev, qid) &&
		     sss_nic_get_sq_free_wqebbs(sq_desc->sq) >= 1 &&
		     test_bit(SSSNIC_INTF_UP, &nic_dev->flags))) {
		netdev_txq = netdev_get_tx_queue(sq_desc->netdev, qid);

		__netif_tx_lock(netdev_txq, smp_processor_id());
		if (__netif_subqueue_stopped(nic_dev->netdev, qid)) {
			netif_wake_subqueue(nic_dev->netdev, qid);
			wake++;
		}
		__netif_tx_unlock(netdev_txq);
	}

	return wake;
}

int sss_nic_tx_poll(struct sss_nic_sq_desc *sq_desc, int budget)
{
	struct sss_nic_tx_desc *tx_desc = NULL;
	struct sss_nic_dev *nic_dev = netdev_priv(sq_desc->netdev);
	u64 tx_byte_cnt = 0;
	u64 nr_pkt_cnt = 0;
	u64 wake = 0;
	u16 sw_ci;
	u16 hw_ci;
	u16 wqebb_cnt = 0;
	int pkt_cnt = 0;

	hw_ci = sss_nic_get_sq_hw_ci(sq_desc->sq);
	dma_rmb();
	sw_ci = sss_nic_get_sq_local_ci(sq_desc->sq);

	do {
		tx_desc = &sq_desc->tx_desc_group[sw_ci];

		if (hw_ci == sw_ci ||
		    ((hw_ci - sw_ci) & sq_desc->qid_mask) < tx_desc->wqebb_cnt)
			break;

		sw_ci = (sw_ci + tx_desc->wqebb_cnt) & (u16)sq_desc->qid_mask;
		prefetch(&sq_desc->tx_desc_group[sw_ci]);

		tx_byte_cnt += tx_desc->bytes;
		nr_pkt_cnt += tx_desc->nr_pkt_cnt;
		wqebb_cnt += tx_desc->wqebb_cnt;
		pkt_cnt++;

		sss_nic_tx_free_skb(nic_dev, tx_desc);
	} while (likely(pkt_cnt < budget));

	sss_nic_update_sq_local_ci(sq_desc->sq, wqebb_cnt);

	wake = sss_nic_stop_subqueue(nic_dev, sq_desc);

	u64_stats_update_begin(&sq_desc->stats.stats_sync);
	sq_desc->stats.tx_bytes += tx_byte_cnt;
	sq_desc->stats.tx_packets += nr_pkt_cnt;
	sq_desc->stats.wake += wake;
	u64_stats_update_end(&sq_desc->stats.stats_sync);

	return pkt_cnt;
}
