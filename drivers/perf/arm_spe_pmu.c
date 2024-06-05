// SPDX-License-Identifier: GPL-2.0-only
/*
 * Perf support for the Statistical Profiling Extension, introduced as
 * part of ARMv8.2.
 *
 * Copyright (C) 2016 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#define PMUNAME					"arm_spe"
#define DRVNAME					PMUNAME "_pmu"
#define pr_fmt(fmt)				DRVNAME ": " fmt

#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/capability.h>
#include <linux/cpuhotplug.h>
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/perf_event.h>
#include <linux/perf/arm_pmu.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>

#include <asm/barrier.h>
#include <asm/cpufeature.h>
#include <asm/mmu.h>
#include <asm/sysreg.h>

#include "../arm/spe/spe.h"

/*
 * Cache if the event is allowed to trace Context information.
 * This allows us to perform the check, i.e, perfmon_capable(),
 * in the context of the event owner, once, during the event_init().
 */
#define SPE_PMU_HW_FLAGS_CX			BIT(0)

static void set_spe_event_has_cx(struct perf_event *event)
{
	if (IS_ENABLED(CONFIG_PID_IN_CONTEXTIDR) && perfmon_capable())
		event->hw.flags |= SPE_PMU_HW_FLAGS_CX;
}

static bool get_spe_event_has_cx(struct perf_event *event)
{
	return !!(event->hw.flags & SPE_PMU_HW_FLAGS_CX);
}

struct arm_spe_pmu_buf {
	int					nr_pages;
	bool					snapshot;
	void					*base;
};

struct arm_spe_pmu {
	struct pmu				pmu;
	struct platform_device			*pdev;
	cpumask_t				supported_cpus;
	struct hlist_node			hotplug_node;

	int					irq; /* PPI */
	u16					pmsver;
	u16					min_period;
	u16					counter_sz;

	u64					features;

	u16					max_record_sz;
	u16					align;
	struct perf_output_handle __percpu	*handle;
};

#define to_spe_pmu(p) (container_of(p, struct arm_spe_pmu, pmu))

/* Convert a free-running index from perf into an SPE buffer offset */
#define PERF_IDX2OFF(idx, buf)	((idx) % ((buf)->nr_pages << PAGE_SHIFT))

/* This sysfs gunk was really good fun to write. */
enum arm_spe_pmu_capabilities {
	SPE_PMU_CAP_ARCH_INST = 0,
	SPE_PMU_CAP_ERND,
	SPE_PMU_CAP_FEAT_MAX,
	SPE_PMU_CAP_CNT_SZ = SPE_PMU_CAP_FEAT_MAX,
	SPE_PMU_CAP_MIN_IVAL,
};

static int arm_spe_pmu_feat_caps[SPE_PMU_CAP_FEAT_MAX] = {
	[SPE_PMU_CAP_ARCH_INST]	= SPE_PMU_FEAT_ARCH_INST,
	[SPE_PMU_CAP_ERND]	= SPE_PMU_FEAT_ERND,
};

static u32 arm_spe_pmu_cap_get(struct arm_spe_pmu *spe_pmu, int cap)
{
	if (cap < SPE_PMU_CAP_FEAT_MAX)
		return !!(spe_pmu->features & arm_spe_pmu_feat_caps[cap]);

	switch (cap) {
	case SPE_PMU_CAP_CNT_SZ:
		return spe_pmu->counter_sz;
	case SPE_PMU_CAP_MIN_IVAL:
		return spe_pmu->min_period;
	default:
		WARN(1, "unknown cap %d\n", cap);
	}

	return 0;
}

static ssize_t arm_spe_pmu_cap_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct arm_spe_pmu *spe_pmu = dev_get_drvdata(dev);
	struct dev_ext_attribute *ea =
		container_of(attr, struct dev_ext_attribute, attr);
	int cap = (long)ea->var;

	return snprintf(buf, PAGE_SIZE, "%u\n",
		arm_spe_pmu_cap_get(spe_pmu, cap));
}

#define SPE_EXT_ATTR_ENTRY(_name, _func, _var)				\
	&((struct dev_ext_attribute[]) {				\
		{ __ATTR(_name, S_IRUGO, _func, NULL), (void *)_var }	\
	})[0].attr.attr

#define SPE_CAP_EXT_ATTR_ENTRY(_name, _var)				\
	SPE_EXT_ATTR_ENTRY(_name, arm_spe_pmu_cap_show, _var)

static struct attribute *arm_spe_pmu_cap_attr[] = {
	SPE_CAP_EXT_ATTR_ENTRY(arch_inst, SPE_PMU_CAP_ARCH_INST),
	SPE_CAP_EXT_ATTR_ENTRY(ernd, SPE_PMU_CAP_ERND),
	SPE_CAP_EXT_ATTR_ENTRY(count_size, SPE_PMU_CAP_CNT_SZ),
	SPE_CAP_EXT_ATTR_ENTRY(min_interval, SPE_PMU_CAP_MIN_IVAL),
	NULL,
};

