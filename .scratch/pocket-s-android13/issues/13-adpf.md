# 13: Tell Android which threads matter, with ADPF

Status: open
Type: task
Label: ready-for-agent
Blocked by: 03

## Goal

The emulator runs guest threads, a renderer thread, a GPU wait thread, a vblank
thread and audio threads. The system does not know which ones a frame depends
on. ADPF is the supported way to say so.

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

After ticket 04 has a baseline, run A/B/A on the 60 FPS title and the 30 FPS
title with the setting off and on. Record:

- FPS, frame interval 99th percentile.
- CPU clock per cluster from ticket 03. If the hint works, the clock rises
  earlier in each frame.
- `cpu-cycles` per frame from simpleperf. If the clock rises and cycles per
  frame stay flat, the threads are running faster.

If the CPU clock does not move, the hint is not being honored, and that is worth
writing down so nobody tries it again.

## Acceptance

- Both targets build and the format check passes.
- The log shows the session created, the preferred update rate, and the
  recreate events with their reasons.
- The A/B/A numbers are in `## Answer`.

## Answer

## Comments
