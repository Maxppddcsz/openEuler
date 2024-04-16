/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Filename	  : mpu_outband_ncsi_cmd_defs.h
 * Version	   : Initial Draft
 * Creation time : 2023/08/26
 * Last Modified :
 * Description   : NCSI protocol out-of-band command related structure
 */
#ifndef MPU_OUTBAND_NCSI_CMD_DEFS_H
#define MPU_OUTBAND_NCSI_CMD_DEFS_H

#pragma pack(1)

typedef enum {
	COMMAND_COMPLETED = 0x00,	   /**< command completed */
	COMMAND_FAILED = 0x01,		  /**< command failed */
	COMMAND_UNAVAILABLE = 0x02,	 /**< command unavailable */
	COMMAND_UNSPORRTED = 0x03	   /**< command unsporrted */
} NCSI_RESPONSE_CODE_E;

typedef enum {
	NO_ERROR = 0x00,				/**< no error */
	INTERFACE_INIT_REQUIRED = 0x01, /**< interface init required */
	INVALID_PARA = 0x02,			/**< invalid parameter */
	CHAN_NOT_READY = 0x03,		  /**< channel not ready */
	PKG_NOT_READY = 0x04,		   /**< package not ready */
	INVALID_PAYLOAD_LEN = 0x05,	 /**< invalid payload len */
	LINK_STATUS_ERROR = 0xA06,	  /**< get link status fail */
	VLAN_TAG_INVALID = 0xB07,		/**< vlan tag invalid */
	MAC_ADD_IS_ZERO = 0xE08,		 /**< mac add is zero */
	FLOW_CONTROL_UNSUPPORTED = 0x09, /**< flow control unsupported */
	CHECKSUM_ERR = 0xA,			  /**< check sum error */
	UNSUPPORTED_COMMAND_TYPE = 0x7FFF	/**< the command type is unsupported only when the response code is 0x03 */
} NCSI_REASON_CODE_E;

typedef enum {
	NCSI_RMII_TYPE = 1,			 /**< rmii client */
	NCSI_MCTP_TYPE = 2,			 /**< MCTP client */
	NCSI_AEN_TYPE = 3			   /**< AEN client */
} NCSI_CLIENT_TYPE_E;

/**
 * @brief ncsi ctrl packet header
 *
 */
typedef struct tag_ncsi_ctrl_packet_header {
	u8 mc_id;			  /**< management control ID */
	u8 head_revision;	  /**< head revision */
	u8 reserved0;		  /**< reserved */
	u8 iid;				/**< instance ID */
	u8 pkt_type;		   /**< packet type */
#ifdef NCSI_BIG_ENDIAN
	u8 pkg_id : 3;		 /**< packet ID */
	u8 inter_chan_id : 5;  /**< channel ID */
#else
	u8 inter_chan_id : 5;  /**< channel ID */
	u8 pkg_id : 3;		 /**< packet ID */
#endif
#ifdef BD_BIG_ENDIAN
	u8 reserved1 : 4;	  /**< reserved1 */
	u8 payload_len_hi : 4; /**< payload len have 12bits */
#else
	u8 payload_len_hi : 4; /**< payload len have 12bits */
	u8 reserved1 : 4;	  /**< reserved1 */
#endif
	u8 payload_len_lo;	 /**< payload len lo */
	u32 reserved2;		 /**< reserved2 */
	u32 reserved3;		 /**< reserved3 */
} ncsi_ctrl_pkt_header_s;

#define NCSI_MAX_PAYLOAD_LEN 1500
#define NCSI_MAC_LEN 6

/**
 * @brief ncsi clear initial state command struct defination
 *
 */
typedef struct tag_ncsi_ctrl_packet {
	ncsi_ctrl_pkt_header_s packet_head; /**< ncsi ctrl packet header */
	u8 payload[NCSI_MAX_PAYLOAD_LEN];   /**< ncsi ctrl packet payload */
} ncsi_ctrl_packet_s;

/**
 * @brief ethernet header description
 *
 */
typedef struct tag_ethernet_header {
	u8 dst_addr[NCSI_MAC_LEN];	   /**< ethernet destination address */
	u8 src_addr[NCSI_MAC_LEN];	   /**< ethernet source address */
	u16 ether_type;				  /**< ethernet type */
} ethernet_header_s;

/**
 * @brief ncsi common packet description
 *
 */
typedef struct tg_ncsi_common_packet {
	ethernet_header_s frame_head;	 /**< common packet ethernet frame header */
	ncsi_ctrl_packet_s ctrl_packet;   /**< common packet ncsi ctrl packet */
} ncsi_common_packet_s, *p_ncsi_common_packet_s;

/**
 * @brief ncsi clear initial state command struct defination
 *
 */
typedef struct tag_ncsi_client_info {
	u8 *name;				  /**< client info client name */
	u32 type;				  /**< client info type of ncsi media  @see enum NCSI_CLIENT_TYPE_E */
	u8 bmc_mac[NCSI_MAC_LEN];  /**< client info BMC mac addr */
	u8 ncsi_mac[NCSI_MAC_LEN]; /**< client info local mac addr */
	u8 reserve[2];			 /**< client info reserved, Four-byte alignment */
	u32 rsp_len;			   /**< client info include pad */
	ncsi_common_packet_s ncsi_packet_rsp; /**< ncsi common packet response */
} ncsi_client_info_s;

/* Clear Initial State Command (0x00) */
/* Payload length does not include the length of the NC-SI header, the
checksum value, or any padding that might be present. */
#define CLEAR_INITIAL_REQ_LEN 0
#define CLEAR_INITIAL_RSP_LEN 4
/**
 * @brief ncsi clear initial state command (0x00) struct defination
 * @see NCSI_CLEAR_INITIAL_STATE
 *
 */
typedef struct tg_clear_initial_state {
	u32 check_sum;			 /**< clear initial state check sum */
} clear_initial_state_s, *p_clear_initial_state_s;

/* Clear Initial State Response (0x80)  */
/**
 * @brief ncsi clear initial state response (0x80) struct defination
 * @see NCSI_CLEAR_INITIAL_STATE_RSP
 *
 */
typedef struct tg_clear_initial_state_rsp {
	u16 rsp_code;			 /**< clear initial state response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;		  /**< clear initial state response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;			/**< clear initial state response check sum */
} clear_initial_state_rsp_s, *p_clear_initial_state_rsp_s;

/* Select Package Command (0x01) */
#define SELECT_PACKAGE_REQ_LEN 4
#define SELECT_PACKAGE_RSP_LEN 4
/**
 * @brief ncsi select packag command (0x01) struct defination
 * @see NCSI_SELECT_PACKAGE
 *
 */
typedef struct tg_select_package {
	u8 reserved1[3];		  /**< select package reserved1 */
#ifdef BIG_ENDIAN
	u8 reserved2 : 7;		 /**< select package reserved2 */
	u8 hd_arbitration : 1;	/**< select package hd arbitration */
#else
	u8 hd_arbitration : 1;	/**< select package hd arbitration */
	u8 reserved2 : 7;		 /**< select package reserved2 */
#endif
	u32 check_sum;			/**< select package check sum */
} select_package_s, *p_select_package_s;

/* Select Package Response (0x81) */
/**
 * @brief ncsi select packag response (0x81) struct defination
 * @see NCSI_SELECT_PACKAGE_RSP
 *
 */
typedef struct tg_select_package_rsp {
	u16 rsp_code;			 /**< select package response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;		  /**< select package response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;			/**< select package response check sum */
} select_package_rsp_s, *p_select_package_rsp_s;

/* Deselect Package Command (0x02)  */
#define DESELECT_PACKAGE_REQ_LEN 0
#define DESELECT_PACKAGE_RSP_LEN 4
/**
 * @brief ncsi deselect packag command (0x02) struct defination
 * @see NCSI_DESELECT_PACKAGE
 *
 */
typedef struct tg_deselect_package {
	u32 check_sum;			/**< deselect package check sum */
} deselect_package_s, *p_deselect_package_s;

/* Deselect Package Response (0x82)  */
/**
 * @brief ncsi deselect packag response (0x82) struct defination
 * @see NCSI_DESELECT_PACKAGE_RSP
 *
 */
typedef struct tg_deselect_package_rsp {
	u16 reason_code;	 /**< deselect package response reason code */
	u16 rsp_code;		/**< deselect package response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u32 check_sum;	   /**< deselect package response check sum @see enum NCSI_REASON_CODE_E */
} deselect_package_rsp_s, *p_deselect_package_rsp_s;

/* Enable Channel Command (0x03)  */
#define ENABLE_CHANNEL_REQ_LEN 0
#define ENABLE_CHANNEL_RSP_LEN 4
/**
 * @brief ncsi enable channel command (0x03) struct defination
 * @see NCSI_ENABLE_CHANNEL
 *
 */
typedef struct tg_enable_channel {
	u32 check_sum;	   /**< enable channel response check sum */
} enable_channel_s, *p_enable_channel_s;

/* Enable Channel Response (0x83) */
/**
 * @brief ncsi enable channel response (0x83) struct defination
 * @see NCSI_ENABLE_CHANNEL_RSP
 *
 */
