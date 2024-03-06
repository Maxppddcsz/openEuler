// SPDX-License-Identifier: GPL-2.0+
/*
 * Common code for CPU Dynamic Affinity Scheduling
 *
 * Copyright (C) 2023-2024 Huawei Technologies Co., Ltd
 *
 * Author: Hui Tang <tanghui20@huawei.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#include "dynamic_affinity.h"

static inline struct cpumask *task_prefer_cpus(struct task_struct *p);
static inline int dynamic_affinity_mode(struct task_struct *p);
static unsigned long capacity_of(int cpu);
static inline unsigned long cfs_rq_load_avg(struct cfs_rq *cfs_rq);

static DEFINE_STATIC_KEY_FALSE(__dynamic_affinity_used);

void dynamic_affinity_enable(void)
{
	static_branch_enable_cpuslocked(&__dynamic_affinity_used);
}

static inline bool dynamic_affinity_used(void)
{
	return static_branch_unlikely(&__dynamic_affinity_used);
}

/*
 * Low utilization threshold for CPU
 *
 * (default: 85%), units: percentage of CPU utilization)
 */
int sysctl_sched_util_low_pct = 85;

static inline bool prefer_cpus_valid(struct task_struct *p)
{
	struct cpumask *prefer_cpus = task_prefer_cpus(p);

	return !cpumask_empty(prefer_cpus) &&
	       !cpumask_equal(prefer_cpus, &p->cpus_allowed) &&
	       cpumask_subset(prefer_cpus, &p->cpus_allowed);
}

/*
 * set_task_select_cpus: select the cpu range for task
 * @p: the task whose available cpu range will to set
 *uto_affinity_used @idlest_cpu: the cpu which is the idlest in prefer cpus
 *
 * If sum of 'util_avg' among 'preferred_cpus' lower than the percentage
 * 'sysctl_sched_util_low_pct' of 'preferred_cpus' capacity, select
 * 'preferred_cpus' range for task, otherwise select 'preferred_cpus' for task.
 *
 * The available cpu range set to p->select_cpus. Idlest cpu in preferred cpus
 * set to @idlest_cpu, which is set to wakeup cpu when fast path wakeup cpu
 * without p->select_cpus.
 */
void set_task_select_cpus(struct task_struct *p, int *idlest_cpu, int sd_flag)
{
	unsigned long util_avg_sum = 0;
	unsigned long tg_capacity = 0;
	long min_util = INT_MIN;
	struct task_group *tg;
	long spare;
	int cpu, mode;

	rcu_read_lock();
	mode = dynamic_affinity_mode(p);
	if (mode == -1) {
		rcu_read_unlock();
		return;
	} else if (mode == 1) {
		p->select_cpus = task_prefer_cpus(p);
		if (idlest_cpu)
			*idlest_cpu = cpumask_first(p->select_cpus);
		sched_qos_affinity_set(p);
		rcu_read_unlock();
		return;
	}

	/* manual mode */
	tg = task_group(p);
	for_each_cpu(cpu, p->prefer_cpus) {
		if (unlikely(!tg->se[cpu]))
			continue;

		if (idlest_cpu && available_idle_cpu(cpu)) {
			*idlest_cpu = cpu;
		} else if (idlest_cpu) {
			spare = (long)(capacity_of(cpu) - tg->se[cpu]->avg.util_avg);
			if (spare > min_util) {
				min_util = spare;
				*idlest_cpu = cpu;
			}
		}

		if (available_idle_cpu(cpu)) {
			rcu_read_unlock();
			p->select_cpus = p->prefer_cpus;
			if (sd_flag & SD_BALANCE_WAKE)
				schedstat_inc(p->se.dyn_affi_stats->nr_wakeups_preferred_cpus);
			return;
		}

		util_avg_sum += tg->se[cpu]->avg.util_avg;
		tg_capacity += capacity_of(cpu);
	}
	rcu_read_unlock();

	if (tg_capacity > cpumask_weight(p->prefer_cpus) &&
	    util_avg_sum * 100 <= tg_capacity * sysctl_sched_util_low_pct) {
		p->select_cpus = p->prefer_cpus;
		if (sd_flag & SD_BALANCE_WAKE)
			schedstat_inc(p->se.dyn_affi_stats->nr_wakeups_preferred_cpus);
	}
}

