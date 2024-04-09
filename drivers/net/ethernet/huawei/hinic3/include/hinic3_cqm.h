/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2021 Huawei Technologies Co., Ltd */

#ifndef CQM_H
#define CQM_H

#include <linux/completion.h>

#ifndef HIUDK_SDK

#include "hinic3_cqm_define.h"
#include "vram_common.h"

///返回值执行成功
#define CQM_SUCCESS 0
///返回值执行失败
#define CQM_FAIL (-1)
///忽略返回值继续执行
#define CQM_CONTINUE 1

///WQE类型为LINK WQE
#define CQM_WQE_WF_LINK 1
///WQE类型为普通WQE
#define CQM_WQE_WF_NORMAL 0

///链式队列模式
#define CQM_QUEUE_LINK_MODE 0
///RING式队列模式
#define CQM_QUEUE_RING_MODE 1
///SRQ队列模式
#define CQM_QUEUE_TOE_SRQ_LINK_MODE 2
///RDMA队列模式
#define CQM_QUEUE_RDMA_QUEUE_MODE 3

///通用linkwqe结构
typedef struct tag_cqm_linkwqe {
	u32 rsv1 : 14;///<保留字段
	u32 wf : 1;///<wf
	u32 rsv2 : 14;///<保留字段
	u32 ctrlsl : 2;///<ctrlsl
	u32 o : 1;///<o bit

	u32 rsv3 : 31;///<保留字段
	u32 lp : 1; /* lp字段决定o-bit含义是否翻转 */

	u32 next_page_gpa_h;///<记录下一个页面的物理地址高32b,给芯片使用
	u32 next_page_gpa_l;///<记录下一个页面的物理地址低32b,给芯片使用

	u32 next_buffer_addr_h;///<记录下一个页面的虚拟地址高32b,给驱动使用
	u32 next_buffer_addr_l;///<记录下一个页面的虚拟地址低32b,给驱动使用
} cqm_linkwqe_s;

///srq linkwqe结构，wqe大小需要保证不超过普通RQE大小
typedef struct tag_cqm_srq_linkwqe {
	cqm_linkwqe_s linkwqe;///<通用linkwqe结构
	u32 current_buffer_gpa_h;///<记录当前页面的物理地址高32b,驱动释放container取消映射时使用
	u32 current_buffer_gpa_l;///<记录当前页面的物理地址低32b,驱动释放container取消映射时使用
	u32 current_buffer_addr_h;///<记录当前页面的虚拟地址高32b，驱动释放container时使用
	u32 current_buffer_addr_l;///<记录当前页面的虚拟地址低32b，驱动释放container时使用

	u32 fast_link_page_addr_h;///<记录container地址所在fastlink 页的虚拟地址高32b，驱动释放fastlink时使用
	u32 fast_link_page_addr_l;///<记录container地址所在fastlink 页的虚拟地址低32b，驱动释放fastlink时使用

	u32 fixed_next_buffer_addr_h;///<记录下一个contianer的虚拟地址高32b，用于驱动资源释放，驱动不可修改
	u32 fixed_next_buffer_addr_l;///<记录下一个contianer的虚拟地址低32b，用于驱动资源释放，驱动不可修改
} cqm_srq_linkwqe_s;

///标准128B WQE的前64B
typedef union tag_cqm_linkwqe_first64B {
	cqm_linkwqe_s basic_linkwqe;///<通用linkwqe结构
	cqm_srq_linkwqe_s toe_srq_linkwqe;///<srq linkwqe结构
	u32 value[16];///<保留字段
} cqm_linkwqe_first64B_s;

///标准128B WQE的后64B
typedef struct tag_cqm_linkwqe_second64B {
	u32 rsvd0[4];///<第一个16B保留字段
	u32 rsvd1[4];///<第二个16B保留字段
	union {
		struct {
			u32 rsvd0[3];
			u32 rsvd1 : 29;
			u32 toe_o : 1;///<toe的o bit
			u32 resvd2 : 2;
		} bs;
		u32 value[4];
	} third_16B;///<第三个16B

	union {
		struct {
			u32 rsvd0[2];
			u32 rsvd1 : 31;
			u32 ifoe_o : 1;///<ifoe的o bit
			u32 rsvd2;
		} bs;
		u32 value[4];
	} forth_16B;///<第四个16B
} cqm_linkwqe_second64B_s;

///标准128B WQE结构
typedef struct tag_cqm_linkwqe_128B {
	cqm_linkwqe_first64B_s first64B;///<标准128B WQE的前64B
	cqm_linkwqe_second64B_s second64B;///<标准128B WQE的后64B
} cqm_linkwqe_128B_s;

