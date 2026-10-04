# Hot-path counters

## The question

Which emulator code runs most, and where does the guest-to-host boundary cost
the most? A counter at each choke point answers this without a profiler and
without distorting the run.

## Why not per-instruction guest counters

The first version of this plan counted guest instructions. It was rejected on
cost and on metric.

**Cost.** `ArmDynarmicCallback::PreCodeTranslationHook`
(`vita3k/cpu/src/dynarmic_cpu.cpp:373`) is the injection point, and the
existing `log_code` path proves that code emitted there runs on every
execution. But dynarmic's IR has one host-interaction opcode only,
`CallHostFunction` (`external/dynarmic/src/dynarmic/ir/opcodes.inc:8`). There
is no `IncrementMem`. A per-instruction counter is therefore one host call per
executed guest instruction, which is unusable at game instruction rates and
distorts the measurement it is taking.

**Metric.** A hot guest instruction describes the game. It does not name a line
of emulator code to change.

## What already exists, and what is still missing

Host-side profiling works and has run. `docs/adr/0001-android-profiling-tools.md`
and `.scratch/pocket-s-android13/issues/03-profile-harness.md` built it, and
ticket 32 holds the results: xxHash 5.41%, `__kernel_clock_gettime` 5.28%,
`add_protect` 1.37%, host atomics 12.9%.

The gap is 6.05% of samples reading `unknown[+717551200c]` and two neighbours
within 20 bytes. That is dynarmic's JIT output in an anonymous code cache.
simpleperf cannot name it because no guest-to-host address map is exposed. That
gap is ticket 03 of this plan, and it is a research question.

## The decision

Counters at choke points the emulator already owns, behind config values that
default to off.

- **01 HLE import counters per NID.** `call_import`
  (`vita3k/modules/module_parent.cpp:152`) is the single choke point for every
  guest-to-host call.
- **02 Translator and fast-path counters.** Four counters on existing branches
  in the dynarmic callbacks.
- **03 Name the unknown samples.** Research. No code unless dynarmic turns out
  to expose what is needed.

### Self-time is a separate flag

Ticket 32 measured `__kernel_clock_gettime` at 5.28% and named it as a cost to
remove. Timing every HLE call reads the clock twice per call, which works
against that finding. So call counts are the default mode and cost one relaxed
atomic increment, and self-time sits behind a second flag that is off by
default.

## Layout

One component, `vita3k/util/`, namespace `hotpath`, next to `perf_log` because
it reuses that writer. `util` is reachable from both `cpu` and `modules`, which
are the two libraries that need it.

Per-NID counters live in a fixed open-addressed table with linear probing and
atomic insert, so the hot path allocates nothing and takes no lock. NID 0 marks
an empty slot and is counted separately as an overflow, so the table cannot lose
a call silently.

Deltas are written per interval through `perf_log`, so a long session reads as a
rate rather than a total. `exchange` on the counters gives the delta and resets
in one atomic step.

## Out of scope

- Per-instruction guest counters. Rejected above.
- Patching dynarmic. See 03.
- Any change to a default setting.
- The GPU. Ticket 00 measured it at 93% busy, so these counters explain the CPU
  side, which is the smaller share.

## What these numbers do not say

A counter says what ran, not why it was slow. A high HLE count marks a function
worth reading. It does not name the fix.