/* *****************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 ******************************************************************************
  Version	   : Initial Draft
  Created	   : 2023-07-30
  Last Modified :
  Description   : erdes/mag cmd definition between driver and mpu
  Function List :
***************************************************************************** */

#ifndef MAG_MPU_CMD_H
#define MAG_MPU_CMD_H

/* serdes/mag消息命令字定义 */
enum mag_cmd {
	/* serdes命令字，统一封装所有serdes命令 */
	SERDES_CMD_PROCESS = 0, /**< serdes cmd @see struct serdes_cmd_in */

	/* mag命令字，按功能划分 */
	/* 端口配置相关 0-29 */
	MAG_CMD_SET_PORT_CFG = 1,  /**< set port cfg fuction @see struct mag_cmd_set_port_cfg */
	MAG_CMD_SET_PORT_ADAPT = 2, /**< set port adapt mode @see struct mag_cmd_set_port_adapt */
	MAG_CMD_CFG_LOOPBACK_MODE = 3, /**< set port loopback mode @see struct mag_cmd_cfg_loopback_mode */

	MAG_CMD_GET_PORT_ENABLE = 5, /**< get port enable status @see struct mag_cmd_get_port_enable */
	MAG_CMD_SET_PORT_ENABLE = 6, /**< set port enable mode @see struct mag_cmd_set_port_enable */
	MAG_CMD_GET_LINK_STATUS = 7, /**< get port link status @see struct mag_cmd_get_link_status */
	MAG_CMD_SET_LINK_FOLLOW = 8, /**< set port link_follow mode @see struct mag_cmd_set_link_follow */
	MAG_CMD_SET_PMA_ENABLE = 9, /**< set pma enable mode @see struct mag_cmd_set_pma_enable */
	MAG_CMD_CFG_FEC_MODE = 10, /**< set port fec mode @see struct mag_cmd_cfg_fec_mode */
	MAG_CMD_GET_BOND_STATUS = 11, /* reserved for future use */

	MAG_CMD_CFG_AN_TYPE = 12, /* reserved for future use */
	MAG_CMD_CFG_LINK_TIME = 13, /**< get link time @see struct mag_cmd_get_link_time */

	MAG_CMD_SET_PANGEA_ADAPT = 15, /**< set pangea adapt mode @see struct mag_cmd_set_pangea_adapt */

	/* bios link配置相关 30-49 */
	MAG_CMD_CFG_BIOS_LINK_CFG = 31, /* reserved for future use */
	MAG_CMD_RESTORE_LINK_CFG = 32, /**< restore link cfg @see struct mag_cmd_restore_link_cfg */
	MAG_CMD_ACTIVATE_BIOS_LINK_CFG = 33, /**< active bios link cfg @see struct mag_cmd_activate_bios_link_cfg */

	/* 光模块、LED、PHY等外设配置管理 50-99 */
	/* LED */
	MAG_CMD_SET_LED_CFG = 50, /**< set led cfg @see struct mag_cmd_set_led_cfg */

	/* PHY */
	MAG_CMD_GET_PHY_INIT_STATUS = 55, /* reserved for future use */

	/* 光模块 */
	MAG_CMD_GET_XSFP_INFO = 60, /**< get xsfp info @see struct mag_cmd_get_xsfp_info */
	MAG_CMD_SET_XSFP_ENABLE = 61, /**< set xsfp enable mode @see struct mag_cmd_set_xsfp_enable */
	MAG_CMD_GET_XSFP_PRESENT = 62, /**< get xsfp present status @see struct mag_cmd_get_xsfp_present */
	MAG_CMD_SET_XSFP_RW = 63, /**< sfp/qsfp single byte read/write, for equipment test @see struct mag_cmd_set_xsfp_rw */
	MAG_CMD_CFG_XSFP_TEMPERATURE = 64, /**< get xsfp temperature @see struct mag_cmd_sfp_temp_out_info */

	/* 事件上报 100-149 */
	MAG_CMD_WIRE_EVENT = 100,
	MAG_CMD_LINK_ERR_EVENT = 101,

	/* DFX、Counter相关 */
	MAG_CMD_EVENT_PORT_INFO = 150, /**< get port event info @see struct mag_cmd_event_port_info */
	MAG_CMD_GET_PORT_STAT = 151, /**< get port state @see struct mag_cmd_get_port_stat */
	MAG_CMD_CLR_PORT_STAT = 152, /**< clear port state @see struct mag_cmd_port_stats_info */
	MAG_CMD_GET_PORT_INFO = 153, /**< get port info @see struct mag_cmd_get_port_info */
	MAG_CMD_GET_PCS_ERR_CNT = 154, /**< pcs err count @see struct mag_cmd_event_port_info */
	MAG_CMD_GET_MAG_CNT = 155, /**< fec code count @see struct mag_cmd_get_mag_cnt */
	MAG_CMD_DUMP_ANTRAIN_INFO = 156, /**< dump anlt info @see struct mag_cmd_dump_antrain_info */

	/* patch预留cmd */
	MAG_CMD_PATCH_RSVD_0 = 200,
	MAG_CMD_PATCH_RSVD_1 = 201,
	MAG_CMD_PATCH_RSVD_2 = 202,
	MAG_CMD_PATCH_RSVD_3 = 203,
	MAG_CMD_PATCH_RSVD_4 = 204,

	MAG_CMD_MAX = 0xFF
};

#endif
