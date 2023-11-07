============================================
Branch Record Buffer Extension aka FEAT_BRBE
============================================

Author: Anshuman Khandual <anshuman.khandual@arm.com>

FEAT_BRBE is an optional architecture feature, which creates branch records
containing information about change in control flow. The branch information
contains source address, target address, and some relevant metadata related
to that change in control flow. BRBE can be configured to filter out branch
records based on type and privilege level.

BRBE Hardware
=============

FEAT_BRBE support on a given implementation, can be determined from system
register field ID_AA64DFR0_EL1.BRBE.

1) Branch Record System Registers
---------------------------------

A single branch record is constructed from three distinct system registers.

1. BRBSRC<N>_EL1 - Branch record source address register
2. BRBTGT<N>_EL1 - Branch record target address register
3. BRBINF<N>_EL1 - Branch record information register

'N' mentioned above ranges inside [0 .. 31] indices which forms a complete
branch records bank and the implementation can have multiple such banks of
branch records.

2) Branch Record Generation System Registers
--------------------------------------------

Branch record generation and capture control system registers

1. BRBCR_EL1	- Branch record generation control
2. BRBCR_EL2	- Branch record generation control
3. BRBFCR_EL1	- Branch record function control

3) Branch Record Generation Filters and Controls
------------------------------------------------

Branch records generation can be filtered based on control flow change type
and respective execution privilege level. Some additional branch record
relevant information such as elapsed cycles count, and prediction can also
be selectively enabled.

4) Branch Record Information
----------------------------

Apart from branch source and destination addresses, captured branch records
also contain information such as prediction, privilege levels, cycle count,
and transaction state, record type.

Perf Implementation
===================

Perf branch stack sampling framework has been enabled on arm64 platform via
this new FEAT_BRBE feature. The following description explains how this has
been implemented in various levels of abstraction from perf core all the
way to ARMv8 PMUv3 implementation.

1) Branch stack abstraction at ARM PMU
--------------------------------------

Basic branch stack abstractions such as 'has_branch_stack' pmu feature flag
in 'struct arm_pmu', defining 'struct branch_records' based branch records
buffer in 'struct pmu_hw_events' have been implemented at ARM PMU level.

2) Branch stack implementation at ARMv8 PMUv3
---------------------------------------------

Basic branch stack driving callbacks armv8pmu_branch_xxx() alongside normal
PMU HW events have been implemented at ARMv8 PMUv3 level with fallback stub
definitions in case where a given ARMv8 PMUv3 does not implement FEAT_BRBE.

**Detect branch stack support**

__armv8pmu_probe_pmu()
	armv8pmu_branch_probe()
		arm_pmu->has_branch_stack = 1

**Allocate branch stack buffers**

__armv8pmu_probe_pmu()
	armv8pmu_branch_probe()
		arm_pmu->has_branch_stack
			- armv8pmu_task_ctx_cache_alloc()
			- branch_records_alloc()

**Allow branch stack event**

armpmu_event_init()
	armpmu->has_branch_stack
		has_branch_stack()
			- branch event allowed to be created

**Check branch stack event feasibility**

__armv8_pmuv3_map_event()
	has_branch_stack()
		- event->attach_state | PERF_ATTACH_TASK_DATA
		- armv8pmu_branch_attr_valid()

**Enable branch record generation**

armv8pmu_enable_event()
	has_branch_stack()
		armv8pmu_branch_enable()

**Disable branch record generation**

armv8pmu_disable_event()
	has_branch_stack()
		armv8pmu_branch_disable()

**Capture branch record at PMU IRQ**

armv8pmu_handle_irq()
	has_branch_stack()
		armv8pmu_branch_read()
		perf_sample_save_brstack()

**Process context sched in or out**

armv8pmu_sched_task()
	armpmu->has_branch_stack()
		- armv8pmu_branch_reset() --> sched_in
		- armv8pmu_branch_save()  --> sched_out

**Reset branch stack buffer**

armv8pmu_reset()
	armpmu->has_branch_stack
		armv8pmu_branch_reset()


3) BRBE implementation at ARMv8 PMUv3
-------------------------------------

FEAT_BRBE specific branch stack callbacks are implemented and are available
via new CONFIG_ARM64_BRBE config option. These implementation callbacks
drive branch records generation control, and capture along side regular PMU
HW events at ARMv8 PMUv3 level.
