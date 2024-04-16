/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Filename	  : mpu_inband_cmd.h
 * Version	   : Initial Draft
 * Creation time : 2023/08/26
 * Last Modified :
 * Description   : In-band commands between the driver and the MPU
 */

#ifndef MPU_INBAND_CMD_H
#define MPU_INBAND_CMD_H

enum hinic3_mgmt_cmd {
	COMM_MGMT_CMD_FUNC_RESET = 0, /**< reset fuction @see struct comm_cmd_func_reset */
	COMM_MGMT_CMD_FEATURE_NEGO,   /**< feature negotiation @see struct comm_cmd_feature_nego */
	COMM_MGMT_CMD_FLUSH_DOORBELL, /**< clear doorbell @see struct comm_cmd_clear_doorbell */
	COMM_MGMT_CMD_START_FLUSH,	/**< clear statefull business txrx resource
									* @see struct comm_cmd_clear_resource */
	COMM_MGMT_CMD_SET_FUNC_FLR, /**< set function flr @see struct comm_cmd_func_flr_set */
	COMM_MGMT_CMD_GET_GLOBAL_ATTR, /**< get global attr @see struct comm_cmd_get_glb_attr */
	COMM_MGMT_CMD_SET_PPF_FLR_TYPE, /**< set ppf flr type @see struct comm_cmd_ppf_flr_type_set */
	COMM_MGMT_CMD_SET_FUNC_SVC_USED_STATE, /**< set function service used state
											 * @see struct comm_cmd_func_svc_used_state */
	COMM_MGMT_CMD_START_FLR, /* MPU 未使用 */

	COMM_MGMT_CMD_CFG_MSIX_NUM = 10, /**< set msix num @see struct comm_cmd_cfg_msix_num */

	COMM_MGMT_CMD_SET_CMDQ_CTXT = 20, /**< set commandq context @see struct comm_cmd_cmdq_ctxt */
	COMM_MGMT_CMD_SET_VAT, /**< set vat table info @see struct comm_cmd_root_ctxt */
	COMM_MGMT_CMD_CFG_PAGESIZE, /**< set rootctx pagesize @see struct comm_cmd_wq_page_size */
	COMM_MGMT_CMD_CFG_MSIX_CTRL_REG, /**< config msix ctrl register @struct comm_cmd_msix_config */
	COMM_MGMT_CMD_SET_CEQ_CTRL_REG, /**< set ceq ctrl register @see struct comm_cmd_ceq_ctrl_reg */
	COMM_MGMT_CMD_SET_DMA_ATTR, /**< set PF/VF DMA table attr @see struct comm_cmd_dma_attr_config */
	COMM_MGMT_CMD_SET_PPF_TBL_HTR_FLG, /**< set PPF func table os hotreplace flag
										* @see struct comm_cmd_ppf_tbl_htrp_config */

	COMM_MGMT_CMD_GET_MQM_FIX_INFO = 40, /**< get mqm fix info @see struct comm_cmd_get_eqm_num */
	COMM_MGMT_CMD_SET_MQM_CFG_INFO, /**< set mqm config info @see struct comm_cmd_eqm_cfg */
	COMM_MGMT_CMD_SET_MQM_SRCH_GPA, /**< set mqm search gpa info @see struct comm_cmd_eqm_search_gpa */
	COMM_MGMT_CMD_SET_PPF_TMR, /**< set ppf tmr @see struct comm_cmd_ppf_tmr_op */
	COMM_MGMT_CMD_SET_PPF_HT_GPA, /**< set ppf ht gpa @see struct comm_cmd_ht_gpa */
	COMM_MGMT_CMD_SET_FUNC_TMR_BITMAT, /**< set smf timer enable status @see struct comm_cmd_func_tmr_bitmap_op */
	COMM_MGMT_CMD_SET_MBX_CRDT, /**< reserved */
	COMM_MGMT_CMD_CFG_TEMPLATE, /**< config template @see struct comm_cmd_cfg_template */
	COMM_MGMT_CMD_SET_MQM_LIMIT, /**< set mqm limit @see struct comm_cmd_set_mqm_limit */

