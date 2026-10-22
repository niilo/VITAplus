# Plus experiment review

Upstream baseline: `a86b5f93db43e4c4640ad1527e2a4695aad0f35b`.
Baseline packaging source: `9197e56160c32ad7610df870c55bbd89a5bb4fa0`.
The build generates the logging identity header required but not tracked by
Plus. Its constants are recorded by hash in each manifest.
The baseline keeps native code, Android frontend behavior, dependencies,
shaders and data files unchanged. Its first device question is whether the
Jak and Uncharted defects match the reference Plus build.

## First candidate: NGS pool block splitting

`MemspaceBlockAllocator::alloc` grows its vector with `resize` before
inserting a split block. When capacity equals size, this appends an extra
zero-length allocated entry. The extra entry has offset zero and breaks the
ordering that `free` relies on for `lower_bound`.

The production pool is used by NGS systems, racks, voices and parameter
buffers. Five tests exercise the production header and guest-pointer wrapper:
first split, repeated splits, free/reuse after growth, alignment/exhaustion,
and guest-pointer offsets. All five failed on the baseline header. One
valid allocation could not be freed in the repeated-growth test. All five
passed after the fix with AddressSanitizer and UndefinedBehaviorSanitizer.

The candidate removes only the extra `resize`; vector insertion already
grows capacity. Allocation alignment, zero-size requests, reinitialization,
coalescing, NGS DSP and scheduler behavior remain as in Plus. The source
declaration also lists the regression test and its CMake registration.

Run the tests with AddressSanitizer and UndefinedBehaviorSanitizer using
`bash .ci/test-native-mempool.sh`. Leak scanning is disabled by default because
this execution environment cannot inspect threads for LeakSanitizer.

On the device, compare music, effects, dialogue, repeated save/load and level
transitions with the baseline. Check Jak and Uncharted graphics for regressions.
This change has no measured rendering or performance benefit.

## Other changes assessed

- File short-read preservation is already in this Plus revision. The earlier
  branch differs mostly in comments at this point. No duplicate port is needed.
- Guest variadic doubles and eight-byte `va_list` alignment are already fixed
  in Plus. Replacing `align(address, 8)` with equivalent arithmetic adds no
  benefit.
- The earlier branch's NGS changes differ substantially from current Plus
  routing, rack lifetime and synchronization code. Keep the current Plus code.
- VM block bookkeeping also contains newer Plus scheduler behavior. Review a
  specific defect before changing it.
- Renderer, shader, guest scheduling, memory mapping, audio timing and power
  changes need the baseline device result before broader evaluation.

Published releases are immutable. The baseline and first candidate share
Android version code 1030800, allowing comparison in either install order.
The manifest records the source, certificate, native build ID and runtime diff.
