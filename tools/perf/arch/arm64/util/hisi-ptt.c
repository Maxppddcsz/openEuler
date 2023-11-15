// SPDX-License-Identifier: GPL-2.0
/*
 * HiSilicon PCIe Trace and Tuning (PTT) support
 * Copyright (c) 2022 HiSilicon Technologies Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/log2.h>
#include <time.h>

#include "../../../util/auxtrace.h"
#include "../../../util/cpumap.h"
#include "../../../util/debug.h"
#include "../../../util/event.h"
#include "../../../util/evlist.h"
#include "../../../util/evsel.h"
#include "../../../util/hisi-ptt.h"
#include "../../../util/pmu.h"
#include "../../../util/session.h"
#include "../../../util/tsc.h"

#define KiB(x) ((x) * 1024)
#define MiB(x) ((x) * 1024 * 1024)

struct hisi_ptt_recording {
	struct auxtrace_record	itr;
	struct perf_pmu *hisi_ptt_pmu;
	struct perf_evlist *evlist;
};

static size_t
hisi_ptt_info_priv_size(struct auxtrace_record *itr __maybe_unused,
			struct perf_evlist *evlist __maybe_unused)
{
	return HISI_PTT_AUXTRACE_PRIV_SIZE;
}

static int hisi_ptt_info_fill(struct auxtrace_record *itr,
			      struct perf_session *session,
			      struct auxtrace_info_event *auxtrace_info,
			      size_t priv_size)
{
	struct hisi_ptt_recording *pttr =
			container_of(itr, struct hisi_ptt_recording, itr);
	struct perf_pmu *hisi_ptt_pmu = pttr->hisi_ptt_pmu;

	if (priv_size != HISI_PTT_AUXTRACE_PRIV_SIZE)
		return -EINVAL;

	if (!session->evlist->nr_mmaps)
		return -EINVAL;

	auxtrace_info->type = PERF_AUXTRACE_HISI_PTT;
	auxtrace_info->priv[0] = hisi_ptt_pmu->type;

	return 0;
}

static int hisi_ptt_set_auxtrace_mmap_page(struct record_opts *opts)
{
	bool privileged = geteuid() == 0 || perf_event_paranoid() < 0;

	if (!opts->full_auxtrace)
		return 0;

	if (opts->full_auxtrace && !opts->auxtrace_mmap_pages) {
		if (privileged) {
			opts->auxtrace_mmap_pages = MiB(16) / page_size;
		} else {
			opts->auxtrace_mmap_pages = KiB(128) / page_size;
			if (opts->mmap_pages == UINT_MAX)
				opts->mmap_pages = KiB(256) / page_size;
		}
	}

	/* Validate auxtrace_mmap_pages */
	if (opts->auxtrace_mmap_pages) {
		size_t sz = opts->auxtrace_mmap_pages * (size_t)page_size;
		size_t min_sz = KiB(8);

		if (sz < min_sz || !is_power_of_2(sz)) {
			pr_err("Invalid mmap size for HISI PTT: must be at least %zuKiB and a power of 2\n",
			       min_sz / 1024);
			return -EINVAL;
		}
	}

	return 0;
}

static int hisi_ptt_recording_options(struct auxtrace_record *itr,
				      struct perf_evlist *evlist,
				      struct record_opts *opts)
{
	struct hisi_ptt_recording *pttr =
			container_of(itr, struct hisi_ptt_recording, itr);
	struct perf_pmu *hisi_ptt_pmu = pttr->hisi_ptt_pmu;
	struct perf_evsel *evsel, *hisi_ptt_evsel = NULL;
	struct perf_evsel *tracking_evsel;
	int err;

	pttr->evlist = evlist;
	evlist__for_each_entry(evlist, evsel) {
		if (evsel->attr.type == hisi_ptt_pmu->type) {
			if (hisi_ptt_evsel) {
				pr_err("There may be only one "
				       HISI_PTT_PMU_NAME "x event\n");
				return -EINVAL;
			}
			evsel->attr.freq = 0;
			evsel->attr.sample_period = 1;
			hisi_ptt_evsel = evsel;
			opts->full_auxtrace = true;
		}
	}

	err = hisi_ptt_set_auxtrace_mmap_page(opts);
	if (err)
		return err;
	/*
	 * To obtain the auxtrace buffer file descriptor, the auxtrace event
	 * must come first.
	 */
	perf_evlist__to_front(evlist, hisi_ptt_evsel);
	perf_evsel__set_sample_bit(hisi_ptt_evsel, TIME);

	/* Add dummy event to keep tracking */
	err = parse_events(evlist, "dummy:u", NULL);
	if (err)
		return err;

	tracking_evsel = perf_evlist__last(evlist);
	perf_evlist__set_tracking_event(evlist, tracking_evsel);

	tracking_evsel->attr.freq = 0;
	tracking_evsel->attr.sample_period = 1;
	perf_evsel__set_sample_bit(tracking_evsel, TIME);

	return 0;
}

static u64 hisi_ptt_reference(struct auxtrace_record *itr __maybe_unused)
{
	return rdtsc();
}

static void hisi_ptt_recording_free(struct auxtrace_record *itr)
{
	struct hisi_ptt_recording *pttr =
			container_of(itr, struct hisi_ptt_recording, itr);

	free(pttr);
}

static int hisi_ptt_read_finish(struct auxtrace_record *itr, int idx)
{
	struct hisi_ptt_recording *sper =
			container_of(itr, struct hisi_ptt_recording, itr);
	struct perf_evsel *evsel;

	evlist__for_each_entry(sper->evlist, evsel) {
		if (evsel->attr.type == sper->hisi_ptt_pmu->type) {
			if (evsel->terminated)
				return 0;
			else
				return perf_evlist__enable_event_idx(
						sper->evlist, evsel, idx);
		}
	}
	return -EINVAL;
}

struct auxtrace_record *hisi_ptt_recording_init(int *err,
						struct perf_pmu *hisi_ptt_pmu)
{
	struct hisi_ptt_recording *pttr;

	if (!hisi_ptt_pmu) {
		*err = -ENODEV;
		return NULL;
	}

	pttr = zalloc(sizeof(*pttr));
	if (!pttr) {
		*err = -ENOMEM;
		return NULL;
	}

	pttr->hisi_ptt_pmu = hisi_ptt_pmu;
	pttr->itr.recording_options = hisi_ptt_recording_options;
	pttr->itr.info_priv_size = hisi_ptt_info_priv_size;
	pttr->itr.info_fill = hisi_ptt_info_fill;
	pttr->itr.free = hisi_ptt_recording_free;
	pttr->itr.reference = hisi_ptt_reference;
	pttr->itr.read_finish = hisi_ptt_read_finish;
	pttr->itr.alignment = 0;

	*err = 0;
	return &pttr->itr;
}
