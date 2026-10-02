# 13: Tell Android which threads matter, with ADPF

Status: open
Type: task
Label: ready-for-agent
Blocked by: 03

## Goal, after ticket 00

**Not a frame-rate lever.** Ticket 00 measured the emulator overlay at
**GPU 99% busy on the stock driver and 93% on Turnip, against CPU 23% and 43%**.
The frame rate is set by the GPU. Moving a thread to another core cannot change
it, and the expected result of the A/B is flat.

What this ticket is still for: **ticket 29.** The prime core sits at 595 MHz
against a 3360 MHz maximum, in most samples of both runs, because
`top-app/cpu.uclamp.min` reads `0.00` and nothing in the app asks for it. ADPF
is the only supported way an app can ask. So the question is narrow and worth
one measurement:

> If the renderer thread lands on cpu7 because ADPF puts it there, does the
> governor raise the bin?

The answer does not change the frame rate. It decides whether the core is
reachable at all from inside an app, which is worth knowing before the next
plan assumes it is not.

## What is available on Android 13

- `PerformanceHintManager.createHintSession(int[] tids, long
  initialTargetWorkDurationNanos)` is API 31, so it works.
- The NDK `APerformanceHint` API is API 33, so it works.
- `Session.setThreads(int[])` is API 34 and is **not** available. The session
  must be created again when the thread set changes. Guest threads come and go
  as games load and unload, so this is the normal case, not the exception.
- `reportActualWorkDuration(long)` is API 31 and works.
- `setPreferPowerEfficiency` is API 35 and is not available.
- Whether Qualcomm honors the hint is not documented. Measure it.

## Steps

1. Use the NDK `APerformanceHint` API from
   `vita3k/renderer/` and `vita3k/display/`, behind a temporary setting
   `adpf`, default off.
2. One session for the threads a frame depends on: the renderer thread, the GPU
   wait thread and the vblank thread. Start with those three. Adding all guest
   threads would tell the system every thread matters, which is the same as
   telling it nothing.
3. Target work duration: 16,667,000 ns for 60 FPS, 33,333,000 ns for 30 FPS.
   Take it from the target frame rate the game wants, which the vblank count
   knows.
4. Call `reportActualWorkDuration` once per frame with the measured wall time
   of those threads. Do not call it more often than once per frame; the
   thermal and perf HALs rate-limit their own callers.
5. Destroy and recreate the session when the thread set changes, because
   `setThreads` is not available. Log every recreate with a reason.
6. Read `APerformanceHint_getPreferredUpdateRateNanos` and log it. It is the
   device's own preferred period and it may not be one frame.

## Measurement

Run with the setting off and on, on the 30 FPS title, with
`tools/android/gameplay_scene.sh`. Record:

- **The cpu7 clock**, from `tools/android/device.sh clocks`. That is the point
  of the ticket.
- FPS, frame interval 99th percentile, and GPU busy percentage. **Record these
  as expected-flat controls**, not as the outcome. If they do move, that is a
  surprise worth recording as such.
- The cgroup `top-app/cpu.uclamp.min` while the game runs, from
  `/dev/cpuctl/top-app/cpu.uclamp.min`, so the reader can see whether the
  governor had any floor to work with.

A null result on cpu7 closes the question for an app: ticket 11 already closed
the other route, and there is no third one.

## Acceptance

- Both targets build and the format check passes.
- The log shows the session created, the preferred update rate, and the
  recreate events with their reasons.
- The A/B/A numbers are in `## Answer`.

## Answer

## Comments
