# Spec: Android 13 performance for the Ayaneo Pocket S

## Goal

Raise the frame rate and hold it steady on the Ayaneo Pocket S, and reduce
the work the emulator asks the GPU and the CPU to do. Every change must be
measured on the device with the protocol below. A change without a
measurement is not done.

This plan replaces `.scratch/pocket-s-optimization/`. That plan was written
against the old code base. The base is now Vita3K-Plus (`.scratch/plus-base/`),
and several settings it assumed are no longer true. This plan also covers the
two questions the user asked for: how to make the Vulkan rendering cheaper,
and which Snapdragon CPU features can save clock cycles.

## Shape of the plan

29 tickets in `issues/`, numbered from `00`. The dependency graph decides what
runs first, not the numbering.

| Tickets | What they do |
| --- | --- |
| 00 | Choose the driver and the GPU power constraint. Every measurement after it runs on one configuration. |
| 01, 02, 03 | Record the device facts, port `perf-log`, build the profiling harness. |
| 04 | The baseline. Every other measurement is compared against it. |
| 05 | The render configuration matrix. Finds the settings and the CPU-or-GPU verdict per title. |
| 06 to 10 | The Vulkan frame path: framebuffer fetch, visibility queries, macroblock sync, present mode, vblank clock. |
| 11 to 14 | Host CPU: thread priority, huge pages, ADPF, code cache size. Ticket 11 found that Android blocks the nice value, so ADPF in ticket 13 is the remaining lever. |
| 15 to 21 | More Vulkan and GPU: attachment layouts, the shader compiler workaround, pipeline stutter, screen filters, the Turnip autotuner, the resolution multiplier, thermal measurement. |
| 22 | The preset. |
| 23 | Verification and the write-up. |
| 24 to 28 | Areas this plan's first draft did not cover: GPU power constraint, dynarmic flags, texture upload, descriptor and uniform limits, output surface size. |

## What changed since the old plan

The old plan assumed the old code base. On the Plus base:

- `high-accuracy` defaults to `true` (upstream: `false`). It turns off texture
  viewport and turns on shader interlock.
- `memory-mapping` defaults to `page-table` on Android. On the stock Adreno
  driver the renderer logs the downgrade and runs it as `double-buffer`
  (`vita3k/renderer/src/vulkan/renderer.cpp:1112`).
- `disable-surface-sync` defaults to `false` (upstream: `true`).
- `guest-cores` defaults to `3` and `accurate-thread-scheduling` to `true`.
  Neither has been measured on this device.
- `async-pipeline-compilation` defaults to `false` (upstream: `true`).
- `perf-log` is gone. It was only on the old base
  (`pocket-s/05-frame-timing-log`, `d392779e`). `tools/android/perf_summary.py`
  and `tools/android/test_perf_summary.py` are still here, and
  `device.sh pull-perf` still exists, but nothing writes the CSV files.

## Hardware

To be confirmed on the device by ticket 01. What the vendor and the driver
name say today:

| Part | Value | Source |
|---|---|---|
| SoC | Snapdragon G3x Gen 2. `ro.soc.model` = `SG8275`, `ro.board.platform` = `kalama` | `pocket-s-optimization/spec.md`, checked 2026-09-28 |
| CPU | 8 cores, ARMv9.0-A: 1 Cortex-X3, 2 Cortex-A715, 2 Cortex-A710, 3 Cortex-A510 | AYANEO product page; Arm core pages |
| CPU clocks | X3 3.36 GHz, A715 and A710 2.8 GHz, A510 2.02 GHz | AYANEO; `cpuinfo_max_freq` checked 2026-09-28 |
| GPU | Adreno A32, up to 1.0 GHz. The stock driver reports `Adreno (TM) 740`. Mesa maps this part to `FDA32`, the same bucket as Adreno 740. | `pocket-s-optimization/spec.md`; Mesa `freedreno_devices.py` |
| RAM | LPDDR5X-8533, 16 GB on the test device | `pocket-s-optimization/spec.md` |
| Display | 6 inch IPS, 2560x1440, 60 Hz, one mode | `pocket-s-optimization/spec.md` |
| Cooling | vapour chamber over 5180 mm2, fan, 15 W sustained (vendor claim) | AYANEO |
| OS | Android 13, build `TKQ1.230811.002`. Kernel 5.10 or 5.15. | `pocket-s-optimization/spec.md` |

The old plan called this part "Snapdragon 8 Gen 2, Adreno 740". The vendor
calls it Snapdragon G3x Gen 2 with an Adreno A32. The two are the same silicon
with different names. Ticket 01 settles which names the device reports.

### ARM features on this part

The cores report ARMv9.0-A. LSE2, SB, BTI and PAuth are mandatory in that
version, so they are present. `dc ZVA` is mandatory from Armv8.2, and i8mm is
mandatory from Armv8.6. SSBS is optional in every Arm version, so the version
does not decide it. FP16 arithmetic (`FHM`), DotProd, FlagM2 and SSBS are
optional, so the architecture version does not decide them either. MTE is
optional. SVE is not expected. Ticket 01 reads `/proc/cpuinfo` and settles every
one of them.

