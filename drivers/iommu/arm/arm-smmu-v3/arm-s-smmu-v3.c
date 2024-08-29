// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, The Linux Foundation. All rights reserved.
 */
#include <linux/crash_dump.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/pci.h>
#include <linux/hashtable.h>
#include <asm/kvm_tmi.h>

#include "arm-smmu-v3.h"
#include "arm-s-smmu-v3.h"
#include "../../dma-iommu.h"

struct cc_dev_config {
	u32	sid; /* BDF number of the device */
	u32	vmid;
	u32	root_bd; /* root bus and device number. Multiple sid can have the same root_bd. */
	bool secure;
	struct hlist_node node;
};

static bool g_s_smmu_id_map_init;

static DEFINE_HASHTABLE(g_cc_dev_htable, MAX_CC_DEV_NUM_ORDER);
static DECLARE_BITMAP(g_s_smmu_id_map, ARM_S_SMMU_MAX_IDS);

enum arm_s_smmu_msi_index {
	EVTQ_MSI_INDEX,
	GERROR_MSI_INDEX,
	PRIQ_MSI_INDEX,
	S_EVTQ_MSI_INDEX,
	S_GERROR_MSI_INDEX,
	ARM_S_SMMU_MAX_MSIS,
};

static phys_addr_t arm_s_smmu_msi_cfg[ARM_S_SMMU_MAX_MSIS][ARM_S_SMMU_MAX_CFGS] = {
	[S_EVTQ_MSI_INDEX] = {
		ARM_SMMU_S_EVTQ_IRQ_CFG0,
		ARM_SMMU_S_EVTQ_IRQ_CFG1,
		ARM_SMMU_S_EVTQ_IRQ_CFG2,
	},
	[S_GERROR_MSI_INDEX] = {
		ARM_SMMU_S_GERROR_IRQ_CFG0,
		ARM_SMMU_S_GERROR_IRQ_CFG1,
		ARM_SMMU_S_GERROR_IRQ_CFG2,
	},
};

static inline void arm_s_smmu_set_irq(struct arm_smmu_device *smmu)
{
	smmu->s_evtq_irq = msi_get_virq(smmu->dev, S_EVTQ_MSI_INDEX);
	smmu->s_gerr_irq = msi_get_virq(smmu->dev, S_GERROR_MSI_INDEX);
}

/* Traverse pcie topology to find the root <bus,device> number
 * return -1 if error
 * return -1 if not pci device
 */
int get_root_bd(struct device *dev)
{
	struct pci_dev *pdev;

	if (!dev_is_pci(dev))
		return -1;
	pdev = to_pci_dev(dev);
	if (pdev->bus == NULL)
		return -1;
	while (pdev->bus->parent != NULL)
		pdev = pdev->bus->self;

	return pci_dev_id(pdev) & MASK_DEV_FUNCTION;
}

/* if dev is a bridge, get all it's children.
 * if dev is a regular device, get itself.
 */
void get_child_devices_rec(struct pci_dev *dev, uint16_t *devs,
	int max_devs, int *ndev, uint16_t *nbind)
{
	struct pci_bus *bus = dev->subordinate;

	if (bus) { /* dev is a bridge */
		struct pci_dev *child;

		list_for_each_entry(child, &bus->devices, bus_list) {
			get_child_devices_rec(child, devs, max_devs, ndev, nbind);
		}
	} else { /* dev is a regular device */
		uint16_t bdf = pci_dev_id(dev);
		int i;
		/* check if bdf is already in devs */
		for (i = 0; i < *ndev; i++) {
			if (devs[i] == bdf)
				return;
		}
		/* check overflow */
		if (*ndev >= max_devs) {
			pr_warn("S_SMMU: devices num over max devs\n");
			return;
		}
		if (dev->driver && strcmp(dev->driver->name, "vfio-pci"))
			(*nbind)++;
		devs[*ndev] = bdf;
		*ndev = *ndev + 1;
	}
}

/* get all devices which share the same root_bd as dev
 * return 0 on failure
 */
int get_sibling_devices(struct device *dev, uint16_t *devs, int max_devs, uint16_t *nbind)
{
	int ndev = 0;
	struct pci_dev *pdev;

	if (!dev_is_pci(dev))
		return ndev;

	pdev = to_pci_dev(dev);
	if (pdev->bus == NULL)
		return ndev;

	while (pdev->bus->parent != NULL)
		pdev = pdev->bus->self;

	get_child_devices_rec(pdev, devs, max_devs, &ndev, nbind);
	return ndev;
}

int set_g_cc_dev(u32 sid, u32 vmid, u32 root_bd, bool secure)
{
	struct cc_dev_config *obj;

	hash_for_each_possible(g_cc_dev_htable, obj, node, sid) {
		if (obj->sid == sid) {
			obj->vmid = vmid;
			obj->root_bd = root_bd;
			obj->secure = secure;
			return 0;
		}
	}

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	obj->sid = sid;
	obj->vmid = vmid;
	obj->root_bd = root_bd;
	obj->secure = secure;

	hash_add(g_cc_dev_htable, &obj->node, sid);
	return 0;
}

