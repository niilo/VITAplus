# Plus experiment review

Upstream baseline: `a86b5f93db43e4c4640ad1527e2a4695aad0f35b`.
Baseline packaging source: `a48c94153af8f13da106d8f9d1e5a41466232f0a`.
The build generates the logging identity header required but not tracked by
Plus. Its constants are recorded by hash in each manifest.
The baseline keeps the renderer, Android frontend behavior, dependencies,
shaders and data files unchanged. Its first device question is whether the
Jak and Uncharted defects match the reference Plus build.

## Shared build correction

The pinned LibAtrac9 revision is `82767fe38823c32536726ea798f392b0b49e66b9`.
Plus commit `ca3ad408` changed one call to `Atrac9DecodeBounded`, but neither
its header nor implementation provides that function. The first full builds
failed at this call. Both experiment APKs restore the preceding `Atrac9Decode`
call. This preserves the pinned dependency and avoids inventing a bounded
decoder or discarding later Plus rendering fixes. It does not add bounded
decoding. The provenance check requires the ATRAC9 file to match upstream
exactly except for that one call.

The baseline declares this one native file. The NGS candidate declares the
same correction plus the three pool fix and test files below.

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
