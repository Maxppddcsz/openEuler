// SPDX-License-Identifier: GPL-2.0

#include <linux/ucc_sched.h>
#include <linux/ucc_common.h>

static DEFINE_MUTEX(revmap_mutex);

static DEFINE_HASHTABLE(vrtsq_rtsq_revmap, VRTSQ_RTSQ_HASH_ORDER);

/**
 * @group: value for this entry.
 * @hash_node : hash node list.
 * @
 */
struct vsqce_idx_revmap_data {
	unsigned int vrtsdId;
	struct xpu_group *group;
	struct hlist_node hash_node;
};

struct xpu_group *select_sq(struct vstream_info *vstream_info)
{
	struct vsqce_idx_revmap_data *revmap_data;

	/* find history */
	mutex_lock(&revmap_mutex);
	hash_for_each_possible(vrtsq_rtsq_revmap, revmap_data, hash_node,
			       (unsigned long)vstream_info->vstreamId) {
		if (revmap_data && revmap_data->group) {
			mutex_unlock(&revmap_mutex);
			return revmap_data->group;
		}
	}
	mutex_unlock(&revmap_mutex);

	revmap_data = kzalloc(sizeof(struct vsqce_idx_revmap_data), GFP_KERNEL);
	if (revmap_data == NULL)
		return NULL;
	/* find XPU group */
	revmap_data->group = xpu_group_find(xpu_root, XPU_TYPE_NPU_310);
	if (revmap_data->group == NULL) {
		ucc_err("find XPU group is failed.\n");
		return NULL;
	}
	/* find device group */
	revmap_data->group = xpu_group_find(revmap_data->group,
					    vstream_info->devId);
	if (revmap_data->group == NULL) {
		ucc_err("find device group is failed.\n");
		return NULL;
	}
	/* find tsgroup */
	revmap_data->group = xpu_group_find(revmap_data->group,
					    vstream_info->tsId);
	if (revmap_data->group == NULL) {
		ucc_err("find ts group is failed.\n");
		return NULL;
	}

	/* select idle xcu */
	revmap_data->group = xpu_idle_group_find(revmap_data->group);
	if (revmap_data->group == NULL) {
		ucc_err("find rtsq group is failed.\n");
		return NULL;
	}

	revmap_data->vrtsdId = vstream_info->vstreamId;
	/* set group used : 1 */
	revmap_data->group->used = 1;

	mutex_lock(&revmap_mutex);
	hash_add(vrtsq_rtsq_revmap, &revmap_data->hash_node,
		 (unsigned long)vstream_info->vstreamId);
	mutex_unlock(&revmap_mutex);
	return revmap_data->group;
}

int ucc_process_task(struct vstream_info *vstream_info, struct tsdrv_ctx *ctx,
		     int *sqenum)
{
	struct xpu_group *group = NULL;

	if (vstream_info == NULL) {
		ucc_err("vsqcq_info is NULL\n");
		return -1;
	}

	group = select_sq(vstream_info);
	if (group == NULL) {
		ucc_err("find group is failed.\n");
		return -1;
	}
	/* send sqe */
	*sqenum = xpu_run(group, vstream_info, ctx);

	return 0;
}
EXPORT_SYMBOL(ucc_process_task);

int ucc_free_task(struct vstream_info *vstream_info, struct tsdrv_ctx *ctx)
{
	struct vsqce_idx_revmap_data *revmap_data;

	ucc_dequeue_task(vstream_info);

	while (!ucc_xcu_is_sched(vstream_info->cu_id))
		schedule_timeout_interruptible(10);

	ucc_dump_statistics_info(&vstream_info->se);

	mutex_lock(&revmap_mutex);
	hash_for_each_possible(vrtsq_rtsq_revmap, revmap_data, hash_node,
			       (unsigned long)vstream_info->vstreamId) {
		if (revmap_data &&
		    revmap_data->vrtsdId == vstream_info->vstreamId &&
		    revmap_data->group) {
			xpu_finish(revmap_data->group, vstream_info, ctx);
			/* set group unused : 0 */
			revmap_data->group->used = 0;
			hash_del(&revmap_data->hash_node);
			kfree(revmap_data);
			revmap_data = NULL;
			break;
		}
	}
	mutex_unlock(&revmap_mutex);

	return 0;
}
EXPORT_SYMBOL(ucc_free_task);

int ucc_wait_cq(struct vstream_info *vstream_info, struct tsdrv_ctx *ctx,
		struct devdrv_report_para *arg, int *cqenum)
{
	struct vsqce_idx_revmap_data *revmap_data;

	hash_for_each_possible(vrtsq_rtsq_revmap, revmap_data, hash_node,
			       (unsigned long)vstream_info->vstreamId) {
		if (revmap_data &&
		    revmap_data->vrtsdId == vstream_info->vstreamId &&
		    revmap_data->group)
			*cqenum = xpu_wait(revmap_data->group, vstream_info,
					   ctx, arg);
	}

	return 0;
}
EXPORT_SYMBOL(ucc_wait_cq);
