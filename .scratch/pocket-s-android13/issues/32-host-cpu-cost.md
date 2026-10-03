# 32: Account for the host CPU cost outside the guest and the renderer

Status: open
Type: research
Label: ready-for-agent
Blocked by: 03

## Question

Seven host-side costs sit outside guest code and outside the renderer, and
together they are more than a fifth of all CPU samples in a gameplay run. What
are they, and is any of them removable?

## What was measured

Ticket 03's 30 second simpleperf run of Uncharted on Turnip, 250300 samples.
Percentages are of all samples.

| self | object | symbol |
| --- | --- | --- |
| 5.41% | libVita3K | `XXH_INLINE_XXH3_64bits_update` |
| 5.28% | vdso | `__kernel_clock_gettime` |
| 1.37% | libVita3K | `add_protect(MemState&, uint32_t, uint32_t, MemPerm, ...)` |
| 1.34% | libVita3K | `__aarch64_ldadd8_rel` on `vita3k-render` |
| 0.84% | libVita3K | `__aarch64_ldadd8_acq_rel` on `vita3k-render` |
| 0.64% | libVita3K | `__aarch64_ldadd8_relax` on `vita3k-render` |
| 0.61% | libc | `__aarch64_cas4_acq` on `vita3k-render` |
| 2.85% | unknown | `unknown[+717551200c]` |
| 2.20% | unknown | `unknown[+7175512010]` |
| 1.00% | unknown | `unknown[+717551201c]` |

The host atomics total 12.9% of all samples across three thread groups: the
guest threads, `vita3k-render` at about 3.75%, and `WorkerThread-0` at about
3.82%.

## Two corrections this ticket starts from

**`accurate-thread-scheduling` is not the cause of the atomics.** An earlier note
in ticket 03 said it was. That was wrong. The setting is a mutex that gates guest
thread execution, `sched_acquire` at `vita3k/kernel/src/thread.cpp:252` and its
one caller at `:610`. It issues no atomics of its own, and `vita3k-render` runs
no guest thread, so no guest scheduling gate applies to the 3.75% on it.
Measuring the setting is still worth doing for pacing, and ticket 05 already
owns that row. Do not repeat the A/B here.

**The guest-side atomics are not this ticket's subject.** They go through the one
shared `Dynarmic::ExclusiveMonitor` that
`Dynarmic::ExclusiveMonitor::CheckAndClear` at 1.49% is, and ticket 25 already
owns whether a dynarmic flag removes it. `PCSA00029` carries
`__aarch64_cas4_acq` 1.91% and `__aarch64_ldset4_acq_rel` 1.44%; leave those to
25 and record the split here so 25 can read it.

## What is worth chasing, in order

1. **The three `unknown` clusters, 6.05% together.** Their addresses sit within
   20 bytes of each other, so this is one out-of-range mapping in one function
   rather than three separate things. Until it is named, 6.05% of the profile is
   unattributed and any conclusion drawn from the rest is provisional. Get the
   name first. `device.sh perf` already pushes the unstripped library, so this is
   a question of what that address range is, not of new tooling.
2. **`clock_gettime` at 5.28%, from the vDSO.** Something is reading the clock
   very often. Candidates worth checking: the perf-log writer that ticket 02
   ported, the vblank clock at `vita3k/display/src/display.cpp:414-416`, and the
   NGS and audio timing. Ticket 10 owns the vblank clock's correctness but not its
   cost. One counter around the clock call sites is enough to tell which.
3. **xxHash at 5.41%.** Four files call it: `vita3k/renderer/src/texture/cache.cpp`,
   `vita3k/renderer/src/vulkan/pipeline_cache.cpp`,
   `vita3k/renderer/src/vulkan/creation.cpp` and
   `vita3k/modules/SceGxm/SceGxm.cpp`. Attribute the 5.41% to one of them before
   changing anything. If it is a per-frame hash of texture data, the cost scales
   with the bytes hashed and the fix is in how much is hashed, not in the hash
   function.
4. **`add_protect` at 1.37%.** `vita3k/mem/src/mem.cpp:612`. Guest memory
   permissions are being changed during the run. Find what calls it on a hot path.
   Ticket 06 owns the memory mapping modes and 12 the large mappings; record what
   this costs so they can use the number.
5. **The 3.75% of atomics on `vita3k-render`.** These are host refcounting and
   container operations in the renderer, not guest memory. Read the call graph for
   `__aarch64_ldadd8_rel` with `device.sh perf` and `-g`, and name the callers.
   This is the only part of the atomic cost that is plainly ours.

## What this is worth

The same as ticket 31: the GPU is the limit on this device, so none of this moves
the frame rate, and the plan asks about energy per played frame. The CPU was at
23 to 43% while the GPU was at 93 to 99% (ticket 00), so there is spare CPU
capacity to spend. Record the energy effect and do not present a CPU percentage
as a frame-rate result.

## Steps

1. Name the three `unknown` clusters before anything else. Record the function.
2. For each of the four remaining items, record one attribution: the file and
   line, and the share of samples it owns. A share with no caller is not an
   answer.
3. Only where a caller is found and is plainly wasteful, propose a change behind
   a temporary config value that defaults to today's behaviour, as
   `docs/agent-loop.md` requires. Do not propose a change for the guest-side
   atomics, which belong to ticket 25.
4. Measure energy with the three commands of `../spec.md`, A/B/A, on the 30 FPS
   title, and record the frame interval 99th percentile beside the power so a
   playability regression is visible.

## Acceptance

- The 6.05% of unnamed samples is either named or shown to be unnameable, with
  the reason.
- One attribution per remaining item, each with a file and a line.
- A statement for each item: removable, not removable, or belongs to another
  ticket.

## Answer

## Comments