/// AEQ类型定义
typedef enum {
	CQM_AEQ_BASE_T_NIC = 0, ///<NIC分16个event:0~15
	CQM_AEQ_BASE_T_ROCE = 16, ///<ROCE分32个event:16~47
	CQM_AEQ_BASE_T_FC = 48, ///<FC分8个event:48~55
	CQM_AEQ_BASE_T_IOE = 56, ///<IOE分8个event:56~63
	CQM_AEQ_BASE_T_TOE = 64, ///<TOE分16个event:64~95
	CQM_AEQ_BASE_T_VBS = 96, ///<VBS分16个event:96~111
	CQM_AEQ_BASE_T_IPSEC = 112, ///<VBS分16个event:112~127
	CQM_AEQ_BASE_T_MAX = 128 ///<最大定义128种event
} cqm_aeq_event_type_e;

///服务注册模板
typedef struct tag_service_register_template {
	u32 service_type;///<服务类型
	u32 srq_ctx_size;///<srq context大小
	u32 scq_ctx_size;///<scq context大小
	void *service_handle;///<ceq/aeq回调函数时传给service driver的指针
	void (*shared_cq_ceq_callback)(void *service_handle, u32 cqn, void *cq_priv);///<ceq回调:shared cq
	void (*embedded_cq_ceq_callback)(void *service_handle, u32 xid, void *qpc_priv);///<ceq回调:embedded cq
	void (*no_cq_ceq_callback)(void *service_handle, u32 xid, u32 qid, void *qpc_priv);///<ceq回调:no cq
	u8 (*aeq_level_callback)(void *service_handle, u8 event_type, u8 *val);///<aeq level回调
	void (*aeq_callback)(void *service_handle, u8 event_type, u8 *val);///<aeq回调
} service_register_template_s;

/// 对象操作类型定义
typedef enum cqm_object_type {
	CQM_OBJECT_ROOT_CTX = 0,				///<0:root context，留在以后兼容root ctx管理
	CQM_OBJECT_SERVICE_CTX,				 ///<1:QPC，连接管理对象
	CQM_OBJECT_MPT,						 ///<2:RDMA业务使用

	CQM_OBJECT_NONRDMA_EMBEDDED_RQ = 10,	///<10:非RDMA业务的RQ，用LINKWQE管理
	CQM_OBJECT_NONRDMA_EMBEDDED_SQ,		 ///<11:非RDMA业务的SQ，用LINKWQE管理
	CQM_OBJECT_NONRDMA_SRQ,				 ///<12:非RDMA业务的SRQ，用MTT管理，但要CQM自己申请MTT
	CQM_OBJECT_NONRDMA_EMBEDDED_CQ,		 ///<13:非RDMA业务的embedded CQ，用LINKWQE管理
	CQM_OBJECT_NONRDMA_SCQ,				 ///<14:非RDMA业务的SCQ，用LINKWQE管理

	CQM_OBJECT_RESV = 20,

	CQM_OBJECT_RDMA_QP = 30,				///<30:RDMA业务的QP，用MTT管理
	CQM_OBJECT_RDMA_SRQ,					///<31:RDMA业务的SRQ，用MTT管理
	CQM_OBJECT_RDMA_SCQ,					///<32:RDMA业务的SCQ，用MTT管理

	CQM_OBJECT_MTT = 50,					///<50:RDMA业务的MTT表
	CQM_OBJECT_RDMARC,					  ///<51:RDMA业务的RC
} cqm_object_type_e;

///BITMAP表申请的失败返回值
#define CQM_INDEX_INVALID ~(0U)
///BITMAP表申请的预留位返回值，说明该INDEX由CQM分配，不支持驱动指定
#define CQM_INDEX_RESERVED (0xfffff)

///为支持ROCE的Q buffer resize，第一个Q buffer空间
#define CQM_RDMA_Q_ROOM_1 (1)
///为支持ROCE的Q buffer resize，第二个Q buffer空间
#define CQM_RDMA_Q_ROOM_2 (2)

///当前Q选择的doorbell方式，硬件doorbell
#define CQM_HARDWARE_DOORBELL (1)
///当前Q选择的doorbell方式，软件doorbell
#define CQM_SOFTWARE_DOORBELL (2)


///CQM buffer单节点结构
typedef struct tag_cqm_buf_list {
	void *va;///<虚拟地址
	dma_addr_t pa;///<物理地址
	u32 refcount;///<buf的引用计数，内部buf管理用
} cqm_buf_list_s;

