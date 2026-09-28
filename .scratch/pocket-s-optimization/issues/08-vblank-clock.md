# 08: Make the emulated vblank clock steady

Status: claimed
Claimed: 2026-09-25 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent
Blocked by: 05
Measure after: 06

## Problem

The vblank thread (`display/src/display.cpp:32-82`):

- reads `std::chrono::system_clock` (`:78`), which is not monotonic and can
  jump;
- ticks every `1000000 / 60` µs (`:32`), which is 60.002 Hz, while the Vita
  runs at about 59.94 Hz;
- is not tied to the host display, so the two clocks drift apart and a
  frame is repeated or dropped every few seconds;
- is a light thread, so the scheduler can put it on a small core, where
  wake-up is late.

Most games wait on this vblank, so its timing sets the frame pacing.

## Steps

1. Measure first. Log the wake-up error of each tick (actual minus planned
   time) to a CSV when `perf-log` is on (ticket 05).
2. Use `std::chrono::steady_clock` and absolute deadlines: deadline N is
   start plus N times the period. Do not add the period to "now".
3. Put the period behind a temporary config value: 16666 µs (today) or
   16683 µs (59.94 Hz).
4. Optional, as its own commit: on Android, align the ticks to the display
   with `AChoreographer_postFrameCallback64` (API 29). Keep the old path as
   the fallback.

## Acceptance

- Both targets build. `container/vita3k.sh test` passes.
- On the device, A/B/A on the 60 FPS title and the 30 FPS title: record the
  wake-up error (99th percentile) and the frame interval 99th percentile.
  Keep a change only if it lowers the frame interval 99th percentile by
  more than the A spread.

## Answer

Code done on branch `pocket-s/08-vblank-clock`, commit b238758c, rebased on
`master` after ticket 19 as 363f1b83. Linux and Android release builds
pass. `container/vita3k.sh test` passes.

- Steps 1 to 3 are done. With `perf-log` on, `vblank.csv` has
  `steady_us,wake_error_us` for each tick.
- The temporary setting is `vblank-period-us` (default 16666). Set 16683
  with `device.sh config-set org.vita3k.emulator vblank-period-us 16683`.
- Step 4 (`AChoreographer`) is not done. Do it only if the A/B shows that
  the clock change alone does not lower the frame interval 99th percentile.

Still to do: the A/B/A runs on the device after ticket 06.