The guest is ARMv7 (`vita3k/cpu/src/dynarmic_cpu.cpp:644` sets
`ArchVersion::v7`), so the host's LSE2, FP16 and DotProd instructions never
appear in JIT output. Those features can only help the emulator's own C++ code,
which is shader translation, texture decode and format conversion. No ticket
in this plan measures those, so no ticket claims a gain from them.

The host CPU levers that remain are the ones with tickets: the code cache page
size (ticket 12), the thread priority (ticket 11), the ADPF hint (ticket 13),
and the dynarmic optimization set (ticket 25).

## What this means for CPU work

The guest is ARMv7 (`vita3k/cpu/src/dynarmic_cpu.cpp:644` sets
`ArchVersion::v7`), so the host's LSE2, FP16 and DotProd instructions never
appear in JIT output. Those features can only help the emulator's own C++ code,
which is shader translation, texture decode and format conversion. No ticket
in this plan measures those, so no ticket claims a gain from them.

The host CPU levers that remain are the ones with tickets: the code cache page
size (ticket 12), the thread priority (ticket 11), the ADPF hint (ticket 13),
and the dynarmic optimization set (ticket 25).

### Android 13 limits that shape the plan

- `PerformanceHintManager.createHintSession` is API 31. The NDK
  `APerformanceHint` API is API 33. `Session.setThreads` is API 34 and is not
  available, so a session must be created again when the thread set changes.
- `ANativeWindow_setFrameRate` is API 30. It is available.
- `Choreographer.postFrameCallback64` is not in the public SDK. Not available.
- `Surface.setProducerThrottlingEnabled` is API 37. Not available.
- `MADV_COLLAPSE` needs Linux 6.1. This device runs 5.15, so the call returns
  `EINVAL`. Probe for it once and do not ship it unguarded. Ticket 01.
- Transparent huge pages on this device are in `always` mode, not the
  `madvise` mode the GKI defconfig selects. Anonymous mappings already get
  2 MB pages, so ticket 12 measures before adding a `madvise` call.
  Confirmed 2026-10-02 by ticket 01.
- `setpriority` on the calling thread passes the capability check, but the
  kernel allows a lower nice value only with `CAP_SYS_NICE` or a nonzero
  `RLIMIT_NICE` soft limit, and an app process has neither. So an app cannot
  raise its own priority by lowering the nice value. It can lower its priority
  by raising the value, which is the opposite of what ticket 11 needs.
  `pthread_setschedparam` with `SCHED_FIFO` returns `EPERM`. An app cannot
  change its cgroup or its uclamp values. Ticket 11 measured this, and
  `RLIMIT_NICE` is 0 in a container.
- `cmd thermalservice` on Android 13 has `override-status` and `reset` only.
  There is no `get-current-status` and no `headroom`.

## Driver

The stock Qualcomm driver and Turnip are both in play. This repository has
already measured the difference on this device: the Uncharted scene runs at
7 FPS on the stock driver and 30 FPS on Turnip, at resolution 2
(`CLAUDE.md`, `tools/android/uncharted_scene.sh`). That is a 4.3x gap.

Other emulator projects report Turnip as equal to the stock driver on FPS
and better on correctness. No source for that is named in this repository, so
treat it as unverified. Ticket 00 confirms the choice on this device with the protocol
below and writes the result here. Until it does, no later ticket may assume a
driver.

Turnip facts that matter here, read from Mesa main:

- `VK_EXT_external_memory_host` is absent. The External Host mapping mode is
  unavailable on both drivers.
- `VK_KHR_present_wait`, `VK_KHR_dynamic_rendering_local_read`,
  `VK_EXT_pipeline_creation_cache_control` and
  `VK_EXT_pipeline_creation_feedback` are present.
- `VK_KHR_pipeline_binary` is absent.
- `VK_EXT_memory_priority` is absent.
- `minUniformBufferOffsetAlignment` is 64. `maxUniformBufferRange` is 64 KiB.
- The advertised descriptor limits are 16777216 per stage, which hides the real
  hardware limits. Qualcomm's real limits for A7xx are multiples of 16 unique
  uniform buffers, textures plus storage buffers, and samplers per pipeline.
  Using more than 16 of any of them costs fill rate.
- `TU_DEBUG` and `TU_AUTOTUNE_*` are read through `os_get_option()`, which on
  Android checks the system property `debug.mesa.tu.debug` first. The Plus
  `tu-debug` config only sets the environment variable, so it cannot reach
  the property route.
- Concurrent binning is off by default in Turnip. `TU_DEBUG=forcecb` or
  driconf `tu_allow_concurrent_binning=true` turns it on.

## Success criteria

Ticket 04 records the baseline. Each title gets a target of 30 or 60 FPS
there. Criteria are measured on a second play of the scene, with a warm shader
cache, in the Ayaneo mode named in ticket 04.

1. **Frame rate held.** At least 95% of recorded seconds have
   `fps >= target - 1`. A recorded second is one line of `frames.csv`, counted
   into its second by `steady_us`.
