// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/vstream.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/ucc_common.h>
#include <linux/ucc_sched.h>

DEFINE_MUTEX(vstreamId_Bitmap_mutex);
static DECLARE_BITMAP(vstreamIdBitmap, DEVDRV_MAX_SQ_NUM);

static DEFINE_MUTEX(vcqId_Bitmap_mutex);
static DECLARE_BITMAP(vcqIdBitmap, DEVDRV_MAX_CQ_NUM);

static DEFINE_MUTEX(revmap_mutex);

static struct vstream_info *vstreamContainer[DEVDRV_MAX_SQ_NUM];
static struct vcq_map_table *vsqcqMapTable[DEVDRV_MAX_CQ_NUM];

#define MAX_SQ_SIZE	(MAX_VSTREAM_SIZE * MAX_VSTREAM_SLOT_SIZE)
#define MAX_CQ_SIZE	(MAX_VSTREAM_SIZE * MAX_CQ_SLOT_SIZE)

#define SQ_USER_ADDR_OFFSET(id)	((unsigned long)REMAP_ALIGN(MAX_SQ_SIZE) * id)
#define CQ_USER_ADDR_OFFSET(id)	((unsigned long)REMAP_ALIGN(MAX_CQ_SIZE) * id)

#define SQ_VSTREAM_DATA(id) vstreamContainer[id]->vsqNode->vstreamData
#define CQ_VSTREAM_DATA(id) vstreamContainer[id]->vcqNode->vstreamData

static struct tsdrv_ctx *get_ctx(int fd)
{
	struct fd f;
	struct davinci_intf_private_stru *file_private_data;
	struct tsdrv_ctx *ctx = NULL;

	f = fdget(fd);
	if (!f.file)
		goto out;

	file_private_data = f.file->private_data;
	if (!file_private_data)
		goto out;

	ctx = file_private_data->priv_filep.private_data;

out:
	fdput(f);
	return ctx;
}

static struct vcq_map_table *vstream_get_map_table(uint32_t id)
{
	return vsqcqMapTable[id];
}

static void free_vstreamId(uint32_t vstreamId)
{
	mutex_lock(&vstreamId_Bitmap_mutex);
	clear_bit(vstreamId, vstreamIdBitmap);
	mutex_unlock(&vstreamId_Bitmap_mutex);
}

static void free_vcqId(uint32_t vcqId, uint32_t flag)
{
	mutex_lock(&vcqId_Bitmap_mutex);
	if (!(flag & TSDRV_CQ_REUSE))
		clear_bit(vcqId, vcqIdBitmap);
	mutex_unlock(&vcqId_Bitmap_mutex);
}

static void vstream_free_map_table(uint32_t vcqId, uint32_t vstreamId,
				   uint32_t flag)
{
	struct vcq_map_table *freeTable = NULL;
	struct vstream_id *vstreamIdNode = NULL;

	freeTable = vstream_get_map_table(vcqId);
	if (!freeTable) {
		ucc_err("No map found for vcq:%d.\n", vcqId);
		return;
	}

	list_for_each_entry(vstreamIdNode, &freeTable->vstreamId_list, list) {
		if (vstreamIdNode->vstreamId == vstreamId) {
			list_del(&vstreamIdNode->list);
			kfree(vstreamIdNode);
			break;
		}
	}
	if (!(flag & TSDRV_CQ_REUSE)) {
		kfree(freeTable->vcqNode->vstreamData);
		kfree(freeTable->vcqNode);
		kfree(freeTable);
	}
}

static void vstream_alloc_ucc_se(struct ucc_se *se)
{
	memset(&se->statistics, 0, sizeof(se->statistics));
	se->on_cu = 0;
	se->state = SE_PREPARE;
	se->flag = UCC_TIF_NONE;
	se->prio = UCC_PRIO_HIGH;
	se->step = UCC_STEP_SLOW;
	raw_spin_lock_init(&se->se_lock);
}

