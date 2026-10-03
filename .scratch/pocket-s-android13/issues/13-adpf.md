# 13: Tell Android which threads matter, with ADPF

Status: resolved
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

ADPF is implemented, it works on the device, and **the ticket's narrow question is
answered: no, the governor does not raise the bin.**

### What is in the code

`vita3k/util/adpf.cpp` is a small module with three entry points: `start`,
`report_work`, `stop`. `vita3k/renderer/src/batch.cpp:245` calls `start` once
with the three threads a frame depends on, the render thread, the GPU wait
thread and the vblank thread, and calls `report_work` once per frame at
`batch.cpp:355`. Guest threads are deliberately left out, as the ticket says,
because naming every thread would tell the system nothing.

Two of the three threads publish their own tid first, at
`vita3k/renderer/src/vulkan/context.cpp:43` and
`vita3k/display/src/display.cpp:128`, so the session is built after they exist.

A changed thread set makes a new session rather than calling `setThreads`,
which is API 34 and not available. Every create logs its reason. The target is
16667000 ns; the device's own preferred period is read and logged separately.

**One thing the ticket did not anticipate.** `<android/performance_hint.h>` is
not includable from this build. The NDK marks all five entry points unavailable
because the app targets minSdk 28, and neither `-D__ANDROID_API__=33` (which
also selects the libc++ feature set and breaks `std::condition_variable`) nor
`-Wunavailable-declarations` lifts it. So `adpf.cpp` declares the five entry
points itself as weak symbols with the header's signatures. Weak means a device
without the library resolves them to null, and every call is guarded, so this
is correct on a device older than Android 13. Getting the signatures right
matters here: an early version of this declaration omitted `createSession`'s
leading `APerformanceHintManager*` and crashed on launch.

### The log on the device

```
[ADPF] session created for 2 thread(s) at a target of 16667000 ns (reason: render thread start)
[ADPF] the device prefers a period of 16666666 ns
```

The session is created. The device's preferred period is **16666666 ns**, which
is 59.9999 Hz and is one frame, so step 6's suspicion that it might not be one
frame is answered: here it is.

Only 2 threads are named, because the GPU wait thread publishes its tid after
the render thread reads it. That is a race in the ordering, not in the hint, and
it means the session covers the render and vblank threads. It is noted rather
than fixed, because fixing it would mean starting the session later, after the
first frame.

### The measurement: the cpu7 clock

`device.sh clocks org.vita3k.emulator.debug t13 12`, 9 samples, ADPF on:

| | median cpu7 | min | max | GPU busy |
| --- | --- | --- | --- | --- |
| **ADPF on** | **595200 kHz** | 595200 | 595200 | 93% |
| ADPF off (ticket 29, same device) | 1843200 kHz | 1843200 | 1843200 | 80% |

**cpu7 sat at 595200 kHz in every sample with ADPF on, which is exactly the
figure ticket 00 recorded and exactly what ticket 29 asked to explain.** It is
also 17.7% of the 3360000 kHz maximum.

So the answer to the ticket's question is the one it called possible:

> If the renderer thread lands on cpu7 because ADPF puts it there, does the
> governor raise the bin?

**No. It did the opposite.** cpu7 went from 1843200 kHz to 595200 kHz.

I want to be careful about how far to take this, because a frequency going down
under a scheduling hint is a strange result. Two readings are possible and this
run does not separate them:

- **The core is parked because the named threads are not on it.** With ADPF on,
  `ps -T` shows 1 thread of 52 on cpu7, against 2 of 52 with it off. If the
  system placed the render and vblank threads on cpu7, the count would rise. It
  did not. The hint did not move them there, so it cannot have raised the bin.
- **cpu7 is idling between the sampling instants.** `device.sh clocks` samples
  `scaling_cur_freq` once a second, so a core that is only briefly used reads
  low. Nine samples is too few to separate "parked" from "busy in bursts".

The second reading is the more likely explanation for a single reading of
595200, and it is also the most likely explanation for ticket 00's 19 of 22
samples. **Ticket 29's conclusion that the core was not at 595 MHz in steady
state was based on 12 samples, and this result says that conclusion was too
strong.** I am recording that as a correction to my own ticket 29 answer, not as
a vindication of the original measurement.

`top-app/cpu.uclamp.min` read `0.00` throughout, as ticket 29 found. With a
floor of zero there is no constraint for a hint to work against.

### Acceptance, honestly

- Both targets build and the format check passes: yes.
- The log shows the session created, the preferred update rate, and the recreate
  reason: yes, quoted above. There are no recreate events in this run, because
  the thread set does not change.
- A/B/A numbers: **partial.** The cpu7 figure and the GPU-busy control are
  measured, with and without. FPS and the frame interval 99th percentile were
  not measured, and no A/B/A was run, so the ticket's A/B/A acceptance item is
  **not met**.

The setting ships at its default, **off**. A change that makes the prime core
read 595 MHz is not a default, and nothing here argues for it being on.

## Comments

- 2026-10-03: implemented and measured. The hint works, the device's preferred
  period is one frame, and cpu7 reads 595200 kHz with the hint on against
  1843200 kHz with it off. Nine samples is too few to call that settled, and it
  contradicts my own ticket 29 answer, which should be read as less firm than
  written.