void free_g_cc_dev_htable(void)
{
	int i;
	struct cc_dev_config *obj;
	struct hlist_node *tmp;

	if (!is_virtcca_cvm_enable())
		return;

	hash_for_each_safe(g_cc_dev_htable, i, tmp, obj, node) {
		hash_del(&obj->node);
		kfree(obj);
	}
}

/* Has the root bus device number switched to secure? */
bool is_cc_root_bd(u32 root_bd)
{
	int bkt;
	struct cc_dev_config *obj;

	hash_for_each(g_cc_dev_htable, bkt, obj, node) {
		if (obj->root_bd == root_bd && obj->secure)
			return true;
	}

	return false;
}

static bool is_cc_vmid(u32 vmid)
{
	int bkt;
	struct cc_dev_config *obj;

	hash_for_each(g_cc_dev_htable, bkt, obj, node) {
		if (vmid > 0 && obj->vmid == vmid)
			return true;
	}

	return false;
}

bool is_cc_dev(u32 sid)
{
	struct cc_dev_config *obj;

	hash_for_each_possible(g_cc_dev_htable, obj, node, sid) {
		if (obj != NULL && obj->sid == sid)
			return obj->secure;
	}

	return false;
}
EXPORT_SYMBOL(is_cc_dev);

void s_smmu_cmdq_need_forward(u64 cmd0, u64 cmd1, u64 *forward)
{
	u64 opcode = FIELD_GET(CMDQ_0_OP, cmd0);

	switch (opcode) {
	case CMDQ_OP_TLBI_EL2_ALL:
	case CMDQ_OP_TLBI_NSNH_ALL:
		*forward = 1;
		break;
	case CMDQ_OP_PREFETCH_CFG:
	case CMDQ_OP_CFGI_CD:
	case CMDQ_OP_CFGI_STE:
	case CMDQ_OP_CFGI_CD_ALL:
		*forward = (uint64_t)is_cc_dev(FIELD_GET(CMDQ_CFGI_0_SID, cmd0));
		break;

	case CMDQ_OP_CFGI_ALL:
		*forward = 1;
		break;
	case CMDQ_OP_TLBI_NH_VA:
	case CMDQ_OP_TLBI_S2_IPA:
	case CMDQ_OP_TLBI_NH_ASID:
	case CMDQ_OP_TLBI_S12_VMALL:
		*forward = (uint64_t)is_cc_vmid(FIELD_GET(CMDQ_TLBI_0_VMID, cmd0));
		break;
	case CMDQ_OP_TLBI_EL2_VA:
	case CMDQ_OP_TLBI_EL2_ASID:
		*forward = 0;
		break;
	case CMDQ_OP_ATC_INV:
		*forward = (uint64_t)is_cc_dev(FIELD_GET(CMDQ_ATC_0_SID, cmd0));
		break;
	case CMDQ_OP_PRI_RESP:
		*forward = (uint64_t)is_cc_dev(FIELD_GET(CMDQ_PRI_0_SID, cmd0));
		break;
	case CMDQ_OP_RESUME:
		*forward = (uint64_t)is_cc_dev(FIELD_GET(CMDQ_RESUME_0_SID, cmd0));
		break;
	case CMDQ_OP_CMD_SYNC:
		*forward = 0;
		break;
	default:
		*forward = 0;
	}
}

void virtcca_smmu_queue_write(struct arm_smmu_device *smmu, u64 *src, size_t n_dwords)
{
	u64 cmd0, cmd1;
	u64 forward = 0;

	if (!is_virtcca_cvm_enable())
		return;

	if (virtcca_smmu_enable(smmu)) {
		if (n_dwords == 2) {
			cmd0 = cpu_to_le64(src[0]);
			cmd1 = cpu_to_le64(src[1]);
			s_smmu_cmdq_need_forward(cmd0, cmd1, &forward);

			/* need forward queue command to TMM */
			if (forward) {
				if (tmi_smmu_queue_write(cmd0, cmd1, smmu->s_smmu_id))
					dev_err(smmu->dev, "S_SMMU: s queue write failed\n");
			}
		}
	}
}

void virtcca_smmu_cmdq_write_entries(struct arm_smmu_device *smmu,
	u64 *cmds, int n, bool sync, struct arm_smmu_ll_queue *llq,
	struct arm_smmu_queue *q)
{
	int i;

	if (!is_virtcca_cvm_enable())
		return;

	if (virtcca_smmu_enable(smmu)) {
		for (i = 0; i < n; ++i) {
			u64 *cmd = &cmds[i * CMDQ_ENT_DWORDS];

			virtcca_smmu_queue_write(smmu, cmd, CMDQ_ENT_DWORDS);
		}
	}

