# 04: Record the baseline

Status: open
Type: experiment
Label: ready-for-human
Blocked by: 00, 02, 03

## Goal

Know where the device is today, on the Plus base, so every later change has
something to compare with. Nothing after this ticket is measurable without it.

## Agent steps

1. Write the protocol from `../spec.md` into this ticket.
2. Build the release APK with `container/vita3k.sh android release` and install
   it.
3. Add a `baseline` command to `tools/android/device.sh` that runs one
   measurement set for one title: warm-up run, then the A/B/A/B/A sequence from
   `../spec.md`, with `device.sh perf`, `device.sh clocks` and
   `device.sh latency` running alongside. Reuse the protocol code so every
   later ticket uses the same path.
4. Add `tools/android/perf_report.py`, which reads the `perf_summary.py`
   output, the clocks CSV and the SurfaceFlinger latency CSV for one run and
   prints one table: FPS mean and spread, frame interval 99th percentile,
   present count per second, GPU busy percentage mean, GPU clock mean, CPU
   clock mean per cluster, thermal status. Every later ticket pastes this table.

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
5. Run the sets. Both drivers where the driver is the variable: stock, and the
   Turnip build the project already uses for `uncharted_scene.sh`.

## Answer

## Comments
