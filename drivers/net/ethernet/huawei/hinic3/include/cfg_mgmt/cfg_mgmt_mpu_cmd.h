/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
 * Description: Header file for communication between the: Host and MPU
 * Author: None
 * Create: 2016/05/07
 */
#ifndef CFG_MGMT_MPU_CMD_H
#define CFG_MGMT_MPU_CMD_H

enum cfg_cmd {
    CFG_CMD_GET_DEV_CAP = 0, /**< Configure the device capability of pf/vf, @see struct cfg_cmd_dev_cap */
    CFG_CMD_GET_HOST_TIMER = 1, /**< Configure the capability of host timer, @see struct cfg_cmd_host_timer */
};

#endif