	if (sync) {
		u32 prod;
		u64 cmd_sync[CMDQ_ENT_DWORDS];
		struct arm_smmu_cmdq_ent ent = {
			.opcode = CMDQ_OP_CMD_SYNC,
		};

		prod = (Q_WRP(llq, llq->prod) | Q_IDX(llq, llq->prod)) + n;
		prod = Q_OVF(llq->prod) | Q_WRP(llq, prod) | Q_IDX(llq, prod);
		if (smmu->options & ARM_SMMU_OPT_MSIPOLL) {
			ent.sync.msiaddr = q->base_dma + Q_IDX(&q->llq, prod) *
				q->ent_dwords * 8;
		}
		memset(cmd_sync, 0, 1 << CMDQ_ENT_SZ_SHIFT);
		cmd_sync[0] |= FIELD_PREP(CMDQ_0_OP, ent.opcode);
		if (ent.sync.msiaddr) {
			cmd_sync[0] |= FIELD_PREP(CMDQ_SYNC_0_CS, CMDQ_SYNC_0_CS_IRQ);
			cmd_sync[1] |= ent.sync.msiaddr & CMDQ_SYNC_1_MSIADDR_MASK;
		} else {
			cmd_sync[0] |= FIELD_PREP(CMDQ_SYNC_0_CS, CMDQ_SYNC_0_CS_SEV);
		}
		cmd_sync[0] |= FIELD_PREP(CMDQ_SYNC_0_MSH, ARM_SMMU_SH_ISH);
		cmd_sync[0] |= FIELD_PREP(CMDQ_SYNC_0_MSIATTR, ARM_SMMU_MEMATTR_OIWB);
		virtcca_smmu_queue_write(smmu, cmd_sync, CMDQ_ENT_DWORDS);
	}
}

void arm_s_smmu_init_one_queue(struct arm_smmu_device *smmu,
							  struct arm_smmu_queue *q,
							  size_t dwords, const char *name)
{
	size_t qsz;
	struct tmi_smmu_queue_params *params_ptr = kzalloc(PAGE_SIZE, GFP_KERNEL_ACCOUNT);

	if (!params_ptr) {
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		return;
	}

	qsz = ((1 << q->llq.max_n_shift) * dwords) << 3;
	if (!strcmp(name, "cmdq")) {
		params_ptr->ns_src = q->base_dma;
		params_ptr->smmu_base_addr = smmu->ioaddr;
		params_ptr->size = qsz;
		params_ptr->smmu_id = smmu->s_smmu_id;
		params_ptr->type = TMI_SMMU_CMD_QUEUE;
		tmi_smmu_queue_create(__pa(params_ptr));
	}

	if (!strcmp(name, "evtq")) {
		params_ptr->ns_src = q->base_dma;
		params_ptr->smmu_base_addr = smmu->ioaddr;
		params_ptr->size = qsz;
		params_ptr->smmu_id = smmu->s_smmu_id;
		params_ptr->type = TMI_SMMU_EVT_QUEUE;
		tmi_smmu_queue_create(__pa(params_ptr));
	}

	kfree(params_ptr);
}

int arm_s_smmu_write_reg_sync(struct arm_smmu_device *smmu, u32 val, u32 cmp_val,
				   unsigned int reg_off, unsigned int ack_off)
{
	u32 reg;

	if (tmi_smmu_write(smmu->ioaddr, reg_off, val, 32))
		return -ENXIO;

	return virtcca_cvm_read_poll_timeout_atomic(tmi_smmu_read, reg, reg == cmp_val,
				       1, ARM_SMMU_POLL_TIMEOUT_US, false,
				       smmu->ioaddr, ack_off, 32);
}

int arm_s_smmu_update_gbpa(struct arm_smmu_device *smmu, u32 set, u32 clr)
{
	int ret;
	u32 reg;

	ret = virtcca_cvm_read_poll_timeout_atomic(tmi_smmu_read, reg, !(reg & S_GBPA_UPDATE),
				       1, ARM_SMMU_POLL_TIMEOUT_US, false,
				       smmu->ioaddr, ARM_SMMU_S_GBPA, 32);
	if (ret)
		return ret;

	reg &= ~clr;
	reg |= set;

	ret = tmi_smmu_write(smmu->ioaddr, ARM_SMMU_S_GBPA, reg | S_GBPA_UPDATE, 32);
	if (ret)
		return ret;

	ret = virtcca_cvm_read_poll_timeout_atomic(tmi_smmu_read, reg, !(reg & S_GBPA_UPDATE),
			1, ARM_SMMU_POLL_TIMEOUT_US, false,
			smmu->ioaddr, ARM_SMMU_S_GBPA, 32);
	if (ret)
		dev_err(smmu->dev, "S_SMMU: s_gbpa not responding to update\n");
	return ret;
}