static struct attribute_group arm_spe_pmu_cap_group = {
	.name	= "caps",
	.attrs	= arm_spe_pmu_cap_attr,
};

/* User ABI */
#define ATTR_CFG_FLD_ts_enable_CFG		config	/* PMSCR_EL1.TS */
#define ATTR_CFG_FLD_ts_enable_LO		0
#define ATTR_CFG_FLD_ts_enable_HI		0
#define ATTR_CFG_FLD_pa_enable_CFG		config	/* PMSCR_EL1.PA */
#define ATTR_CFG_FLD_pa_enable_LO		1
#define ATTR_CFG_FLD_pa_enable_HI		1
#define ATTR_CFG_FLD_pct_enable_CFG		config	/* PMSCR_EL1.PCT */
#define ATTR_CFG_FLD_pct_enable_LO		2
#define ATTR_CFG_FLD_pct_enable_HI		2
#define ATTR_CFG_FLD_jitter_CFG			config	/* PMSIRR_EL1.RND */
#define ATTR_CFG_FLD_jitter_LO			16
#define ATTR_CFG_FLD_jitter_HI			16
#define ATTR_CFG_FLD_branch_filter_CFG		config	/* PMSFCR_EL1.B */
#define ATTR_CFG_FLD_branch_filter_LO		32
#define ATTR_CFG_FLD_branch_filter_HI		32
#define ATTR_CFG_FLD_load_filter_CFG		config	/* PMSFCR_EL1.LD */
#define ATTR_CFG_FLD_load_filter_LO		33
#define ATTR_CFG_FLD_load_filter_HI		33
#define ATTR_CFG_FLD_store_filter_CFG		config	/* PMSFCR_EL1.ST */
#define ATTR_CFG_FLD_store_filter_LO		34
#define ATTR_CFG_FLD_store_filter_HI		34

#define ATTR_CFG_FLD_event_filter_CFG		config1	/* PMSEVFR_EL1 */
#define ATTR_CFG_FLD_event_filter_LO		0
#define ATTR_CFG_FLD_event_filter_HI		63

#define ATTR_CFG_FLD_min_latency_CFG		config2	/* PMSLATFR_EL1.MINLAT */
#define ATTR_CFG_FLD_min_latency_LO		0
#define ATTR_CFG_FLD_min_latency_HI		11

/* Why does everything I do descend into this? */
#define __GEN_PMU_FORMAT_ATTR(cfg, lo, hi)				\
	(lo) == (hi) ? #cfg ":" #lo "\n" : #cfg ":" #lo "-" #hi

#define _GEN_PMU_FORMAT_ATTR(cfg, lo, hi)				\
	__GEN_PMU_FORMAT_ATTR(cfg, lo, hi)