static struct vstream_info *vstream_create_info(struct tsdrv_ctx *ctx,
					   struct normal_alloc_sqcq_para *para)
{
	struct vcq_map_table *mapTable = NULL;

	struct vstream_info *vstream = kzalloc(sizeof(struct vstream_info),
					       GFP_KERNEL);
	if (!vstream)
		return NULL;

	(void)memcpy(vstream->info, para->info,
		     sizeof(uint32_t) * SQCQ_RTS_INFO_LENGTH);

	vstream->privdata = ctx;
	vstream->tsId = para->tsId;
	vstream->vstreamId = para->sqId;
	vstream->vcqId = para->cqId;

	mapTable = vstream_get_map_table(vstream->vcqId);
	if (!mapTable || !mapTable->vcqNode) {
		ucc_err("No map found for vcqId:%d.\n", vstream->vcqId);
		goto free_vstream;
	}
	vstream->vcqNode = mapTable->vcqNode;
	vstream->vsqNode = kmalloc(sizeof(struct vstream_node), GFP_KERNEL);
	if (!vstream->vsqNode) {
		ucc_err("Failed to alloc memory for vsqNode:%d.\n",
			       vstream->vstreamId);
		goto free_vstream;
	}
	vstream->vsqNode->vstreamData = kmalloc(MAX_SQ_SIZE, GFP_KERNEL);
	if (!vstream->vsqNode->vstreamData)
		goto free_vsqNode;
	vstream->vsqNode->id = vstream->vstreamId;
	vstream->vsqNode->head = 0;
	vstream->vsqNode->tail = 0;
	vstream->vsqNode->credit = MAX_VSTREAM_SIZE;
	raw_spin_lock_init(&vstream->vsqNode->spin_lock);
	vstream->send_cnt = 0;
	vstream->p = current;
	vstream_alloc_ucc_se(&vstream->se);

	return vstream;

free_vsqNode:
	kfree(vstream->vsqNode);

free_vstream:
	kfree(vstream);
	return NULL;
}

struct vstream_info *vstream_get_info(uint32_t id)
{
	return vstreamContainer[id];
}

static void vstream_free_info(uint32_t id)
{
	struct vstream_info *freeInfo = vstream_get_info(id);

	ucc_set_vstream_state(freeInfo, SE_DEAD);

	if (freeInfo) {
		if (freeInfo->vsqNode)
			kfree(freeInfo->vsqNode->vstreamData);

		kfree(freeInfo->vsqNode);
	}

	kfree(freeInfo);
}

static int queue_pop_by_num(struct vstream_node *node, uint32_t pop_num)
{
	if (node->credit + pop_num > MAX_VSTREAM_SIZE) {
		ucc_err("Queue usage out-of-bounds");
		return -EACCES;
	}

	node->credit += pop_num;
	node->head = (node->head + pop_num) % MAX_VSTREAM_SIZE;
	return 0;
}

static int queue_pop_by_head(struct vstream_node *node, uint32_t head)
{
	int pop_num = (head - node->head + MAX_VSTREAM_SIZE) %
		      MAX_VSTREAM_SIZE;
	return queue_pop_by_num(node, pop_num);
}

int update_vstream_head(struct vstream_info *vstream_info, int num)
{
	struct vstream_node *node = vstream_info->vsqNode;

	raw_spin_lock(&node->spin_lock);
	if (node->credit + num > MAX_VSTREAM_SIZE) {
		raw_spin_unlock(&node->spin_lock);
		return -1;
	}

	node->credit += num;
	node->head = (node->head + num) % MAX_VSTREAM_SIZE;
	raw_spin_unlock(&node->spin_lock);

	return 0;
}

bool vstream_have_kernel(struct ucc_se *se)
{
	struct vstream_info *vinfo;

	vinfo = container_of(se, struct vstream_info, se);
	return vinfo->vsqNode->credit != MAX_VSTREAM_SIZE;
}

static int queue_push_by_num(struct vstream_node *node, uint32_t push_num)
{
	if (node->credit - push_num < 0)
		return -EACCES;

	node->credit -= push_num;
	node->tail = (node->tail + push_num) % MAX_VSTREAM_SIZE;
	return 0;
}