typedef struct tg_enable_channel_rsp {
	u16 rsp_code;	   /**< enable channel response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;	/**< enable channel response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;	  /**< enable channel response check sum */
} enable_channel_rsp_s, *p_enable_channel_rsp_s;

/* Disable Channel Command (0x04)  */
#define DISABLE_CHANNEL_REQ_LEN 4
#define DISABLE_CHANNEL_RSP_LEN 4
/**
 * @brief ncsi disable channel command (0x04) struct defination
 * @see NCSI_DISABLE_CHANNEL
 *
 */
typedef struct tg_disable_channel {
	u8 reserved1[3];		/**< disable channel command reserved1 */
#ifdef BIG_ENDIAN
	u8 rsvd : 7;			/**< disable channel command reserved */
	u8 ald : 1;			 /**< disable channel command ald */
#else
	u8 ald : 1;			 /**< disable channel command ald */
	u8 rsvd : 7;			/**< disable channel command reserved */
#endif
	u32 check_sum;		  /**< disable channel command check sum */
} disable_channel_s, *p_disable_channel_s;
/* Disable Channel Response (0x84) */
/**
 * @brief ncsi disable channel response (0x84) struct defination
 * @see NCSI_DISABLE_CHANNEL_RSP
 *
 */
typedef struct tg_disable_channel_rsp {
	u16 rsp_code;		/**< disable channel response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;	 /**< disable channel response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;	   /**< disable channel response check sum */
} disable_channel_rsp_s, *p_disable_channel_rsp_s;

/* Reset Channel Command (0x05) */
#define RESET_CHANNEL_REQ_LEN 4
#define RESET_CHANNEL_RSP_LEN 4
/**
 * @brief ncsi reset channel command (0x05) struct defination
 * @see NCSI_RESET_CHANNEL
 *
 */
typedef struct tg_reset_channel {
	u32 rsvd;			  /**< reset channel command reserved */
	u32 check_sum;		 /**< reset channel command check sum */
} reset_channel_s, *p_reset_channel_s;

/* Reset Channel Response (0x85) */
/**
 * @brief ncsi reset channel response (0x85) struct defination
 * @see NCSI_RESET_CHANNEL_RSP
 *
 */
typedef struct tg_reset_channel_rsp {
	u16 rsp_code;		  /**< reset channel response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;	   /**< reset channel response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;		 /**< reset channel response check sum */
} reset_channel_rsp_s, *p_reset_channel_rsp_s;

/* Enable Channel Network TX Command (0x06) */
#define ENABLE_CHN_TX_REQ_LEN 0
#define ENABLE_CHN_TX_RSP_LEN 4
/**
 * @brief ncsi enable channel network TX command (0x06) struct defination
 * @see NCSI_ENABLE_CHANNEL_NETWORK_TX
 *
 */
typedef struct tg_enable_chn_tx {
	u32 check_sum;		 /**< enable channel network TX command check sum */
} enable_chn_tx_s, *p_enable_chn_tx_s;
/*  Enable Channel Network TX Response (0x86) ) */
/**
 * @brief ncsi enable channel network TX response (0x86) struct defination
 * @see NCSI_ENABLE_CHANNEL_NETWORK_TX_RSP
 *
 */
typedef struct tg_enable_chn_tx_rsp {
	u16 reason_code;	   /**< enable channel network TX response reason code @see enum NCSI_REASON_CODE_E */
	u16 rsp_code;		  /**< enable channel network TX response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u32 check_sum;		 /**< enable channel network TX response check sum */
} enable_chn_tx_rsp_s, *p_enable_chn_tx_rsp_s;

/* Disable Channel Network TX Command (0x07) */
#define DISABLE_CHN_TX_REQ_LEN 0
#define DISABLE_CHN_TX_RSP_LEN 4
/**
 * @brief ncsi disable channel network TX command (0x07) struct defination
 * @see NCSI_DISABLE_CHANNEL_NETWORK_TX
 *
 */
typedef struct tg_disable_chn_tx {
	u32 check_sum;		   /**< disable channel network TX command check sum */
} disable_chn_tx_s, *p_disable_chn_tx_s;
/*  Disable Channel Network TX Response (0x87) ) */
/**
 * @brief ncsi disable channel network TX response (0x87) struct defination
 * @see NCSI_DISABLE_CHANNEL_NETWORK_TX_RSP
 *
 */
typedef struct tg_disable_chn_tx_rsp {
	u16 rsp_code;			/**< disable channel network TX response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;		 /**< disable channel network TX response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;		   /**< disable channel network TX response check sum */
} disable_chn_tx_rsp_s, *p_disable_chn_tx_rsp_s;

/* AEN Enable Command (0x08)  */
#define AEN_ENABLE_REQ_LEN 8
#define AEN_ENABLE_RSP_LEN 4
#define AEN_CTRL_LINK_STATUS_SHIFT 0
#define AEN_CTRL_CONFIG_REQ_SHIFT 1
#define AEN_CTRL_DRV_CHANGE_SHIFT 2

typedef union tg_aen_control {
	struct {
		u16 oem_ctl;		/**< AEN control oem control */
		u8 reserved;		/**< AEN control reserved */
#ifdef BD_BIG_ENDIAN
		u8 reserved2 : 5;   /**< AEN control reserved2 */
		u8 drv_change : 1;  /**< AEN control driver change
							 * 1b  = Enable Host NC Driver Status Change AEN 0=disable */
		u8 config_req : 1;  /**< AEN control config_req */
		u8 link_status : 1; /**< AEN control driver change
							 * 1b  = Enable Link Status Change AEN  0=disable */
#else
		u8 link_status : 1; /**< AEN control driver change
							 * 1b  = Enable Link Status Change AEN  0=disable */
		u8 config_req : 1;  /**< AEN control config_req */
		u8 drv_change : 1;  /**< AEN control driver change
							 * 1b  = Enable Host NC Driver Status Change AEN 0=disable */
		u8 reserved2 : 5;   /**< AEN control reserved2 */
#endif
	} bits;

	u32 aen_ctrl;	   /**< AEN control check sum */
} aen_control_s;
/**
 * @brief ncsi AEN enable command (0x08) struct defination
 * @see NCSI_AEN_ENABLE
 *
 */
typedef struct tg_enable_aen {
	u8 reserved1[3];			/**< AEN enable command reserved2 */
	u8 mc_id;				   /**< AEN enable command management control ID */
	aen_control_s aen_control;  /**< AEN control */
	u32 check_sum;			  /**< AEN enable command check sum */
} enable_aen_s, *p_enable_aen_s;
/* AEN Enable Response (0x88)  */
/**
 * @brief ncsi AEN enable response (0x88) struct defination
 * @see NCSI_AEN_ENABLE_RSP
 *
 */
typedef struct tg_enable_aen_rsp {
	u16 rsp_code;	  /**< AEN enable response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;   /**< AEN enable response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;	 /**< AEN enable response check sum */
} enable_aen_rsp_s, *p_enable_aen_rsp_s;

/* set link: 0x09 */
#define SET_LINK_REQ_LEN 8
#define SET_LINK_RSP_LEN 4
/**
 * @brief ncsi set link command (0x09) struct defination
 * @see NCSI_SET_LINK
 *
 */
typedef struct tg_set_link {
	u32 link_settings;		/**< set link command link settings */
	u32 OEM_link_settings;	/**< set link command OEM link settings */
	u32 check_sum;			/**< set link command check sum */
} set_link_s, *p_set_link_s;
/* set link response (0x89) */
/**
 * @brief ncsi set link response (0x89) struct defination
 * @see NCSI_SET_LINK_RSP
 *
 */
typedef struct tg_set_link_rsp {
	u16 rsp_code;			/**< set link response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;		 /**< set link response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;		   /**< set link response check sum */
} set_link_rsp_s, *p_set_link_rsp_s;

/* get link status 0x0A */
#define GET_LINK_STATUS_REQ_LEN 0
#define GET_LINK_STATUS_RSP_LEN 16
/* link speed(fc link speed is mapped to unknown) */
typedef enum {
	LINK_SPEED_10M = 0x2,	/**< 10M */
	LINK_SPEED_100M = 0x5,   /**< 100M */
	LINK_SPEED_1G = 0x7,	 /**< 1G */
	LINK_SPEED_10G = 0x8,	/**< 10G */
	LINK_SPEED_20G = 0x9,	/**< 20G */
	LINK_SPEED_25G = 0xa,	/**< 25G */
	LINK_SPEED_40G = 0xb,	/**< 40G */
	LINK_SPEED_50G = 0xc,	/**< 50G */
	LINK_SPEED_100G = 0xd,   /**< 100G */
	LINK_SPEED_2_5G = 0xe,   /**< 2.5G */
	LINK_SPEED_UNKNOWN = 0xf
} NCSI_CMD_LINK_SPEED_E;
/**
 * @brief ncsi get link status command (0x0A) struct defination
 * @see NCSI_GET_LINK_STATUS
 *
 */
