# 04: Record the baseline

Status: open
Type: experiment
Label: ready-for-human
Blocked by: 00, 02, 03

## Goal

Know where the device is today, on the Plus base, so every later change has
something to compare with. Nothing after this ticket is measurable without it.

## Agent steps

0. Read this first. The reference driver is **Turnip**, decided by ticket 00.
   The stock driver runs this game at 5.76 FPS against a 30 FPS target, so it can
   never satisfy criterion 1 of `../spec.md` and a stock number is not a
   baseline. Baseline on Turnip only. One stock run on the 30 FPS title is kept
   as labelled out-of-protocol evidence, not as a baseline.
1. Write the protocol from `../spec.md` into this ticket. Copy all of it,
   including the step that says a frame rate measured outside the game is not a
   result. That step is not optional: the first ticket 00 pass measured a title
   screen and drew the opposite conclusion about the drivers.
   Also state the measurement configuration, which the protocol requires and no
   ticket currently sets: `perf-log: true`, driver name, resolution multiplier.
2. Build the release APK with `container/vita3k.sh android release` and install
   it.
3. Add a `baseline` command to `tools/android/device.sh` that runs one
   measurement set for one title: warm-up run, then the A/B/A/B/A sequence from
   `../spec.md`, with `device.sh perf`, `device.sh clocks` and
   `device.sh latency` running alongside. Reuse the protocol code so every
   later ticket uses the same path.
4. Add `tools/android/perf_report.py`, which reads the `perf_summary.py`
   output, the clocks CSV and the SurfaceFlinger latency CSV for one run and
   prints one table: driver name, FPS mean and spread, frame interval 99th
   percentile, present count per second, **GPU busy percentage mean**, GPU clock
   mean, CPU clock mean per cluster, thermal status. Every later ticket pastes
   this table.
   GPU busy percentage is the decisive column. Ticket 00 measured 99% and 93%,
   so a change that moves it moves the limit and one that does not move it will
   not move the frame rate.

## Human steps

1. Pick 4 titles from your own library: one 3D title that targets 60 FPS, one
   3D title that targets 30 FPS, one 2D title, and one title that runs badly
   today.
2. For each title write down one scene, the save slot to load, and the exact
   buttons from boot to the scene. Note anything that needs a touch hold, since
   `device.sh hold <x> <y>` holds for 300 ms and a plain tap does not reach the
   game.
3. Name the Ayaneo power mode to use for every run, and set it the same way
   each time. The vendor does not publish the clocks per mode, so the mode is
   part of the protocol, not a detail.
4. Unlock the device before each run.
5. Run the sets on Turnip, using `tools/android/gameplay_scene.sh` with
   `--warmup 120`. Then one stock run on the 30 FPS title, labelled
   out-of-protocol, for comparison with ticket 00.

## Answer

## Comments
