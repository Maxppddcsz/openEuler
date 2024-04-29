/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 *
 * File Name	 : roce_mr.h
 * Version	   : v2.0
 * Created	   : 2021/4/24
 * Last Modified : 2022/2/21
 * Description   : Define standard kernel RoCE MR realted functions.
 */

#ifndef ROCE_MR_H
#define ROCE_MR_H

#include <rdma/ib_verbs.h>
#include <rdma/ib_umem.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>

#include "hinic3_rdma.h"

#include "roce.h"
#include "roce_compat.h"
#include "roce_pd.h"
#include "roce_user.h"

#include "hmm_mr.h"
#include "hmm_comp.h"
#include "hinic3_hmm.h"

#define ROCE3_MR_DEFAULT_ACCESS \
	(IB_ACCESS_REMOTE_READ | IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_ATOMIC)

#define ROCE3_MR_PAGES_ALIGN 64
#define ROCE3_MIN_PAGE 2

#define FRMR_PAGE_SIZE 4096

struct roce3_sig_ctx {
	struct ib_sig_attrs sig_attrs;  /* recorded when submitting WR */
	struct ib_sig_err sig_err_item; /* Get information configuration from CQE when polling CQE */
	bool sig_status_checked;		/* Indicates whether the status is checked */
	bool sig_err_exists;			/* Indicates if an error exists */
	u32 sig_err_count;			  /* Count the number of wrong CQEs */
};

struct roce3_mr {
	struct ib_mr ibmr;		/* mr structure provided by ofed */
	struct ib_umem *umem;	 /* Maintain the structure where the user-mode allocated buffer */
	struct rdma_mr rdmamr;	/* mr structure provided by rdma component */
	struct roce3_sig_ctx sig; /* Record signature related information */
	bool signature_en;		/* Whether the logo contains signature information */
	__be64 *pages;
	u32 npages;
	u32 max_pages;
	size_t page_map_size;
	dma_addr_t page_map;
};

struct roce3_mw {
	struct ib_mw ibmw;	 /* mw structure provided by ofed */
	struct rdma_mw rdmamw; /* mw structure provided by rdma component */
};

/* through ibmroce3_e_mr */
static inline struct roce3_mr *to_roce3_mr(const struct ib_mr *ibmr)
{
	return container_of(ibmr, struct roce3_mr, ibmr);
}

/* through ibmroce3_e_mw */
static inline struct roce3_mw *to_roce3_mw(const struct ib_mw *ibmw)
{
	return container_of(ibmw, struct roce3_mw, ibmw);
}

/* through rdmam find roce3_mr */
static inline struct roce3_mr *rdma_mr_to_roce3_mr(const struct rdma_mr *rdmamr)
{
	return container_of(rdmamr, struct roce3_mr, rdmamr);
}

int roce3_alloc_tpt(struct roce3_device *rdev, struct rdma_mr *mr, u32 npages, u32 page_shift);
void roce3_free_tpt(struct roce3_device *rdev, struct rdma_mr *mr);
void roce3_set_rdma_mr(struct rdma_mr *mr, enum rdma_mr_type mr_type, u32 pdn, u64 iova, u64 size, u32 access);

int roce3_user_mr_reg(struct roce3_device *rdev, struct roce3_mr *mr, u32 pdn, u64 virt_addr,
	u64 length, int access);

#endif // __ROCE_MR_H_
