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

Code merged to `master` as `556b1da1`. The Linux build, the three googletest
suites and the format check pass. The Android reldebug APK builds.

### The setting does not work on Android, and that is a finding

The plan assumed that an app could lower the nice value of its own thread with
no capability. The capability check does pass, but the kernel has a second
condition: `is_nice_reduction()` in `kernel/sched/core.c` allows a lower value
only with `CAP_SYS_NICE` or a nonzero `RLIMIT_NICE` soft limit. An app process
has neither. The tests measured this in the container, which runs as an
unprivileged uid:

```
RLIMIT_NICE soft=0 hard=0
Could not set the nice value of this thread to -1: Inappropriate ioctl for device
```

Raising the nice value, which is the opposite direction and only lowers the
priority, always works. So the one direction an app can take is the one this
ticket does not want.

What is in the code:

- `util::set_thread_nice(int)` in `vita3k/util/`. It returns whether the value
  was applied and logs the failure once with `strerror(errno)`, so the log on
  the device says which case it is.
- The renderer thread sets it for itself at the top of `render_loop`
  (`renderer/src/batch.cpp`). The value travels through
  `renderer::State::render_thread_nice` because the thread is created in one
  place and started in another.
- The GPU wait thread sets it at the top of `VKContext::wait_thread_function`
  (`renderer/src/vulkan/context.cpp`). The value travels through
  `VKState::gpu_wait_thread_nice`, which `VKState::create` fills from the
  config. `VKState::create` runs before the context is built, so the thread
  reads a value that is already set.
- Guest threads keep the default, so they yield to the renderer.
- The default is 0, which changes nothing.

Four googletests in `vita3k/mem/tests/thread_priority_tests.cpp`. They do not
assume which way the call goes. They assert that the reported result matches
the value the thread ends up with, that 0 leaves it alone, that raising the
value works, and that `RLIMIT_NICE` is printed for the record.

### What this means for the plan

The Android specifics section of `../spec.md` said `setpriority` on the calling
thread is allowed. That is true of the capability check and false of the
kernel rule. The line is corrected there.

The ADPF hint in ticket 13 is the remaining lever for CPU placement, and it
does not depend on this one. If ticket 11 measures no change, ticket 13 is the
one to spend time on.

### Still to do on the device

- Set `thread-nice-renderer: -10` and read the log line. If the call is
  refused, this ticket is `rejected` and the log line is the evidence.
- If it is applied, run A/B/A on the 60 FPS title and the 30 FPS title and
  record FPS, the frame interval 99th percentile and `cpu-cycles` per frame.
- `adb shell cat /proc/<pid>/limits` gives `Max nice priority` for the running
  app, which is the value that decides.

## Comments

2026-10-02: the tests were written first, with the assumption that a lower nice
value applies. They failed in the container, and the failure is the finding
above. The tests now assert the contract instead of the assumption.

## Comments