int sched_prefer_cpus_fork(struct task_struct *p, struct task_struct *orig)
{
	p->prefer_cpus = kmalloc(sizeof(cpumask_t), GFP_KERNEL);
	if (!p->prefer_cpus)
		return -ENOMEM;

	if (orig->prefer_cpus)
		cpumask_copy(p->prefer_cpus, orig->prefer_cpus);
	else
		cpumask_clear(p->prefer_cpus);

	p->se.dyn_affi_stats = kzalloc(sizeof(struct dyn_affinity_stats),
				       GFP_KERNEL);
	if (!p->se.dyn_affi_stats) {
		kfree(p->prefer_cpus);
		p->prefer_cpus = NULL;
		return -ENOMEM;
	}
	return 0;
}

void sched_prefer_cpus_free(struct task_struct *p)
{
	kfree(p->prefer_cpus);
	kfree(p->se.dyn_affi_stats);
}

void task_cpus_preferred(struct seq_file *m, struct task_struct *task)
{
	seq_printf(m, "Cpus_preferred:\t%*pb\n",
		   cpumask_pr_args(task->prefer_cpus));
	seq_printf(m, "Cpus_preferred_list:\t%*pbl\n",
		   cpumask_pr_args(task->prefer_cpus));
}

#ifdef CONFIG_QOS_SCHED_SMART_GRID

#define AUTO_AFFINITY_DEFAULT_PERIOD_MS 2000
#define IS_DOMAIN_SET(level, mask)	((1 << (level)) & (mask))

static DEFINE_MUTEX(smart_grid_used_mutex);

static inline unsigned long cpu_util(int cpu);
static unsigned long target_load(int cpu, int type);
static unsigned long capacity_of(int cpu);
static int sched_idle_cpu(int cpu);
static unsigned long weighted_cpuload(struct rq *rq);

int sysctl_affinity_adjust_delay_ms = 5000;

	nodemask_t smart_grid_preferred_nodemask;
unsigned long *smart_grid_preferred_nodemask_bits =
	nodes_addr(smart_grid_preferred_nodemask);

struct static_key __smart_grid_used;

static void smart_grid_usage_inc(void)
{
	static_key_slow_inc(&__smart_grid_used);
}

static void smart_grid_usage_dec(void)
{
	static_key_slow_dec(&__smart_grid_used);
}

static inline struct cpumask *task_prefer_cpus(struct task_struct *p)
{
	struct affinity_domain *ad;

	if (!smart_grid_used())
		return p->prefer_cpus;

	if (task_group(p)->auto_affinity->mode == 0)
		return &p->cpus_allowed;

	ad = &task_group(p)->auto_affinity->ad;
	return ad->domains[ad->curr_level];
}

static inline int dynamic_affinity_mode(struct task_struct *p)
{
	if (!prefer_cpus_valid(p))
		return -1;

	if (smart_grid_used())
		return task_group(p)->auto_affinity->mode == 0 ? -1 : 1;

	return 0;
}

static void affinity_domain_up(struct task_group *tg)
{
	struct affinity_domain *ad = &tg->auto_affinity->ad;
	u16 level = ad->curr_level;

	if (ad->curr_level >= ad->dcount - 1)
		return;

	while (level < ad->dcount) {
		if (IS_DOMAIN_SET(level + 1, ad->domain_mask) &&
		    cpumask_weight(ad->domains[level + 1]) > 0) {
			ad->curr_level = level + 1;
			return;
		}
		level++;
	}
}

static inline int down_level_to(struct affinity_domain *ad)
{
	int level = ad->curr_level;

	if (level <= 0)
		return -1;

	while (level > 0) {
		if (!cpumask_weight(ad->domains[level - 1]))
			return -1;

		if (IS_DOMAIN_SET(level - 1, ad->domain_mask))
			return level - 1;

		level--;
	}

	return -1;
}

static void affinity_domain_down(struct task_group *tg)
{
	int down_level = down_level_to(&tg->auto_affinity->ad);

	if (down_level >= 0)
		tg->auto_affinity->ad.curr_level = down_level;
}