///CQM buffer通用管理结构
typedef struct tag_cqm_buf {
	cqm_buf_list_s *buf_list;///<buffer链表
	cqm_buf_list_s direct;///<将离散的buffer链表映射到一组连续地址上
	u32 page_number;///<page_number=2^n个buf_number
	u32 buf_number;///<buf_list节点个数
	u32 buf_size;///<buf_size=2^n个PAGE_SIZE
	struct vram_buf_info buf_info;
	u32 bat_entry_type;
} cqm_buf_s;

struct completion;
///CQM的对象结构，可以认为是所有队列/CTX等抽象出来的基类
typedef struct tag_cqm_object {
	u32 service_type;   ///<服务类型
	u32 object_type;	///<对象类型，如context,queue,mpt,mtt等
	u32 object_size;	///<对象大小，对于queue/ctx/MPT，单位Byte，对于MTT/RDMARC，单位entry个数，对于container，单位conainer个数
	atomic_t refcount;  ///<引用计数
	struct completion free;///<释放完成量
	void *cqm_handle;///<cqm_handle
} cqm_object_s;

///CQM的QPC，mpt对象结构
typedef struct tag_cqm_qpc_mpt {
	cqm_object_s object;///<对象基类
	u32 xid;///<xid
	dma_addr_t paddr;///<QPC/MTT内存的物理地址
	void *priv; ///<service driver的该对象的私有信息
	u8 *vaddr;///<QPC/MTT内存的虚拟地址
} cqm_qpc_mpt_s;

///queue header结构
typedef struct tag_cqm_queue_header {
	u64 doorbell_record;///<SQ/RQ的db内容
	u64 ci_record;///<CQ的db内容
	u64 rsv1;	  ///<该区域为驱动和微码传递信息的自定义区
	u64 rsv2;	  ///<该区域为驱动和微码传递信息的自定义区
} cqm_queue_header_s;


///队列管理结构：非RDMA业务的queue，embeded队列用linkwqe管理，SRQ和SCQ用mtt管理，但mtt要CQM申请；RDMA业务的queue，用mtt管理
typedef struct tag_cqm_queue {
	cqm_object_s object;			///<对象基类
	u32 index;					  ///<embeded队列、QP没有index，SRQ和SCQ有
	void *priv;					 ///<service driver的该对象的私有信息
	u32 current_q_doorbell;		 ///<当前queue选择的doorbell类型，roce QP同时用HW/SW
	u32 current_q_room;			 ///<roce:当前有效的room buf
	cqm_buf_s q_room_buf_1;	 	///<nonrdma:只能选择q_room_buf_1为q_room_buf
	cqm_buf_s q_room_buf_2;		 ///<RDMA的CQ会重新分配queue room的大小
	cqm_queue_header_s *q_header_vaddr;///<queue header虚拟地址
	dma_addr_t q_header_paddr;///<queue header物理地址
	u8 *q_ctx_vaddr;				///<SRQ和SCQ的ctx虚拟地址
	dma_addr_t q_ctx_paddr;		 ///<SRQ和SCQ的ctx物理地址
	u32 valid_wqe_num;			  ///<创建成功的有效wqe个数
	u8 *tail_container;			 ///<SRQ container的尾指针
	u8 *head_container;			 ///<SRQ container的首针
	u8 queue_link_mode;			 ///<队列创建时确定连接模式:link,ring等
} cqm_queue_s;

///MTT/RDMARC管理结构
typedef struct tag_cqm_mtt_rdmarc {
	cqm_object_s object;///<对象基类
	u32 index_base;///<index_base
	u32 index_number;///<index_number
	u8 *vaddr;///<buffer虚拟地址
} cqm_mtt_rdmarc_s;

/// 发送命令结构
typedef struct tag_cqm_cmd_buf {
	void *buf;	   ///< 命令buf虚拟地址
	dma_addr_t dma;  ///< 命令buf物理地址
	u16 size;		///< 命令buf大小
} cqm_cmd_buf_s;

/// 发送ACK方式定义
typedef enum {
	CQM_CMD_ACK_TYPE_CMDQ = 0,	   ///< ack回写到cmdq
	CQM_CMD_ACK_TYPE_SHARE_CQN = 1,  ///< ack通过root ctx的scq上报
	CQM_CMD_ACK_TYPE_APP_CQN = 2	 ///< ack通过业务的scq上报
} cqm_cmd_ack_type_e;

