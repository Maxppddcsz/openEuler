// SPDX-License-Identifier: GPL-2.0

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include "gcov.h"

/*
 * __gcov_init is called by gcc-generated constructor code for each object
 * file compiled with -fprofile-arcs.
 */
void __gcov_init(struct gcov_info *info)
{
	static unsigned int gcov_version;

	mutex_lock(&gcov_lock);
	if (gcov_version == 0) {
		gcov_version = gcov_info_version(info);
		/*
		 * Printing gcc's version magic may prove useful for debugging
		 * incompatibility reports.
		 */
		pr_info("version magic: 0x%x\n", gcov_version);
	}
	/*
	 * Add new profiling data structure to list and inform event
	 * listener.
	 */
	gcov_info_link(info);
	if (gcov_events_enabled)
		gcov_event(GCOV_ADD, info);
	mutex_unlock(&gcov_lock);
}
EXPORT_SYMBOL(__gcov_init);

/*
 * These functions may be referenced by gcc-generated profiling code but serve
 * no function for kernel profiling.
 */
void __gcov_flush(void)
{
	/* Unused. */
}
EXPORT_SYMBOL(__gcov_flush);

void __gcov_merge_add(gcov_type *counters, unsigned int n_counters)
{
	/* Unused. */
}
EXPORT_SYMBOL(__gcov_merge_add);

void __gcov_merge_single(gcov_type *counters, unsigned int n_counters)
{
	/* Unused. */
}
EXPORT_SYMBOL(__gcov_merge_single);

void __gcov_merge_delta(gcov_type *counters, unsigned int n_counters)
{
	/* Unused. */
}
EXPORT_SYMBOL(__gcov_merge_delta);

void __gcov_merge_ior(gcov_type *counters, unsigned int n_counters)
{
	/* Unused. */
}
EXPORT_SYMBOL(__gcov_merge_ior);

void __gcov_merge_time_profile(gcov_type *counters, unsigned int n_counters)
{
	/* Unused. */
}
EXPORT_SYMBOL(__gcov_merge_time_profile);

void __gcov_merge_icall_topn(gcov_type *counters, unsigned int n_counters)
{
	/* Unused. */
}
EXPORT_SYMBOL(__gcov_merge_icall_topn);

void __gcov_exit(void)
{
	/* Unused. */
}
EXPORT_SYMBOL(__gcov_exit);

#ifdef CONFIG_PGO_KERNEL
/* Maximum number of tracked TOP N value profiles.  */
#define GCOV_TOPN_MAXIMUM_TRACKED_VALUES 32

void __gcov_merge_topn(gcov_type *counters, unsigned int n_counters)
{
	/* Unused. */
}
EXPORT_SYMBOL(__gcov_merge_topn);

struct indirect_call_tuple {
	void *callee;

	gcov_type *counters;
};

/* Kernel does not support __thread keyword. */
struct indirect_call_tuple __gcov_indirect_call;
EXPORT_SYMBOL(__gcov_indirect_call);

gcov_type __gcov_time_profiler_counter;
EXPORT_SYMBOL(__gcov_time_profiler_counter);

/* GCOV key-value pair linked list type.  */
struct gcov_kvp
{
	gcov_type value;
	gcov_type count;
	struct gcov_kvp *next;
};

/*
 * Tries to determine N most commons value among its inputs.
 */
static inline void __gcov_topn_values_profiler_body(gcov_type *counters,
		gcov_type value)
{
	gcov_type count = 1;

	/* In the multi-threaded mode, we can have an already merged profile
	with a negative total value.  In that case, we should bail out.  */
	if (counters[0] < 0)
		return;
	counters[0]++;

	struct gcov_kvp *prev_node = NULL;
	struct gcov_kvp *minimal_node = NULL;
	struct gcov_kvp *current_node = (struct gcov_kvp *)(intptr_t)counters[2];

	while (current_node) {
		if (current_node->value == value) {
			current_node->count += count;
			return;
		}

		if (minimal_node == NULL
		|| current_node->count < minimal_node->count)
			minimal_node = current_node;

		prev_node = current_node;
		current_node = current_node->next;
	}

	if (counters[1] == GCOV_TOPN_MAXIMUM_TRACKED_VALUES) {
		if (--minimal_node->count < count) {
			minimal_node->value = value;
			minimal_node->count = count;
		}

		return;
	} else {
		struct gcov_kvp *new_node = (struct gcov_kvp *)kcalloc(1, sizeof(struct gcov_kvp), GFP_KERNEL);
		// The original gcc code uses calloc.
		// struct gcov_kvp *new_node = (struct gcov_kvp *)calloc(1, sizeof(struct gcov_kvp));
		// struct gcov_kvp *new_node = allocate_gcov_kvp();
		if (new_node == NULL)
			return;

		new_node->value = value;
		new_node->count = count;

		int success = 0;
		if (!counters[2]) {
			counters[2] = (intptr_t)new_node;
			success = 1;
		} else if (prev_node && !prev_node->next) {
			prev_node->next = new_node;
			success = 1;
		}

		/* Increment number of nodes.  */
		if (success)
			counters[1]++;
	}
}

void __gcov_topn_values_profiler(gcov_type *counters, gcov_type value)
{
	__gcov_topn_values_profiler_body(counters, value);
}
EXPORT_SYMBOL(__gcov_topn_values_profiler);

/*
 * Tries to determine the most common value among its inputs.
 */
static inline void __gcov_indirect_call_profiler_body(gcov_type value,
		void *cur_func)
{
	/* Removed the C++ virtual tables contents as kernel is written in C. */
	if (cur_func == __gcov_indirect_call.callee)
		__gcov_topn_values_profiler_body(__gcov_indirect_call.counters, value);

	__gcov_indirect_call.callee = NULL;
}

void __gcov_indirect_call_profiler_v4(gcov_type value, void *cur_func)
{
	__gcov_indirect_call_profiler_body(value, cur_func);
}
EXPORT_SYMBOL(__gcov_indirect_call_profiler_v4);

/*
 * Increase corresponding COUNTER by VALUE.
 */
void __gcov_average_profiler(gcov_type *counters, gcov_type value)
{
	counters[0] += value;
	counters[1]++;
}
EXPORT_SYMBOL(__gcov_average_profiler);

void __gcov_ior_profiler(gcov_type *counters, gcov_type value)
{
	*counters |= value;
}
EXPORT_SYMBOL(__gcov_ior_profiler);

/*
 * If VALUE is a power of two, COUNTERS[1] is incremented.	Otherwise
 * COUNTERS[0] is incremented.
 */
void __gcov_pow2_profiler(gcov_type *counters, gcov_type value)
{
	if (value == 0 || (value & (value - 1)))
		counters[0]++;
	else
		counters[1]++;
}
EXPORT_SYMBOL(__gcov_pow2_profiler);

/*
 * If VALUE is in interval <START, START + STEPS - 1>, then increases the
 * corresponding counter in COUNTERS.	If the VALUE is above or below
 * the interval, COUNTERS[STEPS] or COUNTERS[STEPS + 1] is increased
 * instead.
 */
void __gcov_interval_profiler(gcov_type *counters, gcov_type value,
		int start, unsigned int steps)
{
	gcov_type delta = value - start;

	if (delta < 0)
		counters[steps + 1]++;
	else if (delta >= steps)
		counters[steps]++;
	else
		counters[delta]++;
}
EXPORT_SYMBOL(__gcov_interval_profiler);
#endif