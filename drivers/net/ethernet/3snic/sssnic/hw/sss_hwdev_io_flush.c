// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2021 3snic Technologies Co., Ltd */

#define pr_fmt(fmt) KBUILD_MODNAME ": [BASE]" fmt

#include "sss_kernel.h"
#include "sss_hw.h"
#include "sss_hwdev.h"
#include "sss_hwif_ctrlq_init.h"
#include "sss_hwif_api.h"
#include "sss_hwif_mbx.h"
#include "sss_common.h"

#define SSS_FLR_TIMEOUT					1000

static enum sss_process_ret sss_check_flr_finish_handler(void *priv_data)
{
	struct sss_hwif *hwif = priv_data;
	enum sss_pf_status status;

	status = sss_chip_get_pf_status(hwif);
	if (status == SSS_PF_STATUS_FLR_FINISH_FLAG) {
		sss_chip_set_pf_status(hwif, SSS_PF_STATUS_ACTIVE_FLAG);
		return SSS_PROCESS_OK;
	}

	return SSS_PROCESS_DOING;
}

static int sss_wait_for_flr_finish(struct sss_hwif *hwif)
{
	return sss_check_handler_timeout(hwif, sss_check_flr_finish_handler,
					 SSS_FLR_TIMEOUT, 0xa * USEC_PER_MSEC);
}

static int sss_msg_to_mgmt_no_ack(void *hwdev, u8 mod, u16 cmd,
				  void *buf_in, u16 in_size, u16 channel)
{
	if (!hwdev)
		return -EINVAL;

	if (sss_get_dev_present_flag(hwdev) == 0)
		return -EPERM;

	return sss_send_mbx_to_mgmt_no_ack(hwdev, mod, cmd, buf_in,
					   in_size, channel);
}

int sss_hwdev_flush_io(struct sss_hwdev *hwdev, u16 channel)
{
	struct sss_hwif *hwif = hwdev->hwif;
	struct sss_cmd_clear_doorbell clear_db;
	struct sss_cmd_clear_resource clr_res;
	u16 out_size;
	int err;
	int ret = 0;

	if (hwdev->chip_present_flag == 0)
		return 0;

	if (SSS_GET_FUNC_TYPE(hwdev) != SSS_FUNC_TYPE_VF)
		msleep(100); /* wait ucode 100 ms stop I/O */

	err = sss_wait_ctrlq_stop(hwdev);
	if (err != 0) {
		sdk_warn(hwdev->dev_hdl, "Fail to wait ctrlq stop\n");
		ret = err;
	}

	sss_chip_disable_doorbell(hwif);

	out_size = sizeof(clear_db);
	memset(&clear_db, 0, sizeof(clear_db));
	clear_db.func_id = SSS_GET_HWIF_GLOBAL_ID(hwif);

	err = sss_sync_send_msg_ch(hwdev, SSS_COMM_MGMT_CMD_FLUSH_DOORBELL,
				   &clear_db, sizeof(clear_db),
				   &clear_db, &out_size, channel);
	if (err != 0 || !out_size || clear_db.head.state) {
		sdk_warn(hwdev->dev_hdl,
			 "Fail to flush doorbell, err: %d, status: 0x%x, out_size: 0x%x, channel: 0x%x\n",
			 err, clear_db.head.state, out_size, channel);
		if (err != 0)
			ret = err;
		else
			ret = -EFAULT;
	}

	if (SSS_GET_FUNC_TYPE(hwdev) != SSS_FUNC_TYPE_VF)
		sss_chip_set_pf_status(hwif, SSS_PF_STATUS_FLR_START_FLAG);
	else
		msleep(100);

	memset(&clr_res, 0, sizeof(clr_res));
	clr_res.func_id = SSS_GET_HWIF_GLOBAL_ID(hwif);

	err = sss_msg_to_mgmt_no_ack(hwdev, SSS_MOD_TYPE_COMM,
				     SSS_COMM_MGMT_CMD_START_FLUSH, &clr_res,
				     sizeof(clr_res), channel);
	if (err != 0) {
		sdk_warn(hwdev->dev_hdl, "Fail to notice flush message, err: %d, channel: 0x%x\n",
			 err, channel);
		ret = err;
	}

	if (SSS_GET_FUNC_TYPE(hwdev) != SSS_FUNC_TYPE_VF) {
		err = sss_wait_for_flr_finish(hwif);
		if (err != 0) {
			sdk_warn(hwdev->dev_hdl, "Wait firmware FLR timeout\n");
			ret = err;
		}
	}

	sss_chip_enable_doorbell(hwif);

	err = sss_reinit_ctrlq_ctx(hwdev);
	if (err != 0) {
		sdk_warn(hwdev->dev_hdl, "Fail to reinit ctrlq ctx\n");
		ret = err;
	}

	return ret;
}
