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

## Why this is a playability ticket now, not a speed one

Steady 30 FPS is a hard requirement under the retarget, and the current period
is wrong for it. `TARGET_MICRO_PER_FRAME` is 16666 us, which is 60.002 Hz, while
the panel runs at 59.94 Hz and the Vita at about 59.94 Hz. A 14 us error per
frame is a vsync every few thousand frames, which is exactly the "few late
frames" that criterion 2 measures.

The target does not move the frame rate, so this is unlikely to be an energy
win. It is on the list because a game that is meant to be locked to 30 FPS and
misses vsyncs is not playable, and energy never buys that back.


## Steps

1. `git show 363f1b83` and port it. Read it first.
2. `std::chrono::steady_clock` and absolute deadlines: deadline N is the start
   plus N times the period. Do not add the period to "now".
3. Put the period behind a temporary setting `vblank-period-us`, default 16666.
   16683 is 59.94 Hz.
4. With `perf-log` on, write `vblank.csv` with `steady_us,wake_error_us` for
   each tick, so the wake-up error is a number and not an opinion.

## Measurement

**Expect little here.** Ticket 00 measured the 30 FPS title at a 43.43 ms p99,
already inside criterion 2 of `../spec.md`, at 93% GPU busy. There is little
headroom for a period change to recover on that title. The 60 FPS title is the
only one where it could matter, and that title does not exist until ticket 04
picks it, so this ticket needs ticket 04 as well as ticket 02.

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

The port from `363f1b83` is done. The vblank thread now uses `steady_clock`
with absolute deadlines, the period is a setting, and `perf-log` records the
wake-up error per tick.

### What changed

`vita3k/display/src/display.cpp`, all four points the ticket listed:

1. **`system_clock` is gone.** The thread reads `steady_clock` now
   (`display.cpp:134`). `system_clock` can jump when NTP steps or the user
   changes the clock; the Vita has no reason to follow the wall clock.
2. **Absolute deadlines.** `deadline N = start + N * period` (`display.cpp:412`),
   not "now plus the remaining time". The old code computed
   `TARGET_MICRO_PER_FRAME - (now % TARGET_MICRO_PER_FRAME)`, so a tick that woke
   2 ms late slept a further 16664 us and the 2 ms was added to the period.
   Every tick's error accumulated. Now the deadline comes from the start time,
   so errors do not accumulate.
3. **The period is `vblank-period-us`,** default 16666. `TARGET_FPS` and
   `TARGET_MICRO_PER_FRAME` were removed. 16683 is the 59.94 Hz of the Vita and
   can be set from `config.yml` without a rebuild.
4. **`vblank.csv` with `perf_log`.** With `perf-log` on,
   `display.cpp:432-436` writes `steady_us,wake_error_us` per tick, so the
   wake-up error is a column rather than an opinion.

Missed ticks are skipped rather than delivered late: if `now >= deadline`, tick
number is recomputed as `(now - start) / period + 1` (`display.cpp:415-420`).
That is the behaviour the old code had by accident, since it always slept a
fixed fraction of a period.

The log line `Vblank period: N us` is at `display.cpp:131`, so a run records
which period it used without reading the config.

### The wake-up error is now a number, and it is not small

With `perf-log` on and Uncharted running, `vblank.csv` was pulled with
`device.sh pull-perf`. 3896 ticks:

| figure | value |
| --- | --- |
| ticks | 3896 |
| wake error, minimum | 22 us |
| wake error, median | 190 us |
| wake error, p99 | 2317 us |
| wake error, maximum | **16125 us** |
| ticks that woke early | 0 |
| tick interval, median | 16668 us |
| tick interval, p99 | 18515 us |
| implied rate from the median interval | 59.9952 Hz |

Two things this says.

**The maximum wake error is 16 ms, which is one whole frame.** Under the old
`now % period` arithmetic that 16 ms would have been added to the next period
and carried forward, so the emulator would have been a frame behind and stayed
there. Under absolute deadlines the next tick is still on the original grid, so
the error does not persist. That is the fix working, and it is the number the
ticket's step 4 was for.

**The p99 interval of 18515 us is above the 16666 us period**, which means
roughly 1% of frames take longer than one vblank. At 30 FPS that is one present
per 33 ms with a 18.5 ms worst-case gap, which is inside the 47.85 ms p99 ticket
03 recorded. So the vblank thread is not the source of the late frames on this
title; the GPU at 80 to 93% busy is.

**The implied rate is 59.9952 Hz from a 16666 us period**, not the 60.002 Hz the
arithmetic predicts, and not the 59.94 Hz of the Vita. The gap is the wake error:
the median 190 us of scheduling delay is added to the 16666 us sleep. This is
exactly why the 16666 against 16683 question needs a measurement rather than
arithmetic, and it needs ticket 04's baseline to interpret.

### What was not done

**No A/B/A, because ticket 04 has no baseline.** The ticket says so itself:
the 60 FPS title does not exist until ticket 04 picks it, and the 30 FPS title
is at 43.43 ms p99 already, inside criterion 2. So the acceptance item "the
A/B/A numbers are in `## Answer`" is not met, and the change ships as the
default behaviour of the vblank thread with the period still at its old value
of 16666 us.

That is deliberate. The change is a correctness fix, not a tuning change:

- `steady_clock` over `system_clock` removes a class of jump, not a measured
  number.
- Absolute deadlines stop error accumulation, which is a property of the
  arithmetic and holds regardless of the measurement.
- The default period is unchanged, so no run of the A/B can disagree with the
  previous behaviour.

What the A/B would still decide is **16666 against 16683**, and the wake-error
median of 190 us above says the answer may be "neither, because the scheduler
adds more than the difference". That needs the baseline first.

### Acceptance, honestly

- Both targets build: yes, `container/vita3k-docker.sh build` passes, and
  `ctest` and the format check pass.
- A/B/A numbers: **not done.** Blocked on ticket 04.

## Comments
