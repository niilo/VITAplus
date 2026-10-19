# Pocket S 3x: first implementation and device test

This implements the first phase of [the plan](plan.md), based on `09f207d3`.
It does not establish 60 FPS at 3x. No target device or game content is present
in this cloud environment.

## Changes in this build

- `renderer-event-wait` is a global setting, off by default. Vulkan can wait
  for a command, display event, shutdown, or the existing display deadline.
  The previous empty-queue path uses a 3-microsecond timeout. OpenGL keeps
  its polling path. Display publication uses an atomic flag and a queue wake
  synchronized with the wait mutex, so a notification cannot be lost between
  the predicate check and sleeping. The experimental deadline uses a steady clock. The default keeps its existing
  system-clock deadline.
- Performance logging adds `renderer_queue.csv`, `pipeline_compiles.csv`,
  `gpu_segments.csv`, and `gpu_frames.csv`. Logging remains off by default.
- Queue rows record wait calls, empty returns and elapsed wait time. Elapsed
  wait time includes scheduling delays; it is not CPU execution time saved.
- Pipeline rows measure host elapsed time inside the driver's graphics pipeline creation
  call. This does not include all shader translation or driver background work.
  Missing telemetry is reported as unmeasured. A present header-only channel
  means zero pipeline creation calls in the captured game session.
- Vulkan timestamp pools use the graphics queue's advertised valid bits and
  timestamp period. Results are read after existing frame fence waits, with
  availability flags and without an additional GPU wait. The pool is bounded
  to 1,024 recording pairs per frame. Overflow and unavailable queries make
  the frame incomplete. Pools are allocated only with logging enabled.
- GPU segments are elapsed intervals between render-buffer timestamps. A frame
  span runs from the first recording's start to the last recording's end.
  Intervals can include dependency waits, idle gaps and intervening texture
  uploads. They are not GPU active time. Upload command buffers, presentation
  and overlays have no separate timestamp coverage. Do not sum overlapping
  segment durations as GPU frame time. The final three in-flight frames may
  be omitted at shutdown; use sustained captures with a trailing margin.
- The Android settings, JNI mapping, config copy/equality/hash, search and help
  expose the new switch. Linux reads the same YAML setting.
- Optional build properties identify an isolated Android dev build. Ordinary
  release and dev builds retain their default IDs and labels. A CMake
  `VITA3K_BUILD_LABEL` appears in the native session log.

## Install and configure

The test APK is an optimized `RelWithDebInfo` ARM64 development build.
It uses package `org.vita3k.emulator.debug.pockets3x` and launcher name
`VITAdev 3x`. Its cloud-generated development signing key differs from keys
used outside this environment. The isolated ID allows side-by-side installation
with both VITA+ and VITAdev. It has its own settings, driver storage and game
library configuration. Import or select the required files through the app.
Do not uninstall an existing app to resolve a signature mismatch.

Install the APK, open VITAdev 3x once, and complete its initial setup. Import
Turnip into this package if that is the driver used for the reference. Check
`driverID` in the actual session log; a requested custom-driver name can fall
back to Qualcomm's system driver. Check `res_multiplier=3` in the GPU config
log. Output scaling does not replace 3x internal rendering.

In Settings > Emulator, enable performance logging. Leave **Renderer event wait
(experimental)** off for the reference. Restart the game after changing it.
Keep Vulkan validation off for performance runs; use a separate validation run
to check timestamp and synchronization errors.

The existing helper can set the same values after initial setup:

```sh
package=org.vita3k.emulator.debug.pockets3x
bash tools/android/device.sh config-set "$package" perf-log true
bash tools/android/device.sh config-set "$package" renderer-event-wait false
```

For an identical gameplay walk, run A/B/A/B/A with the switch off/on/off/on/off.
Start with a short correctness and pacing check. Then collect the same scene at
1x, 2x and 3x. Keep resolution, driver, refresh rate, mapping method, frame-limit
patch, battery state and thermal conditions fixed within each comparison.
Run the walk once and discard it before the off/on comparison, so both
settings use warm caches. Capture cold first encounters separately with
comparable initial cache state; do not compare a cold A with a warm B.

After each run, pull logs while the process is still running to retain its
flushed performance files. Let the logger flush, and include a few trailing
frames after the measured scene. Use a new output directory for each run.

```sh
out=tmp/pockets3x/uncharted-3x-wait-off
bash tools/android/device.sh pull-perf "$package" "$out"
bash tools/android/device.sh log "$package" "$out"
python3 tools/android/perf_summary.py "$out" --target 60 --warmup 120
python3 tools/android/perf_report.py "$out" --target 60 --warmup 120
```

Choose the warm-up to match the recorded walk and capture length. A short run
cannot use a 120-second warm-up. Keep raw CSVs and `vita3k.log` together.
Use the existing touch-record/replay and simpleperf tools for each title.
An FPS or CPU change must be checked against the screenshot and gameplay.

For each collection entry, record title ID, version, save and patch hash.
Uncharted, Killzone, Sly 2, Ratchet & Clank and WipEout need separate records;
Jak 1, Jak II and Jak 3 also need separate records. A 30 FPS game needs a valid
unlock before a 60 FPS performance result is possible. Verify wall-clock speed,
physics, cutscenes and audio. The existing FPS hack alone does not prove this.

Steam Deck must run the same correctness/pacing comparison on Vulkan/RADV.
The default setting remains off. Native compilation and a cloud GUI launch
cannot establish physical Deck performance or game compatibility.

## Remaining plan work

Full pipeline recipe capture and loading-time pipeline creation are not in
this phase. Existing shader preload still creates shader modules, and the
existing Vulkan driver pipeline cache remains in use. The new compilation
measurements establish how much first-use time full preparation could remove.
A later recipe format must serialize complete pipeline state and stable shader
identities, validate feature/driver compatibility, handle corrupt caches, and
fall back to normal compilation. Raw pointers and process-specific native
pipeline handles cannot be saved as recipes.

The `sceGxmWaitEvent` stub, guest JIT/AOT, rendering passes, image layouts,
shader precision and game frame-limit patches are unchanged. Implementing these
requires a proven wait contract or measured game/device evidence. The next
selection is the largest demonstrated CPU, compilation, GPU or transfer cost
in these captures. Correct 60 FPS at 3x and the sustained thermal/Deck gates in
the plan remain unmeasured.

## Cloud verification

Linux x86-64 RelWithDebInfo compiled successfully. All 87 C++ tests passed
across the ngs, ngs-scheduler, mem and module CTest entries. This includes five
new queue tests and three timestamp conversion/wrap tests. The Python summary,
report and FPS suites passed 27 tests. The Linux GUI opened under Xvfb and
closed with exit status 0. Clang-format checks passed on changed C++ files.
An independent code review checked wakeup ordering and query lifetime; its
reporting findings were corrected. The Android ARM64 reldebug APK built successfully. Its signature and 16 KiB
ZIP alignment checks passed. Packaging checks found all 57 required data,
builtin shader and icon assets. The packaged and unstripped native libraries
have the same build ID. The adjacent build manifest records artifact hashes. This is the existing RelWithDebInfo development configuration;
confirm final performance with a Release build after the comparisons. Physical GPU query validation, gameplay, FPS, power and thermals
remain unmeasured.