static int queue_push_by_tail(struct vstream_node *node, uint32_t tail)
{
	int push_num = (tail - node->tail + MAX_VSTREAM_SIZE) %
		       MAX_VSTREAM_SIZE;
	return queue_push_by_num(node, push_num);
}

static uint32_t vstream_alloc_vstreamId(void)
{
	uint32_t vstreamId = DEVDRV_MAX_SQ_NUM;

	/* alloc vstreamId */
	mutex_lock(&vstreamId_Bitmap_mutex);
	vstreamId = find_first_zero_bit(vstreamIdBitmap, DEVDRV_MAX_SQ_NUM);
	if (vstreamId == DEVDRV_MAX_SQ_NUM) {
		ucc_err("vstreamId exhausted.\n");
		mutex_unlock(&vstreamId_Bitmap_mutex);
		return DEVDRV_MAX_SQ_NUM;
	}
	set_bit(vstreamId, vstreamIdBitmap);
	mutex_unlock(&vstreamId_Bitmap_mutex);

	return vstreamId;
}

static uint32_t vstream_alloc_vcqid(void)
{
	uint32_t vcqId = DEVDRV_MAX_CQ_NUM;

	/* alloc vcqid */
	mutex_lock(&vcqId_Bitmap_mutex);
	vcqId = find_first_zero_bit(vcqIdBitmap, DEVDRV_MAX_CQ_NUM);
	if (vcqId == DEVDRV_MAX_CQ_NUM) {
		ucc_err("vcqId has been used up.\n");
		mutex_unlock(&vcqId_Bitmap_mutex);
		return DEVDRV_MAX_CQ_NUM;
	}
	set_bit(vcqId, vcqIdBitmap);
	mutex_unlock(&vcqId_Bitmap_mutex);

	ucc_info("vcqId = %d\n", vcqId);
	return vcqId;
}

int vstream_map_pfnaddr(struct tsdrv_ctx *ctx,
			struct normal_alloc_sqcq_para *para)
{
	int err = 0;
	unsigned long vsqAddr;
	unsigned long vcqAddr;
	pgprot_t vm_page_prot;
	struct vm_area_struct *vma = ctx->vma[para->tsId];

	vsqAddr = vma->vm_start + SQ_USER_ADDR_OFFSET(para->sqId);
	vm_page_prot = pgprot_device(vma->vm_page_prot);
	err = remap_pfn_range(vma, vsqAddr,
			      virt_to_pfn(SQ_VSTREAM_DATA(para->sqId)),
			      MAX_SQ_SIZE, vm_page_prot);
	if (err) {
		ucc_err("remap_pfn_range failed,ret=%d.\n", err);
		return -EFAULT;
	}
	if (!(para->flag & TSDRV_CQ_REUSE)) {
		vcqAddr = vma->vm_start + DEVDRV_VM_CQ_MEM_OFFSET +
					CQ_USER_ADDR_OFFSET(para->cqId);
		err = remap_pfn_range(vma, vcqAddr,
				      virt_to_pfn(CQ_VSTREAM_DATA(para->sqId)),
				      MAX_CQ_SIZE, vm_page_prot);
		if (err) {
			ucc_err("remap_pfn_range failed,ret=%d.\n", err);
			return -EFAULT;
		}
	}

	return err;
}

void vstream_unmap_pfnaddr(struct tsdrv_ctx *ctx,
			   struct normal_free_sqcq_para *para)
{
	unsigned long vsqAddr;
	unsigned long vcqAddr;
	size_t cqSize = PAGE_ALIGN(MAX_CQ_SIZE);
	struct vm_area_struct *vma = ctx->vma[para->tsId];

	vsqAddr = vma->vm_start + SQ_USER_ADDR_OFFSET(para->sqId);
	zap_vma_ptes(vma, vsqAddr, MAX_SQ_SIZE);