typedef struct tg_get_link_status {
	u32 check_sum;		  /**< get link status command check sum */
} get_link_status_s, *p_get_link_status_s;

/* get link status response 0x8A */
/**
 * @brief link status struct defination
 *
 */
typedef union {
	struct {
#ifdef BD_BIG_ENDIAN
		u32 reserved3 : 8;		  /**< link status reserved3 */

		u32 reserved2 : 2;		  /**< link status reserved2 */
		u32 oem_link_speed : 1;	 /**< link status oem link speed @see enum NCSI_CMD_LINK_SPEED_E */
		u32 serdes_link : 1;		/**< link status serdes link */
		u32 link_partner8 : 2;	  /**< link status link partner8 */
		u32 rx_flow_control : 1;	/**< link status rx flow control */
		u32 tx_flow_control : 1;	/**< link status tx flow control */

		u32 link_partner7 : 1;	  /**< link status link partner7 */
		u32 link_partner6 : 1;	  /**< link status link partner6 */
		u32 link_partner5 : 1;	  /**< link status link partner5 */
		u32 link_partner4 : 1;	  /**< link status link partner4 */
		u32 link_partner3 : 1;	  /**< link status link partner3 */
		u32 link_partner2 : 1;	  /**< link status link partner2 */
		u32 link_partner1 : 1;	  /**< link status link partner1 */
		u32 channel_available : 1;  /**< link status channel available */

		u32 parallel_detection : 1; /**< link status parallel detection */
		u32 negotiate_complete : 1; /**< link status negotiate complete */
		u32 negotiate_flag : 1;	 /**< link status negotiate flag */
		u32 speed_duplex : 4;	   /**< link status speed duplex */
		u32 link_flag : 1;		  /**< link status link flag */
#else
		u32 reserved3 : 8;		  /**< link status reserved3 */

		u32 tx_flow_control : 1;	/**< link status tx flow control */
		u32 rx_flow_control : 1;	/**< link status rx flow control */
		u32 link_partner8 : 2;	  /**< link status link partner8 */
		u32 serdes_link : 1;		/**< link status serdes link */
		u32 oem_link_speed : 1;	 /**< link status oem link speed @see enum NCSI_CMD_LINK_SPEED_E */
		u32 reserved2 : 2;		  /**< link status reserved2 */

		u32 channel_available : 1;  /**< link status channel available */
		u32 link_partner1 : 1;	  /**< link status link partner1 */
		u32 link_partner2 : 1;	  /**< link status link partner2 */
		u32 link_partner3 : 1;	  /**< link status link partner3 */
		u32 link_partner4 : 1;	  /**< link status link partner4 */
		u32 link_partner5 : 1;	  /**< link status link partner5 */
		u32 link_partner6 : 1;	  /**< link status link partner6 */
		u32 link_partner7 : 1;	  /**< link status link partner7 */

		u32 link_flag : 1;		  /**< link status link flag */
		u32 speed_duplex : 4;	   /**< link status speed duplex */
		u32 negotiate_flag : 1;	 /**< link status negotiate flag */
		u32 negotiate_complete : 1; /**< link status negotiate complete */
		u32 parallel_detection : 1; /**< link status parallel detection */
#endif
	} bits;
	u32 val32;
} ncsi_link_status;

/**
 * @brief ncsi get link status response (0x8A) struct defination
 * @see NCSI_GET_LINK_STATUS_RSP
 *
 */
typedef struct tg_get_link_status_rsp {
	u16 rsp_code;		  /**< get link status response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;	   /**< get link status response reason code @see enum NCSI_REASON_CODE_E */
	u32 link_status;	   /**< get link status response link status */
	u32 other_indications; /**< get link status response other indications */
	u32 OEM_link_status;   /**< get link status response OEM link status */
	u32 check_sum;		 /**< get link status response check sum */
} get_link_status_rsp_s, *p_get_link_status_rsp_s;

/* Set Vlan Filter (0x0B)  */
/* Only VLAN-tagged packets that match the enabled VLAN Filter settings are accepted. */
#define VLAN_MODE_UNSET 0X00
#define VLAN_ONLY 0x01
/* if match the MAC address ,any vlan-tagged and non-vlan-tagged will be
  accepted */
#define ANYVLAN_NONVLAN 0x03
#define VLAN_MODE_SUPPORT 0x05

/* chanel vlan filter enable */
#define CHNL_VALN_FL_ENABLE 0x01
#define CHNL_VALN_FL_DISABLE 0x00

/* vlan id invalid */
#define VLAN_ID_VALID 0x01
#define VLAN_ID_INVALID 0x00

/* VLAN ID */
#define SET_VLAN_FILTER_REQ_LEN 8
#define SET_VLAN_FILTER_RSP_LEN 4

/* ncsi_get_controller_packet_statistics_config */
#define NO_INFORMATION_STATISTICS 0xff

/**
 * @brief ncsi set vlan filter command (0x0B) struct defination
 * @see NCSI_SET_VLAN_FILTER
 *
 */
typedef struct tg_set_vlan_filter {
	u8 reserved1[2];		   /**< set vlan filter command reserved1 */
#ifdef BD_BIG_ENDIAN
	u8 user_priority : 4;	  /**< set vlan filter command user priority */
	u8 vlan_id_hi : 4;		 /**< set vlan filter command vlan id high */
#else
	u8 vlan_id_hi : 4;		 /**< set vlan filter command vlan id high */
	u8 user_priority : 4;	  /**< set vlan filter command user priority */
#endif
	u8 vlan_id_low;			/**< set vlan filter command vlan id low */
	u8 reserved2[2];		   /**< set vlan filter command reserved2 */
	u8 filter;				 /**< set vlan filter command filter */
#ifdef BD_BIG_ENDIAN
	u8 reserved3 : 7;		  /**< set vlan filter command reserved3 */
	u8 enable : 1;			 /**< set vlan filter command enable */
#else
	u8 enable : 1;			 /**< set vlan filter command enable */
	u8 reserved3 : 7;		  /**< set vlan filter command reserved3 */
#endif
	u32 check_sum;			 /**< set vlan filter command check sum */
} set_vlan_filter_s, *p_set_vlan_filter_s;
/* Set Vlan Filter Response (0x8B)  */
/**
 * @brief ncsi set vlan filter response (0x8B) struct defination
 * @see NCSI_SET_VLAN_FILTER_RSP
 *
 */
typedef struct tg_set_vlan_filter_rsp {
	u16 rsp_code;			 /**< set vlan filter response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;		  /**< set vlan filter response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;			/**< set vlan filter response check sum */
} set_vlan_filter_rsp_s, *p_set_vlan_filter_rsp_s;

/* Enable VLAN Command (0x0C)   */
#define ENABLE_VLAN_REQ_LEN 4
#define ENABLE_VLAN_RSP_LEN 4
#define VLAN_FL_MAX_ID 8

/* channel对应的某个vlan filter的寄存器 */
#define VLAN_REG_ADDR(chan_id, filter_selector) \
	(CSR_IPSURX_CSR_IPSURX_NCSI_VLAN7_CTRL_0_REG + (chan_id) * 4 + (VLAN_FL_MAX_ID - (filter_selector)) * 16)

/* 寻找IPSU的VLAN过滤控制寄存器的第chan_id个channel对应的Bit位 */
#define IPSURX_NCSI_RULE_VLAN_EN_BIT(chan_id) ((chan_id) * 1)
/**
 * @brief ncsi enable vlan command (0x0C) struct defination
 * @see NCSI_ENABLE_VLAN
 *
 */
typedef struct tg_enable_vlan {
	u8 reserved[3];		/**< enable vlan command reserved */
	u8 vlan_mode;		  /**< enable vlan command vlan mode */
	u32 check_sum;		 /**< enable vlan command check sum */
} enable_vlan_s, *p_enable_vlan_s;
/* Enable VLAN Response (0x8C)   */
/**
 * @brief ncsi enable vlan response (0x8C) struct defination
 * @see NCSI_ENABLE_VLAN_RSP
 *
 */
typedef struct tg_enable_vlan_rsp {
	u16 rsp_code;		 /**< enable vlan response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;	  /**< enable vlan response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;		/**< enable vlan response check sum */
} enable_vlan_rsp_s, *p_enable_vlan_rsp_s;

/* Disable VLAN Command (0x0D)   */
#define DISABLE_VLAN_REQ_LEN 0
#define DISABLE_VLAN_RSP_LEN 4
/**
 * @brief ncsi disable VLAN command (0x0D) struct defination
 * @see NCSI_DISABLE_VLAN
 *
 */
typedef struct tg_disable_vlan {
	u32 check_sum;	   /**< disable vlan command check sum */
} disable_vlan_s, *p_disable_vlan_s;
/* Disable VLAN Response (0x8D) */
/**
 * @brief ncsi disable VLAN response (0x8D) struct defination
 * @see NCSI_DISABLE_VLAN_RSP
 *
 */
typedef struct tg_disable_vlan_rsp {
	u16 rsp_code;		/**< disable vlan response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;	 /**< disable vlan response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;	   /**< disable vlan response check sum */
} disable_vlan_rsp_s, *p_disable_vlan_rsp_s;

