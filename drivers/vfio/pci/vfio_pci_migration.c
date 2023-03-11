// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Huawei Technologies Co., Ltd. All rights reserved.
 */

#include <linux/module.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/uaccess.h>
#include <linux/vfio.h>
#include <linux/vfio_pci_migration.h>

#include "vfio_pci_private.h"

static LIST_HEAD(vfio_pci_mig_drivers_list);
static DEFINE_MUTEX(vfio_pci_mig_drivers_mutex);

static void vfio_pci_add_mig_drv(struct vfio_pci_vendor_mig_driver *mig_drv)
{
	mutex_lock(&vfio_pci_mig_drivers_mutex);
	atomic_set(&mig_drv->count, 1);
	list_add_tail(&mig_drv->list, &vfio_pci_mig_drivers_list);
	mutex_unlock(&vfio_pci_mig_drivers_mutex);
}

static void vfio_pci_remove_mig_drv(struct vfio_pci_vendor_mig_driver *mig_drv)
{
	mutex_lock(&vfio_pci_mig_drivers_mutex);
	list_del(&mig_drv->list);
	mutex_unlock(&vfio_pci_mig_drivers_mutex);
}

static struct vfio_pci_vendor_mig_driver *
	vfio_pci_find_mig_drv(struct pci_dev *pdev, struct module *module)
{
	struct vfio_pci_vendor_mig_driver *mig_drv = NULL;

	mutex_lock(&vfio_pci_mig_drivers_mutex);
	list_for_each_entry(mig_drv, &vfio_pci_mig_drivers_list, list) {
		if (mig_drv->owner == module) {
			if (mig_drv->bus_num == pdev->bus->number)
				goto out;
		}
	}
	mig_drv = NULL;
out:
	mutex_unlock(&vfio_pci_mig_drivers_mutex);
	return mig_drv;
}

static struct vfio_pci_vendor_mig_driver *
	vfio_pci_get_mig_driver(struct pci_dev *pdev)
{
	struct vfio_pci_vendor_mig_driver *mig_drv = NULL;
	struct pci_dev *pf_dev = pci_physfn(pdev);

	mutex_lock(&vfio_pci_mig_drivers_mutex);
	list_for_each_entry(mig_drv, &vfio_pci_mig_drivers_list, list) {
		if (mig_drv->bus_num == pf_dev->bus->number)
			goto out;
	}
	mig_drv = NULL;
out:
	mutex_unlock(&vfio_pci_mig_drivers_mutex);
	return mig_drv;
}

bool vfio_dev_migration_is_supported(struct pci_dev *pdev)
{
	struct vfio_pci_vendor_mig_driver *mig_driver = NULL;

	mig_driver = vfio_pci_get_mig_driver(pdev);
	if (!mig_driver || !mig_driver->dev_mig_ops) {
		dev_warn(&pdev->dev, "unable to find a mig_drv module\n");
		return false;
	}

	return true;
}

int vfio_pci_device_log_start(struct vfio_pci_device *vdev,
	struct vf_migration_log_info *log_info)
{
	struct vfio_pci_vendor_mig_driver *mig_driver;

	mig_driver = vfio_pci_get_mig_driver(vdev->pdev);
	if (!mig_driver || !mig_driver->dev_mig_ops) {
		dev_err(&vdev->pdev->dev, "unable to find a mig_drv module\n");
		return -EFAULT;
	}

	if (!mig_driver->dev_mig_ops->log_start ||
		(mig_driver->dev_mig_ops->log_start(vdev->pdev,
			log_info) != 0)) {
		dev_err(&vdev->pdev->dev, "failed to set log start\n");
		return -EFAULT;
	}

	return 0;
}

int vfio_pci_device_log_stop(struct vfio_pci_device *vdev, uint32_t uuid)
{
	struct vfio_pci_vendor_mig_driver *mig_driver;

	mig_driver = vfio_pci_get_mig_driver(vdev->pdev);
	if (!mig_driver || !mig_driver->dev_mig_ops) {
		dev_err(&vdev->pdev->dev, "unable to find a mig_drv module\n");
		return -EFAULT;
	}

	if (!mig_driver->dev_mig_ops->log_stop ||
		(mig_driver->dev_mig_ops->log_stop(vdev->pdev, uuid) != 0)) {
		dev_err(&vdev->pdev->dev, "failed to set log stop\n");
		return -EFAULT;
	}

	return 0;
}