#define GEN_PMU_FORMAT_ATTR(name)					\
	PMU_FORMAT_ATTR(name,						\
	_GEN_PMU_FORMAT_ATTR(ATTR_CFG_FLD_##name##_CFG,			\
			     ATTR_CFG_FLD_##name##_LO,			\
			     ATTR_CFG_FLD_##name##_HI))

#define _ATTR_CFG_GET_FLD(attr, cfg, lo, hi)				\
	((((attr)->cfg) >> lo) & GENMASK(hi - lo, 0))

#define ATTR_CFG_GET_FLD(attr, name)					\
	_ATTR_CFG_GET_FLD(attr,						\
			  ATTR_CFG_FLD_##name##_CFG,			\
			  ATTR_CFG_FLD_##name##_LO,			\
			  ATTR_CFG_FLD_##name##_HI)

GEN_PMU_FORMAT_ATTR(ts_enable);
GEN_PMU_FORMAT_ATTR(pa_enable);
GEN_PMU_FORMAT_ATTR(pct_enable);
GEN_PMU_FORMAT_ATTR(jitter);
GEN_PMU_FORMAT_ATTR(branch_filter);
GEN_PMU_FORMAT_ATTR(load_filter);
GEN_PMU_FORMAT_ATTR(store_filter);
GEN_PMU_FORMAT_ATTR(event_filter);
GEN_PMU_FORMAT_ATTR(min_latency);

static struct attribute *arm_spe_pmu_formats_attr[] = {
	&format_attr_ts_enable.attr,
	&format_attr_pa_enable.attr,
	&format_attr_pct_enable.attr,
	&format_attr_jitter.attr,
	&format_attr_branch_filter.attr,
	&format_attr_load_filter.attr,
	&format_attr_store_filter.attr,
	&format_attr_event_filter.attr,
	&format_attr_min_latency.attr,
	NULL,
};

static struct attribute_group arm_spe_pmu_format_group = {
	.name	= "format",
	.attrs	= arm_spe_pmu_formats_attr,
};

static ssize_t arm_spe_pmu_get_attr_cpumask(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct arm_spe_pmu *spe_pmu = dev_get_drvdata(dev);

	return cpumap_print_to_pagebuf(true, buf, &spe_pmu->supported_cpus);
}
static DEVICE_ATTR(cpumask, S_IRUGO, arm_spe_pmu_get_attr_cpumask, NULL);

static struct attribute *arm_spe_pmu_attrs[] = {
	&dev_attr_cpumask.attr,
	NULL,
};

static struct attribute_group arm_spe_pmu_group = {
	.attrs	= arm_spe_pmu_attrs,
};

static const struct attribute_group *arm_spe_pmu_attr_groups[] = {
	&arm_spe_pmu_group,
	&arm_spe_pmu_cap_group,
	&arm_spe_pmu_format_group,
	NULL,
};

struct arm_spe_pmu *spe_pmu_local;

/* Convert between user ABI and register values */
static u64 arm_spe_event_to_pmscr(struct perf_event *event)
{
	struct perf_event_attr *attr = &event->attr;
	u64 reg = 0;

	reg |= ATTR_CFG_GET_FLD(attr, ts_enable) << SYS_PMSCR_EL1_TS_SHIFT;
	reg |= ATTR_CFG_GET_FLD(attr, pa_enable) << SYS_PMSCR_EL1_PA_SHIFT;
	reg |= ATTR_CFG_GET_FLD(attr, pct_enable) << SYS_PMSCR_EL1_PCT_SHIFT;

	if (!attr->exclude_user)
		reg |= BIT(SYS_PMSCR_EL1_E0SPE_SHIFT);

	if (!attr->exclude_kernel)
		reg |= BIT(SYS_PMSCR_EL1_E1SPE_SHIFT);

	if (get_spe_event_has_cx(event))
		reg |= BIT(SYS_PMSCR_EL1_CX_SHIFT);

	return reg;
}

static void arm_spe_event_sanitise_period(struct perf_event *event)
{
	struct arm_spe_pmu *spe_pmu = to_spe_pmu(event->pmu);
	u64 period = event->hw.sample_period;
	u64 max_period = SYS_PMSIRR_EL1_INTERVAL_MASK
			 << SYS_PMSIRR_EL1_INTERVAL_SHIFT;

	if (period < spe_pmu->min_period)
		period = spe_pmu->min_period;
	else if (period > max_period)
		period = max_period;
	else
		period &= max_period;

	event->hw.sample_period = period;
}

static u64 arm_spe_event_to_pmsirr(struct perf_event *event)
{
	struct perf_event_attr *attr = &event->attr;
	u64 reg = 0;

	arm_spe_event_sanitise_period(event);

	reg |= ATTR_CFG_GET_FLD(attr, jitter) << SYS_PMSIRR_EL1_RND_SHIFT;
	reg |= event->hw.sample_period;

	return reg;
}

static u64 arm_spe_event_to_pmsfcr(struct perf_event *event)
{
	struct perf_event_attr *attr = &event->attr;
	u64 reg = 0;

	reg |= ATTR_CFG_GET_FLD(attr, load_filter) << SYS_PMSFCR_EL1_LD_SHIFT;
	reg |= ATTR_CFG_GET_FLD(attr, store_filter) << SYS_PMSFCR_EL1_ST_SHIFT;
	reg |= ATTR_CFG_GET_FLD(attr, branch_filter) << SYS_PMSFCR_EL1_B_SHIFT;

	if (reg)
		reg |= BIT(SYS_PMSFCR_EL1_FT_SHIFT);

	if (ATTR_CFG_GET_FLD(attr, event_filter))
		reg |= BIT(SYS_PMSFCR_EL1_FE_SHIFT);

	if (ATTR_CFG_GET_FLD(attr, min_latency))
		reg |= BIT(SYS_PMSFCR_EL1_FL_SHIFT);

	return reg;
}

static u64 arm_spe_event_to_pmsevfr(struct perf_event *event)
{
	struct perf_event_attr *attr = &event->attr;
	return ATTR_CFG_GET_FLD(attr, event_filter);
}

static u64 arm_spe_event_to_pmslatfr(struct perf_event *event)
{
	struct perf_event_attr *attr = &event->attr;
	return ATTR_CFG_GET_FLD(attr, min_latency)
	       << SYS_PMSLATFR_EL1_MINLAT_SHIFT;
}

static void arm_spe_pmu_pad_buf(struct perf_output_handle *handle, int len)
{
	struct arm_spe_pmu_buf *buf = perf_get_aux(handle);
	u64 head = PERF_IDX2OFF(handle->head, buf);

	memset(buf->base + head, ARM_SPE_BUF_PAD_BYTE, len);
	if (!buf->snapshot)
		perf_aux_output_skip(handle, len);
}

static u64 arm_spe_pmu_next_snapshot_off(struct perf_output_handle *handle)
{
	struct arm_spe_pmu_buf *buf = perf_get_aux(handle);
	struct arm_spe_pmu *spe_pmu = to_spe_pmu(handle->event->pmu);
	u64 head = PERF_IDX2OFF(handle->head, buf);
	u64 limit = buf->nr_pages * PAGE_SIZE;

	/*
	 * The trace format isn't parseable in reverse, so clamp
	 * the limit to half of the buffer size in snapshot mode
	 * so that the worst case is half a buffer of records, as
	 * opposed to a single record.
	 */
	if (head < limit >> 1)
		limit >>= 1;

	/*
	 * If we're within max_record_sz of the limit, we must
	 * pad, move the head index and recompute the limit.
	 */
	if (limit - head < spe_pmu->max_record_sz) {
		arm_spe_pmu_pad_buf(handle, limit - head);
		handle->head = PERF_IDX2OFF(limit, buf);
		limit = ((buf->nr_pages * PAGE_SIZE) >> 1) + handle->head;
	}

	return limit;
}

static u64 __arm_spe_pmu_next_off(struct perf_output_handle *handle)
{
	struct arm_spe_pmu *spe_pmu = to_spe_pmu(handle->event->pmu);
	struct arm_spe_pmu_buf *buf = perf_get_aux(handle);
	const u64 bufsize = buf->nr_pages * PAGE_SIZE;
	u64 limit = bufsize;
	u64 head, tail, wakeup;

	/*
	 * The head can be misaligned for two reasons:
	 *
	 * 1. The hardware left PMBPTR pointing to the first byte after
	 *    a record when generating a buffer management event.
	 *
	 * 2. We used perf_aux_output_skip to consume handle->size bytes
	 *    and CIRC_SPACE was used to compute the size, which always
	 *    leaves one entry free.
	 *
	 * Deal with this by padding to the next alignment boundary and
	 * moving the head index. If we run out of buffer space, we'll
	 * reduce handle->size to zero and end up reporting truncation.
	 */
	head = PERF_IDX2OFF(handle->head, buf);
	if (!IS_ALIGNED(head, spe_pmu->align)) {
		unsigned long delta = roundup(head, spe_pmu->align) - head;

		delta = min(delta, handle->size);
		arm_spe_pmu_pad_buf(handle, delta);
		head = PERF_IDX2OFF(handle->head, buf);
	}

	/* If we've run out of free space, then nothing more to do */
	if (!handle->size)
		goto no_space;

	/* Compute the tail and wakeup indices now that we've aligned head */
	tail = PERF_IDX2OFF(handle->head + handle->size, buf);
	wakeup = PERF_IDX2OFF(handle->wakeup, buf);

	/*
	 * Avoid clobbering unconsumed data. We know we have space, so
	 * if we see head == tail we know that the buffer is empty. If
	 * head > tail, then there's nothing to clobber prior to
	 * wrapping.
	 */
	if (head < tail)
		limit = round_down(tail, PAGE_SIZE);

	/*
	 * Wakeup may be arbitrarily far into the future. If it's not in
	 * the current generation, either we'll wrap before hitting it,
	 * or it's in the past and has been handled already.
	 *
	 * If there's a wakeup before we wrap, arrange to be woken up by
	 * the page boundary following it. Keep the tail boundary if
	 * that's lower.
	 */
	if (handle->wakeup < (handle->head + handle->size) && head <= wakeup)
		limit = min(limit, round_up(wakeup, PAGE_SIZE));

	if (limit > head)
		return limit;

	arm_spe_pmu_pad_buf(handle, handle->size);
no_space:
	perf_aux_output_flag(handle, PERF_AUX_FLAG_TRUNCATED);
	perf_aux_output_end(handle, 0);
	return 0;
}

static u64 arm_spe_pmu_next_off(struct perf_output_handle *handle)
{
	struct arm_spe_pmu_buf *buf = perf_get_aux(handle);
	struct arm_spe_pmu *spe_pmu = to_spe_pmu(handle->event->pmu);
	u64 limit = __arm_spe_pmu_next_off(handle);
	u64 head = PERF_IDX2OFF(handle->head, buf);

	/*
	 * If the head has come too close to the end of the buffer,
	 * then pad to the end and recompute the limit.
	 */
	if (limit && (limit - head < spe_pmu->max_record_sz)) {
		arm_spe_pmu_pad_buf(handle, limit - head);
		limit = __arm_spe_pmu_next_off(handle);
	}

	return limit;
}

static void arm_spe_perf_aux_output_begin(struct perf_output_handle *handle,
					  struct perf_event *event)
{
	u64 base, limit;
	struct arm_spe_pmu_buf *buf;

	/* Start a new aux session */
	buf = perf_aux_output_begin(handle, event);
	if (!buf) {
		event->hw.state |= PERF_HES_STOPPED;
		/*
		 * We still need to clear the limit pointer, since the
		 * profiler might only be disabled by virtue of a fault.
		 */
		limit = 0;
		goto out_write_limit;
	}

	limit = buf->snapshot ? arm_spe_pmu_next_snapshot_off(handle)
			      : arm_spe_pmu_next_off(handle);
	if (limit)
		limit |= BIT(SYS_PMBLIMITR_EL1_E_SHIFT);

	limit += (u64)buf->base;
	base = (u64)buf->base + PERF_IDX2OFF(handle->head, buf);
	write_sysreg_s(base, SYS_PMBPTR_EL1);

out_write_limit:
	write_sysreg_s(limit, SYS_PMBLIMITR_EL1);
}

static void arm_spe_perf_aux_output_end(struct perf_output_handle *handle)
{
	struct arm_spe_pmu_buf *buf = perf_get_aux(handle);
	u64 offset, size;

	offset = read_sysreg_s(SYS_PMBPTR_EL1) - (u64)buf->base;
	size = offset - PERF_IDX2OFF(handle->head, buf);

	if (buf->snapshot)
		handle->head = offset;

	perf_aux_output_end(handle, size);
}

static void arm_spe_pmu_disable_and_drain_local(void)
{
	/* Disable profiling at EL0 and EL1 */
	write_sysreg_s(0, SYS_PMSCR_EL1);
	isb();

	/* Drain any buffered data */
	psb_csync();
	dsb(nsh);

	/* Disable the profiling buffer */
	write_sysreg_s(0, SYS_PMBLIMITR_EL1);
	isb();
}

/* IRQ handling */
static enum arm_spe_buf_fault_action
arm_spe_pmu_buf_get_fault_act(struct perf_output_handle *handle)
{
	const char *err_str;
	u64 pmbsr;
	enum arm_spe_buf_fault_action ret;

	/*
	 * Ensure new profiling data is visible to the CPU and any external
	 * aborts have been resolved.
	 */
	psb_csync();
	dsb(nsh);

	/* Ensure hardware updates to PMBPTR_EL1 are visible */
	isb();

	/* Service required? */
	pmbsr = read_sysreg_s(SYS_PMBSR_EL1);
	if (!(pmbsr & BIT(SYS_PMBSR_EL1_S_SHIFT)))
		return SPE_PMU_BUF_FAULT_ACT_SPURIOUS;

	/*
	 * If we've lost data, disable profiling and also set the PARTIAL
	 * flag to indicate that the last record is corrupted.
	 */
	if (pmbsr & BIT(SYS_PMBSR_EL1_DL_SHIFT))
		perf_aux_output_flag(handle, PERF_AUX_FLAG_TRUNCATED |
					     PERF_AUX_FLAG_PARTIAL);

	/* Report collisions to userspace so that it can up the period */
	if (pmbsr & BIT(SYS_PMBSR_EL1_COLL_SHIFT))
		perf_aux_output_flag(handle, PERF_AUX_FLAG_COLLISION);

	/* We only expect buffer management events */
	switch (pmbsr & (SYS_PMBSR_EL1_EC_MASK << SYS_PMBSR_EL1_EC_SHIFT)) {
	case SYS_PMBSR_EL1_EC_BUF:
		/* Handled below */
		break;
	case SYS_PMBSR_EL1_EC_FAULT_S1:
	case SYS_PMBSR_EL1_EC_FAULT_S2:
		err_str = "Unexpected buffer fault";
		goto out_err;
	default:
		err_str = "Unknown error code";
		goto out_err;
	}

	/* Buffer management event */
	switch (pmbsr &
		(SYS_PMBSR_EL1_BUF_BSC_MASK << SYS_PMBSR_EL1_BUF_BSC_SHIFT)) {
	case SYS_PMBSR_EL1_BUF_BSC_FULL:
		ret = SPE_PMU_BUF_FAULT_ACT_OK;
		goto out_stop;
	default:
		err_str = "Unknown buffer status code";
	}

out_err:
	pr_err_ratelimited("%s on CPU %d [PMBSR=0x%016llx, PMBPTR=0x%016llx, PMBLIMITR=0x%016llx]\n",
			   err_str, smp_processor_id(), pmbsr,
			   read_sysreg_s(SYS_PMBPTR_EL1),
			   read_sysreg_s(SYS_PMBLIMITR_EL1));
	ret = SPE_PMU_BUF_FAULT_ACT_FATAL;

out_stop:
	arm_spe_perf_aux_output_end(handle);
	return ret;
}


static u64 arm_spe_pmsevfr_res0(u16 pmsver)
{
	switch (pmsver) {
	case ID_AA64DFR0_PMSVER_8_2:
		return SYS_PMSEVFR_EL1_RES0_8_2;
	case ID_AA64DFR0_PMSVER_8_3:
	/* Return the highest version we support in default */
	default:
		return SYS_PMSEVFR_EL1_RES0_8_3;
	}
}

/* Perf callbacks */
static int arm_spe_pmu_event_init(struct perf_event *event)
{
	u64 reg;
	struct perf_event_attr *attr = &event->attr;
	struct arm_spe_pmu *spe_pmu = to_spe_pmu(event->pmu);

	/* This is, of course, deeply driver-specific */
	if (attr->type != event->pmu->type)
		return -ENOENT;

	if (event->cpu >= 0 &&
	    !cpumask_test_cpu(event->cpu, &spe_pmu->supported_cpus))
		return -ENOENT;

	if (arm_spe_event_to_pmsevfr(event) & arm_spe_pmsevfr_res0(spe_pmu->pmsver))
		return -EOPNOTSUPP;

	if (attr->exclude_idle)
		return -EOPNOTSUPP;

	/*
	 * Feedback-directed frequency throttling doesn't work when we
	 * have a buffer of samples. We'd need to manually count the
	 * samples in the buffer when it fills up and adjust the event
	 * count to reflect that. Instead, just force the user to specify
	 * a sample period.
	 */
	if (attr->freq)
		return -EINVAL;

	reg = arm_spe_event_to_pmsfcr(event);
	if ((reg & BIT(SYS_PMSFCR_EL1_FE_SHIFT)) &&
	    !(spe_pmu->features & SPE_PMU_FEAT_FILT_EVT))
		return -EOPNOTSUPP;

	if ((reg & BIT(SYS_PMSFCR_EL1_FT_SHIFT)) &&
	    !(spe_pmu->features & SPE_PMU_FEAT_FILT_TYP))
		return -EOPNOTSUPP;

	if ((reg & BIT(SYS_PMSFCR_EL1_FL_SHIFT)) &&
	    !(spe_pmu->features & SPE_PMU_FEAT_FILT_LAT))
		return -EOPNOTSUPP;

	set_spe_event_has_cx(event);
	reg = arm_spe_event_to_pmscr(event);
	if (!perfmon_capable() &&
	    (reg & (BIT(SYS_PMSCR_EL1_PA_SHIFT) |
		    BIT(SYS_PMSCR_EL1_PCT_SHIFT))))
		return -EACCES;

	return 0;
}

static void arm_spe_pmu_start(struct perf_event *event, int flags)
{
	u64 reg;
	struct arm_spe_pmu *spe_pmu = to_spe_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	struct perf_output_handle *handle = this_cpu_ptr(spe_pmu->handle);

	arm_spe_set_user(ARM_SPE_USER_PERF);

	hwc->state = 0;
	arm_spe_perf_aux_output_begin(handle, event);
	if (hwc->state)
		return;

	reg = arm_spe_event_to_pmsfcr(event);
	write_sysreg_s(reg, SYS_PMSFCR_EL1);

	reg = arm_spe_event_to_pmsevfr(event);
	write_sysreg_s(reg, SYS_PMSEVFR_EL1);

	reg = arm_spe_event_to_pmslatfr(event);
	write_sysreg_s(reg, SYS_PMSLATFR_EL1);

	if (flags & PERF_EF_RELOAD) {
		reg = arm_spe_event_to_pmsirr(event);
		write_sysreg_s(reg, SYS_PMSIRR_EL1);
		isb();
		reg = local64_read(&hwc->period_left);
		write_sysreg_s(reg, SYS_PMSICR_EL1);
	}

	reg = arm_spe_event_to_pmscr(event);
	isb();
	write_sysreg_s(reg, SYS_PMSCR_EL1);
}

static void arm_spe_pmu_stop(struct perf_event *event, int flags)
{
	struct arm_spe_pmu *spe_pmu = to_spe_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	struct perf_output_handle *handle = this_cpu_ptr(spe_pmu->handle);

	/* If we're already stopped, then nothing to do */
	if (hwc->state & PERF_HES_STOPPED) {
		/*
		 * PERF_HES_STOPPED maybe set in arm_spe_perf_aux_output_begin,
		 * we switch user here.
		 */
		arm_spe_set_user(ARM_SPE_USER_MEM_SAMPLING);
		return;
	}

	/* Stop all trace generation */
	arm_spe_pmu_disable_and_drain_local();

	if (flags & PERF_EF_UPDATE) {
		/*
		 * If there's a fault pending then ensure we contain it
		 * to this buffer, since we might be on the context-switch
		 * path.
		 */
		if (perf_get_aux(handle)) {
			enum arm_spe_buf_fault_action act;

			act = arm_spe_pmu_buf_get_fault_act(handle);
			if (act == SPE_PMU_BUF_FAULT_ACT_SPURIOUS)
				arm_spe_perf_aux_output_end(handle);
			else
				write_sysreg_s(0, SYS_PMBSR_EL1);
		}

		/*
		 * This may also contain ECOUNT, but nobody else should
		 * be looking at period_left, since we forbid frequency
		 * based sampling.
		 */
		local64_set(&hwc->period_left, read_sysreg_s(SYS_PMSICR_EL1));
		hwc->state |= PERF_HES_UPTODATE;
	}

	hwc->state |= PERF_HES_STOPPED;
	arm_spe_set_user(ARM_SPE_USER_MEM_SAMPLING);
}

static int arm_spe_pmu_add(struct perf_event *event, int flags)
{
	int ret = 0;
	struct arm_spe_pmu *spe_pmu = to_spe_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int cpu = event->cpu == -1 ? smp_processor_id() : event->cpu;

	if (!cpumask_test_cpu(cpu, &spe_pmu->supported_cpus))
		return -ENOENT;

	hwc->state = PERF_HES_UPTODATE | PERF_HES_STOPPED;

	if (flags & PERF_EF_START) {
		arm_spe_pmu_start(event, PERF_EF_RELOAD);
		if (hwc->state & PERF_HES_STOPPED)
			ret = -EINVAL;
	}

	return ret;
}

static void arm_spe_pmu_del(struct perf_event *event, int flags)
{
	arm_spe_pmu_stop(event, PERF_EF_UPDATE);
}

static void arm_spe_pmu_read(struct perf_event *event)
{
}

static void *arm_spe_pmu_setup_aux(struct perf_event *event, void **pages,
				   int nr_pages, bool snapshot)
{
	int i, cpu = event->cpu;
	struct page **pglist;
	struct arm_spe_pmu_buf *buf;

	/* We need at least two pages for this to work. */
	if (nr_pages < 2)
		return NULL;

	/*
	 * We require an even number of pages for snapshot mode, so that
	 * we can effectively treat the buffer as consisting of two equal
	 * parts and give userspace a fighting chance of getting some
	 * useful data out of it.
	 */
	if (snapshot && (nr_pages & 1))
		return NULL;

	if (cpu == -1)
		cpu = raw_smp_processor_id();

	buf = kzalloc_node(sizeof(*buf), GFP_KERNEL, cpu_to_node(cpu));
	if (!buf)
		return NULL;

	pglist = kcalloc(nr_pages, sizeof(*pglist), GFP_KERNEL);
	if (!pglist)
		goto out_free_buf;

	for (i = 0; i < nr_pages; ++i)
		pglist[i] = virt_to_page(pages[i]);

	buf->base = vmap(pglist, nr_pages, VM_MAP, PAGE_KERNEL);
	if (!buf->base)
		goto out_free_pglist;

	buf->nr_pages	= nr_pages;
	buf->snapshot	= snapshot;

	kfree(pglist);
	return buf;

out_free_pglist:
	kfree(pglist);
out_free_buf:
	kfree(buf);
	return NULL;
}

static void arm_spe_pmu_free_aux(void *aux)
{
	struct arm_spe_pmu_buf *buf = aux;

	vunmap(buf->base);
	kfree(buf);
}

/* Initialisation and teardown functions */
static int arm_spe_pmu_perf_init(struct arm_spe_pmu *spe_pmu)
{
	static atomic_t pmu_idx = ATOMIC_INIT(-1);

	int idx;
	char *name;
	struct device *dev = &spe_pmu->pdev->dev;

	spe_pmu->pmu = (struct pmu) {
		.module = THIS_MODULE,
		.capabilities	= PERF_PMU_CAP_EXCLUSIVE | PERF_PMU_CAP_ITRACE,
		.attr_groups	= arm_spe_pmu_attr_groups,
		/*
		 * We hitch a ride on the software context here, so that
		 * we can support per-task profiling (which is not possible
		 * with the invalid context as it doesn't get sched callbacks).
		 * This requires that userspace either uses a dummy event for
		 * perf_event_open, since the aux buffer is not setup until
		 * a subsequent mmap, or creates the profiling event in a
		 * disabled state and explicitly PERF_EVENT_IOC_ENABLEs it
		 * once the buffer has been created.
		 */
		.task_ctx_nr	= perf_sw_context,
		.event_init	= arm_spe_pmu_event_init,
		.add		= arm_spe_pmu_add,
		.del		= arm_spe_pmu_del,
		.start		= arm_spe_pmu_start,
		.stop		= arm_spe_pmu_stop,
		.read		= arm_spe_pmu_read,
		.setup_aux	= arm_spe_pmu_setup_aux,
		.free_aux	= arm_spe_pmu_free_aux,
	};

	idx = atomic_inc_return(&pmu_idx);
	name = devm_kasprintf(dev, GFP_KERNEL, "%s_%d", PMUNAME, idx);
	if (!name) {
		dev_err(dev, "failed to allocate name for pmu %d\n", idx);
		return -ENOMEM;
	}

	return perf_pmu_register(&spe_pmu->pmu, name, -1);
}

static void arm_spe_pmu_perf_destroy(struct arm_spe_pmu *spe_pmu)
{
	perf_pmu_unregister(&spe_pmu->pmu);
}

void arm_spe_sampling_process(enum arm_spe_buf_fault_action act)
{
	struct perf_output_handle *handle = this_cpu_ptr(spe_pmu_local->handle);
	struct perf_event *event = handle->event;
	u64 pmbsr;

	if (!perf_get_aux(handle))
		return;

	/*
	 * If we've lost data, disable profiling and also set the PARTIAL
	 * flag to indicate that the last record is corrupted.
	 */
	if (FIELD_GET(PMBSR_EL1_DL, pmbsr))
		perf_aux_output_flag(handle, PERF_AUX_FLAG_TRUNCATED |
				      PERF_AUX_FLAG_PARTIAL);

	/* Report collisions to userspace so that it can up the period */
	if (FIELD_GET(PMBSR_EL1_COLL, pmbsr))
		perf_aux_output_flag(handle, PERF_AUX_FLAG_COLLISION);

	arm_spe_perf_aux_output_end(handle);

	if (act == SPE_PMU_BUF_FAULT_ACT_OK) {
		if (!(handle->aux_flags & PERF_AUX_FLAG_TRUNCATED)) {
			arm_spe_perf_aux_output_begin(handle, event);
			isb();
		}
	}
}

static bool arm_spe_pmu_set_cap(struct arm_spe_pmu *spe_pmu)
{
	struct arm_spe *p;
	struct device *dev = &spe_pmu->pdev->dev;

	p = arm_spe_get_desc();
	if (!p) {
		dev_err(dev, "get spe pmu cap from arm spe driver failed!");
		return false;
	}

	spe_pmu->supported_cpus = p->supported_cpus;
	spe_pmu->irq = p->irq;
	spe_pmu->pmsver = p->pmsver;
	spe_pmu->align = p->align;
	spe_pmu->features = p->features;
	spe_pmu->min_period = p->min_period;
	spe_pmu->max_record_sz = p->max_record_sz;
	spe_pmu->counter_sz = p->counter_sz;

	return true;
}

static const struct of_device_id arm_spe_pmu_of_match[] = {
	{ .compatible = "arm,statistical-profiling-extension-v1", .data = (void *)1 },
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(of, arm_spe_pmu_of_match);

static const struct platform_device_id arm_spe_match[] = {
	{ ARMV8_SPE_PMU_PDEV_NAME, 0},
	{ }
};
MODULE_DEVICE_TABLE(platform, arm_spe_match);

static int arm_spe_pmu_device_probe(struct platform_device *pdev)
{
	int ret;
	struct arm_spe_pmu *spe_pmu;
	struct device *dev = &pdev->dev;

	/*
	 * If kernelspace is unmapped when running at EL0, then the SPE
	 * buffer will fault and prematurely terminate the AUX session.
	 */
	if (arm64_kernel_unmapped_at_el0()) {
		dev_warn_once(dev, "profiling buffer inaccessible. Try passing \"kpti=off\" on the kernel command line\n");
		return -EPERM;
	}

	spe_pmu = devm_kzalloc(dev, sizeof(*spe_pmu), GFP_KERNEL);
	if (!spe_pmu) {
		dev_err(dev, "failed to allocate spe_pmu\n");
		return -ENOMEM;
	}

	spe_pmu->handle = alloc_percpu(typeof(*spe_pmu->handle));
	if (!spe_pmu->handle)
		return -ENOMEM;

	spe_pmu->pdev = pdev;
	platform_set_drvdata(pdev, spe_pmu);

	if (!arm_spe_pmu_set_cap(spe_pmu))
		goto out_free_handle;

	spe_pmu_local = spe_pmu;

	ret = arm_spe_pmu_perf_init(spe_pmu);
	if (ret)
		goto out_free_handle;

	return 0;

out_free_handle:
	free_percpu(spe_pmu->handle);
	return ret;
}

static int arm_spe_pmu_device_remove(struct platform_device *pdev)
{
	struct arm_spe_pmu *spe_pmu = platform_get_drvdata(pdev);

	arm_spe_pmu_perf_destroy(spe_pmu);
	free_percpu(spe_pmu->handle);
	return 0;
}

static struct platform_driver arm_spe_pmu_driver = {
	.id_table = arm_spe_match,
	.driver	= {
		.name		= DRVNAME,
		.of_match_table	= of_match_ptr(arm_spe_pmu_of_match),
		.suppress_bind_attrs = true,
	},
	.probe	= arm_spe_pmu_device_probe,
	.remove	= arm_spe_pmu_device_remove,
};

static int __init arm_spe_pmu_init(void)
{
	arm_spe_sampling_for_perf_callback_register(arm_spe_sampling_process);
	return platform_driver_register(&arm_spe_pmu_driver);
}

static void __exit arm_spe_pmu_exit(void)
{
	arm_spe_sampling_for_perf_callback_register(NULL);
	platform_driver_unregister(&arm_spe_pmu_driver);
}

late_initcall(arm_spe_pmu_init);
module_exit(arm_spe_pmu_exit);

MODULE_DESCRIPTION("Perf driver for the ARMv8.2 Statistical Profiling Extension");
MODULE_AUTHOR("Will Deacon <will.deacon@arm.com>");
MODULE_LICENSE("GPL v2");