/* get MAC Address 0x0E */
#define SET_MAC_ADDRESS_REQ_LEN 8
#define SET_MAC_ADDRESS_RSP_LEN 4

#define UNICAST_ADDRESS_TYPE		  (0x0)
#define MULTICAST_ADDRESS_TYPE		(0x1)

/**
 * @brief ncsi get MAC Address command (0x0E) struct defination
 * @see NCSI_SET_MAC_ADDRESS
 *
 */
typedef struct tg_set_mac_address {
	u8 mac_filter5;	  /**< set MAC Address command mac filter5 */
	u8 mac_filter4;	  /**< set MAC Address command mac filter4 */
	u8 mac_filter3;	  /**< set MAC Address command mac filter3 */
	u8 mac_filter2;	  /**< set MAC Address command mac filter2 */
	u8 mac_filter1;	  /**< set MAC Address command mac filter1 */
	u8 mac_filter0;	  /**< set MAC Address command mac filter0 */
	u8 mac_number;	   /**< set MAC Address command mac number */
#ifdef BD_BIG_ENDIAN
	u8 address_type : 3; /**< set MAC Address command address type */
	u8 reserved : 4;	 /**< set MAC Address command reserved */
	u8 mac_enable : 1;   /**< set MAC Address command mac enable */
#else
	u8 mac_enable : 1;   /**< set MAC Address command mac enable */
	u8 reserved : 4;	 /**< set MAC Address command reserved */
	u8 address_type : 3; /**< set MAC Address command address type */
#endif
	u32 check_sum;	   /**< set MAC Address command check sum */
} set_mac_address_s, *p_set_mac_address_s;
/* set MAC Address response (0x8E) */
/**
 * @brief ncsi get MAC Address response (0x8E) struct defination
 * @see NCSI_SET_MAC_ADDRESS_RSP
 *
 */
typedef struct tg_set_mac_address_rsp {
	u16 rsp_code;		/**< set MAC Address response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;	 /**< set MAC Address response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;	   /**< set MAC Address response check sum */
} set_mac_address_rsp_s, *p_set_mac_address_rsp_s;

/* Enable BC Filter Command (0x10) */
#define ENABLE_BROADFILTER_REQ_LEN 4
#define ENABLE_BROADFILTER_RSP_LEN 4

typedef union {
	struct {
#ifdef BD_BIG_ENDIAN
		u32 reserved1 : 28;		 /**< boradcast filter reserved1 */
		u32 netbios_packet : 1;	 /**< boradcast filter netbios packet, This field is optional */
		u32 dhcp_server_packet : 1; /**< boradcast filter dhcp server packet, This field is optional */
		u32 dhcp_client_packet : 1; /**< boradcast filter dhcp client packet, This field is optional */
		u32 arp_packet : 1;		 /**< boradcast filter arp packet, This field is mandatory */
#else
		u32 arp_packet : 1;		 /**< boradcast filter arp packet, This field is mandatory */
		u32 dhcp_client_packet : 1; /**< boradcast filter dhcp client packet, This field is optional */
		u32 dhcp_server_packet : 1; /**< boradcast filter dhcp server packet, This field is optional */
		u32 netbios_packet : 1;	 /**< boradcast filter netbios packet, This field is optional */
		u32 reserved1 : 28;		 /**< boradcast filter reserved1 */
#endif
	} bits;
	u32 val32;
} boradcast_filter, *p_boradcast_filter;

/**
 * @brief ncsi enable broadcast filter command (0x10) struct defination
 * @see NCSI_ENABLE_BROADCAST_FILTERING
 *
 */
typedef struct tg_enable_broadcast {
	boradcast_filter brd_filter; /**< enable broadcast filter command boradcast filter */
	u32 check_sum;			   /**< enable broadcast filter command check sum */
} enable_broadcast_s, *p_enable_broadcast_s;
/**
 * @brief enable broadcast filter response (0x90) struct defination
 * @see NCSI_ENABLE_BROADCAST_FILTERING_RSP
 *
 */
typedef struct tg_enable_broadcast_rsp {
	u16 rsp_code;				/**< enable broadcast filter response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;			 /**< enable broadcast filter response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;			   /**< enable broadcast filter response check sum */
} enable_broadcast_rsp_s, *p_enable_broadcast_rsp_s;

/* disbale broadcast filter command(0x11) */
#define DISABLE_BROADFILTER_REQ_LEN 0
#define DISABLE_BROADFILTER_RSP_LEN 4
/**
 * @brief ncsi disbale broadcast filter command(0x11) struct defination
 * @see NCSI_DISABLE_BROADCAST_FILTERING
 *
 */
typedef struct tg_disable_broadcast {
	u32 check_sum;	/**< disable broadcast filter response check sum */
} disable_broadcast_s, *p_disable_broadcast_s;
/**
 * @brief ncsi disbale broadcast filter response(0x91) struct defination
 * @see NCSI_DISABLE_BROADCAST_FILTERING_RSP
 *
 */
typedef struct tg_disable_broadcast_rsp {
	u16 rsp_code;	/**< disable broadcast filter response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code; /**< disable broadcast filter response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;   /**< disable broadcast filter response check sum */
} disable_broadcast_rsp_s, *p_disable_broadcast_rsp_s;

/* Enable Global Multicast Filter Command (0x12) */
#define ENABLE_MULTICAST_REQ_LEN 4
#define ENABLE_MULTICAST_RSP_LEN 4

typedef union {
	struct tg_multicast_packet_filter {
#ifdef BD_BIG_ENDIAN
		u8 reserved1[3];						  /**< multicast packet filter reserved1 */
		u8 reserved2 : 2;						 /**< multicast packet filter reserved2 */
		u8 IPv6_Neighbor_Solicitation : 1;			/**< multicast packet IPv6 Neighbor Solicitation */
		u8 IPv6_MLD : 1;							  /**< multicast packet IPv6 MLD */
		u8 DHCPv6_multicasts_from_server_to_clients : 1; /**< multicast packet DHCPv6 multicasts from server to clients
														   * listening on well-known UDP ports */
		u8 DHCPv6_relay_and_server_multicast : 1; /**< multicast packet DHCPv6 relay and server multicast */
		u8 IPv6_router_advertisement : 1;		 /**< multicast packet IPv6 router advertisement */
		u8 IPv6_neighbor_advertisement : 1;	   /**< multicast packet IPv6 neighbor advertisement */
#else
		u8 IPv6_neighbor_advertisement : 1;	   /**< multicast packet IPv6 neighbor advertisement */
		u8 IPv6_router_advertisement : 1;		 /**< multicast packet IPv6 router advertisement */
		u8 DHCPv6_relay_and_server_multicast : 1; /**< multicast packet DHCPv6 relay and server multicast */
		u8 DHCPv6_multicasts_from_server_to_clients : 1; /**< multicast packet DHCPv6 multicasts from server to clients
														   * listening on well-known UDP ports */
		u8 IPv6_MLD : 1;							  /**< multicast packet IPv6 MLD */
		u8 IPv6_Neighbor_Solicitation : 1;			/**< multicast packet IPv6 Neighbor Solicitation */
		u8 reserved2 : 2;						 /**< multicast packet filter reserved2 */
		u8 reserved1[3];						  /**< multicast packet filter reserved1 */
#endif
	} bits;
	u32 val32;
} multicast_packet_filter, *p_multicast_packet_filter;

/**
 * @brief ncsi enable global multicast filter command(0x12) struct defination
 * @see NCSI_ENABLE_GLOBAL_MULTICAST_FILTERING
 *
 */
typedef struct tg_enable_multicast {
	multicast_packet_filter multicast_filter; /**< enable global multicast filter command multicast packet filter */
	u32 check_sum;							/**< enable global multicast filter command check sum */
} enable_multicast_s, *p_enable_multicast_s;

/**
 * @brief ncsi enable global multicast filter response(0x92) struct defination
 * @see NCSI_ENABLE_GLOBAL_MULTICAST_FILTERING_RSP
 *
 */
typedef struct tg_enable_multicast_rsp {
	u16 rsp_code;	/**< enable global multicast filter response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code; /**< enable global multicast filter response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;   /**< enable global multicast filter response check sum */
} enable_multicast_rsp_s, *p_enable_multicast_rsp_s;

/* Disable Global Multicast Filter Command (0x13) */
#define DISABLE_MULTICAST_REQ_LEN 0
#define DISABLE_MULTICAST_RSP_LEN 4
/**
 * @brief ncsi disable global multicast filter command(0x13) struct defination
 * @see NCSI_ENABLE_GLOBAL_MULTICAST_FILTERING
 *
 */
typedef struct tg_disable_multicast {
	u32 check_sum;			  /**< disable global multicast filter command check sum */
} disable_multicast_s, *p_disable_multicast_s;
/**
 * @brief ncsi disable global multicast filter response(0x93) struct defination
 * @see NCSI_DISABLE_GLOBAL_MULTICAST_FILTERING_RSP
 *
 */
