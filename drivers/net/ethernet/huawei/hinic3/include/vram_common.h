/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2022. All rights reserved.
 * Description: Header File, vram common
 * Create: 2023/7/19
 */
#ifndef VRAM_COMMON_H
#define VRAM_COMMON_H

#include <linux/pci.h>
#include <linux/notifier.h>

#define VRAM_BLOCK_SIZE_2M		0x200000UL
#define KEXEC_SIGN			"hinic-in-kexec"
// now vram_name max len is 14, when add other vram, attention this value
#define VRAM_NAME_MAX_LEN		16

#define VRAM_CQM_GLB_FUNC_BASE		"F"
#define VRAM_CQM_FAKE_MEM_BASE		"FK"
#define VRAM_CQM_CLA_BASE		"C"
#define VRAM_CQM_CLA_TYPE_BASE		"T"
#define VRAM_CQM_CLA_SMF_BASE		"SMF"
#define VRAM_CQM_CLA_COORD_X		"X"
#define VRAM_CQM_CLA_COORD_Y		"Y"
#define VRAM_CQM_CLA_COORD_Z		"Z"
#define VRAM_CQM_BITMAP_BASE		"B"

#define VRAM_NIC_DCB			"DCB"
#define VRAM_NIC_VRAM			"NIC_VRAM"

#define VRAM_VBS_BASE_IOCB		"BASE_IOCB"
#define VRAM_VBS_EX_IOCB		"EX_IOCB"
#define VRAM_VBS_RXQS_CQE		"RXQS_CQE"

#define VRAM_VBS_VOLQ_MTT		"VOLQ_MTT"
#define VRAM_VBS_VOLQ_MTT_PAGE		"MTT_PAGE"

#define VRAM_VROCE_ENTRY_POOL		"VROCE_ENTRY"
#define VRAM_VROCE_GROUP_POOL		"VROCE_GROUP"
#define VRAM_VROCE_UUID			"VROCE_UUID"
#define VRAM_VROCE_VID			"VROCE_VID"
#define VRAM_VROCE_BASE			"VROCE_BASE"
#define VRAM_VROCE_DSCP			"VROCE_DSCP"
#define VRAM_VROCE_QOS			"VROCE_QOS"
#define VRAM_VROCE_DEV			"VROCE_DEV"
#define VRAM_VROCE_RGROUP_HT_CNT	"RGROUP_CNT"
#define VRAM_VROCE_RACL_HT_CNT		"RACL_CNT"

#define VRAM_NAME_APPLY_LEN 64

#define MPU_OS_HOTREPLACE_FLAG          0x1
struct vram_buf_info {
	char buf_vram_name[VRAM_NAME_APPLY_LEN];
	int use_vram;
};

enum KUP_HOOK_POINT {
	PRE_FREEZE,
	FREEZE_TO_KILL,
	PRE_UPDATE_KERNEL,
	FLUSH_DURING_KUP,
	POST_UPDATE_KERNEL,
	UNFREEZE_TO_RUN,
	POST_RUN,
	KUP_HOOK_MAX,
};

typedef int (*register_nvwa_notifier_t)(int hook, struct notifier_block *nb);
typedef int (*unregister_nvwa_notifier_t)(int hook, struct notifier_block *nb);
typedef int (*register_euleros_reboot_notifier_t)(struct notifier_block *nb);
typedef int (*unregister_euleros_reboot_notifier_t)(struct notifier_block *nb);
typedef void __iomem *(*vram_kalloc_t)(char *name, u64 size);
typedef void (*vram_kfree_t)(void __iomem *vaddr, char *name, u64 size);
typedef gfp_t (*vram_get_gfp_vram)(void);

void lookup_vram_related_symbols(void);
int hi_register_nvwa_notifier(int hook, struct notifier_block *nb);
int hi_unregister_nvwa_notifier(int hook, struct notifier_block *nb);
int hi_register_euleros_reboot_notifier(struct notifier_block *nb);
int hi_unregister_euleros_reboot_notifier(struct notifier_block *nb);
void __iomem *hi_vram_kalloc(char *name, u64 size);
void hi_vram_kfree(void __iomem *vaddr, char *name, u64 size);
gfp_t hi_vram_get_gfp_vram(void);

int hi_set_kexec_status(int status);
int hi_get_kexec_status(void);

int get_use_vram_flag(void);
void set_use_vram_flag(bool flag);
int vram_get_kexec_flag(void);

#endif /* VRAM_COMMON_H */