	COMM_MGMT_CMD_GET_FW_VERSION = 60, /**< get firmware version @see struct comm_cmd_get_fw_version */
	COMM_MGMT_CMD_GET_BOARD_INFO, /**< get board info @see struct comm_cmd_board_info */
	COMM_MGMT_CMD_SYNC_TIME, /**< synchronize host time to MPU @see struct comm_cmd_sync_time */
	COMM_MGMT_CMD_GET_HW_PF_INFOS, /**< get pf info @see struct comm_cmd_hw_pf_infos */
	COMM_MGMT_CMD_SEND_BDF_INFO, /**< send bdf info @see struct comm_cmd_bdf_info */
	COMM_MGMT_CMD_GET_VIRTIO_BDF_INFO, /**< get virtio bdf info @see struct mpu_pcie_device_info_s */
	COMM_MGMT_CMD_GET_SML_TABLE_INFO, /**< get sml table info @see struct comm_cmd_get_sml_tbl_data */
	COMM_MGMT_CMD_GET_SDI_INFO, /**< get sdi info @see struct comm_cmd_sdi_info */
	COMM_MGMT_CMD_ROOT_CTX_LOAD, /**< get root context info @see struct comm_cmd_root_ctx_load_req_s */
	COMM_MGMT_CMD_GET_HW_BOND, /**< get bond info @see struct comm_cmd_hw_bond_infos */

	COMM_MGMT_CMD_UPDATE_FW = 80, /**< update firmware @see struct cmd_update_fw
									* @see struct comm_info_head */
	COMM_MGMT_CMD_ACTIVE_FW, /**< cold active firmware @see struct cmd_active_firmware */
	COMM_MGMT_CMD_HOT_ACTIVE_FW, /**< hot active firmware @see struct cmd_hot_active_fw */
	COMM_MGMT_CMD_HOT_ACTIVE_DONE_NOTICE, /**< reserved */
	COMM_MGMT_CMD_SWITCH_CFG, /**< switch config file @see struct cmd_switch_cfg */
	COMM_MGMT_CMD_CHECK_FLASH, /**< check flash @see struct comm_info_check_flash */
	COMM_MGMT_CMD_CHECK_FLASH_RW, /**< check whether flash reads and writes normally
									* @see struct comm_cmd_hw_bond_infos */
	COMM_MGMT_CMD_RESOURCE_CFG, /**< reserved */
	COMM_MGMT_CMD_UPDATE_BIOS, /**< update bios firmware @see struct cmd_update_fw */
	COMM_MGMT_CMD_MPU_GIT_CODE, /**< get mpu git tag @see struct cmd_get_mpu_git_code */

	COMM_MGMT_CMD_FAULT_REPORT = 100, /**< report fault event to driver */
	COMM_MGMT_CMD_WATCHDOG_INFO, /**< report software watchdog timeout to driver
									* @see struct comm_info_sw_watchdog */
	COMM_MGMT_CMD_MGMT_RESET, /**< report mpu chip reset to driver */
	COMM_MGMT_CMD_FFM_SET, /* report except interrupt to driver */

	COMM_MGMT_CMD_GET_LOG = 120, /**< get the log of the dictionary @see struct nic_log_info_request */
	COMM_MGMT_CMD_TEMP_OP, /**< temperature operation  @see struct comm_temp_in_info
							 * @see struct comm_temp_out_info */
	COMM_MGMT_CMD_EN_AUTO_RST_CHIP, /**< set or query the enable status of the chip self-reset function
									* @see struct comm_cmd_enable_auto_rst_chip */
	COMM_MGMT_CMD_CFG_REG, /**< reserved */
	COMM_MGMT_CMD_GET_CHIP_ID, /**< get chip id @see struct comm_chip_id_info */
	COMM_MGMT_CMD_SYSINFO_DFX, /**< reserved */
	COMM_MGMT_CMD_PCIE_DFX_NTC, /**< reserved */
	COMM_MGMT_CMD_DICT_LOG_STATUS, /**< set or query the status of dictionary log  @see struct mpu_log_status_info */
	COMM_MGMT_CMD_MSIX_INFO, /**< read msix map table @see struct comm_cmd_msix_info */
	COMM_MGMT_CMD_CHANNEL_DETECT, /**< auto channel detect @see struct comm_cmd_channel_detect */
	COMM_MGMT_CMD_DICT_COUNTER_STATUS, /**< get flash counter status @see struct flash_counter_info */
	COMM_MGMT_CMD_UCODE_SM_COUNTER, /**< get ucode sm counter @see struct comm_read_ucode_sm_req
									  * @see struct comm_read_ucode_sm_resp */
	COMM_MGMT_CMD_CLEAR_LOG, /**< clear log @see struct comm_cmd_clear_log_s */