	if (!(para->flag & TSDRV_CQ_REUSE)) {
		vcqAddr = vma->vm_start + DEVDRV_VM_CQ_MEM_OFFSET +
					CQ_USER_ADDR_OFFSET(para->cqId);
		zap_vma_ptes(vma, vcqAddr, cqSize);
	}
}

static int vstream_update_vcqtable(uint32_t vcqId, uint32_t vstreamId,
				   uint32_t flag)
{
	int err = -ENOSPC;
	struct vcq_map_table *vcqTable = NULL;
	struct vstream_id *vstreamIdNode = NULL;

	if (!(flag & TSDRV_CQ_REUSE)) {
		vcqTable = kmalloc(sizeof(struct vcq_map_table), GFP_KERNEL);
		if (!vcqTable)
			return -ENOMEM;

		vcqTable->vcqId = vcqId;
		vcqTable->vcqNode = kmalloc(sizeof(struct vstream_node),
					    GFP_KERNEL);
		if (!vcqTable->vcqNode) {
			err = -ENOMEM;
			goto free_vcqTable;
		}

		vcqTable->vcqNode->vstreamData = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!vcqTable->vcqNode->vstreamData) {
			err = -ENOMEM;
			goto free_vcqNode;
		}
		vcqTable->vcqNode->id = vcqId;
		vcqTable->vcqNode->head = 0;
		vcqTable->vcqNode->tail = 0;
		vcqTable->vcqNode->credit = MAX_VSTREAM_SIZE;
		INIT_LIST_HEAD(&vcqTable->vstreamId_list);
		vsqcqMapTable[vcqId] = vcqTable;
	} else {
		vcqTable = vsqcqMapTable[vcqId];
	}
	vstreamIdNode = kmalloc(sizeof(struct vstream_id), GFP_KERNEL);
	if (!vstreamIdNode) {
		err = -ENOMEM;

		if (!(flag & TSDRV_CQ_REUSE))
			goto free_vstreamData;
		return err;
	}
	vstreamIdNode->vstreamId = vstreamId;
	list_add(&vstreamIdNode->list, &vcqTable->vstreamId_list);

	return 0;

free_vstreamData:
	kfree(vcqTable->vcqNode->vstreamData);

free_vcqNode:
	kfree(vcqTable->vcqNode);

free_vcqTable:
	kfree(vcqTable);

	return err;
}

int ascend_vstream_alloc(struct vstream_args *arg)
{
	uint32_t vstreamId;
	uint32_t vcqId = DEVDRV_MAX_CQ_NUM;
	int err = -EINVAL;
	struct vstream_info *vstream = NULL;
	struct tsdrv_ctx *ctx = NULL;
	struct normal_alloc_sqcq_para *sqcq_alloc_para = &arg->va_args.ascend;

	ctx = get_ctx(sqcq_alloc_para->fd);
	if (!ctx)
		return err;

	vstreamId = vstream_alloc_vstreamId();
	if (vstreamId == DEVDRV_MAX_SQ_NUM) {
		ucc_err("vstreamId alloc failed.\n");
		return err;
	}
	if (!(sqcq_alloc_para->flag & TSDRV_CQ_REUSE))
		vcqId = vstream_alloc_vcqid();
	else
		vcqId = sqcq_alloc_para->cqId;

	if (vcqId >= DEVDRV_MAX_CQ_NUM) {
		ucc_err("vcqId alloc failed.\n");
		goto free_vstreamIds;
	}
	err = vstream_update_vcqtable(vcqId, vstreamId, sqcq_alloc_para->flag);
	if (err) {
		ucc_err("vcqtable update failed, vcqId:%d, vstreamId:%d, flag:%d.\n",
			 vcqId, vstreamId, sqcq_alloc_para->flag);
		goto free_vcqid;
	}

	sqcq_alloc_para->sqId = vstreamId;
	sqcq_alloc_para->cqId = vcqId;
	vstream = vstream_create_info(ctx, sqcq_alloc_para);
	if (!vstream) {
		ucc_err("vstream create failed: vcqId:%d, vstreamId:%d.\n",
			vcqId, vstreamId);
		err = -ENOSPC;
		goto free_vcqtable;
	}

	vstream->devId = sqcq_alloc_para->devId;
	vstreamContainer[vstreamId] = vstream;

	vstream->group = select_sq(vstream);
	if (!vstream->group) {
		ucc_err("Failed to select sq\n");
		err = -EINVAL;
		goto free_vstream_info;
	}

	err = vstream_map_pfnaddr(ctx, sqcq_alloc_para);
	if (err) {
		ucc_err("vstream map failed, ret=%d.\n", err);
		goto free_vstream_info;
	}
	return 0;

free_vstream_info:
	vstream_free_info(vstreamId);

free_vcqtable:
	vstream_free_map_table(vcqId, vstreamId, sqcq_alloc_para->flag);

free_vcqid:
	free_vcqId(vcqId, sqcq_alloc_para->flag);

free_vstreamIds:
	free_vstreamId(vstreamId);

	return err;
}

