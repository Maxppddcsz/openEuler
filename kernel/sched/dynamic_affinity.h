/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_DYNAMIC_AFFINITY_INTERNAL_H
#define _LINUX_SCHED_DYNAMIC_AFFINITY_INTERNAL_H

#ifdef CONFIG_QOS_SCHED_SMART_GRID
#define AD_LEVEL_MAX		8

struct affinity_domain {
	int			dcount;
	int			curr_level;
	u32			domain_mask;
#ifdef CONFIG_SCHEDSTATS
	u64			stay_cnt[AD_LEVEL_MAX];
#endif
	struct cpumask		*domains[AD_LEVEL_MAX];
	struct cpumask		*domains_orig[AD_LEVEL_MAX];
};

struct auto_affinity {
	raw_spinlock_t		lock;
	u64			mode;
	ktime_t			period;
	struct hrtimer		period_timer;
	int			period_active;
	struct affinity_domain	ad;
	struct task_group	*tg;
};

extern void start_auto_affinity(struct auto_affinity *auto_affi);
extern void stop_auto_affinity(struct auto_affinity *auto_affi);
extern int init_auto_affinity(struct task_group *tg);
extern void destroy_auto_affinity(struct task_group *tg);
extern void tg_update_affinity_domains(int cpu, int online);
extern u64 cpu_affinity_mode_read_u64(struct cgroup_subsys_state *css,
				      struct cftype *cft);
extern int cpu_affinity_mode_write_u64(struct cgroup_subsys_state *css,
				       struct cftype *cftype, u64 mode);
extern int cpu_affinity_period_write_uint(struct cgroup_subsys_state *css,
					  struct cftype *cftype, u64 period);
extern u64 cpu_affinity_period_read_uint(struct cgroup_subsys_state *css,
					 struct cftype *cft);
extern int cpu_affinity_domain_mask_write_u64(struct cgroup_subsys_state *css,
					      struct cftype *cftype, u64 mask);
extern u64 cpu_affinity_domain_mask_read_u64(struct cgroup_subsys_state *css,
					     struct cftype *cft);
extern int cpu_affinity_stat_show(struct seq_file *sf, void *v);
#endif
#endif
