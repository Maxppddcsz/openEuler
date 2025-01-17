/* SPDX-License-Identifier: GPL-2.0 */
/*
 * arm_spe_decoder.h: Arm Statistical Profiling Extensions support
 * Copyright (c) 2019-2020, Arm Ltd.
 * Copyright (c) 2024-2025, Huawei Technologies Ltd.
 */

#ifndef INCLUDE__ARM_SPE_DECODER_H__
#define INCLUDE__ARM_SPE_DECODER_H__

#include <linux/stddef.h>

#include "arm-spe-pkt-decoder.h"

enum arm_spe_sample_type {
	ARM_SPE_L1D_ACCESS		= 1 << 0,
	ARM_SPE_L1D_MISS		= 1 << 1,
	ARM_SPE_LLC_ACCESS		= 1 << 2,
	ARM_SPE_LLC_MISS		= 1 << 3,
	ARM_SPE_TLB_ACCESS		= 1 << 4,
	ARM_SPE_TLB_MISS		= 1 << 5,
	ARM_SPE_BRANCH_MISS		= 1 << 6,
	ARM_SPE_REMOTE_ACCESS		= 1 << 7,
};

enum arm_spe_op_type {
	ARM_SPE_LD			= 1 << 0,
	ARM_SPE_ST			= 1 << 1,
};

enum arm_spe_neoverse_data_source {
	ARM_SPE_NV_L1D			= 0x0,
	ARM_SPE_NV_L2			= 0x8,
	ARM_SPE_NV_PEER_CORE		= 0x9,
	ARM_SPE_NV_LOCAL_CLUSTER	= 0xa,
	ARM_SPE_NV_SYS_CACHE		= 0xb,
	ARM_SPE_NV_PEER_CLUSTER		= 0xc,
	ARM_SPE_NV_REMOTE		= 0xd,
	ARM_SPE_NV_DRAM			= 0xe,
};

struct arm_spe_record {
	enum arm_spe_sample_type	type;
	int				err;
	u32				op;
	u32				latency;
	u64				from_ip;
	u64				to_ip;
	u64				timestamp;
	u64				virt_addr;
	u64				phys_addr;
	u64				context_id;
	u16				source;
};

struct arm_spe_buffer {
	const unsigned char		*buf;
	size_t				len;
	u64				offset;
	u64				trace_nr;
};

struct arm_spe_decoder {
	struct arm_spe_record		record;
	const unsigned char		*buf;
	size_t				len;
	struct arm_spe_pkt		packet;
};

void arm_spe_decoder_free(struct arm_spe_decoder *decoder);
void arm_spe_decode_buf(const unsigned char *buf, size_t len);
void arm_spe_record_enqueue(struct arm_spe_record *record);

#endif
