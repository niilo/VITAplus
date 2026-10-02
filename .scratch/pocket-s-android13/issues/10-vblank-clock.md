# 10: Make the emulated vblank clock steady

Status: open
Type: task
Label: ready-for-agent
Blocked by: 02

## Problem

The vblank thread at `vita3k/display/src/display.cpp:411-413`:

- reads `std::chrono::system_clock`, which is not monotonic and can jump;
- computes the next sleep from `time_ms % TARGET_MICRO_PER_FRAME`, so a late
  wake does not shorten the next period, it lengthens it;
- uses `TARGET_MICRO_PER_FRAME` = `1000000 / 60` = 16666 us, which is 60.002 Hz,
  while the Vita runs at about 59.94 Hz;
- is not tied to the host display, so the two clocks drift apart.

Most games wait on this tick, so its timing sets the frame pacing.

This is the same work as `.scratch/pocket-s-optimization/issues/08-vblank-clock.md`,
claimed, with the code on `origin/pocket-s/08-vblank-clock` (`363f1b83`) built
on the old base. Port it.

## Steps

1. `git show 363f1b83` and port it. Read it first.
2. `std::chrono::steady_clock` and absolute deadlines: deadline N is the start
   plus N times the period. Do not add the period to "now".
3. Put the period behind a temporary setting `vblank-period-us`, default 16666.
   16683 is 59.94 Hz.
4. With `perf-log` on, write `vblank.csv` with `steady_us,wake_error_us` for
   each tick, so the wake-up error is a number and not an opinion.

## Measurement

After ticket 04 has a baseline, run A/B/A on the 60 FPS title and the 30 FPS
title at 16666 and at 16683. Record the wake-up error 99th percentile, the
frame interval 99th percentile, and GPU clock. Keep a change only if it lowers
the frame interval 99th percentile by more than the A spread.

`AChoreographer` from native code is not available. `Choreographer.postFrameCallback64`
is not in the public SDK. Do not plan around it.

## Acceptance

- Both targets build, `container/vita3k.sh test` passes, the format check
  passes.
- The A/B/A numbers are in `## Answer`.

## Answer

## Comments
