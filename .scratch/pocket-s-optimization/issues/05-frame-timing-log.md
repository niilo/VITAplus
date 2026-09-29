# 05: Log a time for every frame

Status: claimed
Claimed: 2026-09-25 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent
Blocked by: none

## Goal

Measure frame rate and frame pacing without the screen. Per-second averages
hide stutter, so the log must have one line per frame.

## Context

- `update_runtime_metrics` (`app/src/app.cpp:289-340`) keeps one FPS value
  and one average ms value per second. The ring holds FPS values. No
  per-frame time exists.
- Emulated frames are counted at `modules/SceDisplay/SceDisplay.cpp:159`.
- Host presents happen at `vulkan/screen_renderer.cpp:563`
  (`presentKHR`).
- A new setting needs `config.h`, `native_config.cpp` (field IDs near
  `:128-160`, reads and writes near `:317` and `:459`), and the copy,
  `equals` and `hashCode` parts of `EmulatorConfig.kt`.

## Steps

1. Add the setting `perf-log` (bool, default false) to `CONFIG_INDIVIDUAL`
   in `config/include/config/config.h`, and map it for Android as above.
   Add a switch in the Debug section (`SettingsSections.kt:1369`).
2. When on, write to the log folder:
   - `frames.csv`: one line per emulated frame: `steady_us,title_id`. Take
     the time with `std::chrono::steady_clock` at `SceDisplay.cpp:159`.
   - `presents.csv`: one line per host present: `steady_us,result`, taken
     right after `presentKHR`.
   Buffer the lines in memory and write them from a separate thread or
   once per second, so the log does not slow the frame.
3. Add `tools/android/perf_summary.py <folder> --target 30|60 --warmup 30`.
   It prints: average FPS, percent of seconds with `fps >= target - 1`,
   the 99th percentile and the maximum of frame intervals, the number of
   intervals over 1.5 times the target frame time, and the present count
   per second. Skip the first `--warmup` seconds.
4. Add a unit test for the summary with a small CSV file under
   `tools/android/testdata/`.

## Acceptance

- Both targets build.
- `python3 tools/android/perf_summary.py tools/android/testdata/...` gives
  the expected numbers.
- On the device (during ticket 06), a 60 second run gives about `60 x fps`
  lines in `frames.csv`, and the summary prints all values.

## Answer

Code done and merged to `master`: d392779e (merge bd247362). Linux
and Android release builds pass. `python3 tools/android/test_perf_summary.py`
passes (3 tests).

- The files are in `perf/` next to `vita3k.log`:
  `/sdcard/Android/data/<package>/files/perf/`. `device.sh pull-perf`
  pulls that folder.
- `perf_log::start()` runs at the start of `run_app()`, so each game start
  truncates the files.
- Other tickets add channels with `perf_log::write("<name>", "<header>",
  line)`. Each channel is `<name>.csv`.

Still to do on the device (ticket 06): check that a 60 second run gives
about `60 x fps` lines in `frames.csv`.