2. **Steady frames.** Over the 60 second record of one run, the 99th
   percentile of frame intervals is at most 1.5 times the target frame time.
3. **No heat drop.** In a 20 minute run, the average FPS of minutes 18 to 20
   is within 3% of the average of minutes 1 to 3, and the Android thermal
   status stays at 3 (SEVERE) or below. The values are 0 none, 1 light,
   2 moderate, 3 severe, 4 critical, 5 emergency, 6 shutdown.
4. **No regression.** On every title the average FPS is not lower than the
   baseline by more than the spread of the A runs.
5. **GPU work is understood.** Every change in this plan is recorded with the
   GPU busy percentage and the GPU clock from `/sys/class/kgsl/kgsl-3d0/`, so
   a reader can tell whether a change moved GPU time or CPU time.
6. **No desktop regression.** `container/vita3k.sh test` passes, the format
   check passes, and a title from ticket 04 is checked on a desktop build
   before the plan closes. `gpu_busy_percentage` does not exist there, so this
   check is a correctness check, not a speed check.

A title that cannot meet 1 or 2 counts as a success if it improves over the
baseline by more than the A spread and meets 3 and 4. Record why.

## Measurement protocol

Every experiment uses this protocol. Record results under the ticket's
`## Answer`.

1. **Package.** Use only the release package `org.vita3k.emulator`, built with
   `container/vita3k.sh android release`. Games, drivers and `config.yml` are
   per package. Never mix packages in one set of runs.
2. **Same build.** A and B use the same APK. Put a code change behind a
   temporary config value and switch it with `tools/android/device.sh
   config-set`. Compare two builds only when the ticket says that is not
   possible.
3. **Driver.** Name the driver in every record: stock, or Turnip with the file
   name and build. `custom-driver-name` selects it. Never compare runs on
   different drivers.
4. **Device state.** Charger connected, battery at 50% or more, airplane mode
   on, brightness fixed at 50%, Ayaneo mode as named in ticket 04, run
   `adb shell am kill-all` before the set. Run `device.sh config-guard` so no
   per-game config file overrides the test.
5. **Warm-up.** Run once and discard. It fills the shader and pipeline caches.
   Tickets that measure compile stutter say when to skip this. Run
   `adb shell am kill-all` once, before the warm-up run, and not between runs:
   it clears background processes, and between runs it would also change the
   cache state that step 5 depends on.
6. **Order.** Run A, B, A, B, A. Each run: reach the scene, wait 30 seconds,
   record 60 seconds. Between runs stop the app and wait until the CPU
   temperature read by `tools/android/device.sh clocks` is within 2 degrees C of
   the first run's start temperature, or 3 minutes, whichever comes first. If
   no CPU temperature zone is readable without root, ticket 01 says so and the
   protocol uses the 3 minute wait alone.
7. **Validity.** The set is valid if the three A averages are within 3% of
   each other. B differs from A only if its difference from the A mean is
   larger than the A spread (maximum minus minimum). If the set is not valid,
   repeat it once, then write "not valid" and the numbers.
8. **Record.** Build commit, driver, every setting that differs from the
   defaults, the `perf_summary.py` output for each run, the KGSL
   `gpu_busy_percentage` mean, and the KGSL `gpuclk_khz` mean.
9. **Raw files.** Keep logs, CSV files, traces and screenshots under `tmp/`.
   Do not commit them.

## Constraints

- Do not change behavior on other devices unless it is measured there too.
  Device-specific defaults go through the preset in ticket 22.
- Nine settings have no field in `android/app/src/main/java/org/vita3k/emulator/data/EmulatorConfig.kt`
  and cannot be set from the Android settings screen: `guest-cores`,
  `cpu-pool-size`, `hashless-texture-cache`, `disable-programmable-blending`,
  `surface-sync-clamp-rt`, `preempt-on-wake`, `preempt-on-wake-us`,
  `disable-raster-order` and, after ticket 02, `perf-log`. A preset value for
  one of these reaches the app through `config.yml` only, which is a different
  path from every other preset value. Ticket 22 records which ones take that
  path.
- Match the device on `Build.MANUFACTURER` = `AYANEO` and `Build.MODEL`, or on
  the Vulkan device ID that ticket 01 records. Do not match GPU names that
  contain "740": the stock driver reports `Adreno (TM) 740` on more than one
  device.
- Build both targets before a commit: `container/vita3k.sh build` and
  `container/vita3k.sh android release`.
- Follow the writing standard in `CLAUDE.md` for all text.
- A measurement run needs the device unlocked and the scene reachable by
  `tools/android/device.sh`. Games come from the user's own library. Do not
  add game content to the repository.

## Out of scope

- Other Android devices, except as a regression check.
- The AYN Thor. Its GPU and driver differ.
- Game-specific rendering bugs, unless a benchmark title cannot be measured
  without the fix.
- The zip games feature (`.scratch/zip-games/`). It shares the apps list and
  the launch path with this work, so merge order matters, but it has its own
  plan.