int arm_s_smmu_device_disable(struct arm_smmu_device *smmu)
{
	int ret = 0;

	ret = arm_s_smmu_write_reg_sync(smmu, 0, 0, ARM_SMMU_S_CR0, ARM_SMMU_S_CR0ACK);
	if (ret) {
		dev_err(smmu->dev, "S_SMMU: failed to clear s_cr0\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		return ret;
	}
	return 0;
}

irqreturn_t arm_s_smmu_evtq_thread(int irq, void *dev)
{
	struct arm_smmu_device *smmu = dev;

	if (virtcca_smmu_enable(smmu))
		tmi_handle_s_evtq(smmu->s_smmu_id);

	return IRQ_HANDLED;
}

irqreturn_t arm_s_smmu_gerror_handler(int irq, void *dev)
{
	u32 gerror, gerrorn, active;
	u64 ret;
	struct arm_smmu_device *smmu = dev;

	ret = tmi_smmu_read(smmu->ioaddr, ARM_SMMU_S_GERROR, 32);
	if (ret >> 32) {
		dev_err(smmu->dev, "S_SMMU: get arm_smmu_s_gerror register failed\n");
		return IRQ_NONE;
	}
	gerror = (u32)ret;

	ret = tmi_smmu_read(smmu->ioaddr, ARM_SMMU_S_GERRORN, 32);
	if (ret >> 32) {
		dev_err(smmu->dev, "S_SMMU: get arm_smmu_s_gerror register failed\n");
		return IRQ_NONE;
	}
	gerrorn = (u32)ret;

	active = gerror ^ gerrorn;
	if (!(active & GERROR_ERR_MASK))
		return IRQ_NONE; /* No errors pending */

	dev_warn(smmu->dev,
		 "S_SMMU: unexpected secure global error reported, this could be serious, active %x\n",
		 active);

	if (active & GERROR_SFM_ERR) {
		dev_err(smmu->dev, "S_SMMU: device has entered service failure mode!\n");
		arm_s_smmu_device_disable(smmu);
	}

	if (active & GERROR_MSI_GERROR_ABT_ERR)
		dev_warn(smmu->dev, "S_SMMU: gerror msi write aborted\n");

	if (active & GERROR_MSI_PRIQ_ABT_ERR)
		dev_warn(smmu->dev, "S_SMMU: priq msi write aborted\n");

	if (active & GERROR_MSI_EVTQ_ABT_ERR)
		dev_warn(smmu->dev, "S_SMMU: evtq msi write aborted\n");

	if (active & GERROR_MSI_CMDQ_ABT_ERR)
		dev_warn(smmu->dev, "S_SMMU: cmdq msi write aborted\n");

	if (active & GERROR_PRIQ_ABT_ERR)
		dev_err(smmu->dev, "S_SMMU: priq write aborted -- events may have been lost\n");

	if (active & GERROR_EVTQ_ABT_ERR)
		dev_err(smmu->dev, "S_SMMU: evtq write aborted -- events may have been lost\n");

	if (active & GERROR_CMDQ_ERR)
		dev_warn(smmu->dev, "S_SMMU: cmdq err\n");

	if (tmi_smmu_write(smmu->ioaddr, ARM_SMMU_S_GERRORN, gerror, 32)) {
		dev_err(smmu->dev, "S_SMMU: write arm_smmu_s_gerrorn failed\n");
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

void arm_s_smmu_disable_irq(struct arm_smmu_device *smmu)
{
	int ret;

	ret = arm_s_smmu_write_reg_sync(smmu, 0, 0,
		ARM_SMMU_S_IRQ_CTRL, ARM_SMMU_S_IRQ_CTRLACK);
	if (ret) {
		dev_err(smmu->dev, "S_SMMU: failed to disable secure irqs\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
	}
}

void arm_s_smmu_enable_irq(struct arm_smmu_device *smmu, u32 irqen_flags)
{
	int ret;

	ret = arm_s_smmu_write_reg_sync(smmu, irqen_flags,
		irqen_flags, ARM_SMMU_S_IRQ_CTRL, ARM_SMMU_S_IRQ_CTRLACK);
	if (ret) {
		dev_err(smmu->dev, "S_SMMU: failed to enable irq for secure evtq\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
	}
}

void platform_get_s_irq_byname_optional(struct platform_device *pdev, struct arm_smmu_device *smmu)
{
	int irq;

	irq = platform_get_irq_byname_optional(pdev, "s_eventq");
	if (irq > 0)
		smmu->s_evtq_irq = irq;

	irq = platform_get_irq_byname_optional(pdev, "s_gerror");
	if (irq > 0)
		smmu->s_gerr_irq = irq;
}

u32 arm_smmu_tmi_dev_attach(struct arm_smmu_domain *arm_smmu_domain,
	struct kvm *kvm)
{
	int ret = 0;
	u64 cmd[CMDQ_ENT_DWORDS] = {0};
	unsigned long flags;
	int i, j;
	struct arm_smmu_master *master;
	struct virtcca_cvm *virtcca_cvm = (struct virtcca_cvm *)kvm->arch.virtcca_cvm;

	spin_lock_irqsave(&arm_smmu_domain->devices_lock, flags);
	list_for_each_entry(master, &arm_smmu_domain->devices, domain_head) {
		if (master && master->num_streams >= 0) {
			for (i = 0; i < master->num_streams; ++i) {
				u32 sid = master->streams[i].id;

				for (j = 0; j < i; j++)
					if (master->streams[j].id == sid)
						break;
				if (j < i)
					continue;
				ret = tmi_dev_attach(sid, virtcca_cvm->rd,
					arm_smmu_domain->smmu->s_smmu_id);
				if (ret) {
					dev_err(arm_smmu_domain->smmu->dev, "S_SMMU: dev protected failed!\n");
					ret = -ENXIO;
					goto out;
				}
				cmd[0] |= FIELD_PREP(CMDQ_0_OP, CMDQ_OP_CFGI_STE);
				cmd[0] |= FIELD_PREP(CMDQ_CFGI_0_SID, sid);
				cmd[1] |= FIELD_PREP(CMDQ_CFGI_1_LEAF, true);
				tmi_smmu_queue_write(cmd[0], cmd[1],
					arm_smmu_domain->smmu->s_smmu_id);
			}
		}
	}

out:
	spin_unlock_irqrestore(&arm_smmu_domain->devices_lock, flags);
	return ret;
}

int virtcca_smmu_secure_dev_ste_create(struct arm_smmu_device *smmu,
	struct arm_smmu_master *master, u32 sid)
{
	struct tmi_smmu_ste_params *params_ptr;
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;
	struct arm_smmu_strtab_l1_desc *desc = &cfg->l1_desc[sid >> STRTAB_SPLIT];

	params_ptr = kzalloc(PAGE_SIZE, GFP_KERNEL_ACCOUNT);
	if (!params_ptr)
		return -ENOMEM;

	/* Sync Level 2 STE to TMM */
	params_ptr->ns_src = desc->l2ptr_dma + ((sid & ((1 << STRTAB_SPLIT) - 1)) * STE_ENTRY_SIZE);
	params_ptr->sid = sid;
	params_ptr->smmu_id = smmu->s_smmu_id;

	if (tmi_smmu_ste_create(__pa(params_ptr)) != 0) {
		dev_err(smmu->dev, "S_SMMU: failed to create ste level 2\n");
		return -EINVAL;
	}

	kfree(params_ptr);

	return 0;
}

int virtcca_smmu_secure_set_dev(struct arm_smmu_domain *smmu_domain, struct arm_smmu_master *master,
	struct device *dev)
{
	int i, j;
	u64 ret = 0;
	uint16_t nbind = 0;
	uint16_t root_bd = get_root_bd(dev);
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	if (!is_cc_root_bd(root_bd)) {
		struct tmi_dev_delegate_params *params = kzalloc(
			sizeof(struct tmi_dev_delegate_params), GFP_KERNEL);

		params->root_bd = root_bd;
		params->num_dev = get_sibling_devices(dev, params->devs, MAX_DEV_PER_PORT, &nbind);
		if (params->num_dev >= MAX_DEV_PER_PORT || nbind) {
			kfree(params);
			return -EINVAL;
		}
		dev_info(smmu->dev, "S_SMMU: Delegate %d devices as %02x:%02x to secure\n",
				params->num_dev, root_bd >> DEV_BUS_NUM,
				(root_bd & MASK_DEV_BUS) >> DEV_FUNCTION_NUM);
		ret = tmi_dev_delegate(__pa(params));
		if (!ret) {
			for (i = 0; i < params->num_dev; i++) {
				ret = set_g_cc_dev(params->devs[i], 0, root_bd, true);
				if (ret)
					break;
			}
		}
		kfree(params);
	}

	if (ret) {
		dev_err(smmu->dev, "S_SMMU: failed to delegate device to secure\n");
		return ret;
	}

	for (i = 0; i < master->num_streams; ++i) {
		u32 sid = master->streams[i].id;

		for (j = 0; j < i; j++)
			if (master->streams[j].id == sid)
				break;
		if (j < i)
			continue;
		if (!is_cc_dev(sid)) {
			dev_err(smmu->dev, "S_SMMU: sid is not cc dev\n");
			return -EINVAL;
		}
		ret = set_g_cc_dev(sid, smmu_domain->s2_cfg.vmid, root_bd, true);
		if (ret)
			break;
	}

	return ret;
}

bool virtcca_smmu_write_msi_msg(struct msi_desc *desc, struct msi_msg *msg)
{
	phys_addr_t doorbell;

	if (!is_virtcca_cvm_enable())
		return false;

	if (desc->msi_index == S_EVTQ_MSI_INDEX ||
		desc->msi_index == S_GERROR_MSI_INDEX) {
		struct device *dev = msi_desc_to_dev(desc);
		struct arm_smmu_device *smmu = dev_get_drvdata(dev);
		phys_addr_t *cfg = arm_s_smmu_msi_cfg[desc->msi_index];

		doorbell = (((u64)msg->address_hi) << 32) | msg->address_lo;
		doorbell &= MSI_CFG0_ADDR_MASK;
		tmi_smmu_write((u64)smmu->ioaddr, cfg[0], doorbell, 64);
		tmi_smmu_write((u64)smmu->ioaddr, cfg[1], msg->data, 32);
		tmi_smmu_write((u64)smmu->ioaddr, cfg[2], ARM_SMMU_MEMATTR_DEVICE_nGnRE, 32);
		return true;
	}
	return false;
}

static void arm_s_smmu_setup_msis(struct arm_smmu_device *smmu)
{
	int ret;
	struct device *dev = smmu->dev;

	arm_s_smmu_set_irq_cfg(smmu);

	if (!(smmu->features & ARM_SMMU_FEAT_MSI))
		return;

	if (!dev->msi.domain) {
		dev_info(smmu->dev, "S_SMMU: msi_domain absent - falling back to wired irqs\n");
		return;
	}

	/* Allocate MSIs for s_evtq, s_gerror. Ignore cmdq */
	ret = platform_msi_domain_alloc_range_irqs(dev, S_EVTQ_MSI_INDEX,
		S_GERROR_MSI_INDEX, _arm_smmu_write_msi_msg);
	if (ret) {
		dev_warn(dev, "S_SMMU: failed to allocate msis - falling back to wired irqs\n");
		return;
	}

	arm_s_smmu_set_irq(smmu);
}

static void arm_s_smmu_setup_unique_irqs(struct arm_smmu_device *smmu, bool resume)
{
	int irq, ret;

	if (!resume)
		arm_s_smmu_setup_msis(smmu);

	irq = smmu->s_evtq_irq;
	if (virtcca_smmu_enable(smmu) && irq) {
		ret = devm_request_threaded_irq(smmu->dev, irq, NULL,
						arm_s_smmu_evtq_thread,
						IRQF_ONESHOT,
						"arm-smmu-v3-s_evtq", smmu);
		if (ret < 0) {
			smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
			dev_warn(smmu->dev, "S_SMMU: failed to enable s_evtq irq\n");
		}
	}

	irq = smmu->s_gerr_irq;
	if (virtcca_smmu_enable(smmu) && irq) {
		ret = devm_request_irq(smmu->dev, irq, arm_s_smmu_gerror_handler,
				       0, "arm-smmu-v3-s_gerror", smmu);
		if (ret < 0) {
			smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
			dev_warn(smmu->dev, "S_SMMU: failed to enable s_gerror irq\n");
		}
	}
}

static int virtcca_smmu_write_reg_sync(struct arm_smmu_device *smmu, u32 val,
				   unsigned int reg_off, unsigned int ack_off)
{
	u32 reg;

	writel_relaxed(val, smmu->base + reg_off);
	return readl_relaxed_poll_timeout(smmu->base + ack_off, reg, reg == val,
					  1, ARM_SMMU_POLL_TIMEOUT_US);
}

static void arm_s_smmu_setup_irqs(struct arm_smmu_device *smmu, bool resume)
{
	int irq, ret;
	u32 irqen_flags = IRQ_CTRL_EVTQ_IRQEN | IRQ_CTRL_GERROR_IRQEN;

	ret = virtcca_smmu_write_reg_sync(smmu, 0, ARM_SMMU_IRQ_CTRL,
				      ARM_SMMU_IRQ_CTRLACK);
	if (ret) {
		dev_err(smmu->dev, "S_SMMU: failed to disable irqs\n");
		return;
	}
	/* Disable IRQs first */
	arm_s_smmu_disable_irq(smmu);

	irq = smmu->combined_irq;
	if (!irq)
		arm_s_smmu_setup_unique_irqs(smmu, resume);

	if (smmu->features & ARM_SMMU_FEAT_PRI)
		irqen_flags |= IRQ_CTRL_PRIQ_IRQEN;

	/* Enable interrupt generation on the SMMU */
	ret = virtcca_smmu_write_reg_sync(smmu, irqen_flags,
				      ARM_SMMU_IRQ_CTRL, ARM_SMMU_IRQ_CTRLACK);
	if (ret)
		dev_warn(smmu->dev, "S_SMMU: failed to enable irqs\n");

	arm_s_smmu_enable_irq(smmu, IRQ_CTRL_EVTQ_IRQEN | IRQ_CTRL_GERROR_IRQEN);
}

int arm_s_smmu_id_alloc(void)
{
	int idx;

	do {
		idx = find_first_zero_bit(g_s_smmu_id_map, ARM_S_SMMU_MAX_IDS);
		if (idx == ARM_S_SMMU_MAX_IDS) {
			pr_warn("S_SMMU: s_smmu_id over than max ids\n");
			return ARM_S_SMMU_INVALID_ID;
		}
	} while (test_and_set_bit(idx, g_s_smmu_id_map));

	return idx;
}

void arm_s_smmu_id_free(int idx)
{
	if (idx != ARM_S_SMMU_INVALID_ID)
		clear_bit(idx, g_s_smmu_id_map);
}

bool arm_s_smmu_map_init(struct arm_smmu_device *smmu, resource_size_t ioaddr)
{
	if (!g_s_smmu_id_map_init) {
		set_bit(0, g_s_smmu_id_map);
		hash_init(g_cc_dev_htable);
		g_s_smmu_id_map_init = true;
	}
	smmu->ioaddr = ioaddr;

	if (tmi_smmu_pcie_core_check(ioaddr)) {
		smmu->s_smmu_id = arm_s_smmu_id_alloc();
		return true;
	}

	smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
	return false;
}

void arm_s_smmu_device_enable(struct arm_smmu_device *smmu,
	u32 enables, bool bypass, bool disable_bypass)
{
	int ret = 0;

	/* Enable the SMMU interface, or ensure bypass */
	if (!bypass || disable_bypass) {
		enables |= CR0_SMMUEN;
	} else {
		ret = arm_s_smmu_update_gbpa(smmu, 0, S_GBPA_ABORT);
		if (ret) {
			dev_err(smmu->dev, "S_SMMU: failed to update s gbpa!\n");
			smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
			return;
		}
	}
	/* Mask BIT1 and BIT4 which are RES0 in SMMU_S_CRO */
	ret = arm_s_smmu_write_reg_sync(smmu, enables & ~SMMU_S_CR0_RESERVED,
		enables & ~SMMU_S_CR0_RESERVED, ARM_SMMU_S_CR0, ARM_SMMU_S_CR0ACK);

	if (ret) {
		dev_err(smmu->dev, "S_SMMU: failed to enable s smmu!\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
	}

	dev_info(smmu->dev, "S_SMMU: secure smmu id:%lld init end!\n", smmu->s_smmu_id);
}

bool arm_s_smmu_idr1_support_secure(struct arm_smmu_device *smmu)
{
	u64 rv;

	rv = tmi_smmu_read(smmu->ioaddr, ARM_SMMU_S_IDR1, 32);
	if (rv >> 32) {
		dev_err(smmu->dev, "S_SMMU: get arm_smmu_s_idr1 register failed!\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		return false;
	}

	if (!(rv & S_IDR1_SECURE_IMPL)) {
		dev_err(smmu->dev, "S_SMMU: does not implement secure state!\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		return false;
	}

	if (!(rv & S_IDR1_SEL2)) {
		dev_err(smmu->dev, "S_SMMU: secure stage2 translation not supported!\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		return false;
	}
	dev_info(smmu->dev, "S_SMMU: secure smmu id:%lld start init!\n", smmu->s_smmu_id);
	return true;
}

int virtcca_smmu_secure_dev_operator(struct iommu_domain *domain,
	struct arm_smmu_device *smmu, struct arm_smmu_master *master, struct device *dev)
{
	int i, j;
	int ret;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);

	if (!is_virtcca_cvm_enable())
		return 0;

	if (!domain->secure)
		return 0;

	if (!virtcca_smmu_enable(smmu)) {
		dev_err(smmu->dev, "S_SMMU: security smmu not initialized for the device\n");
		return -EINVAL;
	}

	ret = virtcca_smmu_secure_set_dev(smmu_domain, master, dev);
	if (ret)
		return ret;

	for (i = 0; i < master->num_streams; i++) {
		u32 sid = master->streams[i].id;
		/* Bridged PCI devices may end up with duplicated IDs */
		for (j = 0; j < i; j++)
			if (master->streams[j].id == sid)
				break;
		if (j < i)
			continue;
		if (virtcca_smmu_secure_dev_ste_create(smmu, master, sid))
			return -ENOMEM;
	}

	dev_info(smmu->dev, "S_SMMU: attach confidential dev: %s", dev_name(dev));

	return ret;
}

void virtcca_smmu_device_init(struct platform_device *pdev,
	struct arm_smmu_device *smmu, resource_size_t ioaddr,
	bool resume, bool disable_bypass)
{
	u64 rv;
	int ret, irq;
	u32 reg, enables;
	struct tmi_smmu_cfg_params *params_ptr;

	if (!is_virtcca_cvm_enable())
		return;

	if (!arm_s_smmu_map_init(smmu, ioaddr))
		return;

	if (!virtcca_smmu_enable(smmu) || !arm_s_smmu_idr1_support_secure(smmu))
		return;

	irq = platform_get_irq_byname_optional(pdev, "combined");
	if (irq <= 0)
		platform_get_s_irq_byname_optional(pdev, smmu);

	arm_s_smmu_init_one_queue(smmu, &smmu->cmdq.q, CMDQ_ENT_DWORDS, "cmdq");

	arm_s_smmu_init_one_queue(smmu, &smmu->evtq.q, EVTQ_ENT_DWORDS, "evtq");

	rv = tmi_smmu_read(smmu->ioaddr, ARM_SMMU_S_CR0, 32);
	if (rv >> 32) {
		dev_err(smmu->dev, "S_SMMU: failed to read s_cr0\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		return;
	}

	ret = (int)rv;
	if (ret & S_CR0_SMMUEN) {
		dev_warn(smmu->dev, "S_SMMU: secure smmu currently enabled! resetting...\n");
		arm_s_smmu_update_gbpa(smmu, S_GBPA_ABORT, 0);
	}

	ret = arm_s_smmu_device_disable(smmu);
	if (ret) {
		dev_err(smmu->dev, "S_SMMU: failed to disable s smmu\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		return;
	}

	/* CR1 (table and queue memory attributes) */
	reg = FIELD_PREP(CR1_TABLE_SH, ARM_SMMU_SH_ISH) |
	      FIELD_PREP(CR1_TABLE_OC, CR1_CACHE_WB) |
	      FIELD_PREP(CR1_TABLE_IC, CR1_CACHE_WB) |
	      FIELD_PREP(CR1_QUEUE_SH, ARM_SMMU_SH_ISH) |
	      FIELD_PREP(CR1_QUEUE_OC, CR1_CACHE_WB) |
	      FIELD_PREP(CR1_QUEUE_IC, CR1_CACHE_WB);

	ret = tmi_smmu_write(smmu->ioaddr, ARM_SMMU_S_CR1, reg, 32);
	if (ret) {
		dev_err(smmu->dev, "S_SMMU: failed to write s_cr1\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		return;
	}

	/* CR2 (random crap) */
	reg = CR2_PTM | CR2_RECINVSID;

	if (smmu->features & ARM_SMMU_FEAT_E2H)
		reg |= CR2_E2H;

	ret = tmi_smmu_write(smmu->ioaddr, ARM_SMMU_S_CR2, reg, 32);
	if (ret) {
		dev_err(smmu->dev, "S_SMMU: failed to write s_cr2\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		return;
	}

	params_ptr = kzalloc(PAGE_SIZE, GFP_KERNEL_ACCOUNT);
	if (!params_ptr) {
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		return;
	}

	params_ptr->is_cmd_queue = 1;
	params_ptr->smmu_id = smmu->s_smmu_id;
	params_ptr->ioaddr = smmu->ioaddr;
	params_ptr->strtab_base_RA_bit =
		(smmu->strtab_cfg.strtab_base >> S_STRTAB_BASE_RA_SHIFT) & 0x1;
	params_ptr->q_base_RA_WA_bit =
		(smmu->cmdq.q.q_base >> S_CMDQ_BASE_RA_SHIFT) & 0x1;
	if (tmi_smmu_device_reset(__pa(params_ptr)) != 0) {
		dev_err(smmu->dev, "S_SMMU: failed to set s cmd queue regs\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		kfree(params_ptr);
		return;
	}

	enables = CR0_CMDQEN;
	ret = arm_s_smmu_write_reg_sync(smmu, enables, enables, ARM_SMMU_S_CR0,
				      ARM_SMMU_S_CR0ACK);
	if (ret) {
		dev_err(smmu->dev, "S_SMMU: failed to enable secure command queue\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		kfree(params_ptr);
		return;
	}

	enables |= CR0_EVTQEN;

	/* Secure event queue */
	memset(params_ptr, 0, sizeof(struct tmi_smmu_ste_params));
	params_ptr->is_cmd_queue = 0;
	params_ptr->ioaddr = smmu->ioaddr;
	params_ptr->smmu_id = smmu->s_smmu_id;
	params_ptr->q_base_RA_WA_bit =
		 (smmu->evtq.q.q_base >> S_EVTQ_BASE_WA_SHIFT) & 0x1;
	params_ptr->strtab_base_RA_bit =
		(smmu->strtab_cfg.strtab_base >> S_STRTAB_BASE_RA_SHIFT) & 0x1;
	if (tmi_smmu_device_reset(__pa(params_ptr)) != 0) {
		dev_err(smmu->dev, "S_SMMU: failed to set s event queue regs\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		kfree(params_ptr);
		return;
	}
	kfree(params_ptr);

	/* Enable secure eventq */
	ret = arm_s_smmu_write_reg_sync(smmu, enables, enables, ARM_SMMU_S_CR0,
					ARM_SMMU_S_CR0ACK);
	if (ret) {
		dev_err(smmu->dev, "S_SMMU: failed to disable secure event queue\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		return;
	}

	ret = arm_s_smmu_write_reg_sync(smmu, SMMU_S_INIT_INV_ALL, 0,
		ARM_SMMU_S_INIT, ARM_SMMU_S_INIT);
	if (ret) {
		dev_err(smmu->dev, "S_SMMU: failed to write s_init\n");
		smmu->s_smmu_id = ARM_S_SMMU_INVALID_ID;
		return;
	}

	arm_s_smmu_setup_irqs(smmu, resume);

	/* Enable the Secure SMMU interface, or ensure bypass */
	arm_s_smmu_device_enable(smmu, enables, smmu->bypass, disable_bypass);
}