static enum hrtimer_restart sched_auto_affi_period_timer(struct hrtimer *timer)
{
	struct auto_affinity *auto_affi =
		container_of(timer, struct auto_affinity, period_timer);
	struct task_group *tg = auto_affi->tg;
	struct affinity_domain *ad = &auto_affi->ad;
	struct cpumask *span = ad->domains[ad->curr_level];
	int cpu, util_low_pct, down_level;
	unsigned long util_avg_sum = 0;
	unsigned long tg_capacity = 0;
	unsigned long flags;
	int ratio = 2;

	for_each_cpu(cpu, span) {
		util_avg_sum += cpu_util(cpu);
		tg_capacity += capacity_of(cpu);
	}

	raw_spin_lock_irqsave(&auto_affi->lock, flags);
	/* May be re-entrant by stop_auto_affinity, So check again. */
	if (auto_affi->period_active == 0) {
		raw_spin_unlock_irqrestore(&auto_affi->lock, flags);
		return HRTIMER_NORESTART;
	}

	down_level = down_level_to(ad);
	if (down_level >= 0)
		ratio = cpumask_weight(ad->domains[ad->curr_level]) /
			cpumask_weight(ad->domains[down_level]) + 1;

	util_low_pct = auto_affi->util_low_pct >= 0 ? auto_affi->util_low_pct :
			sysctl_sched_util_low_pct;
	if (util_avg_sum * 100 >= tg_capacity * util_low_pct)
		affinity_domain_up(tg);
	else if (util_avg_sum * 100 * ratio < tg_capacity * util_low_pct)
		affinity_domain_down(tg);

	schedstat_inc(ad->stay_cnt[ad->curr_level]);
	hrtimer_forward_now(timer, auto_affi->period);
	raw_spin_unlock_irqrestore(&auto_affi->lock, flags);
	return HRTIMER_RESTART;
}

static int tg_update_affinity_domain_down(struct task_group *tg, void *data)
{
	struct auto_affinity *auto_affi = tg->auto_affinity;
	struct affinity_domain *ad;
	int *cpu_state = data;
	unsigned long flags;
	int i;

	if (!auto_affi)
		return 0;

	ad = &tg->auto_affinity->ad;
	raw_spin_lock_irqsave(&auto_affi->lock, flags);

	for (i = 0; i < ad->dcount; i++) {
		if (!cpumask_test_cpu(cpu_state[0], ad->domains_orig[i]))
			continue;

		/* online */
		if (cpu_state[1]) {
			cpumask_set_cpu(cpu_state[0], ad->domains[i]);
		} else {
			cpumask_clear_cpu(cpu_state[0], ad->domains[i]);
			if (!cpumask_weight(ad->domains[i]))
				affinity_domain_up(tg);
		}

	}
	raw_spin_unlock_irqrestore(&auto_affi->lock, flags);

	return 0;
}

void tg_update_affinity_domains(int cpu, int online)
{
	int cpu_state[2];

	cpu_state[0] = cpu;
	cpu_state[1] = online;

	rcu_read_lock();
	walk_tg_tree(tg_update_affinity_domain_down, tg_nop, cpu_state);
	rcu_read_unlock();
}

void start_auto_affinity(struct auto_affinity *auto_affi)
{
	ktime_t delay_ms;

	mutex_lock(&smart_grid_used_mutex);
	raw_spin_lock_irq(&auto_affi->lock);
	if (auto_affi->period_active == 1) {
		raw_spin_unlock_irq(&auto_affi->lock);
		mutex_unlock(&smart_grid_used_mutex);
		return;
	}

	auto_affi->period_active = 1;
	auto_affi->mode = 1;
	delay_ms = ms_to_ktime(sysctl_affinity_adjust_delay_ms);
	hrtimer_forward_now(&auto_affi->period_timer, delay_ms);
	hrtimer_start_expires(&auto_affi->period_timer,
				HRTIMER_MODE_ABS_PINNED);
	raw_spin_unlock_irq(&auto_affi->lock);

	smart_grid_usage_inc();
	mutex_unlock(&smart_grid_used_mutex);
}