int vfio_pci_device_log_status_query(struct vfio_pci_device *vdev)
{
	struct vfio_pci_vendor_mig_driver *mig_driver;

	mig_driver = vfio_pci_get_mig_driver(vdev->pdev);
	if (!mig_driver || !mig_driver->dev_mig_ops) {
		dev_err(&vdev->pdev->dev, "unable to find a mig_drv module\n");
		return -EFAULT;
	}

	if (!mig_driver->dev_mig_ops->get_log_status ||
		(mig_driver->dev_mig_ops->get_log_status(vdev->pdev) != 0)) {
		dev_err(&vdev->pdev->dev, "failed to get log status\n");
		return -EFAULT;
	}

	return 0;
}

int vfio_pci_device_init(struct pci_dev *pdev)
{
	struct vfio_pci_vendor_mig_driver *mig_drv;

	mig_drv = vfio_pci_get_mig_driver(pdev);
	if (!mig_drv || !mig_drv->dev_mig_ops) {
		dev_err(&pdev->dev, "unable to find a mig_drv module\n");
		return -EFAULT;
	}

	if (mig_drv->dev_mig_ops->init)
		return mig_drv->dev_mig_ops->init(pdev);

	return -EFAULT;
}

void vfio_pci_device_uninit(struct pci_dev *pdev)
{
	struct vfio_pci_vendor_mig_driver *mig_drv;

	mig_drv = vfio_pci_get_mig_driver(pdev);
	if (!mig_drv || !mig_drv->dev_mig_ops) {
		dev_err(&pdev->dev, "unable to find a mig_drv module\n");
		return;
	}

	if (mig_drv->dev_mig_ops->uninit)
		mig_drv->dev_mig_ops->uninit(pdev);
}

static void vfio_pci_device_release(struct pci_dev *pdev,
	struct vfio_pci_vendor_mig_driver *mig_drv)
{
	if (mig_drv->dev_mig_ops->release)
		mig_drv->dev_mig_ops->release(pdev);
}

static int vfio_pci_device_get_info(struct pci_dev *pdev,
	struct vfio_device_migration_info *mig_info,
	struct vfio_pci_vendor_mig_driver *mig_drv)
{
	if (mig_drv->dev_mig_ops->get_info)
		return mig_drv->dev_mig_ops->get_info(pdev, mig_info);
	return -EFAULT;
}

static int vfio_pci_device_enable(struct pci_dev *pdev,
	struct vfio_pci_vendor_mig_driver *mig_drv)
{
	if (!mig_drv->dev_mig_ops->enable ||
		(mig_drv->dev_mig_ops->enable(pdev) != 0)) {
		return -EINVAL;
	}

	return 0;
}

static int vfio_pci_device_disable(struct pci_dev *pdev,
	struct vfio_pci_vendor_mig_driver *mig_drv)
{
	if (!mig_drv->dev_mig_ops->disable ||
		(mig_drv->dev_mig_ops->disable(pdev) != 0))
		return -EINVAL;

	return 0;
}

static int vfio_pci_device_pre_enable(struct pci_dev *pdev,
	struct vfio_pci_vendor_mig_driver *mig_drv)
{
	if (!mig_drv->dev_mig_ops->pre_enable ||
		(mig_drv->dev_mig_ops->pre_enable(pdev) != 0))
		return -EINVAL;

	return 0;
}

static int vfio_pci_device_state_save(struct pci_dev *pdev,
	struct vfio_pci_migration_data *data)
{
	struct vfio_device_migration_info *mig_info = data->mig_ctl;
	struct vfio_pci_vendor_mig_driver *mig_drv = data->mig_driver;
	void *base = (void *)mig_info;
	int ret = 0;

	if ((mig_info->device_state & VFIO_DEVICE_STATE_RUNNING) != 0) {
		ret = vfio_pci_device_disable(pdev, mig_drv);
		if (ret) {
			dev_err(&pdev->dev, "failed to stop VF function!\n");
			return ret;
		}
		mig_info->device_state &= ~VFIO_DEVICE_STATE_RUNNING;
	}