/// 发送命令buffer长度
#define CQM_CMD_BUF_LEN 0x800

#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#if !defined(HIUDK_ULD) && !defined(HIUDK_SDK_ADPT)

#define  hiudk_cqm_object_delete(x, y) cqm_object_delete(y)
#define  hiudk_cqm_object_funcid(x, y) cqm_object_funcid(y)
#define  hiudk_cqm_object_offset_addr(x, y, z, m) cqm_object_offset_addr(y, z, m)
#define  hiudk_cqm_object_put(x, y) cqm_object_put(y)
#define  hiudk_cqm_object_resize_alloc_new(x, y, z) cqm_object_resize_alloc_new(y, z)
#define  hiudk_cqm_object_resize_free_new(x, y) cqm_object_resize_free_new(y)
#define  hiudk_cqm_object_resize_free_old(x, y) cqm_object_resize_free_old(y)
#define  hiudk_cqm_object_share_recv_queue_add_container(x, y) cqm_object_share_recv_queue_add_container(y)
#define  hiudk_cqm_object_srq_add_container_free(x, y, z) cqm_object_srq_add_container_free(y, z)
#define  hiudk_cqm_ring_software_db(x, y, z) cqm_ring_software_db(y, z)
#define  hiudk_cqm_srq_used_rq_container_delete(x, y, z) cqm_srq_used_rq_container_delete(y, z)

#endif

#ifndef HIUDK_ULD

/**
 * @brief CQM初始化
 * @details 网卡驱动每探测到一个pcie设备时调用，必须在初始化上层业务之前调用
 * @param ex_handle 表征PF的设备指针
 * @retval 0 成功
 * @retval -1 失败
 * @author
 * @date 2019-5-4
 */
extern s32 cqm3_init(void *ex_handle);

/**
 * @brief CQM去初始化
 * @details 每移除一个function被调用一次
 * @param ex_handle 表征PF的设备指针
 * @retval void
 * @author
 * @date 2019-5-4
 */
extern void cqm3_uninit(void *ex_handle);

/**
 * @brief 业务驱动向CQM注册回调模板
 * @details 业务驱动向CQM注册回调模板
 * @param ex_handle 表征PF的设备指针
 * @param service_template 服务注册模板
 * @retval 0 成功
 * @retval -1 失败
 * @author
 * @date 2019-5-4
 */
extern s32 cqm3_service_register(void *ex_handle, service_register_template_s *service_template);

/**
 * @brief 业务驱动向CQM注销
 * @details 业务驱动向CQM注销
 * @param ex_handle 表征PF的设备指针
 * @param service_type 服务类型
 * @retval void
 * @author
 * @date 2019-5-4
 */
extern void cqm3_service_unregister(void *ex_handle, u32 service_type);

/**
 * @brief 业务驱动向CQM设置需要初始化的fake vf数量
 * @details 业务驱动向CQM设置需要初始化的fake vf数量
 * @param ex_handle 表征PF的设备指针
 * @param fake_vf_num_cfg 需要初始化的fake vf数量
 * @retval 0 成功
 * @retval -1 失败
 * @author
 * @date 2022-2-10
 */
extern s32 cqm3_fake_vf_num_set(void *ex_handle, u16 fake_vf_num_cfg);

/**
 * @brief 是否需要安全内存
 * @details 此接口用于判断是否需要申请安全内存
 * @param udkdev 表征PF/VF的设备指针
 * @retval true 支持
 * @retval false 不支持
 * @author
 * @date 2023-09-23
 */
extern bool cqm3_need_secure_mem(void *ex_handle);

/**
 * @brief 创建FC SRQ
 * @details 队列中有效wqe个数必须要满足传入的wqe个数。因为linkwqe只能填在页尾，真实有效个数超过需求，需要告知业务多创建的个数。
 * @param ex_handle 表征PF的设备指针
 * @param service_type 服务类型
 * @param object_type 对象类型
 * @param wqe_number wqe数目
 * @param wqe_size wqe大小
 * @param object_priv 对象私有信息指针
 * @retval cqm_queue_s 队列结构指针
 * @author
 * @date 2019-5-4
 */
extern cqm_queue_s *cqm3_object_fc_srq_create(void *ex_handle, u32 service_type,
	cqm_object_type_e object_type, u32 wqe_number, u32 wqe_size, void *object_priv);

