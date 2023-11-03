// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(C) 2015 Linaro Limited. All rights reserved.
 * Author: Mathieu Poirier <mathieu.poirier@linaro.org>
 */

#include <stdbool.h>
#include <linux/coresight-pmu.h>

#include "../../util/auxtrace.h"
#include "../../util/evlist.h"
#include "../../util/pmu.h"
#include "cs-etm.h"
#include "arm-spe.h"

static struct perf_pmu **find_all_arm_spe_pmus(int *nr_spes, int *err)
{
	struct perf_pmu **arm_spe_pmus = NULL;
	int ret, i, nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	/* arm_spe_xxxxxxxxx\0 */
	char arm_spe_pmu_name[sizeof(ARM_SPE_PMU_NAME) + 10];

	arm_spe_pmus = zalloc(sizeof(struct perf_pmu *) * nr_cpus);
	if (!arm_spe_pmus) {
		pr_err("spes alloc failed\n");
		*err = -ENOMEM;
		return NULL;
	}

	for (i = 0; i < nr_cpus; i++) {
		ret = sprintf(arm_spe_pmu_name, "%s%d", ARM_SPE_PMU_NAME, i);
		if (ret < 0) {
			pr_err("sprintf failed\n");
			*err = -ENOMEM;
			return NULL;
		}

		arm_spe_pmus[*nr_spes] = perf_pmu__find(arm_spe_pmu_name);
		if (arm_spe_pmus[*nr_spes]) {
			pr_debug2("%s %d: arm_spe_pmu %d type %d name %s\n",
				 __func__, __LINE__, *nr_spes,
				 arm_spe_pmus[*nr_spes]->type,
				 arm_spe_pmus[*nr_spes]->name);
			(*nr_spes)++;
		}
	}

	return arm_spe_pmus;
}

static struct perf_pmu *find_pmu_for_event(struct perf_pmu **pmus,
					   int pmu_nr, struct perf_evsel *evsel)
{
	int i;

	if (!pmus)
		return NULL;

	for (i = 0; i < pmu_nr; i++) {
		if (evsel->attr.type == pmus[i]->type)
			return pmus[i];
	}

	return NULL;
}

struct auxtrace_record
*auxtrace_record__init(struct perf_evlist *evlist, int *err)
{
	struct perf_pmu	*cs_etm_pmu = NULL;
	struct perf_pmu **arm_spe_pmus = NULL;
	struct perf_evsel *evsel;
	struct perf_pmu *found_etm = NULL;
	struct perf_pmu *found_spe = NULL;
	int auxtrace_event_cnt = 0;
	int nr_spes = 0;

	if (!evlist)
		return NULL;

	cs_etm_pmu = perf_pmu__find(CORESIGHT_ETM_PMU_NAME);
	arm_spe_pmus = find_all_arm_spe_pmus(&nr_spes, err);

	evlist__for_each_entry(evlist, evsel) {
		if (cs_etm_pmu && !found_etm)
			found_etm = find_pmu_for_event(&cs_etm_pmu, 1, evsel);

		if (arm_spe_pmus && !found_spe)
			found_spe = find_pmu_for_event(arm_spe_pmus,
						       nr_spes,
						       evsel);
	}

	free(arm_spe_pmus);

	if (found_etm)
		auxtrace_event_cnt++;

	if (found_spe)
		auxtrace_event_cnt++;

	if (auxtrace_event_cnt > 1) {
		pr_err("Concurrent AUX trace operation not currently supported\n");
		*err = -EOPNOTSUPP;
		return NULL;
	}

	if (found_etm)
		return cs_etm_record_init(err);

#if defined(__aarch64__)
	if (found_spe)
		return arm_spe_recording_init(err, found_spe);
#endif

	/*
	 * Clear 'err' even if we haven't found an event - that way perf
	 * record can still be used even if tracers aren't present.  The NULL
	 * return value will take care of telling the infrastructure HW tracing
	 * isn't available.
	 */
	*err = 0;
	return NULL;
}
