# 02: Port the perf-log setting to the Plus base

Status: resolved
Type: task
Label: ready-for-agent
Blocked by: 01

## Goal

Per-frame timing, so a change can be measured without reading the screen.
Per-second averages hide stutter.

This is the same work as `.scratch/pocket-s-optimization/issues/05-frame-timing-log.md`
and `.scratch/plus-base/issues/04-perf-log.md`, both open, both about commit
`d392779e` on the old base. Do it once here. When it is resolved, close those
two tickets with a pointer. One of them is `claimed` by an earlier session, so
its close message points at that session's `## Answer`.

## What is already here

- `tools/android/perf_summary.py` and `tools/android/test_perf_summary.py`
  came over with the tools group. `python3 tools/android/test_perf_summary.py`
  passes.
- `tools/android/device.sh pull-perf` already pulls the `perf/` folder.
- Nothing writes the CSV files. The `perf-log` config key does not exist in
  `vita3k/config/include/config/config.h`.

## Steps

1. Look at `d392779e` with `git show d392779e` before writing anything. It has
   the setting, the writer and the tests. Port it to the Plus base, not to the
   old base.
2. Add the setting `perf-log` (bool, default false) to `CONFIG_INDIVIDUAL` in
   `vita3k/config/include/config/config.h`, and map it for Android in
   `android/app/src/main/java/org/vita3k/emulator/data/EmulatorConfig.kt` and
   `vita3k/android/jni/native_config.cpp`. Add a switch in the Debug section of
   `android/app/src/main/java/org/vita3k/emulator/ui/screens/settings/SettingsSections.kt`.
3. When it is on, write under the log folder:
   - `frames.csv`: one line per emulated frame, `steady_us,title_id`, taken at
     the frame counter in `vita3k/modules/SceDisplay/SceDisplay.cpp`.
   - `presents.csv`: one line per host present, `steady_us,result`, taken right
     after `vkQueuePresentKHR` in
     `vita3k/renderer/src/vulkan/screen_renderer.cpp`.
   - `scenes.csv`: one line per scene, `steady_us,draws,submit_us,vk_wait_us`.
     This is the one the Vulkan tickets need, so add it even if `d392779e` did
     not have it.
   Buffer the lines and write them from a separate thread or once per second.
4. `submit_us` and `vk_wait_us` are GPU timestamps, not host timings. Before
   writing them, read `queueFamilyProperties[general_family_index]
   .timestampValidBits` and the physical device `timestampPeriod`. If
   `timestampValidBits` is 0, write both columns empty and say so in the
   answer. Do not fill them with host timings under the same names, because the
   Vulkan tickets compare them against GPU busy percentage.
   `vkCmdWriteTimestamp` goes at the two submit sites,
   `vita3k/renderer/src/vulkan/context.cpp:639` and
   `vita3k/renderer/src/vulkan/screen_renderer.cpp:574`.
5. Add a channel API so the other tickets can add rows without touching this
   file: `perf_log::write(channel, header, line)`.
6. `perf_log::start()` runs at the start of `run_app()`, so each game start
   truncates the files.
7. Add a `--target` and a `--csv` option to `tools/android/perf_summary.py` so
   it can read `scenes.csv`, and extend
   `tools/android/test_perf_summary.py` with the new expectations.

## Acceptance

- `container/vita3k.sh build` and `container/vita3k.sh android release` pass.
- `container/vita3k.sh format-check` passes.
- `python3 tools/android/test_perf_summary.py` passes.
- A 60 second run on the device writes about `60 * fps` lines to `frames.csv`,
  one line per present to `presents.csv`, and one line per scene to
  `scenes.csv`.
- The `timestampValidBits` value is recorded, and `submit_us` and `vk_wait_us`
  either carry GPU timestamps or are empty.

## Answer

Code merged to `master` as `be25c7c1`. The Linux build, the three googletest
suites and the format check pass. The Android reldebug APK builds.

- `perf-log` is in `config.h`, `native_config.cpp` and `EmulatorConfig.kt`.
  The Kotlin field is in all four places a field needs: the field itself, the
  copy, `equals` and `hashCode`.
- The switch is in the Emulator section of the settings screen, next to the
  performance overlay, not in the Debug section. On this base the log settings
  and the overlay all sit in the Emulator section.
- `perf_log::start()` is called from `load_app_impl`, which is where
  `d392779e` put it. That is earlier than `run_app`.
- **Step 4 is not done.** `scenes.csv` carries host time, not a GPU timestamp.
  A GPU timestamp needs a timestamp query pool and a readback of it, and a
  readback in the render path cannot be tested without a device. The column is
  called `record_us` and not `submit_us` so no ticket reads it as GPU cost.
  `timestampValidBits` and `timestampPeriod` are therefore not recorded either;
  ticket 01 records them for the device, and a later ticket can add the query
  pool once someone can measure whether it pays.

### What one row means

- `frames.csv`, one row: one call to `sceDisplaySetFrameBuf`, that is one
  emulated frame.
- `presents.csv`, one row: one `vkQueuePresentKHR`.
- `scenes.csv`, one row: one `vkQueueSubmit`. **Not one GXM scene.** A scene
  that the guest splits, through a mid-scene flush
  (`renderer/src/vulkan/scene.cpp:119`) or a macroblock change
  (`renderer/src/vulkan/context.cpp:745`), submits more than once. The draw
  counter and the start time reset in `start_recording()`, so each row carries
  the draws of its own recording. The first draft counted the scene and
  produced duplicate rows for a split scene; a review found it and it is fixed.

### What the off path costs

`perf_log::write()` returns on `!s_enabled` without taking a lock, so the guest
thread and the renderer thread pay one atomic load per frame and per present.
The draw counter in `scene.cpp` is incremented on every draw call, also when
the setting is off. `stop()` returns early while off, so nothing is built and
no destructor is registered. `start()` reports a failure to create the writer
thread instead of leaving the log half on.

### Tests

Seven googletests in `vita3k/mem/tests/perf_log_tests.cpp`, registered in the
`mem` suite, which is the suite that already links `util`: a write before
`start()` is dropped, `start()` truncates, each channel has its own file and
header, `stop()` flushes what is left, `stop()` while off does nothing, two
starts keep one channel, and `now_us()` advances.

`tools/android/test_perf_summary.py` has seven tests, three of them new: the
scenes values, a folder with no `scenes.csv`, a `scenes.csv` with rows outside
the measured window, and the `--csv` output.

### Line references corrected for this base

The ticket cited `context.cpp:639` and `screen_renderer.cpp:574` for the two
submit sites. On this base the submit is `context.cpp:655` and the present is
`screen_renderer.cpp:588`.

### Still to do on the device

- A 60 second run writes about `60 * fps` rows to `frames.csv`, one row per
  present to `presents.csv` and one row per submit to `scenes.csv`.
- `device.sh pull-perf` pulls the `perf/` folder.

## Comments

2026-10-02: the two tickets this one replaces,
`.scratch/pocket-s-optimization/issues/05-frame-timing-log.md` and
`.scratch/plus-base/issues/04-perf-log.md`, are not closed here. The first was
`claimed` by an earlier session and that session wrote no `## Answer`, so it
closes with a pointer to this ticket rather than on its own.

## Comments