void stop_auto_affinity(struct auto_affinity *auto_affi)
{
	struct affinity_domain *ad = &auto_affi->ad;

	mutex_lock(&smart_grid_used_mutex);
	raw_spin_lock_irq(&auto_affi->lock);
	if (auto_affi->period_active == 0) {
		raw_spin_unlock_irq(&auto_affi->lock);
		mutex_unlock(&smart_grid_used_mutex);
		return;
	}
	auto_affi->period_active = 0;
	auto_affi->mode = 0;
	ad->curr_level = ad->dcount > 0 ? ad->dcount - 1 : 0;
	raw_spin_unlock_irq(&auto_affi->lock);

	smart_grid_usage_dec();
	mutex_unlock(&smart_grid_used_mutex);
}

void free_affinity_domains(struct affinity_domain *ad)
{
	int i;

	for (i = 0; i < AD_LEVEL_MAX; i++) {
		kfree(ad->domains[i]);
		kfree(ad->domains_orig[i]);
		ad->domains[i] = NULL;
		ad->domains_orig[i] = NULL;
	}
	ad->dcount = 0;
}

static int init_affinity_domains_orig(struct affinity_domain *ad)
{
	int i, j;

	for (i = 0; i < ad->dcount; i++) {
		ad->domains_orig[i] = kmalloc(sizeof(cpumask_t), GFP_KERNEL);
		if (!ad->domains_orig[i])
			goto err;

		cpumask_copy(ad->domains_orig[i], ad->domains[i]);
	}

	return 0;
err:
	for (j = 0; j < i; j++) {
		kfree(ad->domains_orig[j]);
		ad->domains_orig[j] = NULL;
	}
	return -ENOMEM;
}

struct nid_stats {
	unsigned long util;
	unsigned long compute_capacity;
	int idlest_cpu;
};

static inline void update_nid_stats(struct nid_stats *ns, int nid)
{
	int min_util = INT_MAX;
	int cpu, avg_util;

	memset(ns, 0, sizeof(*ns));
	for_each_cpu(cpu, cpumask_of_node(nid)) {
		ns->compute_capacity += capacity_of(cpu);
		ns->util += cpu_util(cpu);
		avg_util = cpu_util(cpu) * SCHED_CAPACITY_SCALE /
				capacity_of(cpu);
		if (avg_util < min_util) {
			ns->idlest_cpu = cpu;
			min_util = avg_util;
		}
	}
}

static int auto_affinity_find_idlest_cpu(void)
{
	int nid, imbalance_pct, is_prefer;
	unsigned long long util_min = UINT_MAX;
	int idlest_is_prefer = 0;
	struct nid_stats ns;
	int idlest_nid = 0;
	int idlest_cpu = 0;

	for_each_online_node(nid) {
		if (!cpumask_intersects(cpumask_of_node(nid),
				       housekeeping_cpumask(HK_FLAG_DOMAIN)))
			continue;

		update_nid_stats(&ns, nid);

		is_prefer = 0;
		if (node_isset(nid, smart_grid_preferred_nodemask)) {
			if (ns.util * 100 <
			    ns.compute_capacity * sysctl_sched_util_low_pct) {
				idlest_nid = nid;
				idlest_cpu = ns.idlest_cpu;
				break;
			}
			is_prefer = 1;
		}

		if (is_prefer && !idlest_is_prefer)
			/* higher ~15% */
			imbalance_pct = 117;
		else if (!is_prefer && idlest_is_prefer)
			/* lower ~15% */
			imbalance_pct = 85;
		else
			imbalance_pct = 100;

		if (ns.util * 100 < util_min * imbalance_pct) {
			util_min = ns.util * 100 / imbalance_pct;
			idlest_nid = nid;
			idlest_cpu = ns.idlest_cpu;
			idlest_is_prefer = is_prefer;
		}
	}

	return idlest_cpu;
}