	if (mig_drv->dev_mig_ops && mig_drv->dev_mig_ops->save) {
		ret = mig_drv->dev_mig_ops->save(pdev, base,
			mig_info->data_offset, data->state_size);
		if (ret) {
			dev_err(&pdev->dev, "failed to save device state!\n");
			return -EINVAL;
		}
	} else {
		return -EFAULT;
	}

	mig_info->data_size = data->state_size;
	mig_info->pending_bytes = mig_info->data_size;
	return ret;
}

static int vfio_pci_device_state_restore(struct vfio_pci_migration_data *data)
{
	struct vfio_device_migration_info *mig_info = data->mig_ctl;
	struct vfio_pci_vendor_mig_driver *mig_drv = data->mig_driver;
	struct pci_dev *pdev = data->vf_dev;
	void *base = (void *)mig_info;
	int ret;

	if (mig_drv->dev_mig_ops && mig_drv->dev_mig_ops->restore) {
		ret = mig_drv->dev_mig_ops->restore(pdev, base,
			mig_info->data_offset, mig_info->data_size);
		if (ret) {
			dev_err(&pdev->dev, "failed to restore device state!\n");
			return -EINVAL;
		}
		return 0;
	}

	return -EFAULT;
}

static int vfio_pci_set_device_state(struct vfio_pci_migration_data *data,
	u32 state)
{
	struct vfio_device_migration_info *mig_ctl = data->mig_ctl;
	struct vfio_pci_vendor_mig_driver *mig_drv = data->mig_driver;
	struct pci_dev *pdev = data->vf_dev;
	int ret = 0;

	if (state == mig_ctl->device_state)
		return 0;

	if (!mig_drv->dev_mig_ops)
		return -EINVAL;

	switch (state) {
	case VFIO_DEVICE_STATE_RUNNING:
		if (!(mig_ctl->device_state &
			VFIO_DEVICE_STATE_RUNNING))
			ret = vfio_pci_device_enable(pdev, mig_drv);
		break;
	case VFIO_DEVICE_STATE_SAVING | VFIO_DEVICE_STATE_RUNNING:
		/*
		 * (pre-copy) - device should start logging data.
		 */
		ret = 0;
		break;
	case VFIO_DEVICE_STATE_SAVING:
		/* stop the vf function, save state */
		ret = vfio_pci_device_state_save(pdev, data);
		break;
	case VFIO_DEVICE_STATE_STOP:
		if (mig_ctl->device_state & VFIO_DEVICE_STATE_RUNNING)
			ret = vfio_pci_device_disable(pdev, mig_drv);
		break;
	case VFIO_DEVICE_STATE_RESUMING:
		ret = vfio_pci_device_pre_enable(pdev, mig_drv);
		break;
	default:
		ret = -EFAULT;
		break;
	}

	if (ret)
		return ret;

	mig_ctl->device_state = state;
	return 0;
}

static ssize_t vfio_pci_handle_mig_dev_state(
	struct vfio_pci_migration_data *data,
	char __user *buf, size_t count, bool iswrite)
{
	struct vfio_device_migration_info *mig_ctl = data->mig_ctl;
	u32 device_state;
	int ret;

	if (count != sizeof(device_state))
		return -EINVAL;

	if (iswrite) {
		if (copy_from_user(&device_state, buf, count))
			return -EFAULT;

		ret = vfio_pci_set_device_state(data, device_state);
		if (ret)
			return ret;
	} else {
		if (copy_to_user(buf, &mig_ctl->device_state, count))
			return -EFAULT;
	}

	return count;
}

static ssize_t vfio_pci_handle_mig_pending_bytes(
	struct vfio_device_migration_info *mig_info,
	char __user *buf, size_t count, bool iswrite)
{
	u64 pending_bytes;

	if (count != sizeof(pending_bytes) || iswrite)
		return -EINVAL;

	if (mig_info->device_state ==
		(VFIO_DEVICE_STATE_SAVING | VFIO_DEVICE_STATE_RUNNING)) {
		/* In pre-copy state we have no data to return for now,
		 * return 0 pending bytes
		 */
		pending_bytes = 0;
	} else {
		pending_bytes = mig_info->pending_bytes;
	}

	if (copy_to_user(buf, &pending_bytes, count))
		return -EFAULT;

	return count;
}

