/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2021 3snic Technologies Co., Ltd */

#ifndef SSS_HW_STATISTICS_H
#define SSS_HW_STATISTICS_H

#include <linux/types.h>
#include <linux/atomic.h>

#include "sss_hw_event.h"
#include "sss_hw_aeq.h"

struct cqm_stats {
	atomic_t cqm_cmd_alloc_cnt;
	atomic_t cqm_cmd_free_cnt;
	atomic_t cqm_send_cmd_box_cnt;
	atomic_t cqm_send_cmd_imm_cnt;
	atomic_t cqm_db_addr_alloc_cnt;
	atomic_t cqm_db_addr_free_cnt;
	atomic_t cqm_fc_srq_create_cnt;
	atomic_t cqm_srq_create_cnt;
	atomic_t cqm_rq_create_cnt;
	atomic_t cqm_qpc_mpt_create_cnt;
	atomic_t cqm_nonrdma_queue_create_cnt;
	atomic_t cqm_rdma_queue_create_cnt;
	atomic_t cqm_rdma_table_create_cnt;
	atomic_t cqm_qpc_mpt_delete_cnt;
	atomic_t cqm_nonrdma_queue_delete_cnt;
	atomic_t cqm_rdma_queue_delete_cnt;
	atomic_t cqm_rdma_table_delete_cnt;
	atomic_t cqm_func_timer_clear_cnt;
	atomic_t cqm_func_hash_buf_clear_cnt;
	atomic_t cqm_scq_callback_cnt;
	atomic_t cqm_ecq_callback_cnt;
	atomic_t cqm_nocq_callback_cnt;
	atomic_t cqm_aeq_callback_cnt[112];
};

struct sss_link_event_stats {
	atomic_t link_down_stats;
	atomic_t link_up_stats;
};

struct sss_fault_event_stats {
	/* TODO :SSS_NIC_NODE_ID_MAX: temp use the value of 1822(22) */
	atomic_t chip_fault_stats[22][SSS_FAULT_LEVEL_MAX];
	atomic_t fault_type_stat[SSS_FAULT_TYPE_MAX];
	atomic_t pcie_fault_stats;
};

struct sss_hw_stats {
	atomic_t heart_lost_stats;
	struct cqm_stats cqm_stats;
	struct sss_link_event_stats sss_link_event_stats;
	struct sss_fault_event_stats sss_fault_event_stats;
	atomic_t nic_ucode_event_stats[SSS_ERR_MAX];
};

#define SSS_CHIP_FAULT_SIZE (110 * 1024)

#endif
