# 02: Port the perf-log setting to the Plus base

Status: claimed
Claimed: 2026-10-02 agent session
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

## Comments