/**
 * @brief 创建RQ
 * @details 在使用SRQ时,RQ队列创建
 * @param ex_handle 表征PF的设备指针
 * @param service_type 服务类型
 * @param object_type 对象类型
 * @param init_rq_num container数目
 * @param container_size container大小
 * @param wqe_size wqe大小
 * @param object_priv 对象私有信息指针
 * @retval cqm_queue_s 队列结构指针
 * @author
 * @date 2019-5-4
 */
cqm_queue_s *cqm3_object_recv_queue_create(void *ex_handle, u32 service_type,
	cqm_object_type_e object_type, u32 init_rq_num, u32 container_size, u32 wqe_size, void *object_priv);

/**
 * @brief 创建TOE业务的SRQ
 * @details 创建TOE业务的SRQ
 * @param ex_handle 表征PF的设备指针
 * @param service_type 服务类型
 * @param object_type 对象类型
 * @param container_number container数目
 * @param container_size container大小
 * @param wqe_size wqe大小
 * @retval cqm_queue_s 队列结构指针
 * @author
 * @date 2019-5-4
 */
cqm_queue_s *cqm3_object_share_recv_queue_create(void *ex_handle, u32 service_type,
	cqm_object_type_e object_type, u32 container_number, u32 container_size, u32 wqe_size);

/**
 * @brief 创建QPC和MPT
 * @details 创建QPC和MPT，此接口会休眠
 * @param ex_handle 表征PF的设备指针
 * @param service_type 服务类型
 * @param object_type 对象类型
 * @param object_size 对象大小，单位Byte
 * @param object_priv 服务层的私有结构，可以为NULL
 * @param index 根据该值申请预留的qpn，如果要自动分配需填入CQM_INDEX_INVALID
 * @retval cqm_qpc_mpt_s QPC/MPT结构指针
 * @author
 * @date 2019-5-4
 */
extern cqm_qpc_mpt_s *cqm3_object_qpc_mpt_create(void *ex_handle, u32 service_type,
	cqm_object_type_e object_type, u32 object_size, void *object_priv, u32 index,
	bool low2bit_align_en);

/**
 * @brief 创建非RDMA业务的队列
 * @details 创建非RDMA业务的队列，此接口会休眠
 * @param ex_handle 表征PF的设备指针
 * @param service_type 服务类型
 * @param object_type 对象类型
 * @param wqe_number 包含link wqe的数目
 * @param wqe_size 定长，大小为2^n
 * @param object_priv 服务层的私有结构，可以为NULL
 * @retval cqm_queue_s 队列结构指针
 * @author
 * @date 2019-5-4
 */
extern cqm_queue_s *cqm3_object_nonrdma_queue_create(void *ex_handle, u32 service_type,
	cqm_object_type_e object_type, u32 wqe_number, u32 wqe_size, void *object_priv);

/**
 * @brief 创建RDMA业务的队列
 * @details 创建RDMA业务的队列，此接口会休眠
 * @param ex_handle 表征PF的设备指针
 * @param service_type 服务类型
 * @param object_type 对象类型
 * @param object_size 对象大小
 * @param object_priv 服务层的私有结构，可以为NULL
 * @param room_header_alloc 是否要申请queue room和header空间
 * @retval cqm_queue_s 队列结构指针
 * @author
 * @date 2019-5-4
 */
extern cqm_queue_s *cqm3_object_rdma_queue_create(void *ex_handle, u32 service_type,
	cqm_object_type_e object_type, u32 object_size, void *object_priv, bool room_header_alloc, u32 xid);

/**
 * @brief 创建RDMA业务的mtt和rdmarc
 * @details 创建RDMA业务的mtt和rdmarc
 * @param ex_handle 表征PF的设备指针
 * @param service_type 服务类型
 * @param object_type 对象类型
 * @param index_base 起始index编号
 * @param index_number index数量
 * @retval cqm_mtt_rdmarc_s MTT/RDMARC结构指针
 * @author
 * @date 2019-5-4
 */
extern cqm_mtt_rdmarc_s *cqm3_object_rdma_table_get(void *ex_handle, u32 service_type,
	cqm_object_type_e object_type, u32 index_base, u32 index_number);

/**
 * @brief 根据index获得object
 * @details 根据index获得object
 * @param ex_handle 表征PF的设备指针
 * @param object_type 对象类型
 * @param index 支持qpn,mptn,scqn,srqn
 * @param bh 是否禁用中断下半部
 * @retval cqm_object_s *对象指针
 * @author
 * @date 2019-5-4
 */
extern cqm_object_s *cqm3_object_get(void *ex_handle, cqm_object_type_e object_type, u32 index, bool bh);