	COMM_MGMT_CMD_CHECK_IF_SWITCH_WORKMODE = 140, /**< check if switch workmode reserved
													* @see struct comm_cmd_check_if_switch_workmode */
	COMM_MGMT_CMD_SWITCH_WORKMODE, /**< switch workmode reserved @see struct comm_cmd_switch_workmode */

	COMM_MGMT_CMD_MIGRATE_DFX_HPA = 150, /**< query migrate varialbe @see struct comm_cmd_migrate_dfx_s */
	COMM_MGMT_CMD_BDF_INFO, /**< get bdf info @see struct cmd_get_bdf_info_s */
	COMM_MGMT_CMD_NCSI_CFG_INFO_GET_PROC, /**< get ncsi config info @see struct comm_cmd_ncsi_cfg_s */
	COMM_MGMT_CMD_CPI_TCAM_DBG, /**< enable or disable the scheduled cpi tcam task, set task interval time
								  * @see struct comm_cmd_cpi_tcam_dbg_s */

	COMM_MGMT_CMD_SECTION_RSVD_0 = 160, /**< rsvd0 section */
	COMM_MGMT_CMD_SECTION_RSVD_1 = 170, /**< rsvd1 section */
	COMM_MGMT_CMD_SECTION_RSVD_2 = 180, /**< rsvd2 section */
	COMM_MGMT_CMD_SECTION_RSVD_3 = 190, /**< rsvd3 section */

	COMM_MGMT_CMD_GET_TDIE_ID = 199, /**< get totem die id @see struct comm_cmd_get_totem_die_id */
	COMM_MGMT_CMD_GET_UDIE_ID = 200, /**< get unicorn die id @see struct comm_cmd_get_die_id */
	COMM_MGMT_CMD_GET_EFUSE_TEST, /**< reserved */
	COMM_MGMT_CMD_EFUSE_INFO_CFG, /**< set efuse config @see struct comm_efuse_cfg_info */
	COMM_MGMT_CMD_GPIO_CTL, /**< reserved */
	COMM_MGMT_CMD_HI30_SERLOOP_START,	/**< set serloop start reserved @see struct comm_cmd_hi30_serloop */
	COMM_MGMT_CMD_HI30_SERLOOP_STOP,	 /**< set serloop stop reserved @see struct comm_cmd_hi30_serloop */
	COMM_MGMT_CMD_HI30_MBIST_SET_FLAG,   /**< reserved */
	COMM_MGMT_CMD_HI30_MBIST_GET_RESULT, /**< reserved */
	COMM_MGMT_CMD_ECC_TEST, /**< reserved */
	COMM_MGMT_CMD_FUNC_BIST_TEST, /**< reserved */

	COMM_MGMT_CMD_VPD_SET = 210, /**< reserved */
	COMM_MGMT_CMD_VPD_GET, /**< reserved */

	COMM_MGMT_CMD_ERASE_FLASH, /**< erase flash sector @see struct cmd_sector_info */
	COMM_MGMT_CMD_QUERY_FW_INFO, /**< get firmware info
								   * @see struct cmd_query_fw */
	COMM_MGMT_CMD_GET_CFG_INFO, /**< get config info in flash reserved @see struct comm_cmd_get_cfg_info_t */
	COMM_MGMT_CMD_GET_UART_LOG, /**< collect hinicshell log  @see struct nic_cmd_get_uart_log_info */
	COMM_MGMT_CMD_SET_UART_CMD, /**< hinicshell send command to mpu @see struct nic_cmd_set_uart_log_cmd */
	COMM_MGMT_CMD_SPI_TEST, /**< reserved */

