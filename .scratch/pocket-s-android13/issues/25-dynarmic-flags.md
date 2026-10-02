# 25: Test dynarmic optimization flags

Status: open
Type: experiment
Label: ready-for-human
Blocked by: none

## Gate, closed by ticket 00

Do this ticket only if a title is CPU-bound, with guest JIT code in the top
functions of a simpleperf report.

**Ticket 00 already closed the gate.** The emulator overlay reads GPU 99% busy
on the stock driver and 93% on Turnip, against CPU 23% and 43%. The frame rate
is set by the GPU. Set `Status: rejected` and record those numbers here.

Do not wait for ticket 05 to run a full matrix first. Ticket 05's own threshold
would also misclassify a 93%-busy title as not-CPU-bound, because it requires
`gpu_busy_percentage` below 90%, so the gate would close for the wrong reason
even if it were deferred.

If the gate is ever reopened, run `Unsafe_IgnoreGlobalMonitor` alone. It is the
only flag whose mechanism matches the code: it removes the shared monitor from
the optimization set, and every guest exclusive access takes that monitor.
`Unsafe_ReducedErrorFP` and `Unsafe_InaccurateNaN` also change float results,
which is a correctness risk for a measured gain of nothing.

This replaces `.scratch/pocket-s-optimization/issues/09-dynarmic-flags.md`,
which is open on the old base and has never been measured.

## Context

`vita3k/cpu/src/dynarmic_cpu.cpp:664` sets
`cpu_opt ? all_safe_optimizations : no_optimizations` and nothing else.
`config.global_monitor` points at one shared `Dynarmic::ExclusiveMonitor` sized
`MAX_CORE_COUNT` = 150 (`vita3k/cpu/include/cpu/common.h:38`, assigned at
`dynarmic_cpu.cpp:656`). Every guest exclusive access takes that monitor, so
the guest's locks and atomics serialize on one host mutex.

The flags to test, in order of expected effect:

- `Unsafe_IgnoreGlobalMonitor`. Removes the global monitor from the
  optimization set. This is the flag that matches the shared lock above.
- `Unsafe_UnfuseFMA`. Emits a multiply and an add as two instructions instead
  of one fused instruction.
- `Unsafe_ReducedErrorFP`. Allows faster float that does not round correctly.
- `Unsafe_InaccurateNaN`. Allows NaN handling that is not correct.

`Dynarmic::fastmem_exclusive_access` is not on this list. The emulator builds
the `Dynarmic::A32` frontend and backend (`dynarmic_cpu.cpp:642`, `:667`), and
that option is only read by the x64 backend of dynarmic. Read
`external/dynarmic/include/dynarmic/interface/a32/config.h` after
`git submodule update --init --recursive` and record the exact name and line of
each flag before using it.

## Steps

1. Add one temporary config value per flag, each defaulting to today's value.
2. Run A/B/A per flag alone, on the CPU-bound titles only. Record FPS, the frame
   interval 99th percentile, and `cpu-cycles` per frame from simpleperf.
   `cpu-cycles` per frame is the metric that shows whether the work got cheaper.
3. For each flag that helps on FPS, play the scene for 5 minutes and record any
   crash, hang, wrong physics or wrong animation. A flag with no problem after
   5 minutes is not cleared, so say so in the answer.
4. `Unsafe_ReducedErrorFP` and `Unsafe_InaccurateNaN` change float results.
   Record a screenshot of a scene with water, a gradient and a particle effect,
   in A and in B, and write down every difference.

## Acceptance

- A table of flag, title, FPS against A, `cpu-cycles` per frame against A, and
  any problem seen in 5 minutes.
- Flags that are kept are behind a normal config setting, default off on every
  device, so ticket 22 turns them on for this device only.
- The gate numbers from ticket 05 that opened or closed this ticket.

## Answer

## Comments
