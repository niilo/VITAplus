# 13: Help the scheduler: ADPF hints, thread priority and thermal data

Status: open
Type: experiment
Label: ready-for-agent
Blocked by: 07

## Question

Does it help to tell Android which threads matter, so they run on big
cores at a higher clock?

## Context

- No host thread priority or affinity is set. Guest threads map 1:1 to
  `SDL_CreateThread` (`kernel/src/kernel.cpp:168`).
- ADPF (`PerformanceHintManager`): `createHintSession` is API 31. The
  threads must belong to the app. `setThreads` is API 34, so on this
  Android 13 device the session must be created again when the thread set
  changes. The NDK `APerformanceHint` API is API 33. A null session means
  the vendor does not support ADPF.
- `sched_setaffinity` works for the app's own threads, but Android resets
  it when the app moves between foreground and background (cpuset change).
- `setpriority` changes only the share of one core. It does not raise the
  clock or move a thread to a big core.

## Steps

1. **Observe.** Use the ticket 07 Perfetto data: which cores the render
   thread and the 3 guest threads with the highest `utime`
   (`/proc/self/task/*/stat`) run on, and at which clock. Map threads by
   tid, because names are cut to 15 characters.
2. **ADPF.** Through a JNI call, get the tids of the render thread and
   the 3 busiest guest threads. Create a hint session with a target equal
   to the title's frame time (16.67 ms or 33.33 ms). Each frame, report the
   busy time of the frame, not the wall time: a thread that sleeps on
   vblank must not report the sleep. Log if the session is null.
3. **Priority.** Raise the render thread with
   `setpriority(PRIO_PROCESS, tid, -4)`. Read field 19 of
   `/proc/self/task/<tid>/stat` to confirm.
4. **Affinity, only if 2 and 3 do not help.** Pin the render thread and the
   guest main thread to the prime and performance cores. Read the core
   layout from `cpuinfo_max_freq`. Apply it again on resume.
5. **Thermal.** Log `PowerManager.getCurrentThermalStatus()` changes and
   `getThermalHeadroom(10)` (API 30) once per second, as columns in the
   ticket 05 data.
6. **Turbo.** On titles that ticket 07 marks GPU-bound only: A = turbo off,
   B = turbo on, then a 20 minute run with turbo on. Turbo holds the GPU at
   its top clock and can make the CPU throttle.

Each of steps 2, 3, 4 and 6 is its own A/B/A, behind a temporary config
value.

## Output

Under `## Answer`: a table of step, title, average FPS, 99th percentile ms,
and for 20 minute runs the FPS drop and thermal status. Keep what helps.

## Answer