	/* TODO: ALL reg read/write merge to COMM_MGMT_CMD_CFG_REG */
	COMM_MGMT_CMD_MPU_REG_GET, /**< get mpu register value @see struct dbgtool_up_reg_opt_info */
	COMM_MGMT_CMD_MPU_REG_SET, /**< set mpu register value @see struct dbgtool_up_reg_opt_info */

	COMM_MGMT_CMD_REG_READ = 220, /**< read register value @see struct comm_info_reg_read_write */
	COMM_MGMT_CMD_REG_WRITE, /**< write register value @see struct comm_info_reg_read_write */
	COMM_MGMT_CMD_MAG_REG_WRITE, /**< write mag register value @see struct comm_info_dfx_mag_reg */
	COMM_MGMT_CMD_ANLT_REG_WRITE, /**< read register value @see struct comm_info_dfx_anlt_reg */

	COMM_MGMT_CMD_HEART_EVENT, /**< ncsi heart event @see struct comm_cmd_heart_event */
	COMM_MGMT_CMD_NCSI_OEM_GET_DRV_INFO, /**< nsci oem get driver info */
	COMM_MGMT_CMD_LASTWORD_GET, /**< report lastword to driver @see comm_info_up_lastword_s */
	COMM_MGMT_CMD_READ_BIN_DATA, /**< reserved */
	COMM_MGMT_CMD_GET_REG_VAL, /**< read register value @see struct comm_cmd_mbox_csr_rd_req */
	COMM_MGMT_CMD_SET_REG_VAL, /**< write register value @see struct comm_cmd_mbox_csr_wt_req */

	/* TODO: check if needed */
	COMM_MGMT_CMD_SET_VIRTIO_DEV = 230,  /**< set the virtio device
										   * @see struct comm_cmd_set_virtio_dev */
	COMM_MGMT_CMD_SET_MAC, /**< set mac address @see struct comm_info_mac */
	/* MPU patch cmd */
	COMM_MGMT_CMD_LOAD_PATCH, /**< load hot patch @see struct cmd_update_fw */
	COMM_MGMT_CMD_REMOVE_PATCH, /**< remove hot patch @see struct cmd_patch_remove */
	COMM_MGMT_CMD_PATCH_ACTIVE, /**< actice hot patch @see struct cmd_patch_active */
	COMM_MGMT_CMD_PATCH_DEACTIVE, /**< deactice hot patch @see struct cmd_patch_deactive */
	COMM_MGMT_CMD_PATCH_SRAM_OPTIMIZE, /**< set hot patch sram optimize */
	/* container host process */
	COMM_MGMT_CMD_CONTAINER_HOST_PROC, /**< container host process reserved
										 * @see struct comm_cmd_con_sel_sta */
	/* nsci counter */
	COMM_MGMT_CMD_NCSI_COUNTER_PROC, /**< get ncsi counter @see struct nsci_counter_in_info_s */
	COMM_MGMT_CMD_CHANNEL_STATUS_CHECK, /**< check channel status reserved
										  * @see struct channel_status_check_info_s */

	COMM_MGMT_CMD_RSVD_0 = 240, /**< hot patch reserved cmd */
	COMM_MGMT_CMD_RSVD_1, /**< hot patch reserved cmd */
	COMM_MGMT_CMD_RSVD_2, /**< hot patch reserved cmd */
	COMM_MGMT_CMD_RSVD_3, /**< hot patch reserved cmd */
	COMM_MGMT_CMD_RSVD_4, /**< hot patch reserved cmd */
	/* 无效字段，版本收编删除，编译使用 */
	COMM_MGMT_CMD_SEND_API_ACK_BY_UP, /**< reserved */

	/* for tool ver compatible info 任何版本都不要修改此值 */
	COMM_MGMT_CMD_GET_VER_COMPATIBLE_INFO = 254, /**< get compatible info
												   * @see struct comm_cmd_compatible_info */

	/* 注：添加cmd，不能修改已有命令字的值，请在前方rsvd
	 * section中添加；原则上所有分支cmd表完全一致
	 */
	COMM_MGMT_CMD_MAX = 255,
};

#endif