static ssize_t vfio_pci_handle_mig_data_offset(
	struct vfio_device_migration_info *mig_info,
	char __user *buf, size_t count, bool iswrite)
{
	u64 data_offset = mig_info->data_offset;

	if (count != sizeof(data_offset) || iswrite)
		return -EINVAL;

	if (copy_to_user(buf, &data_offset, count))
		return -EFAULT;

	return count;
}

static ssize_t vfio_pci_handle_mig_data_size(
	struct vfio_device_migration_info *mig_info,
	char __user *buf, size_t count, bool iswrite)
{
	u64 data_size;

	if (count != sizeof(data_size))
		return -EINVAL;

	if (iswrite) {
		/* data_size is writable only during resuming state */
		if (mig_info->device_state != VFIO_DEVICE_STATE_RESUMING)
			return -EINVAL;

		if (copy_from_user(&data_size, buf, sizeof(data_size)))
			return -EFAULT;

		mig_info->data_size = data_size;
	} else {
		if (mig_info->device_state != VFIO_DEVICE_STATE_SAVING)
			return -EINVAL;

		if (copy_to_user(buf, &mig_info->data_size,
			sizeof(data_size)))
			return -EFAULT;
	}

	return count;
}

static ssize_t vfio_pci_handle_mig_dev_cmd(struct vfio_pci_migration_data *data,
	char __user *buf, size_t count, bool iswrite)
{
	struct vfio_pci_vendor_mig_driver *mig_drv = data->mig_driver;
	struct pci_dev *pdev = data->vf_dev;
	u32 device_cmd;
	int ret = -EFAULT;

	if (count != sizeof(device_cmd) || !iswrite || !mig_drv->dev_mig_ops)
		return -EINVAL;

	if (copy_from_user(&device_cmd, buf, count))
		return -EFAULT;

	switch (device_cmd) {
	case VFIO_DEVICE_MIGRATION_CANCEL:
		if (mig_drv->dev_mig_ops->cancel)
			ret = mig_drv->dev_mig_ops->cancel(pdev);
		break;
	default:
		dev_err(&pdev->dev, "cmd is invaild\n");
		return -EINVAL;
	}

	if (ret != 0)
		return ret;

	return count;
}

static ssize_t vfio_pci_handle_mig_drv_version(
	struct vfio_device_migration_info *mig_info,
	char __user *buf, size_t count, bool iswrite)
{
	u32 version_id = mig_info->version_id;

	if (count != sizeof(version_id) || iswrite)
		return -EINVAL;

	if (copy_to_user(buf, &version_id, count))
		return -EFAULT;

	return count;
}

static ssize_t vfio_pci_handle_mig_data_rw(
	struct vfio_pci_migration_data *data,
	char __user *buf, size_t count, u64 pos, bool iswrite)
{
	struct vfio_device_migration_info *mig_ctl = data->mig_ctl;
	void *data_addr = data->vf_data;

	if (count == 0) {
		dev_err(&data->vf_dev->dev, "qemu operation data size error!\n");
		return -EINVAL;
	}

	data_addr += pos - mig_ctl->data_offset;
	if (iswrite) {
		if (copy_from_user(data_addr, buf, count))
			return -EFAULT;

		mig_ctl->pending_bytes += count;
		if (mig_ctl->pending_bytes > data->state_size)
			return -EINVAL;
	} else {
		if (copy_to_user(buf, data_addr, count))
			return -EFAULT;

		if (mig_ctl->pending_bytes < count)
			return -EINVAL;

		mig_ctl->pending_bytes -= count;
	}

	return count;
}