/**
 * @brief 申请一个cmd buffer
 * @details 申请一个cmd buffer，buffer大小固定2K，buffer内容没有清零，需要业务清零
 * @param ex_handle 表征PF的设备指针
 * @retval cqm_cmd_buf_s 命令buffer指针
 * @author
 * @date 2019-5-4
 */
extern cqm_cmd_buf_s *cqm3_cmd_alloc(void *ex_handle);

/**
 * @brief 释放一个cmd buffer
 * @details 释放一个cmd buffer
 * @param ex_handle 表征PF的设备指针
 * @param cmd_buf 待释放的cmd buffer指针
 * @retval void
 * @author
 * @date 2019-5-4
 */
extern void cqm3_cmd_free(void *ex_handle, cqm_cmd_buf_s *cmd_buf);

/**
 * @brief box方式发送一个cmd，该接口会挂完成量，造成休眠
 * @details box方式发送一个cmd，该接口会挂完成量，造成休眠
 * @param ex_handle 表征PF的设备指针
 * @param mod 发送cmd的模块
 * @param cmd 命令字
 * @param buf_in 输入命令buffer
 * @param buf_out 返回命令buffer
 * @param out_param 命令返回的inline data出参
 * @param timeout 命令超时时间，单位ms
 * @param channel 调用者channel id
 * @retval 0 成功
 * @retval !=0 失败
 * @author
 * @date 2019-5-4
 */
extern s32 cqm3_send_cmd_box(void *ex_handle, u8 mod, u8 cmd,
	cqm_cmd_buf_s *buf_in, cqm_cmd_buf_s *buf_out, u64 *out_param, u32 timeout,
	u16 channel);

/**
 * @brief box方式发送一个cmd，开放cos_id, 该接口会挂完成量，造成休眠
 * @details box方式发送一个cmd，该接口会挂完成量，造成休眠
 * @param ex_handle 表征PF的设备指针
 * @param mod 发送cmd的模块
 * @param cmd 命令字
 * @param cos_id
 * @param buf_in 输入命令buffer
 * @param buf_out 返回命令buffer
 * @param out_param 命令返回的inline data出参
 * @param timeout 命令超时时间，单位ms
 * @param channel 调用者channel id
 * @retval 0 成功
 * @retval !=0 失败
 * @author
 * @date 2020-4-11
 */
extern s32 cqm3_lb_send_cmd_box(void *ex_handle, u8 mod, u8 cmd, u8 cos_id,
	cqm_cmd_buf_s *buf_in, cqm_cmd_buf_s *buf_out, u64 *out_param, u32 timeout,
	u16 channel);

/**
 * @brief box方式发送一个cmd，开放cos_id, 该接口不会等待直接返回
 * @details box方式发送一个cmd，该接口不会等待直接返回
 * @param ex_handle 表征PF的设备指针
 * @param mod 发送cmd的模块
 * @param cmd 命令字
 * @param cos_id
 * @param buf_in 输入命令buffer
 * @param channel 调用者channel id
 * @retval 0 成功
 * @retval !=0 失败
 * @author
 * @date 2023-5-19
 */
extern s32 cqm3_lb_send_cmd_box_async(void *ex_handle, u8 mod,
	u8 cmd, u8 cos_id, struct tag_cqm_cmd_buf *buf_in, u16 channel);

/**
 * @brief imm方式发送一个cmd，该接口会挂完成量，造成休眠
 * @details imm方式发送一个cmd，该接口会挂完成量，造成休眠
 * @param ex_handle 表征PF的设备指针
 * @param mod 发送cmd的模块
 * @param cmd 命令字
 * @param buf_in 输入命令buffer
 * @param out_param 返回命令buffer
 * @param timeout 命令超时时间，单位ms
 * @param channel 调用者channel id
 * @retval 0 成功
 * @retval !=0 失败
 * @author
 * @date 2019-5-4
 */
extern s32 cqm3_send_cmd_imm(void *ex_handle, u8 mod, u8 cmd,
	cqm_cmd_buf_s *buf_in, u64 *out_param, u32 timeout, u16 channel);

/**
 * @brief 申请硬件doorbell和dwqe
 * @details 申请一页硬件doorbell和dwqe，具有相同的index，得到的均为物理地址，每个function最多有1K个
 * @param ex_handle 表征PF的设备指针
 * @param db_addr 一页doorbell的物理起始地址
 * @param dwqe_addr 一页dwqe的物理起始地址
 * @retval 0 成功
 * @retval !=0 失败
 * @author
 * @date 2019-5-4
 */