typedef struct tg_disable_multicast_rsp {
	u16 rsp_code;	/**< disable global multicast filter response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code; /**< disable global multicast filter response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;   /**< disable global multicast filter response check sum */
} disable_multicast_rsp_s, *p_disable_multicast_rsp_s;

/* ncsi flow control (0x14) */
#define FLOW_CONTROL_REQ_LEN 4
#define FLOW_CONTROL_RSP_LEN 4
/**
 * @brief ncsi set ncsi flow control command(0x14) struct defination
 * @see NCSI_SET_NCSI_FLOW_CONTROL
 *
 */
typedef struct tg_set_flow_control {
	u8 reserved[3];			/**< set flow control command reserved */
	u8 flow_control_enable;	/**< set flow control command flow control enable */
	u32 check_sum;			 /**< set flow control command check sum */
} set_flow_control_s, *p_set_flow_control_s;
/**
 * @brief ncsi set ncsi flow control response(0x94) struct defination
 * @see NCSI_SET_NCSI_FLOW_CONTROL_RSP
 *
 */
typedef struct tg_set_flow_control_rsp {
	u16 rsp_code;			  /**< set flow control response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;		   /**< set flow control response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;			 /**< set flow control response check sum */
} set_flow_control_rsp_s, *p_set_flow_control_rsp_s;

/* Get Version ID Command (0x15) */
#define GET_VERSION_ID_REQ_LEN 0
#define GET_VERSION_ID_RSP_LEN 40
/* fw_name的最大值 */
#define FW_NAME_MAX_SIZE (12)
/**
 * @brief ncsi get version id command(0x15) struct defination
 * @see NCSI_GET_VERSION_ID
 *
 */
typedef struct tg_get_version_id {
	u32 check_sum;			/**< get version id command check sum */
} get_version_id_s, *p_get_version_id_s;
/**
 * @brief ncsi get version id response(0x95) struct defination
 * @see NCSI_GET_VERSION_ID_RSP
 *
 */
typedef struct tg_get_version_id_rsp {
	u16 rsp_code;					 /**< get version id response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				  /**< get version id response reason code */
	u8 pl_major;					  /**< get version id response pl major */
	u8 pl_minor;					  /**< get version id response pl minor */
	u8 update;						/**< get version id response update */
	u8 alpha1;						/**< get version id response alpha1 */
	u8 reserved[3];				   /**< get version id response reserved */
	u8 alpha2;						/**< get version id response alpha2 */
	u8 name_string[FW_NAME_MAX_SIZE]; /**< get version id response name_string
										 * BIG_ENDIAN, The first byte received is the first byte of the string. */
	u8 ms_byte3;					  /**< get version id response ms byte3 */
	u8 byte2;						 /**< get version id response byte2 */
	u8 byte1;						 /**< get version id response byte1 */
	u8 ls_byte0;					  /**< get version id response ls byte0 */
	u16 pci_did;					  /**< get version id response pci did */
	u16 pci_vid;					  /**< get version id response pci vid */
	u16 pci_ssid;					 /**< get version id response pci ssid */
	u16 pci_svid;					 /**< get version id response pci svid */
	u32 manufacturer_id;			  /**< get version id response manufacturer id
										 * This field is unused, the value shall be set to 0xFFFFFFFF */
	u32 check_sum;					/**< get version id response check sum */
} get_version_id_rsp_s, *p_get_version_id_rsp_s;

/* get_capabilities: 0x16 */
#define GET_CAPABILITIES_REQ_LEN 0
#define GET_CAPABILITIES_RSP_LEN 32
/**
 * @brief ncsi get capabilities command(0x16) struct defination
 * @see NCSI_GET_CAPABILITIES
 *
 */
typedef struct tg_get_capabilities {
	u32 check_sum;					/**< get capabilities command check sum */
} get_capabilities_s, *p_get_capabilities_s;

/* NCSI channel capabilities */
typedef struct tag_ncsi_chan_capa {
	u32 capa_flags;					 /**< NCSI channel capabilities capa flags */
	u32 bcast_filter;				   /**< NCSI channel capabilities bcast filter */
	u32 multicast_filter;			   /**< NCSI channel capabilities multicast filter */
	u32 buffering;					  /**< NCSI channel capabilities buffering */
	u32 aen_ctrl;					   /**< NCSI channel capabilities aen ctrl */
	u8 vlan_count;					  /**< NCSI channel capabilities vlan count */
	u8 mixed_count;					 /**< NCSI channel capabilities mixed count */
	u8 multicast_count;				 /**< NCSI channel capabilities multicast count */
	u8 unicast_count;				   /**< NCSI channel capabilities unicast count */
	u16 rsvd;						   /**< NCSI channel capabilities reserved */
	u8 vlan_mode;					   /**< NCSI channel capabilities vlan mode */
	u8 chan_count;					  /**< NCSI channel capabilities channel count */
} ncsi_chan_capa_s;
/**
 * @brief ncsi get capabilities response(0x96) struct defination
 * @see NCSI_GET_CAPABILITIES_RSP
 *
 */
typedef struct tg_get_capabilities_rsp {
	u16 rsp_code;					   /**< get capabilities response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;					/**< get capabilities response reason code @see enum NCSI_REASON_CODE_E */
	ncsi_chan_capa_s chan_capabilities; /**< get capabilities response channel capabilities */
	u32 check_sum;					  /**< get capabilities response check sum */
} get_capabilities_rsp_s, *p_get_capabilities_rsp_s;

/* get parameters(0x17) */
#define PARAMETERS_REQ_LEN 0
#define PARAMETERS_RSP_LEN 72
/**
 * @brief ncsi get parameters command(0x17) struct defination
 * @see NCSI_GET_PARAMETERS
 *
 */
typedef struct tg_get_parameters {
	u32 check_sum;					 /**< get parameters command check sum */
} get_parameters_s, *p_get_parameters_s;

typedef struct tg_g_ncsi_parameters {
	u8 mac_address_count;
	u8 reserved1[2];
	u8 mac_address_flags;
	u8 vlan_tag_count;
	u8 reserved2;
	u16 vlan_tag_flags;
	u32 link_settings;
	u32 broadcast_packet_filter_settings;
	u8 broadcast_packet_filter_status : 1;
	u8 channel_enable : 1;
	u8 channel_network_tx_enable : 1;
	u8 global_mulicast_packet_filter_status : 1;
	u8 config_flags_reserved1 : 4;			   /**< bit0-3:mac_add0——mac_add3 address type：0 unicast，1 multileaving */
	u8 config_flags_reserved2[3];
	u8 vlan_mode;								/**< current vlan mode */
	u8 flow_control_enable;
	u16 reserved3;
	u32 AEN_control;
	u8 mac_add[4][6];
	u16 vlan_tag[VLAN_FL_MAX_ID];
} g_ncsi_parameters, *p_g_ncsi_parameters;

/**
 * @brief ncsi get parameters response(0x97) struct defination
 * @see NCSI_GET_PARAMETERS_RSP
 *
 */
typedef struct tg_get_parameters_rsp {
	u16 rsp_code;					/**< get parameters response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				 /**< get parameters response reason code @see enum NCSI_REASON_CODE_E */
	g_ncsi_parameters parameters;	/**< get parameters response ncsi parameters */
	u32 check_sum;
} get_parameters_rsp_s, *p_get_parameters_rsp_s;

/* get current packet statistics for the Ethernet Controller(0x18) */
#define PACKET_STATISTICS_REQ_LEN 0
#define PACKET_STATISTICS_RSP_LEN 204
/**
 * @brief ncsi get controller packet statistics command(0x18) struct defination
 * @see NCSI_GET_CONTROLLER_PACKET_STATISTICS
 *
 */
typedef struct tg_get_packet_statistics {
	u32 check_sum;
} get_packet_statistics_s, *p_get_packet_statistics_s;

typedef struct tg_controller_packet_statistics {
	u32 counter_cleared_from_last_read_MS;
	u32 counter_cleared_from_last_read_LS;
	u64 total_bytes_received;
	u64 total_bytes_transmitted;
	u64 total_unicast_packets_received;
	u64 total_multicast_packets_received;
	u64 total_broadcast_packets_received;
	u64 total_unicast_packets_transmitted;
	u64 total_multicast_packets_transmitted;
	u64 total_broadcast_packets_transmitted;
	u32 FCS_receive_errors;
	u32 alignment_errors;
	u32 false_carrier_detections;
	u32 runt_packets_received;
	u32 jabber_packets_received;
	u32 pause_XON_frames_received;
	u32 pause_XOFF_frames_received;
	u32 pause_XON_frames_transmitted;
	u32 pause_XOFF_frames_transmitted;
	u32 single_collision_transmit_frames;
	u32 multiple_collision_transmit_frames;
	u32 late_collision_frames;
	u32 excessive_collision_frames;
	u32 control_frames_received;
	u32 B64_frames_received;
	u32 B65_127_frames_received;
	u32 B128_255_frames_received;
	u32 B256_511_frames_received;
	u32 B512_1023_frames_received;
	u32 B1024_1522_frames_received;
	u32 B1523_9022_frames_received;
	u32 B64_frames_transmitted;
	u32 B65_127_frames_transmitted;
	u32 B128_255_frames_transmitted;
	u32 B256_511_frames_transmitted;
	u32 B512_1023_frames_transmitted;
	u32 B1024_1522_frames_transmitted;
	u32 B1523_9022_frames_transmitted;
	u64 valid_bytes_received;
	u32 error_runt_packets_received;
	u32 error_jabber_packets_received;
} controller_packet_statistics, *p_controller_packet_statistics;
/**
 * @brief ncsi get controller packet statistics response(0x98) struct defination
 * @see NCSI_GET_CONTROLLER_PACKET_STATISTICS_RSP
 *
 */
