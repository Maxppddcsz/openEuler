/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2021 3snic Technologies Co., Ltd */

#ifndef SSS_NIC_IO_H
#define SSS_NIC_IO_H

#include "sss_hw.h"
#include "sss_hw_wq.h"
#include "sss_nic_io_define.h"

#define SSSNIC_RQ_WQEBB_SHIFT		3
#define SSSNIC_CQE_SIZE_SHIFT		4
#define SSSNIC_SQ_WQEBB_SHIFT		4
#define SSSNIC_MIN_QUEUE_DEPTH		128
#define SSSNIC_MAX_RX_QUEUE_DEPTH	16384
#define SSSNIC_MAX_TX_QUEUE_DEPTH	65536
#define SSSNIC_SQ_WQEBB_SIZE		BIT(SSSNIC_SQ_WQEBB_SHIFT)

/* ******************** DOORBELL DEFINE INFO ******************** */
#define DB_INFO_CFLAG_SHIFT		23
#define DB_INFO_QID_SHIFT		0
#define DB_INFO_TYPE_SHIFT		27
#define DB_INFO_NON_FILTER_SHIFT	22
#define DB_INFO_COS_SHIFT		24

#define DB_INFO_COS_MASK		0x7U
#define DB_INFO_QID_MASK		0x1FFFU
#define DB_INFO_CFLAG_MASK		0x1U
#define DB_INFO_TYPE_MASK		0x1FU
#define DB_INFO_NON_FILTER_MASK		0x1U
#define SSSNIC_DB_INFO_SET(val, member)	\
		(((u32)(val) & DB_INFO_##member##_MASK) << \
		 DB_INFO_##member##_SHIFT)

#define DB_PI_HIGH_MASK			0xFFU
#define DB_PI_LOW_MASK			0xFFU
#define DB_PI_HI_SHIFT			8
#define SRC_TYPE			1
#define DB_PI_HIGH(pi)		(((pi) >> DB_PI_HI_SHIFT) & DB_PI_HIGH_MASK)
#define DB_PI_LOW(pi)			((pi) & DB_PI_LOW_MASK)
#define DB_ADDR(queue, pi)	((u64 *)((queue)->db_addr) + DB_PI_LOW(pi))

#define sss_nic_get_sq_local_pi(sq) SSS_WQ_MASK_ID(&sq->wq, sq->wq.pi)
#define sss_nic_get_sq_local_ci(sq) SSS_WQ_MASK_ID(&sq->wq, sq->wq.ci)
#define sss_nic_get_sq_hw_ci(sq)                                               \
	SSS_WQ_MASK_ID(&sq->wq, sss_hw_cpu16(*(u16 *)sq->tx.ci_addr))

#define sss_nic_get_rq_local_pi(rq) SSS_WQ_MASK_ID(&rq->wq, rq->wq.pi)
#define sss_nic_get_rq_local_ci(rq) SSS_WQ_MASK_ID(&rq->wq, rq->wq.ci)

/* CFLAG_DATA_PATH */
#define RQ_CFLAG_DP			1
#define SQ_CFLAG_DP			0

enum sss_nic_queue_type {
	SSSNIC_SQ,
	SSSNIC_RQ,
	SSSNIC_MAX_QUEUE_TYPE
};

struct sss_nic_db {
	u32 db_info;
	u32 pi_hi;
};

enum sss_nic_rq_wqe_type {
	SSSNIC_COMPACT_RQ_WQE,
	SSSNIC_NORMAL_RQ_WQE,
	SSSNIC_EXTEND_RQ_WQE,
};

int sss_nic_io_resource_init(struct sss_nic_io *nic_io);
int sss_nic_init_qp_info(struct sss_nic_io *nic_io, struct sss_nic_qp_info *qp_info);
int sss_nic_alloc_qp(struct sss_nic_io *nic_io,
		     struct sss_irq_desc *qp_msix_arry, struct sss_nic_qp_info *qp_info);
void sss_nic_io_resource_deinit(struct sss_nic_io *nic_io);
void sss_nic_free_qp(struct sss_nic_io *nic_io, struct sss_nic_qp_info *qp_info);
void sss_nic_deinit_qp_info(struct sss_nic_io *nic_io, struct sss_nic_qp_info *qp_info);
int sss_nic_init_qp_ctx(struct sss_nic_io *nic_io);
void sss_nic_deinit_qp_ctx(void *hwdev);

/* *
 * @brief sss_nic_update_sq_local_ci - update send queue local consumer index
 * @param sq: send queue
 * @param wqe_cnt: number of wqebb
 */
static inline void sss_nic_update_sq_local_ci(struct sss_nic_io_queue *sq,
					      u16 wqebb_cnt)
{
	sss_update_wq_ci(&sq->wq, wqebb_cnt);
}

/* *
 * @brief sss_nic_get_sq_wqe_with_owner - get send queue wqe with owner
 * @param sq: send queue
 * @param wqebb_cnt: wqebb counter
 * @param pi: return current pi
 * @param owner: return owner bit
 * @param second_part_wqebbs_addr: second part wqebbs base address
 * @param first_part_wqebbs_num: number wqebbs of first part
 * @retval : first part wqebbs base address
 */
static inline void *sss_nic_get_sq_wqe_with_owner(struct sss_nic_io_queue *sq,
						  u16 wqebb_cnt, u16 *pi, u16 *owner,
		void **second_part_wqebbs_addr, u16 *first_part_wqebbs_num)
{
	void *wqe = sss_wq_get_multi_wqebb(&sq->wq, wqebb_cnt, pi,
					   second_part_wqebbs_addr, first_part_wqebbs_num);

	*owner = sq->owner;
	if (unlikely(*pi + wqebb_cnt >= sq->wq.q_depth))
		sq->owner = !sq->owner;

	return wqe;
}

/* *
 * @brief sss_nic_get_and_update_sq_owner - get and update send queue owner bit
 * @param sq: send queue
 * @param curr_pi: current pi
 * @param wqebb_cnt: wqebb counter
 * @retval : owner bit
 */
static inline u16 sss_nic_get_and_update_sq_owner(struct sss_nic_io_queue *sq,
						  u16 curr_pi, u16 wqebb_cnt)
{
	u16 owner = sq->owner;

	if (unlikely(curr_pi + wqebb_cnt >= sq->wq.q_depth))
		sq->owner = !sq->owner;

	return owner;
}

/* *
 * @brief sss_nic_update_rq_hw_pi - update receive queue hardware pi
 * @param rq: receive queue
 * @param pi: pi
 */
static inline void sss_nic_update_rq_hw_pi(struct sss_nic_io_queue *rq, u16 pi)
{
	*rq->rx.pi_vaddr = cpu_to_be16((pi & rq->wq.id_mask) << rq->wqe_type);
}

/* *
 * @brief sss_nic_update_rq_local_ci - update receive queue local consumer index
 * @param sq: receive queue
 * @param wqe_cnt: number of wqebb
 */
static inline void sss_nic_update_rq_local_ci(struct sss_nic_io_queue *rq,
					      u16 wqebb_cnt)
{
	sss_update_wq_ci(&rq->wq, wqebb_cnt);
}

/* *
 * @brief sss_nic_rq_wqe_addr - get receive queue wqe address by queue index
 * @param rq: receive queue
 * @param idx: wq index
 * @retval: wqe base address
 */
static inline void *sss_nic_rq_wqe_addr(struct sss_nic_io_queue *rq, u16 id)
{
	return sss_wq_wqebb_addr(&rq->wq, id);
}

/* *
 * @brief sss_nic_rollback_sq_wqebbs - rollback send queue wqe
 * @param sq: send queue
 * @param wqebb_cnt: wqebb counter
 * @param owner: owner bit
 */
static inline void sss_nic_rollback_sq_wqebbs(struct sss_nic_io_queue *sq,
					      u16 wqebb_cnt, u16 owner)
{
	if (owner != sq->owner)
		sq->owner = (u8)owner;
	sq->wq.pi -= wqebb_cnt;
}

/* *
 * @brief sss_nic_get_sq_one_wqebb - get send queue wqe with single wqebb
 * @param sq: send queue
 * @param pi: return current pi
 * @retval : wqe base address
 */
static inline void *sss_nic_get_sq_one_wqebb(struct sss_nic_io_queue *sq, u16 *pi)
{
	return sss_wq_get_one_wqebb(&sq->wq, pi);
}

/* *
 * @brief sss_nic_get_sq_multi_wqebb - get send queue wqe with multiple wqebbs
 * @param sq: send queue
 * @param wqebb_cnt: wqebb counter
 * @param pi: return current pi
 * @param second_part_wqebbs_addr: second part wqebbs base address
 * @param first_part_wqebbs_num: number wqebbs of first part
 * @retval : first part wqebbs base address
 */
static inline void *sss_nic_get_sq_multi_wqebbs(struct sss_nic_io_queue *sq,
						u16 wqebb_cnt, u16 *pi,
		void **second_part_wqebbs_addr,
		u16 *first_part_wqebbs_num)
{
	return sss_wq_get_multi_wqebb(&sq->wq, wqebb_cnt, pi,
				      second_part_wqebbs_addr,
				      first_part_wqebbs_num);
}

/* *
 * @brief sss_nic_get_sq_free_wqebbs - get send queue free wqebb
 * @param sq: send queue
 * @retval : number of free wqebb
 */
static inline u16 sss_nic_get_sq_free_wqebbs(struct sss_nic_io_queue *sq)
{
	return sss_wq_free_wqebb(&sq->wq);
}

/* *
 * @brief sss_nic_write_db - write doorbell
 * @param queue: nic io queue
 * @param cos: cos index
 * @param cflag: 0--sq, 1--rq
 * @param pi: product index
 */
static inline void sss_nic_write_db(struct sss_nic_io_queue *queue,
				    int cos, u8 cflag, u16 pi)
{
	struct sss_nic_db doorbell;

	doorbell.db_info = SSSNIC_DB_INFO_SET(SRC_TYPE, TYPE) | SSSNIC_DB_INFO_SET(cflag, CFLAG) |
			   SSSNIC_DB_INFO_SET(cos, COS) | SSSNIC_DB_INFO_SET(queue->qid, QID);
	doorbell.pi_hi = DB_PI_HIGH(pi);
	doorbell.db_info = sss_hw_be32(doorbell.db_info);
	doorbell.pi_hi = sss_hw_be32(doorbell.pi_hi);

	/*set doorbell fields before writing it to DB address*/
	wmb();

	writeq(*((u64 *)&doorbell), DB_ADDR(queue, pi));
}

#endif
