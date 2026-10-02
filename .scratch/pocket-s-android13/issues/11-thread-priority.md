# 11: Raise the priority of the renderer and wait threads

Status: claimed
Claimed: 2026-10-02 agent session
Type: task
Label: ready-for-agent
Blocked by: 03

## Goal

The renderer thread and the GPU wait thread are the two host threads that must
not be late. Nothing in the tree sets a host thread priority. On Android 13 an
app can lower its own nice value with no capability, and that is the only
thread control available.

## What is allowed on Android 13

| Call | From an unprivileged app |
|---|---|
| `setpriority(PRIO_PROCESS, 0, n)` on the calling thread | allowed |
| `setpriority(PRIO_PROCESS, tid, n)` on another thread | `EPERM` |
| `pthread_setschedparam` with `SCHED_FIFO` | `EPERM`, no `CAP_SYS_NICE` |
| `sched_setaffinity` narrowing to the current mask | allowed |
| `sched_setaffinity` widening | `EPERM` |
| `sched_getaffinity`, `sched_getcpu` | allowed |

`Process.setThreadPriority` only sets a nice value. It does not change the
cgroup. A `std::thread` does not inherit the creator's nice value, so each
thread must set its own.

Android's own guidance says an app should not set CPU affinity, because devices
often ignore it and the OS picks the core type better than the app can. So this
ticket does not set affinity.

## Steps

1. Add one helper in the renderer that calls
   `setpriority(PRIO_PROCESS, 0, n)` from inside the thread it applies to, and
   ignores the result other than logging it once.
2. Apply it to the renderer thread (`vita3k/renderer/src/batch.cpp:328`) and the
   GPU wait thread (`vita3k/renderer/src/vulkan/creation.cpp:61`). Start at
   nice -10 for both.
3. Leave the guest threads created in `vita3k/kernel/src/kernel.cpp:192` at
   the default, so they yield to the renderer.
4. Add a temporary setting `thread-nice-renderer`, default 0, so A and B can be
   run without a rebuild. 0 means leave the default.
5. Re-apply the nice value if the app is resumed after the Activity was
   stopped. A foreground service can be restarted without a new thread, so the
   value does not need to be set again, but check it.

## Measurement

After ticket 04 has a baseline, run A/B/A on the 60 FPS title and the 30 FPS
title at nice 0 and nice -10. Record FPS, the frame interval 99th percentile,
and `cpu-cycles` per frame from simpleperf. `cpu-cycles` per frame is the
metric that separates "the work got cheaper" from "the clock went up".

If the win is not there, set `Status: rejected` with the numbers.

## Acceptance

- Both targets build and the format check passes.
- `adb shell cat /proc/$(pidof org.vita3k.emulator)/task/<tid>/stat` shows the
  expected nice value for the renderer thread while a game runs.
- The A/B/A numbers are in `## Answer`.

## Answer

## Comments