typedef struct tg_get_packet_statistics_rsp {
	u16 rsp_code;
	u16 reason_code;
	controller_packet_statistics pkt_statistics;
	u32 check_sum;
} get_packet_statistics_rsp_s, *p_get_packet_statistics_rsp_s;

/* request the packet statistics specific to the NC-SI (0x19) */
#define GET_NCSI_STATISTICS_REQ_LEN 0
#define GET_NCSI_STATISTICS_RSP_LEN 32
/**
 * @brief request the packet statistics specific to the NC-SI command(0x19) struct defination
 * @see NCSI_GET_NCSI_STATISTICS
 *
 */
typedef struct tg_get_ncsi_statistics {
	u32 check_sum;
} get_ncsi_statistics_s, *p_get_ncsi_statistics_s;
/**
 * @brief request the packet statistics specific to the NC-SI response(0x99) struct defination
 * @see NCSI_GET_NCSI_STATISTICS_RSP
 *
 */
typedef struct tg_get_ncsi_statistics_rsp {
	u16 rsp_code;	/**< get ncsi statistics response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code; /**< get ncsi statistics response reason code @see enum NCSI_REASON_CODE_E */
	u32 ncsi_commands_received;
	u32 ncsi_control_packets_dropped;
	u32 ncsi_command_type_error;
	u32 ncsi_command_checksun_errors;
	u32 ncsi_receive_packets;
	u32 ncsi_transmit_packets;
	u32 AENs_sent;
	u32 check_sum;
} get_ncsi_statistics_rsp_s, *p_get_ncsi_statistics_rsp_s;

/* request NC-SI Pass-through packet statistics(0x1A) */
#define GET_PASSTHOUGH_STATISTICS_REQ_LEN 0
#define GET_PASSTHOUGH_STATISTICS_RSP_LEN 48
/**
 * @brief request NC-SI Pass-through packet statistics command(0x1A) struct defination
 * @see NCSI_GET_PASSTHOUGH_STATISTICS
 *
 */
typedef struct tg_get_passthrough_statistics {
	u32 check_sum;
} get_passthrough_statistics_s, *p_get_passthrough_statistics_s;
/**
 * @brief request NC-SI Pass-through packet statistics response(0x9A) struct defination
 * @see NCSI_GET_PASSTHOUGH_STATISTICS_RSP
 *
 */
typedef struct tg_get_passthrough_statistics_rsp {
	u16 rsp_code;	/**< get passthrough statisticse response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code; /**< get passthrough statisticse response @see enum NCSI_REASON_CODE_E */
	u64 pass_through_TX_received;
	u32 pass_through_TX_dropped;
	u32 pass_through_TX_channel_state_error;
	u32 pass_through_TX_packet_undersized_error;
	u32 pass_through_TX_packet_oversized_error;
	u32 pass_through_RX_received;
	u32 pass_through_RX_dropped;
	u32 pass_through_RX_packet_channel_state_errors;
	u32 pass_through_RX_packet_undersized_error;
	u32 pass_through_RX_packet_oversized_error;
	u32 check_sum;
} get_passthrough_statistics_rsp_s, *p_get_passthrough_statistics_rsp_s;

/* OEM:get channel state (0X1B) */
#define GET_CHANNEL_STATE_REQ_LEN 0
#define GET_CHANNEL_STATE_RSP_LEN 8
/**
 * @brief ncsi get channel state command(0X1B) struct defination
 * @see NCSI_GET_CHANNEL_STATE
 *
 */
typedef struct tg_get_channel_state {
	u32 check_sum;
} get_channel_state_s, *p_get_channel_state_s;

/**
 * @brief ncsi get channel state response(0X9B) struct defination
 * @see NCSI_GET_CHANNEL_STATE_RSP
 *
 */
typedef struct tg_get_channel_state_rsp {
	u16 rsp_code;		   /**< get channel state response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;		/**< get channel state response reason code @see enum NCSI_REASON_CODE_E */
	u32 channel_state;	  /**< get channel state response channel state */
	u32 check_sum;		  /**< get channel state response check sum */
} get_channel_state_rsp_s, *p_get_channel_state_rsp_s;

/* get regiset value (0X1C) */
#define GET_REGISTER_VALUE_REQ_LEN 12
#define GET_REGISTER_VALUE_RSP_LEN 8
/**
 * @brief ncsi get register value command(0X1C) struct defination
 * @see NCSI_GET_REGISTER_VALUE
 *
 */
typedef struct tg_get_register_value {
	u32 base_address;
	u32 offset_address;
	u32 module;
	u32 check_sum;
} get_register_value_s, *p_get_register_value_s;

/**
 * @brief ncsi get regiset value response(0X9C) struct defination
 * @see NCSI_GET_REGISTER_VALUE_RSP
 *
 */
typedef struct tg_get_register_value_rsp {
	u16 rsp_code;	   /**< get regiset value response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;	/**< get regiset value response reason code @see enum NCSI_REASON_CODE_E */
	u32 register_value;
	u32 check_sum;
} get_register_value_rsp_s, *p_get_register_value_rsp_s;

/* Set Fwd Act Cammand 0x1D */
#define SET_FWD_ACT_REQ_LEN 4
#define SET_FWD_ACT_RSP_LEN 4
/**
 * @brief ncsi Set Fwd Act command(0x1D) struct defination
 * @see NCSI_SET_FWD_ACT
 *
 */
typedef struct tg_set_fwd_act {
	u8 reserved[3];
	u8 fwd_mode;
	u32 check_sum;   /**< Set Fwd Act command check sum */
} set_fwd_act_s, *p_set_fwd_act_s;
/**
 * @brief ncsi Set Fwd Act response(0x9D) struct defination
 * @see NCSI_SET_FWD_ACT_RSP
 *
 */
typedef struct tg_set_fwd_act_rsp {
	u16 rsp_code;	/**< Set Fwd Act response rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code; /**< Set Fwd Act response reason code @see enum NCSI_REASON_CODE_E */
	u32 check_sum;   /**< Set Fwd Act response check sum */
} set_fwd_act_rsp_s, *p_set_fwd_act_rsp_s;

/* NCSI OEM命令新增错误原因码 */
typedef enum {
	OEM_MANUFAC_ID_ERROR = 0x8001, /**< 内部标识用，不回填reason code到rsp packet */
	OEM_CMD_HEAD_INFO_INVALID = 0x8002,
	OEM_GET_INFO_FAILED = 0x8003,
	OEM_ERR_CODE_NUM_OVER = 0x8004,
	OEM_ERR_UP_GET_DRIVER_INFO_FAILED = 0x8005,
	OEM_CABLE_TYPE_NOT_SUPPORT = 0x8006,
	OEM_CABLE_TYPE_UNDEF = 0x8007,
	OEM_OPTICAL_MODULE_ABS = 0x8008,
	OEM_ENABLE_LLDP_CAPTURE_FAILED = 0x8009,
	/* 0x9000~0xFFFF用作ncsi标准命令新增的oem reason code */
	OEM_GET_CONTROLLER_STATISTIC_FAILED = 0x9000, /**< 网卡统计信息获取失败reason code */
	OEM_ENABLE_LLDP_OVER_NCSI_FAILED = 0x9001,
	OEM_GET_LLDP_OVER_NCSI_STATUS_FAILED = 0x9002
} NCSI_OEM_REASON_CODE_E;

/* ncsi oem命令相关 */
/* ncsi oem命令公共头 */
typedef struct tg_ncsi_oem_cmd_head {
	u32 manufac_id; /**< 厂商id，huawei(0x07db) */
	u8 cmd_rev;
	u8 hw_cmd_id;  /**< cmd号 */
	u8 sub_cmd_id; /**< 子cmd号 */
	u8 index;	  /**< index:只针对关注pf_id的命令有效（除了获取日志0x12命令代表获取日志类型），其余情况看做rsv字段 */
} ncsi_oem_cmd_head_s;

/* ncsi oem命令响应包payload公共部分(包含payload前12Bytes) */
typedef struct tg_ncsi_oem_rsp_payload_comm {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
} ncsi_oem_rsp_payload_comm_s;

/* ncsi oem命令请求包 */
/**
 * @brief ncsi oem command(0x50) struct defination
 * @see NCSI_OEM_COMMAND
 *
 */
typedef struct tg_ncsi_oem_cmd_req {
	ncsi_ctrl_pkt_header_s packet_head; /**< ncsi ctrl packet header */
	ncsi_oem_cmd_head_s oem_head;	   /**< oem command header */
	u32 check_sum;
} ncsi_oem_cmd_req_s;