int ascend_vstream_free(struct vstream_args *arg)
{
	int err = 0;
	struct vstream_info *vstreamInfo = NULL;
	struct normal_free_sqcq_para *sqcq_free_para = &arg->vf_args.ascend;
	uint32_t vstreamId = sqcq_free_para->sqId;
	uint32_t vcqId = sqcq_free_para->cqId;

	if (vstreamId >= DEVDRV_MAX_SQ_NUM || vcqId >= DEVDRV_MAX_CQ_NUM) {
		ucc_err("vstream index out-of-range, vstreamId=%d, vcqId=%d.\n",
			vstreamId, vcqId);
		return -EPERM;
	}

	vstreamInfo = vstream_get_info(vstreamId);
	if (!vstreamInfo) {
		ucc_err("vstreamInfo get failed, vstreamId=%d.\n", vstreamId);
		return -EPERM;
	}
	err = ucc_free_task(vstreamInfo, vstreamInfo->privdata);

	free_vcqId(vcqId, sqcq_free_para->flag);
	vstream_free_map_table(vcqId, vstreamId, sqcq_free_para->flag);

	vstream_unmap_pfnaddr(vstreamInfo->privdata, sqcq_free_para);

	vstream_free_info(vstreamId);
	free_vstreamId(vstreamId);
	return err;
}

int ascend_vstream_kick(struct vstream_args *arg)
{
	int err = 0;
	struct tsdrv_sqcq_data_para *sqcq_data_para = &arg->vk_args.ascend;
	int vstreamId = sqcq_data_para->id;
	int tail = sqcq_data_para->val;
	struct vstream_info *vstreamInfo = NULL;
	int push_num;

	vstreamInfo = vstream_get_info(vstreamId);
	vstreamInfo->p = current;

	if (!vstreamInfo) {
		ucc_err("vstreamInfo get failed, vstreamId=%d.\n", vstreamId);
		return -ENOMEM;
	}

	push_num = (tail - vstreamInfo->vsqNode->tail + MAX_VSTREAM_SIZE) %
		   MAX_VSTREAM_SIZE;

	raw_spin_lock(&vstreamInfo->vsqNode->spin_lock);
	err = queue_push_by_tail(vstreamInfo->vsqNode, tail);
	if (err) {
		raw_spin_unlock(&vstreamInfo->vsqNode->spin_lock);
		ucc_err("queue_push_by_tail error, ret = %d\n", err);
		return err;
	}
	raw_spin_unlock(&vstreamInfo->vsqNode->spin_lock);

	err = ucc_wake_up(&vstreamInfo->se);
	return err;
}