static ssize_t vfio_pci_dev_migrn_rw(struct vfio_pci_device *vdev,
	char __user *buf, size_t count, loff_t *ppos, bool iswrite)
{
	unsigned int index =
		VFIO_PCI_OFFSET_TO_INDEX(*ppos) - VFIO_PCI_NUM_REGIONS;
	struct vfio_pci_migration_data *data =
		(struct vfio_pci_migration_data *)vdev->region[index].data;
	loff_t pos = *ppos & VFIO_PCI_OFFSET_MASK;
	struct vfio_device_migration_info *mig_ctl = data->mig_ctl;
	int ret;

	if (pos >= vdev->region[index].size)
		return -EINVAL;

	count = min(count, (size_t)(vdev->region[index].size - pos));
	if (pos >= VFIO_MIGRATION_REGION_DATA_OFFSET)
		return vfio_pci_handle_mig_data_rw(data,
			buf, count, pos, iswrite);

	switch (pos) {
	case VFIO_DEVICE_MIGRATION_OFFSET(device_state):
		ret = vfio_pci_handle_mig_dev_state(data,
			buf, count, iswrite);
		break;
	case VFIO_DEVICE_MIGRATION_OFFSET(pending_bytes):
		ret = vfio_pci_handle_mig_pending_bytes(mig_ctl,
			buf, count, iswrite);
		break;
	case VFIO_DEVICE_MIGRATION_OFFSET(data_offset):
		ret = vfio_pci_handle_mig_data_offset(mig_ctl,
			buf, count, iswrite);
		break;
	case VFIO_DEVICE_MIGRATION_OFFSET(data_size):
		ret = vfio_pci_handle_mig_data_size(mig_ctl,
			buf, count, iswrite);
		break;
	case VFIO_DEVICE_MIGRATION_OFFSET(device_cmd):
		ret = vfio_pci_handle_mig_dev_cmd(data,
			buf, count, iswrite);
		break;
	case VFIO_DEVICE_MIGRATION_OFFSET(version_id):
		ret = vfio_pci_handle_mig_drv_version(mig_ctl,
			buf, count, iswrite);
		break;
	default:
		dev_err(&vdev->pdev->dev, "invalid pos offset\n");
		ret = -EFAULT;
		break;
	}

	if (mig_ctl->device_state == VFIO_DEVICE_STATE_RESUMING &&
		mig_ctl->pending_bytes == data->state_size &&
		mig_ctl->data_size == data->state_size) {
		if (vfio_pci_device_state_restore(data) != 0) {
			dev_err(&vdev->pdev->dev, "Failed to restore device state!\n");
			return -EFAULT;
		}
		mig_ctl->pending_bytes = 0;
		mig_ctl->data_size = 0;
	}

	return ret;
}

static void vfio_pci_dev_migrn_release(struct vfio_pci_device *vdev,
	struct vfio_pci_region *region)
{
	struct vfio_pci_migration_data *data = region->data;

	if (data) {
		kfree(data->mig_ctl);
		kfree(data);
	}
}

static const struct vfio_pci_regops vfio_pci_migration_regops = {
	.rw = vfio_pci_dev_migrn_rw,
	.release = vfio_pci_dev_migrn_release,
};

static int vfio_pci_migration_info_init(struct pci_dev *pdev,
	struct vfio_device_migration_info *mig_info,
	struct vfio_pci_vendor_mig_driver *mig_drv)
{
	int ret;

	ret = vfio_pci_device_get_info(pdev, mig_info, mig_drv);
	if (ret) {
		dev_err(&pdev->dev, "failed to get device info\n");
		return ret;
	}

	if (mig_info->data_size > VFIO_MIGRATION_BUFFER_MAX_SIZE) {
		dev_err(&pdev->dev, "mig_info->data_size %llu is invalid\n",
			mig_info->data_size);
		return -EINVAL;
	}

	mig_info->data_offset = VFIO_MIGRATION_REGION_DATA_OFFSET;
	return ret;
}

static int vfio_device_mig_data_init(struct vfio_pci_device *vdev,
	struct vfio_pci_migration_data *data)
{
	struct vfio_device_migration_info *mig_ctl;
	u64 mig_offset;
	int ret;

	mig_ctl = kzalloc(sizeof(*mig_ctl), GFP_KERNEL);
	if (!mig_ctl)
		return -ENOMEM;

	ret = vfio_pci_migration_info_init(vdev->pdev, mig_ctl,
		data->mig_driver);
	if (ret) {
		dev_err(&vdev->pdev->dev, "get device info error!\n");
		goto err;
	}

	mig_offset = sizeof(struct vfio_device_migration_info);
	data->state_size = mig_ctl->data_size;
	data->mig_ctl = krealloc(mig_ctl, mig_offset + data->state_size,
		GFP_KERNEL);
	if (!data->mig_ctl) {
		ret = -ENOMEM;
		goto err;
	}