#define OEM_PROC_FAILED_RSP_LEN (4)	 /* oem命令内部处理失败后给bmc侧返回包payload大小 */
/* oem命令内部处理失败后给bmc侧返回包结构 */
typedef struct tg_oem_proc_failed_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	u32 check_sum;					 /**< check sum */
} oem_proc_failed_rsp_s;

/**
 * @brief oem get network interface bdf response(huawei_id:0x0, sub_id:0x1) struct defination
 * @see OEM_GET_NETWORK_INTERFACE_BDF
 *
 */
typedef struct tg_oem_get_bdf_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u8 rsvd;
	u8 bus;							/**< bus id */
	u8 device;						 /**< device id */
	u8 function;					   /**< function id */
	u32 check_sum;					 /**< check sum */
} oem_get_bdf_rsp_s;

/* get the pcie interface ability(huawei_id:0x0, sub_id:0x5) */
#define OEM_GET_PCIE_ABILITY_RSP_LEN (16)
/**
 * @brief oem get the pcie interface ability response(huawei_id:0x0, sub_id:0x5) struct defination
 * @see OEM_GET_PCIE_ABILITY
 *
 */
typedef struct tg_oem_get_pcie_ability_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u8 pcie_link_width;				/**< pcie link width */
	u8 pcie_link_speed;				/**< pcie link speed */
	u8 rsv[2];
	u32 check_sum;					 /**< check sum */
} oem_get_pcie_ability_rsp_s;

/* get the pcie interface status(huawei_id:0x0, sub_id:0x6) */
#define OEM_GET_PCIE_STATUS_RSP_LEN (16)
/**
 * @brief oem get the pcie interface status(huawei_id:0x0, sub_id:0x6) struct defination
 * @see OEM_GET_PCIE_STATUS
 *
 */
typedef struct tg_oem_get_pcie_status_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u8 pcie_link_width;				/**< pcie link width */
	u8 pcie_link_speed;				/**< pcie link speed */
	u8 rsv[2];
	u32 check_sum;					 /**< check sum */
} oem_get_pcie_status_rsp_s;

/* get the network interface media type(huawei_id:0x0, sub_id:0x9) */
#define OEM_GET_NETWORK_INTERFACE_MEDIA_TYPE_SP_LEN (16)
/**
 * @brief oem get network interface media type response(huawei_id:0x0, sub_id:0x9) struct defination
 * @see OEM_GET_NETWORK_INTERFACE_MEDIA_TYPE
 *
 */
typedef struct tag_oem_get_network_interface_media_type_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command heade */
	u8 media_type; /**< 0x0: Fiber(BASE-SR/LR/ER),0x1:DAC(BASE-CR),0x2:Copper(BASE-T/RJ45),0x3:Backplane(BASE-KR) */
	u8 rsv[3];
	u32 check_sum;					 /**< check sum */
} oem_get_network_interface_media_type_rsp_s;

/* get the junction temperature(huawei_id:0x0, sub_id:0x0a) */
#define OEM_GET_JUNCTION_TEMP_RSP_LEN (16)
/**
 * @brief oem get the junction temperature response(huawei_id:0x0, sub_id:0x0a) struct defination
 * @see OEM_GET_JUNCTION_TEMP
 *
 */
typedef struct tag_oem_get_junction_temp_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u16 junction_temp;				 /**< junction temperature */
	u8 rsv[2];
	u32 check_sum;					 /**< check sum */
} oem_get_junction_temp_rsp_s;

/* get the optical module temperature(huawei_id:0x0, sub_id:0x0b) */
/**
 * @brief oem get the optical module temperature response(huawei_id:0x0, sub_id:0x0b) struct defination
 * @see OEM_GET_OPTICAL_MODULE_TEMP
 *
 */
#define OEM_GET_OPT_MODU_TEMP_RSP_LEN (16)
typedef struct tag_oem_get_opt_modu_temp_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u16 opt_modu_temp;				 /**< optical module temperature */
	u8 rsv[2];
	u32 check_sum;					 /**< check sum */
} oem_get_opt_modu_temp_rsp_s;

/* get the err code(huawei_id:0x0, sub_id:0x0c) */
#define NCSI_OEM_ERR_CODE_MAX_NUM (11) /* 上报错误码最大数目,当前定义为11 */
#define OEM_GET_ERR_CODE_RSP_LEN (36)

/* get the transceiver or cable information(huawei_id:0x0, sub_id:0x0d) */
#define OEM_GET_CABLE_INFO_RSP_LEN (120)
#define PART_NUM_MAX_LEN (16)
#define VENDOR_MAX_LEN (16)
#define SERIAL_NUM_MAX_LEN (16)
#define CABLE_INFO_UNSUPPORTED16 (0xFFFF)
#define CABLE_INFO_UNSUPPORTED8 (0xFF)

#define QSFP_WAVE_LENGTH_DIVIDER (20)
/**
 * @brief oem get the transceiver or cable information response(huawei_id:0x0, sub_id:0x0d) struct defination
 * @see OEM_GET_NETWORK_INTERFACE_TRANS_CABLE_INFO
 *
 */
typedef struct tag_oem_get_trans_or_cable_info_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */

	u8 device_part_number[PART_NUM_MAX_LEN];	 /**< 设备part_number */
	u8 device_vendor[VENDOR_MAX_LEN];			/**< 设备厂商 */
	u8 device_serial_number[SERIAL_NUM_MAX_LEN]; /**< 设备序列号 */

	u8 device_identifier;   /**< 设备识别SFP+/QSFP+/SFP28/QSFP28 */
	u8 device_type;		 /**< 设备类型(SR/LR/CR_Passive/CR_Active/SR4/LR4/CR4_Passive/CR4_Active) */
	u8 device_connect_type; /**< 设备连接类型(LC/MPO/DAC/RJ45) */
	u8 rsv;

	u16 device_trans_distance; /**< 设备传输距离 */
	u16 device_wavelen;		/**< 设备波长 */

	u16 work_para_temp;			/**< 工作温度 */
	u16 work_para_voltage;		 /**< 工作电压 */
	u16 work_para_tx_bias_current; /**< tx电流 */
	u16 work_para_tx_power;		/**< tx功率 */
	u16 work_para_rx_power;		/**< rx功率 */
	u16 warn_threshold_low_temp;   /**< 低温告警阈值 */
	u16 warn_threshold_high_temp;  /**< 高温告警阈值 */
	u16 warn_threshold_tx_power;   /**< tx功率告警阈值 */
	u16 warn_threshold_rx_power;   /**< rx功率告警阈值 */
	u16 alarm_threshold_low_temp;  /**< 低温警报阈值 */
	u16 alarm_threshold_high_temp; /**< 低温警报阈值 */
	u16 alarm_threshold_tx_power;  /**< tx功率警报阈值 */
	u16 alarm_threshold_rx_power;  /**< rx功率警报阈值 */
	u8 rx_los_state;
	u8 tx_fult_state;

	u8 rsv1[24];

	u32 check_sum;				 /**< check sum */
} oem_get_trans_or_cable_info_rsp_s;

/* enable LLDP capture(huawei_id:0x0, sub_id:0x0e) */
#define OEM_ENABLE_LLDP_CAPTURE_RSP_LEN (12)
/**
 * @brief oem enable LLDP capture response(0xe) struct defination
 * @see OEM_ENABLE_LLDP_CAPTURE
 *
 */
typedef struct tag_oem_enable_lldp_capture_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */

	u32 check_sum;					 /**< check sum */
} oem_enable_lldp_capture_rsp_s;

/* get lldp capbility(huawei_id:0x0, sub_id:0x0f) */
#define OEM_GET_LLDP_CAPBILITY_RSP_LEN (16)
/**
 * @brief oem get lldp capbility response(huawei_id:0x0, sub_id:0x0f) struct defination
 * @see OEM_GET_LLDP_CAPBILITY
 *
 */
typedef struct tag_oem_get_lldp_capbility_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u8 capbility;
	u8 rsv[3];
	u32 check_sum;					 /**< check sum */
} oem_get_lldp_capbility_rsp_s;

/**
 * @brief oem get lldp capbility response(huawei_id:0x0, sub_id:0x11) struct defination
 * @see OEM_GET_HW_OEM_CMD_CAPABILITY (0x11)
 *
 */
typedef struct tag_oem_get_oem_command_cap {
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u32 check_sum;					 /**< check sum */
} oem_get_oem_command_cap;

/**
 * @brief oem get oem command capbility response(huawei_id:0x0, sub_id:0x11) struct defination
 * @see OEM_GET_HW_OEM_CMD_CAPABILITY (0x11)
 * @see OEM_ENABLE_LOW_POWER_MODE (0x40C)
 * @see OEM_GET_LOW_POWER_MODE_STATUS (0x40D)
 * @see OEM_ENABLE_LLDP_OVER_NCSI (0x40A)
 * @see OEM_GET_LLDP_OVER_NCSI_STATUS (0x40B)
 *
 */