int ascend_callback_vstream_wait(struct vstream_args *arg)
{
	int err = 0;
	int cqeNum = 0;
	int cqeSum = 0;
	struct vstream_info *vstreamInfo = NULL;
	struct vcq_map_table *vcqTable = NULL;
	struct vcq_map_table *waitTable = NULL;
	struct vstream_id *vstreamIdNode = NULL;
	struct devdrv_report_para *report_para = &arg->cvw_args;
	uint32_t *sqlist;
	uint32_t sqlist_num = 0;
	uint32_t vstreamId, vcqId;

	sqlist = kmalloc_array(DEVDRV_MAX_SQ_NUM, sizeof(uint32_t), GFP_KERNEL);
	if (!sqlist)
		return -ENOMEM;

	vcqId = report_para->cq_id;
	if (vcqId >= DEVDRV_MAX_CQ_NUM) {
		ucc_err("vcqId out-of-range, vcqId=%d.\n", vcqId);
		err = -EPERM;
		goto out;
	}

	mutex_lock(&vcqId_Bitmap_mutex);
	waitTable = vstream_get_map_table(vcqId);
	if (!waitTable) {
		ucc_err("No map found for vcq:%d.\n", vcqId);
		mutex_unlock(&vcqId_Bitmap_mutex);
		err = -EPERM;
		goto out;
	}

	list_for_each_entry(vstreamIdNode, &waitTable->vstreamId_list, list)
		sqlist[sqlist_num++] = vstreamIdNode->vstreamId;
	mutex_unlock(&vcqId_Bitmap_mutex);

	//get sqInfo from hardware
	for (vstreamId = 0; vstreamId < sqlist_num; vstreamId++) {
		vstreamInfo = vstream_get_info(sqlist[vstreamId]);
		if (!vstreamInfo)
			continue;
		err |= ucc_wait_cq(vstreamInfo, vstreamInfo->privdata,
				   report_para, &cqeNum);
		cqeSum += cqeNum;
		if (cqeNum)
			break;
	}

	//update cqInfo
	mutex_lock(&vcqId_Bitmap_mutex);
	vcqTable = vstream_get_map_table(vcqId);
	if (!vcqTable) {
		ucc_err("No map found for vcq:%d.\n", vcqId);
		err = -EPERM;
		goto out;
	}

	err = queue_push_by_num(vcqTable->vcqNode, cqeSum);
	if (err) {
		mutex_unlock(&vcqId_Bitmap_mutex);
		ucc_err("failed to queue_push_by_num, ret = %d.\n", err);
		goto out;
	}
	report_para->cq_tail = vcqTable->vcqNode->tail;
	mutex_unlock(&vcqId_Bitmap_mutex);

out:
	kfree(sqlist);
	return err;
}

int ascend_callback_vstream_kick(struct vstream_args *arg)
{
	u32 vcqId, release_head;
	struct vstream_info *vstreamInfo = NULL;
	int err = 0;

	vcqId = arg->cvk_args.id;
	release_head = arg->cvk_args.val;
	if (vcqId >= DEVDRV_MAX_CQ_NUM || release_head >= MAX_VSTREAM_SIZE) {
		ucc_err("vstream index out-of-range, vcqId=%d, release_head=%d.\n",
			vcqId, release_head);
		return -EPERM;
	}

	mutex_lock(&vcqId_Bitmap_mutex);
	vstreamInfo = vstream_get_info(vcqId);
	if (!vstreamInfo) {
		err = -EPERM;
		goto out;
	}

	err = queue_pop_by_head(vstreamInfo->vcqNode, release_head);

out:
	mutex_unlock(&vcqId_Bitmap_mutex);
	return err;
}

int ascend_vstream_get_head(struct vstream_args *arg)
{
	u32 vstreamId = arg->vh_args.id;
	struct vstream_info *vstreamInfo = NULL;

	if (vstreamId >= DEVDRV_MAX_SQ_NUM) {
		ucc_err("vstreamId out-of-range, vstreamId=%d.\n", vstreamId);
		return -EINVAL;
	}

	vstreamInfo = vstream_get_info(vstreamId);
	if (!vstreamInfo) {
		ucc_err("vstreamInfo get failed, vstreamId=%d.\n", vstreamId);
		return -EINVAL;
	}
	arg->vh_args.val = vstreamInfo->vsqNode->head;

	return 0;
}

