/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_DYNAMIC_AFFINITY_H
#define _LINUX_SCHED_DYNAMIC_AFFINITY_H

struct dyn_affinity_stats {
#ifdef CONFIG_SCHEDSTATS
	u64				nr_wakeups_preferred_cpus;
	u64				nr_wakeups_force_preferred_cpus;
#endif
};

extern void dynamic_affinity_enable(void);
extern int sched_prefer_cpus_fork(struct task_struct *p,
				  struct task_struct *orig);
extern void sched_prefer_cpus_free(struct task_struct *p);
extern void task_cpus_preferred(struct seq_file *m, struct task_struct *task);
extern int set_prefer_cpus_ptr(struct task_struct *p,
			       const struct cpumask *new_mask);

#ifdef CONFIG_QOS_SCHED_SMART_GRID
extern unsigned long *smart_grid_preferred_nodemask_bits;
extern int proc_cpu_affinity_domain_nodemask(struct ctl_table *table, int write,
					     void __user *buffer, size_t *lenp,
					     loff_t *ppos);

extern struct static_key __smart_grid_used;
static inline bool smart_grid_used(void)
{
	return static_key_false(&__smart_grid_used);
}
#else
static inline bool smart_grid_used(void)
{
	return false;
}
#endif

#endif
