# 05: Performance findings from the Jak and Daxter sessions

Status: open
Type: research
Label: ready-for-agent

## What was measured

Two sessions on `dev.8`, Pocket S, Android 13, MesaTurnip 26.3.0-devel.
Logs `tmp/play-jak/vita3k.log` and `tmp/play-jak2/vita3k.log`. The user
reports FPS was constantly 20 in the second session. No FPS counter is in the
log, so that number is the user's observation.

Config in force, from `log_gpu_configuration`:

```
mapping_method=PageTable  screen_filter=Bilinear  res_multiplier=2
high_accuracy=true  accurate_thread_scheduling=true  guest_cores=3
swapchain=2560x1440  present mode Fifo, 4 images
should_use_shader_interlock=false  programmable_blending=true
```

## The renderer stalls before scene render, twice

```
STUCK-SCENE WATCHDOG (dump 1/3): presenting (360 SetFrameBuf accepted,
  renderer still executing commands) but only 2 pipeline(s) ever compiled,
  frozen for 720 vblanks (~12s) — wedged BEFORE scene render
STUCK-SCENE WATCHDOG (dump 2/3): ... frozen for 1320 vblanks (~22s)
```

720 and 1320 vblanks at 60 Hz. The renderer never reaches the scene render,
so whatever is slow is not fragment work and not draw submission. Both stalls
are at startup, 00:38:55 and 00:39:05, while pipelines are still being
compiled.

## 43 pipelines fail to create, all ErrorOutOfHostMemory

```
vk::Device::createGraphicsPipeline: ErrorOutOfHostMemory
```

43 occurrences, 00:39:28 to 00:47:58. The device has 4.5 GB available and the
app sits at 1.1 GB RSS with 4446 MiB sys avail in the heartbeats, so this is
not a system-wide shortage. Two candidates, not separated by this data:

1. A Vulkan host allocation failing against a limit or a pool.
2. Fragmentation from a 2560x1440 swapchain with 4 images while pipelines
   are still being compiled at the same time.

A failed pipeline is a draw that cannot run, which matches "elements
vanishing". Ticket 27 owns descriptor and uniform limits, ticket 28 owns the
output surface size.

## The guest scheduling gate is held almost the entire time

`[GUEST-SCHED]` reports every 10 s:

```
acquires=35946 (100.0% uncontended) preempts=0
held_total_ms=10017 held_avg_us=278.7 held_max_ms=6.29
```

Two things stand out.

**The gate is held 10017 ms out of a 10 s window.** It is essentially never
free. That is expected for a gate that only admits 3 guest threads at a time,
and it is not by itself a defect.

**100.0 % uncontended with 0 preempts and wait_total_ms=0.0** is the useful
part. No thread is ever blocked waiting for the gate, so the gate is not the
thing slowing the game down. It is being acquired about 3600 times a second
and never contended. Each acquire is an atomic round trip that buys nothing,
which is exactly the profile ticket 32 described for the host atomics. The
cheap win is to stop taking it so often, not to make it faster.

`held_max_ms` reaches 1104.57 in one window, which lines up with the 12 s and
22 s stalls above and is worth following: a single guest thread held the gate
for over a second.

## CPU side: the prime core sits at 595 MHz

`cpu7` reads 595200 while the other cores read 844800 to 1843200. The app
used 207 to 211 % across cores. Ticket 29 recorded this exact 595 MHz figure
and rejected it as the expected low bin for the prime core. On a title where
the CPU is the busiest resource that assumption is worth retesting under
sustained load rather than a single read.

## What is a candidate, and what is not measured

Not measured, in the order worth trying:

1. **`res_multiplier=2`.** The swapchain is 2560x1440. At 1.0 it is 1280x720,
   a quarter of the pixels. One config value, and it also shrinks the
   swapchain that is the most likely source of the host-memory pressure above.
   This is the first thing to try because it could fix both symptoms.
2. **The gate's acquire rate.** About 3600 a second, always uncontended.
3. **The prime core clock.** Untested under load.