typedef struct tag_oem_get_oem_command_cap_resp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */

	u32 bitmap;	/**< Capability Bit Map
					  * bit0: Enable/disable low power consumption mode
					  * bit1: Get low power consumption mode
					  * bit2: Enable/disable LLDP forward over NCSI
					  * bit3: Get LLDP forward over NCSI status
					  * bit4: Need set Neighbor Solicitation destination multicast address in IPV6 mode
					  * other bit: Reserved
					  */
	u32 rsvd[13];
	u32 check_sum;					 /**< check sum */
} oem_get_oem_command_cap_resp;

typedef struct tag_oem_get_log_info_head {
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u32 offset;
	u32 len;
	u32 check_sum;					 /**< check sum */
} oem_get_log_head;

#define OEM_GET_LOG_INFO_MAX_LEN (1024)	// 一次性传输最大1024B
#define OEM_GET_LOG_INFO_RSP_LEN (1036)
/**
 * @brief oem get log info response(huawei_id:0x0, sub_id:0x12) struct defination
 * @see OEM_GET_LOG_INFO
 *
 */
typedef struct tag_oem_get_log_info_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u8 log_data[OEM_GET_LOG_INFO_MAX_LEN];
	u32 check_sum;					 /**< check sum */
} oem_get_log_rsp;

/**
 * @brief oem enable lldp over ncsi command(0x40A) struct defination
 * @see OEM_ENABLE_LLDP_OVER_NCSI
 *
 */
typedef struct tag_oem_enable_lldp_over_ncsi {
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u8 enable;
	u8 rsvd[3];
	u32 check_sum;					 /**< check sum */
} oem_enable_lldp_over_ncsi;

/**
 * @brief oem enable lldp over ncsi response(0x40A) struct defination
 * @see OEM_ENABLE_LLDP_OVER_NCSI
 *
 */
typedef struct tag_oem_enable_lldp_over_ncsi_resp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u32 check_sum;					 /**< check sum */
} oem_enable_lldp_over_ncsi_resp;

/**
 * @brief oem get lldp over ncsi status command(0x40B) struct defination
 * @see OEM_GET_LLDP_OVER_NCSI_STATUS
 *
 */
typedef struct tag_oem_get_lldp_over_ncsi_status {
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u32 check_sum;					 /**< check sum */
} oem_get_lldp_over_ncsi_status;

/**
 * @brief oem get lldp over ncsi status response(0x40B) struct defination
 * @see OEM_GET_LLDP_OVER_NCSI_STATUS
 *
 */
typedef struct tag_oem_get_lldp_over_ncsi_status_resp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u8 enable;
	u8 rsvd[3];
	u32 check_sum;					 /**< check sum */
} oem_get_lldp_over_ncsi_status_resp;

/**
 * @brief oem enable low power mode command(0x40C) struct defination
 * @see OEM_ENABLE_LOW_POWER_MODE
 *
 */
typedef struct tag_oem_enable_low_power_mode {
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem cmd公共头 */
	u8 enable;
	u8 rsvd[3];
	u32 check_sum;					 /**< check sum */
} oem_enable_low_power_mode;

/**
 * @brief oem oem enable low power mode response(0x40C) struct defination
 * @see OEM_ENABLE_LOW_POWER_MODE
 *
 */
typedef struct tag_oem_enable_low_power_mode_resp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u32 check_sum;					 /**< check sum */
} oem_enable_low_power_mode_resp;

/**
 * @brief oem get low power mode status command(0x40D) struct defination
 * @see OEM_GET_LOW_POWER_MODE_STATUS
 *
 */
typedef struct tag_oem_get_low_power_mode_status {
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header @see struct ncsi_oem_cmd_head_s */
	u32 check_sum;					 /**< check sum */
} oem_get_low_power_mode_status;

/**
 * @brief oem get low power mode status response(0x40D) struct defination
 * @see OEM_GET_LOW_POWER_MODE_STATUS
 *
 */
typedef struct tag_oem_get_low_power_mode_status_resp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u8 enable;
	u8 rsvd[3];
	u32 check_sum;					 /**< check sum */
} oem_get_low_power_mode_status_resp;

/* get the network interface mac addr(huawei_id:0x1, sub_id:0x00) */
#define MAC_ADDRESS_LEN (6)
#define OEM_GET_NETWORK_INTERFACE_MAC_ADDR_RSP_LEN (64)
#define OEM_NETWORK_INTERFACE_MAC_ADDR_MAX_NUM (8) /* 数据结构中最多只呈现8个mac addr */
/**
 * @brief oem get netork interface mac addr response(0x100) struct defination
 * @see OEM_GET_NETWORK_INTERFACE_MAC_ADDR
 *
 */
typedef struct tag_oem_get_netork_interface_mac_addr_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u16 mac_addr_count;				/**< mac_addr实际数目(可能不止8个，最多只显示8个) */
	u8 rsv[2];
	u8 mac_addr[MAC_ADDRESS_LEN * OEM_NETWORK_INTERFACE_MAC_ADDR_MAX_NUM]; /* mac addr,必须确保4字节对齐 */

	u32 check_sum;					 /**< check sum */
} oem_get_netork_interface_mac_addr_rsp_s;

/* get the network interface dcbx(huawei_id:0x1, sub_id:0x02) */
#define OEM_GET_NETWORK_INTERFACE_DCBX_RSP_LEN (48)

/**
 * @brief oem get networ interface dcbx response(0x102) struct defination
 * @see OEM_GET_NETWORK_INTERFACE_DCBX
 *
 */
typedef struct tag_oem_get_network_interface_dcbx_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u8 up2cos[8];					  /**< user_priority优先级到cos的映射关系 */
	u8 up_pgid[8];					 /**< DCB ETS的COS TC映射关系 */
	u8 pgpct[8];					   /**< tc之间的带宽比 */
	u8 strict[8];					  /**< 调度方式为SP还是DWRR */
	u8 pfcmap;						 /**< dcb pfc使能和关闭状态 */
	u8 rsv[3];

	u32 check_sum;					 /**< check sum */
} oem_get_network_interface_dcbx_rsp_s;

#define OEM_PGPCT_TC_WGT_INDEX 100

/* get dafault mac address(huawei_id:0x1, sub_id:0x04) */
#define MAC_ADDRESS_NUM (6) // 有定义，后面可以搞
#define OEM_GET_DEFAULT_MAC_ADDR_RSP_LEN (64)
#define OEM_DEFAULT_MAC_ADDRESS_MAX_COUNT (8) /* default mac地址最大数目 */
#define PORT_DEFAULT_MAC_ADDR_MAX_SIZE (MAC_ADDRESS_NUM * OEM_DEFAULT_MAC_ADDRESS_MAX_COUNT)

/**
 * @brief oem get default mac addr response(0x104) struct defination
 * @see OEM_GET_NETWORK_DEFAULT_MAC_ADDR
 *
 */
typedef struct tag_oem_get_default_mac_addr_rsp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u8 mac_count;					  /**< mac地址数目 */
	u8 rsv[3];
	u8 mac_addr[PORT_DEFAULT_MAC_ADDR_MAX_SIZE]; /**< default mac addr,必须确保4字节对齐 */

	u32 check_sum;					 /**< check sum */
} oem_get_default_mac_addr_rsp_s;

/**
 * @brief ncsi oem set volatile mac command(0x508) struct defination
 * @see OEM_SET_VOLATILE_MAC
 *
 */
typedef struct tag_oem_set_volatile_mac {
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u8 mac[MAC_ADDRESS_LEN];
	u8 rsv[2];
	u32 check_sum;					 /**< check sum */
} oem_set_volatile_mac;

#define OEM_SET_VOLATILE_MAC_RESP_LEN 12
#define OEM_GET_VOLATILE_MAC_RESP_LEN 20

/**
 * @brief ncsi oem set volatile mac response(0x508) struct defination
 * @see OEM_SET_VOLATILE_MAC
 *
 */
typedef struct tag_oem_set_volatile_mac_resp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u32 check_sum;					 /**< check sum */
} oem_set_volatile_mac_resp;

/**
 * @brief ncsi oem get volatile mac command(0x509) struct defination
 * @see OEM_SET_VOLATILE_MAC
 *
 */
typedef struct tag_oem_get_volatile_mac {
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u32 check_sum;					 /**< check sum */
} oem_get_volatile_mac;

/**
 * @brief ncsi oem get volatile mac response(0x509) struct defination
 * @see OEM_SET_VOLATILE_MAC
 *
 */
typedef struct tag_oem_get_volatile_mac_resp {
	u16 rsp_code;					  /**< rsp code @see enum NCSI_RESPONSE_CODE_E */
	u16 reason_code;				   /**< reason code @see enum NCSI_OEM_REASON_CODE_E */
	ncsi_oem_cmd_head_s ncsi_oem_head; /**< oem command header */
	u8 mac[MAC_ADDRESS_LEN];
	u8 rsv[2];
	u32 check_sum;					 /**< check sum */
} oem_get_volatile_mac_resp;

#pragma pack()

#endif
