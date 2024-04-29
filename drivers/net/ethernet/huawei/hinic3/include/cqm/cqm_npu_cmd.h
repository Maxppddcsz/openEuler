/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2023. All rights reserved.
 * Description: cqm common command interface define.
 * Author: None
 * Create: 2015/11/13
 */
#ifndef CQM_NPU_CMD_H
#define CQM_NPU_CMD_H

typedef enum {
	CQM_CMD_T_INVALID = 0, /**< Invalid command */
	CQM_CMD_T_BAT_UPDATE,  /**< Update the bat configration of the funciton, @see struct tag_cqm_cmdq_bat_update */
	CQM_CMD_T_CLA_UPDATE,  /**< Update the cla configration of the funciton, @see struct tag_cqm_cla_update_cmd */
	CQM_CMD_T_BLOOMFILTER_SET,   /**< Set the bloomfilter configration of the funciton,
									  @see struct tag_cqm_bloomfilter_cmd */
	CQM_CMD_T_BLOOMFILTER_CLEAR, /**< Clear the bloomfilter configration of the funciton,
									  @see struct tag_cqm_bloomfilter_cmd */
	CQM_CMD_T_RSVD,		/**< Unused */
	CQM_CMD_T_CLA_CACHE_INVALID, /**< Invalidate the cla cacheline,  @see struct tag_cqm_cla_cache_invalid_cmd */
	CQM_CMD_T_BLOOMFILTER_INIT,  /**< Init the bloomfilter configration of the funciton,
									  @see struct tag_cqm_bloomfilter_init_cmd */
	CQM_CMD_T_MAX
} cqm_cmd_type_e;

#endif /* CQM_NPU_CMD_H */