Not a candidate: fragment shading. The renderer stalls *before* the scene
render, and the GPU reads 13 to 43 % busy, so fragment cost is not what is
limiting this title.

Turnip side: nothing in this session points at the driver. There is no
device loss, no hang, and the 43 pipeline failures are `ErrorOutOfHostMemory`,
which is a host-side allocation result rather than a driver fault. The SPIR-V
`OpBitcast` rejections are a bug in our translator, not in Turnip, and they
produce failed pipelines rather than slow ones.

## Not measured

- No FPS number from a log. The perf CSVs on the device hold Uncharted.
- No CPU profile of this session, so the split between the guest, the mixer
  and the renderer is unknown.
- Whether the 595 MHz reading is a limit or the normal bin under load.

## Third session: resolution 1 made no difference, and why

`dev.12`, and the user tried `res_multiplier=1`. Findings:

**42 pipelines still fail with `ErrorOutOfHostMemory`**, 07:55:25 to 07:59:26.
Essentially the same count as the 43 at resolution 2. So the pipeline failures
are **not** a resolution problem.

**And the swapchain did not change.** The log shows both:

```
session config: res_multiplier=1
create_swapchain: extent=2560x1440
```

`res_multiplier` scales the rendered guest image, not the output surface. The
swapchain is still 2560x1440 in both runs, so halving the internal resolution
left every Vulkan allocation the same size. That is why the host-memory
pressure did not move, and it is the most useful thing this session showed.

**So the `ErrorOutOfHostMemory` cause is still open, and it is not the
resolution.** Candidates now:

1. A per-pipeline host memory limit or pool being hit as pipelines accumulate.
   The counter in the message, "N succeeded so far", reaches the hundreds, so
   the count is growing when it fails.
2. Fragmentation from the 2560x1440 swapchain with 4 images, which never
   changed.
3. Something the app allocates per pipeline, since 42 of them fail and the
   rest succeed.

Ticket 28 owns the output surface size. Shrinking the swapchain is the direct
test of candidate 2 and nothing else has moved it yet.

`swapchain-scale` now exists for that, in commit `c4d2daed`. It multiplies the
swapchain extent, which `resolution-multiplier` never touched. Default 1.0,
clamped to 0.25 to 1.0, both dimensions rounded down to a whole number of
8-pixel tiles. The device is set to 0.5 for the next run.

## The version string lies when Gradle reuses a configure

The emulator reported "failed to initialize emulator" three times on 2026-10-05
at 08:48, 08:49 and 09:05. The log line is:

```
[E] [initialize_session]: Failed to initialise config.
```

**It was not a config failure and it was not the swapchain change.** Deleting
`config.yml` and letting the app write a new one made the next launch succeed,
which is what made it look like a config problem.

The actual cause: the installed APK was `v1.2.1-dev.15` but the log said
`v1.2.1-dev.12`. The stale version was the only symptom, and it was wrong
because **`configure_file(... FORCE)` from commit `3467d9fd` never ran.**
Gradle reuses the configure it already did under `android/app/.cxx`, so the
generated `vita3k/config/version.cpp` still carried the value from when it was
first written:

```
version.cpp mtime     Oct  5 01:10
build ran             Oct  5 08:26
```

So the earlier claim that FORCE is "the whole fix and does not need the cache
gone" was wrong. FORCE guarantees the write **when configure runs**; it does
nothing when the configure step is skipped entirely. That is what the deleted
`CMakeCache.txt` was trying to solve, and it was the right idea implemented
badly: deleting the cache breaks SDL.

What works is removing `android/app/.cxx`, which is what this session did. That
forces a full reconfigure and SDL configures cleanly from scratch, unlike a
configure with a deleted cache but stale CMake state.

Two lessons worth keeping:

1. **A stale version string is a real signal**, not cosmetic. It was the only
   evidence that the configure had been skipped, and it was the reason a
   correct-looking APK could not be told apart from a stale one.
2. **Deleting `config.yml` fixed the symptom and hid the cause.** The next
   launch succeeded, which suggested the config was at fault. It was not. Do
   not delete user config to diagnose an emulator failure.