# 04: Record the baseline

Status: claimed
Type: experiment
Label: ready-for-human
Blocked by: 00, 02, 03
Claimed: 2026-10-03 cline session (pocket-s-04-baseline-harness)

## Goal

Know where the device is today, on the Plus base, so every later change has
something to compare with. Nothing after this ticket is measurable without it.

## What the baseline has to contain after the retarget

For each title, the target frame rate (30 for a console-locked game, 60 for an
unlocked one) and four numbers over the same 60 second window:

| | why |
| --- | --- |
| average FPS | criterion 1 |
| frame interval p99 **and the fraction of late frames** | criterion 2 |
| mean device power in watts | criterion 3, the thing being optimised |
| GPU clock mean and GPU busy percentage | says whether a change moved the GPU or the CPU |

Without the power figure the baseline cannot answer any question this plan now
asks. Without the late-frame fraction it cannot answer whether a change kept
playability.

The title set needs **one console-locked 30 FPS title and one unlocked title**.
A single 30 FPS title cannot exercise criterion 1 for the 60 case, and the two
have different criteria. Uncharted is the 30 FPS title; `PCSA00015` (WipEout
2048) and `PCSA00097` (Sly 2) are the candidates for the unlocked one and
only a person who knows the games can say which they are.

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

Steps 3 and 4 are done. The code part of this ticket needs no device.

- **`tools/android/device.sh baseline <package> <title id> <label>
  [key=value]...`** runs one measurement set for one title. It runs `am
  kill-all` once, then a discarded warm-up, then A, B, A, B, A. Each run
  reaches the scene, samples the device power and the SurfaceFlinger latency
  beside the movement, decides validity with `run_is_valid.sh`, and writes
  `tmp/baseline/<label>/`. A carries no setting and B applies the `key=value`
  pairs, so A and B differ only in the setting and use the same APK.
- **`tools/android/perf_report.py <run dir>`** prints the table this ticket
  asks for: driver, target FPS, average FPS, frame interval p99 and max, the
  late-frame fraction, presents per second, mean device power with its minimum,
  GPU busy percentage, GPU clock against its cap, the three CPU cluster clocks
  and the worst thermal status during the run, and the SurfaceFlinger interval
  p99. Every missing input prints what is missing rather than a zero.
- **`tools/android/test_perf_report.py`** covers the report: the units, the
  zero-column case, the late-frame fraction, the percentile, the header-based
  lookup of the power CSV, and the missing-file cases. 10 tests.

### Two defects found in review, both fixed here

**The samplers did not cover the gameplay.** The first version started
`device_power_sample.sh` and `cmd_latency` at the same moment as the scene walk
and sampled for `seconds` (60). `gameplay_scene.sh` needs 50 + 9 + 4 + 55 = 118
seconds to reach the level, so both samplers finished about a minute before the
camera started moving. Every power figure would have described the menus, and
`run_is_valid.sh` rule 4 rejects exactly that, so no run would have passed. The
scene script now runs in the background, the samplers start after
`lead_seconds`, and the movement outlasts the sampling window. `lead_seconds` is
read out of the scene script rather than written here, so a change to those waits
cannot silently move the window off the gameplay. `BASELINE_LEAD` overrides it
for another title.

**An invalid run still printed a full table.** Now `run_is_valid.sh` decides
first, and an invalid run prints its reasons instead of a report. Criterion 6 of
`../spec.md` says a run that fails that check is not a measurement.

Two smaller fixes from the same review: `local` inside the cooldown loop body
made `set -u` fail on the second pass, so the variables are declared before the
loop; and `read_cpu_temp` returned nothing, because `adb shell` loses the
quoting of a multi-line argument and the device shell read the inner `$(...)` as
a separate command. It now passes the payload through `sh -c`, the way
`cmd_clocks` does. It reads 38000 to 50300 mdeg on the connected device.

### What is left

Steps 1, 2 and the `## Human steps` need a person. The baseline numbers do, too:
this ticket stays `claimed`. The four titles, the scene for each, the save slot,
the buttons from boot and the Ayaneo power mode are step 1 to 5 of `## Human
steps`, and none of them is knowable without the device and the games.

## Comments

- 2026-10-03: the report was checked against a recorded run in
  `tmp/gameplay/turnip-energy/` with its power CSV. It reads 6.47 FPS, a 37.05 ms
  p99 and a 92 percent GPU busy, which is the shape of table the later tickets
  paste. That run is not a baseline: it is the menu-time sample that
  `run_is_valid.sh` rejected, and it is quoted here only to show the tool
  parsing real files.
- 2026-10-03: `perf_report.py` finds the power CSV by its header, not by its
  name. The sampler takes the output path as an argument, so the name is
  whatever the caller chose, and `clocks.csv` and `latency.csv` have their own
  headers.