static int init_affinity_domains(struct affinity_domain *ad)
{
	struct sched_domain *tmp;
	int ret = -ENOMEM;
	int dcount = 0;
	int i = 0;
	int cpu;

	for (i = 0; i < AD_LEVEL_MAX; i++) {
		ad->domains[i] = kmalloc(sizeof(cpumask_t), GFP_KERNEL);
		if (!ad->domains[i])
			goto err;
	}

	rcu_read_lock();
	cpu = cpumask_first_and(cpu_active_mask,
				housekeeping_cpumask(HK_FLAG_DOMAIN));
	for_each_domain(cpu, tmp) {
		dcount++;
	}

	if (dcount > AD_LEVEL_MAX) {
		rcu_read_unlock();
		ret = -EINVAL;
		goto err;
	}

	i = 0;
	cpu = auto_affinity_find_idlest_cpu();
	for_each_domain(cpu, tmp) {
		cpumask_copy(ad->domains[i], sched_domain_span(tmp));
		__schedstat_set(ad->stay_cnt[i], 0);
		i++;
	}
	rcu_read_unlock();

	ad->dcount = dcount;
	ad->curr_level = ad->dcount > 0 ? ad->dcount - 1 : 0;
	ad->domain_mask = (1 << ad->dcount) - 1;

	ret = init_affinity_domains_orig(ad);
	if (ret)
		goto err;

	return 0;
err:
	free_affinity_domains(ad);
	return ret;
}

int init_auto_affinity(struct task_group *tg)
{
	struct auto_affinity *auto_affi;
	int ret;

	auto_affi = kzalloc(sizeof(*auto_affi), GFP_KERNEL);
	if (!auto_affi)
		return -ENOMEM;

	raw_spin_lock_init(&auto_affi->lock);
	auto_affi->mode = 0;
	auto_affi->period_active = 0;
	auto_affi->util_low_pct = -1;
	auto_affi->period = ms_to_ktime(AUTO_AFFINITY_DEFAULT_PERIOD_MS);
	hrtimer_init(&auto_affi->period_timer, CLOCK_MONOTONIC,
		HRTIMER_MODE_ABS_PINNED);
	auto_affi->period_timer.function = sched_auto_affi_period_timer;

	ret = init_affinity_domains(&auto_affi->ad);
	if (ret) {
		kfree(auto_affi);
		if (ret == -EINVAL)
			ret = 0;
		return ret;
	}

	auto_affi->tg = tg;
	tg->auto_affinity = auto_affi;
	return 0;
}

void destroy_auto_affinity(struct task_group *tg)
{
	struct auto_affinity *auto_affi = tg->auto_affinity;

	if (unlikely(!auto_affi))
		return;

	if (auto_affi->period_active)
		smart_grid_usage_dec();

	hrtimer_cancel(&auto_affi->period_timer);
	free_affinity_domains(&auto_affi->ad);

	kfree(tg->auto_affinity);
	tg->auto_affinity = NULL;
}

int tg_set_dynamic_affinity_mode(struct task_group *tg, u64 mode)
{
	struct auto_affinity *auto_affi = tg->auto_affinity;

	if (unlikely(!auto_affi))
		return -EPERM;

	/* auto mode */
	if (mode == 1) {
		start_auto_affinity(auto_affi);
	} else if (mode == 0) {
		stop_auto_affinity(auto_affi);
	} else {
		return -EINVAL;
	}

	return 0;
}

static inline struct task_group *css_tg(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct task_group, css) : NULL;
}

u64 cpu_affinity_mode_read_u64(struct cgroup_subsys_state *css,
			       struct cftype *cft)
{
	struct task_group *tg = css_tg(css);

	if (unlikely(!tg->auto_affinity))
		return -EPERM;

	return tg->auto_affinity->mode;
}

int cpu_affinity_mode_write_u64(struct cgroup_subsys_state *css,
				   struct cftype *cftype, u64 mode)
{
	return tg_set_dynamic_affinity_mode(css_tg(css), mode);
}

int tg_set_affinity_period(struct task_group *tg, u64 period_ms)
{
	if (unlikely(!tg->auto_affinity))
		return -EPERM;

	if (!period_ms || period_ms > U64_MAX / NSEC_PER_MSEC)
		return -EINVAL;

	raw_spin_lock_irq(&tg->auto_affinity->lock);
	tg->auto_affinity->period = ms_to_ktime(period_ms);
	raw_spin_unlock_irq(&tg->auto_affinity->lock);
	return 0;
}

