# 09: Test dynarmic JIT flags

Status: open
Type: experiment
Label: ready-for-agent
Blocked by: 07

## Question

Do dynarmic flags that are off today make CPU-bound titles faster, without
breaking them?

## Context

- `cpu/src/dynarmic_cpu.cpp:349` uses `all_safe_optimizations` only.
- `fastmem_exclusive_access` is off (dynarmic default,
  `external/dynarmic/src/dynarmic/interface/A32/config.h:195`). So every
  LDREX and STREX goes through a slow callback. Vita code uses these for
  locks and atomics.
- Do this ticket only if ticket 07 marks a title CPU-bound with guest JIT
  code in its top functions.

## Steps

Add one temporary config value per flag. Test each flag alone with the
protocol, on the CPU-bound titles:

1. `fastmem_exclusive_access = true`. First check in the dynarmic source
   that the arm64 backend supports it. If not, skip it and say so.
2. `Unsafe_IgnoreGlobalMonitor`.
3. `Unsafe_UnfuseFMA`.
4. `Unsafe_ReducedErrorFP`.
5. `Unsafe_InaccurateNaN`.

For each flag, also play the scene for 5 minutes and look for crashes,
hangs, or wrong physics or animation.

## Output

Under `## Answer`: a table of flag, title, average FPS against A, and
problems. Keep the flags that help and cause no problems. Make them a
normal setting (default off everywhere) that the preset (ticket 16) can
turn on. Remove the others.

## Answer