extern s32 cqm3_db_addr_alloc(void *ex_handle, void __iomem **db_addr, void __iomem **dwqe_addr);

/**
 * @brief 释放一页硬件doorbell和dwqe
 * @details 释放一页硬件doorbell和dwqe
 * @param ex_handle 表征PF的设备指针
 * @param db_addr 一页doorbell的物理起始地址
 * @param dwqe_addr 一页dwqe的物理起始地址
 * @retval void
 * @author
 * @date 2019-5-4
 */
extern void cqm3_db_addr_free(void *ex_handle, const void __iomem *db_addr, void __iomem *dwqe_addr);

extern void *cqm3_get_db_addr(void *ex_handle, u32 service_type);

/**
 * @brief 敲硬件doorbell
 * @details 敲硬件doorbell
 * @param ex_handle 表征PF的设备指针
 * @param service_type 每种内核态业务会分配一页硬件doorbell page
 * @param db_count doorbell中超出64b的PI[7:0]
 * @param db doorbell内容，由业务组织，如存在大小端转换，需要业务完成
 * @retval 0 成功
 * @retval -1 失败
 * @author
 * @date 2019-5-4
 */
extern s32 cqm3_ring_hardware_db(void *ex_handle, u32 service_type, u8 db_count, u64 db);

extern s32 cqm3_get_hardware_db_addr(void *ex_handle, u64 *addr,
									 u32 service_type);

extern s32 cqm_ring_hardware_db_fc(void *ex_handle, u32 service_type, u8 db_count, u8 pagenum, u64 db);

/**
 * @brief 提供敲db接口，由CQM完成pri到cos的转换
 * @details 提供敲db接口，由CQM完成pri到cos的转换。业务传进来的db需要是主机序，由该接口完成网络序转换
 * @param ex_handle 表征PF的设备指针
 * @param service_type 每种内核态业务会分配一页硬件doorbell page
 * @param db_count doorbell中超出64b的PI[7:0]
 * @param db doorbell内容，由业务组织，如存在大小端转换，需要业务完成
 * @retval 0 成功
 * @retval -1 失败
 * @author
 * @date 2019-5-4
 */
extern s32 cqm3_ring_hardware_db_update_pri(void *ex_handle, u32 service_type, u8 db_count, u64 db);

/**
 * @brief bloom filter的id增加引用计数
 * @details bloom filter的id增加引用计数，由0->1时发送API置位，此接口会休眠
 * @param ex_handle 表征PF的设备指针
 * @param id id
 * @retval 0 成功
 * @retval -1 失败
 * @author
 * @date 2019-5-4
 */
extern s32 cqm3_bloomfilter_inc(void *ex_handle, u16 func_id, u64 id);

/**
 * @brief bloom filter的id减少引用计数
 * @details bloom filter的id减少引用计数，减为0时发送API清零，此接口会休眠
 * @param ex_handle 表征PF的设备指针
 * @param id id
 * @retval 0 成功
 * @retval -1 失败
 * @author
 * @date 2019-5-4
 */
extern s32 cqm3_bloomfilter_dec(void *ex_handle, u16 func_id, u64 id);

/**
 * @brief 获取gid表的base虚拟地址
 * @details 获取gid表的base虚拟地址
 * @param ex_handle 表征PF的设备指针
 * @retval void *gid表的base虚拟地址
 * @author
 * @date 2019-5-4
 */
extern void *cqm3_gid_base(void *ex_handle);

/**
 * @brief 获取Timer的base虚拟地址
 * @details 获取Timer的base虚拟地址
 * @param ex_handle 表征PF的设备指针
 * @retval void *Timer的base虚拟地址
 * @author
 * @date 2020-5-21
 */
extern void *cqm3_timer_base(void *ex_handle);

/**
 * @brief 清零timer buffer
 * @details 根据funtion id进行对应timer buffer 清零。要求funtion id从0开始计算，且timer buffer按funtion id依次排列
 * @param ex_handle 表征PF的设备指针
 * @param function_id funtion id
 * @retval void
 * @author
 * @date 2019-5-4
 */
extern void cqm3_function_timer_clear(void *ex_handle, u32 function_id);

/**
 * @brief 清零hash buffer
 * @details 根据funtion id进行对应hash buffer 清零
 * @param ex_handle 表征PF的设备指针
 * @param global_funcid
 * @retval void
 * @author
 * @date 2019-5-4
 */
extern void cqm3_function_hash_buf_clear(void *ex_handle, s32 global_funcid);

extern s32 cqm3_ring_direct_wqe_db(void *ex_handle, u32 service_type, u8 db_count, void *direct_wqe);