	data->vf_data = (void *)((char *)data->mig_ctl + mig_offset);
	memset(data->vf_data, 0, data->state_size);
	data->mig_ctl->data_size = 0;

	ret = vfio_pci_register_dev_region(vdev, VFIO_REGION_TYPE_MIGRATION,
		VFIO_REGION_SUBTYPE_MIGRATION,
		&vfio_pci_migration_regops, mig_offset + data->state_size,
		VFIO_REGION_INFO_FLAG_READ | VFIO_REGION_INFO_FLAG_WRITE, data);
	if (ret) {
		kfree(data->mig_ctl);
		return ret;
	}

	return 0;
err:
	kfree(mig_ctl);
	return ret;
}

int vfio_pci_migration_init(struct vfio_pci_device *vdev)
{
	struct vfio_pci_vendor_mig_driver *mig_driver = NULL;
	struct vfio_pci_migration_data *data = NULL;
	struct pci_dev *pdev = vdev->pdev;
	int ret;

	mig_driver = vfio_pci_get_mig_driver(pdev);
	if (!mig_driver || !mig_driver->dev_mig_ops) {
		dev_err(&pdev->dev, "unable to find a mig_driver module\n");
		return -EINVAL;
	}

	if (!try_module_get(mig_driver->owner)) {
		pr_err("module %s is not live\n", mig_driver->owner->name);
		return -ENODEV;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		module_put(mig_driver->owner);
		return -ENOMEM;
	}

	data->mig_driver = mig_driver;
	data->vf_dev = pdev;

	ret = vfio_device_mig_data_init(vdev, data);
	if (ret) {
		dev_err(&pdev->dev, "failed to init vfio device migration data!\n");
		goto err;
	}

	return ret;
err:
	kfree(data);
	module_put(mig_driver->owner);
	return ret;
}

void vfio_pci_migration_exit(struct vfio_pci_device *vdev)
{
	struct vfio_pci_vendor_mig_driver *mig_driver = NULL;

	mig_driver = vfio_pci_get_mig_driver(vdev->pdev);
	if (!mig_driver || !mig_driver->dev_mig_ops) {
		dev_warn(&vdev->pdev->dev, "mig_driver is not found\n");
		return;
	}

	if (module_refcount(mig_driver->owner) > 0) {
		vfio_pci_device_release(vdev->pdev, mig_driver);
		module_put(mig_driver->owner);
	}
}

int vfio_pci_register_migration_ops(struct vfio_device_migration_ops *ops,
	struct module *mod, struct pci_dev *pdev)
{
	struct vfio_pci_vendor_mig_driver *mig_driver = NULL;

	if (!ops || !mod || !pdev)
		return -EINVAL;

	mig_driver = vfio_pci_find_mig_drv(pdev, mod);
	if (mig_driver) {
		pr_info("%s migration ops has already been registered\n",
			mod->name);
		atomic_add(1, &mig_driver->count);
		return 0;
	}

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	mig_driver = kzalloc(sizeof(*mig_driver), GFP_KERNEL);
	if (!mig_driver) {
		module_put(THIS_MODULE);
		return -ENOMEM;
	}

	mig_driver->pdev = pdev;
	mig_driver->bus_num = pdev->bus->number;
	mig_driver->owner = mod;
	mig_driver->dev_mig_ops = ops;

	vfio_pci_add_mig_drv(mig_driver);

	return 0;
}
EXPORT_SYMBOL_GPL(vfio_pci_register_migration_ops);

void vfio_pci_unregister_migration_ops(struct module *mod, struct pci_dev *pdev)
{
	struct vfio_pci_vendor_mig_driver *mig_driver = NULL;

	if (!mod || !pdev)
		return;

	mig_driver = vfio_pci_find_mig_drv(pdev, mod);
	if (!mig_driver) {
		pr_err("mig_driver is not found\n");
		return;
	}

	if (atomic_sub_and_test(1, &mig_driver->count)) {
		vfio_pci_remove_mig_drv(mig_driver);
		kfree(mig_driver);
		module_put(THIS_MODULE);
		pr_info("%s succeed to unregister migration ops\n",
			THIS_MODULE->name);
	}
}
EXPORT_SYMBOL_GPL(vfio_pci_unregister_migration_ops);