u64 tg_get_affinity_period(struct task_group *tg)
{
	if (unlikely(!tg->auto_affinity))
		return -EPERM;

	return ktime_to_ms(tg->auto_affinity->period);
}

int cpu_affinity_period_write_uint(struct cgroup_subsys_state *css,
				   struct cftype *cftype, u64 period)
{
	return tg_set_affinity_period(css_tg(css), period);
}

u64 cpu_affinity_period_read_uint(struct cgroup_subsys_state *css,
				  struct cftype *cft)
{
	return tg_get_affinity_period(css_tg(css));
}

int cpu_affinity_domain_mask_write_u64(struct cgroup_subsys_state *css,
				       struct cftype *cftype, u64 mask)
{
	struct task_group *tg = css_tg(css);
	struct affinity_domain *ad;
	u16 full;

	if (unlikely(!tg->auto_affinity))
		return -EPERM;

	ad = &tg->auto_affinity->ad;
	full = (1 << ad->dcount) - 1;
	if (mask > full)
		return -EINVAL;

	raw_spin_lock_irq(&tg->auto_affinity->lock);
	ad->domain_mask = mask;
	raw_spin_unlock_irq(&tg->auto_affinity->lock);
	return 0;
}

u64 cpu_affinity_domain_mask_read_u64(struct cgroup_subsys_state *css,
				      struct cftype *cft)
{
	struct task_group *tg = css_tg(css);

	if (unlikely(!tg->auto_affinity))
		return -EPERM;

	return tg->auto_affinity->ad.domain_mask;
}

int cpu_affinity_util_low_pct_write(struct cgroup_subsys_state *css,
				    struct cftype *cftype, s64 util_pct)
{
	struct task_group *tg = css_tg(css);

	if (unlikely(!tg->auto_affinity))
		return -EPERM;

	if ((util_pct < 0 && util_pct != -1) || util_pct > 100)
		return -EINVAL;

	raw_spin_lock_irq(&tg->auto_affinity->lock);
	tg->auto_affinity->util_low_pct = util_pct;
	raw_spin_unlock_irq(&tg->auto_affinity->lock);
	return 0;
}

s64 cpu_affinity_util_low_pct_read(struct cgroup_subsys_state *css,
				   struct cftype *cft)
{
	struct task_group *tg = css_tg(css);

	if (unlikely(!tg->auto_affinity))
		return -EPERM;

	return tg->auto_affinity->util_low_pct;
}

int cpu_affinity_stat_show(struct seq_file *sf, void *v)
{
	struct task_group *tg = css_tg(seq_css(sf));
	struct auto_affinity *auto_affi = tg->auto_affinity;
	struct affinity_domain *ad;
	int i;

	if (unlikely(!auto_affi))
		return -EPERM;

	ad = &auto_affi->ad;
	seq_printf(sf, "period_active %d\n", auto_affi->period_active);
	seq_printf(sf, "dcount %d\n", ad->dcount);
	seq_printf(sf, "domain_mask 0x%x\n", ad->domain_mask);
	seq_printf(sf, "curr_level %d\n", ad->curr_level);
	for (i = 0; i < ad->dcount; i++)
		seq_printf(sf, "sd_level %d, cpu list %*pbl, stay_cnt %llu\n",
			i, cpumask_pr_args(ad->domains[i]),
			schedstat_val(ad->stay_cnt[i]));

	return 0;
}

int proc_cpu_affinity_domain_nodemask(struct ctl_table *table, int write,
				      void __user *buffer, size_t *lenp,
				      loff_t *ppos)
{
	int err;

	mutex_lock(&smart_grid_used_mutex);

	err = proc_do_large_bitmap(table, write, buffer, lenp, ppos);
	if (!err && write)
		nodes_and(smart_grid_preferred_nodemask,
			  smart_grid_preferred_nodemask,
			  node_online_map);

	mutex_unlock(&smart_grid_used_mutex);
	return err;
}
#else
static inline bool prefer_cpus_valid(struct task_struct *p);

static inline struct cpumask *task_prefer_cpus(struct task_struct *p)
{
	return p->prefer_cpus;
}

static inline int dynamic_affinity_mode(struct task_struct *p)
{
	if (!prefer_cpus_valid(p))
		return -1;

	return 0;
}
#endif