extern s32 cqm_ring_direct_wqe_db_fc(void *ex_handle, u32 service_type, void *direct_wqe);

#endif

#ifndef HIUDK_ULD
/**
 * @brief srq申请新的container，创建后好挂链
 * @details srq申请新的container，创建后好挂链
 * @param common 队列结构指针
 * @retval 0 成功
 * @retval -1 失败
 * @author
 * @date 2019-5-4
 */
s32 cqm3_object_share_recv_queue_add_container(cqm_queue_s *common);

/**
 * @brief srq申请新的container，创建后不挂链，由业务完成挂链
 * @details srq申请新的container，创建后不挂链，由业务完成挂链
 * @param common 队列结构指针
 * @param container_addr 返回的container地址
 * @retval 0 成功
 * @retval -1 失败
 * @author
 * @date 2019-5-4
 */
s32 cqm3_object_srq_add_container_free(cqm_queue_s *common, u8 **container_addr);

/**
 * @brief 敲软件doorbell
 * @details 敲软件doorbell
 * @param object 对象指针
 * @param db_record 软件doorbell内容，如存在大小端转换，需要业务完成
 * @retval 0 成功
 * @retval -1 失败
 * @author
 * @date 2019-5-4
 */
extern s32 cqm3_ring_software_db(cqm_object_s *object, u64 db_record);

/**
 * @brief 对象引用计数释放
 * @details cqm_object_get函数调用以后要使用该接口put，否则object释放不了
 * @param object 对象指针
 * @retval void
 * @author
 * @date 2019-5-4
 */
extern void cqm3_object_put(cqm_object_s *object);

/**
 * @brief 获得对象的所在function ID
 * @details 获得对象的所在function ID
 * @param object 对象指针
 * @retval >=0 function的ID
 * @retval -1 失败
 * @author
 * @date 2020-4-15
 */
extern s32 cqm3_object_funcid(cqm_object_s *object);

/**
 * @brief 给对象申请一块新空间
 * @details 目前只对roce业务有用，调整CQ的buffer大小，但cqn和cqc不变，申请新的buffer空间，不释放旧buffer空间，当前有效buffer仍为旧buffer
 * @param object 对象指针
 * @param object_size 新buffer大小
 * @retval 0 成功
 * @retval -1 失败
 * @author
 * @date 2019-5-4
 */
extern s32 cqm3_object_resize_alloc_new(cqm_object_s *object, u32 object_size);

/**
 * @brief 给对象释放新申请buffer空间
 * @details 本函数释放新申请buffer空间，用于业务的异常处理分支
 * @param object 对象指针
 * @retval void
 * @author
 * @date 2019-5-4
 */
extern void cqm3_object_resize_free_new(cqm_object_s *object);

/**
 * @brief 给对象释旧buffer空间
 * @details 本函数释放旧的buffer，并将当前有效buffer设置为新buffer
 * @param object 对象指针
 * @retval void
 * @author
 * @date 2019-5-4
 */
extern void cqm3_object_resize_free_old(cqm_object_s *object);

/**
 * @brief 释放container
 * @details 释放container
 * @param object 对象指针
 * @param container 要释放的container指针
 * @retval void
 * @author
 * @date 2019-5-4
 */
extern void cqm3_srq_used_rq_container_delete(cqm_object_s *object, u8 *container);

/**
 * @brief 删除创建的对象
 * @details 删除创建的对象，该函数会休眠等待所有对该对象的操作完成才返回
 * @param object 对象指针
 * @retval void
 * @author
 * @date 2019-5-4
 */
extern void cqm3_object_delete(cqm_object_s *object);

/**
 * @brief 获得对象buffer指定偏移处的物理地址和虚拟地址
 * @details 仅支持rdma table查找，获得对象buffer指定偏移处的物理地址和虚拟地址
 * @param object 对象指针
 * @param offset 对于rdma table，offset为index绝对编号
 * @param paddr 仅对rdma table才返回物理地址
 * @retval u8 *buffer指定偏移处的虚拟地址
 * @author
 * @date 2019-5-4
 */
extern u8 *cqm3_object_offset_addr(cqm_object_s *object, u32 offset, dma_addr_t *paddr);

s32 cqm3_dtoe_share_recv_queue_create(void *ex_handle, u32 contex_size,
					 u32 *index_count, u32 *index);

void cqm3_dtoe_free_srq_bitmap_index(void *ex_handle, u32 index_count, u32 index);

#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* CQM_H */

