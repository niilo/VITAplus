# Spec: Android 13 performance for the Ayaneo Pocket S

## Goal

**Minimise the energy the emulator uses per played frame, without making the
game less playable.**

Concretely, in this order:

- A game locked to 30 FPS must hold a steady 30 FPS. Not an average of 30:
  every frame on time, with no late frames.
- A game with an unlocked frame rate must reach 60 FPS where the hardware
  allows.
- Subject to those two, the device must draw as little power as possible.

Frame rate on its own is not the goal and a higher frame rate is not
automatically better. A change that raises the frame rate and raises the power
is a regression. A change that lowers the power and holds the frame rate
steadily is a win, even when the average frame rate does not move.

Measured on 2026-10-02, before this retarget: Uncharted, Turnip, resolution 2,
29.96 FPS with a 43.43 ms p99 interval and 93% GPU busy. The 43.43 ms p99
**fails** the steadiness bar in this spec. Under the old goal that run passed;
under this one it is a playability defect to fix. The energy half of the plan
had not been measured at all.

Every change must be measured on the device with the protocol below. A change
without a measurement is not done. A measurement from a run that fails
`tools/android/run_is_valid.sh` is not a measurement.

This plan replaces `.scratch/pocket-s-optimization/`. That plan was written
against the old code base. The base is now Vita3K-Plus (`.scratch/plus-base/`),
and several settings it assumed are no longer true. This plan also covers the
two questions the user asked for: how to make the Vulkan rendering cheaper,
and which Snapdragon CPU features can save clock cycles.

## Shape of the plan

31 tickets in `issues/`, numbered from `00` to `30`. The dependency graph decides
what runs first, not the numbering. Tickets 00, 01 and 02 are resolved; every
other row below is work that has not been done.

| Tickets | What they do |
| --- | --- |
| 00 | Choose the driver. Resolved: the plan measures on Turnip. The GPU power constraint was not answered and is ticket 24. |
| 01, 02 | Device facts and the perf-log port. Both resolved. |
| 03 | The profiling harness. Built and merged. `trace`, `clocks` and `latency` run on the device, and the release APK builds. `perf` and `latency` need a game in play. |
| 04 | The baseline. Every other measurement is compared against it. |
| 05 | The render configuration matrix. The framebuffer-fetch rows are already answered; the write-back and thread rows are not. |
| 06 to 10 | The Vulkan frame path: framebuffer fetch, visibility queries, macroblock sync, present mode, vblank clock. |
| 11 to 14 | Host CPU. **All four are expected to close as rejected or inert.** Ticket 00 measured the GPU at 93 to 99% busy and the CPU at 23 to 43%, and ticket 11 found the kernel refuses the nice value. Ticket 12 is expected to be rejected outright because transparent huge pages are already `always` here. Ticket 13 survives only as the delivery mechanism for ticket 29's core-placement question, and its frame-rate outcome is expected to be flat. |
| 15 to 21 | More Vulkan and GPU: attachment layouts, the shader compiler workaround, pipeline stutter, screen filters, the Turnip autotuner, the resolution multiplier, thermal measurement. |
| 22 | The preset. |
| 23 | Verification and the write-up. |
| 24 to 28 | Areas this plan's first draft did not cover: GPU power constraint, dynarmic flags, texture upload, descriptor and uniform limits, output surface size. With the GPU saturated, 18, 19, 20, 24 and 28 are the real remaining levers. |
| 29 | The prime core stays at 595 MHz while its maximum is 3360 MHz. Found by ticket 00. |
| 30 | Why the stock driver cannot use rasterization order attachment access, and whether it can be made to. Found by ticket 00; the largest unowned question in the plan. |

## What changed since the old plan

The old plan assumed the old code base. On the Plus base:

- `high-accuracy` defaults to `true` (upstream: `false`). It turns off texture
  viewport and would turn on shader interlock, except that neither driver on
  this device has `fragmentShaderSampleInterlock`, so it does neither. Measured
  inert on the stock driver at 6.39 against 5.76 FPS.
- `memory-mapping` defaults to `page-table` on Android. On the stock Adreno
  driver the renderer logs the downgrade and runs it as `double-buffer`
  (`vita3k/renderer/src/vulkan/renderer.cpp:1112`).
- `disable-surface-sync` defaults to `false` (upstream: `true`).
- `guest-cores` defaults to `3` and `accurate-thread-scheduling` to `true`.
  Neither has been measured on this device.
- `async-pipeline-compilation` defaults to `false` (upstream: `true`).
- `perf-log` was gone too. It only ever existed on the old base
  (`pocket-s/05-frame-timing-log`, `d392779e`). Ticket 02 ported it: it is now in
  `config.h`, `native_config.cpp`, `EmulatorConfig.kt` and the settings screen,
  and it writes `frames.csv`, `presents.csv` and `scenes.csv`. Every measurement
  in this plan needs it on.

Two of the six settings above have since been measured and neither is a lever:
`high-accuracy` is inert, and `disable-programmable-blending` is rejected
(ticket 05).

## Hardware

Ticket 01 measured all of this on the device on 2026-10-02. The vendor and the
driver names, and what the device reports:

| Part | Value | Source |
|---|---|---|
| SoC | Snapdragon G3x Gen 2. `ro.soc.model` = `SG8275`, `ro.board.platform` = `kalama` | `pocket-s-optimization/spec.md`, checked 2026-09-28 |
| CPU | 8 cores, ARMv9.0-A: 1 Cortex-X3, 2 Cortex-A715, 2 Cortex-A710, 3 Cortex-A510. Measured clusters: `related_cpus` gives cpu0 to cpu2, cpu3 to cpu6, cpu7. Governor `walt`. | ticket 01 |
| CPU clocks | cpu0 to cpu2 2016 MHz, cpu3 to cpu6 2803.2 MHz, cpu7 3360 MHz. Under load the big cluster reaches 1843 MHz and cpu7 sits at 595 MHz. | ticket 01 `cpuinfo_max_freq`, ticket 00 |
| GPU | Adreno A32. `gpu_model` reads `AdrenoA32`, the stock driver reports `Adreno (TM) 740`, and Mesa maps the part to `FDA32`, the same bucket as Adreno 740. The frequency table lists 1000 MHz as the top bin, `max_gpuclk` reads 680 MHz, and the observed maximum under load is 680 MHz. | ticket 01 |
| RAM | LPDDR5X-8533, 16 GB on the test device | `pocket-s-optimization/spec.md` |
| Display | 6 inch IPS, 2560x1440, 60 Hz, one mode | `pocket-s-optimization/spec.md` |
| Cooling | vapour chamber over 5180 mm2, fan, 15 W sustained (vendor claim) | AYANEO |
| OS | Android 13, build `TKQ1.230811.002`. Kernel 5.10 or 5.15. | `pocket-s-optimization/spec.md` |

The old plan called this part "Snapdragon 8 Gen 2, Adreno 740". The vendor
calls it Snapdragon G3x Gen 2 with an Adreno A32. The two are the same silicon
with different names. Ticket 01 settles which names the device reports.

### ARM features on this part

The cores report ARMv9.0-A. Ticket 01 read `/proc/cpuinfo`; the same line is on
all eight cores:

```
atomics asimdhp asimddp flagm flagm2 ssbs sb paca pacg i8mm bf16 bti dit
```

Present: `atomics` (LSE), `asimdhp` (FP16), `asimddp` (DotProd), `flagm2`,
`ssbs`, `sb`, `paca`/`pacg` (PAuth), `i8mm`, `bf16`, `bti`, `dit`. Absent:
`sve`, `mte`.

`i8mm` and `bf16` are past the ARMv9.0-A baseline, so the architecture version
would not have decided them. That is why the earlier wording was replaced by the
measurement.

The guest is ARMv7 (`vita3k/cpu/src/dynarmic_cpu.cpp:644` sets
`ArchVersion::v7`), so the host's LSE2, FP16 and DotProd instructions never
appear in JIT output. Those features can only help the emulator's own C++ code,
which is shader translation, texture decode and format conversion. No ticket
in this plan measures those, so no ticket claims a gain from them.

With the GPU at 93 to 99% busy and the CPU at 23 to 43%, none of those is
expected to move the frame rate, and ticket 00 measured it. The CPU tickets are
about recording the answer, not about recovering frames, and they should be run
at low effort.

The CPU questions that survive are the prime core in ticket 29 and the
placement hint that would deliver it, ticket 13.

## What this means for CPU work

The guest is ARMv7 (`vita3k/cpu/src/dynarmic_cpu.cpp:644` sets
`ArchVersion::v7`), so the host's LSE2, FP16 and DotProd instructions never
appear in JIT output. Those features can only help the emulator's own C++ code,
which is shader translation, texture decode and format conversion. No ticket
in this plan measures those, so no ticket claims a gain from them.

With the GPU at 93 to 99% busy and the CPU at 23 to 43%, none of those is
expected to move the frame rate, and ticket 00 measured it. The CPU tickets are
about recording the answer, not about recovering frames, and they should be run
at low effort.

The CPU questions that survive are the prime core in ticket 29 and the
placement hint that would deliver it, ticket 13.

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

**This plan measures on Turnip.** Ticket 00 measured Uncharted Golden Abyss in
gameplay, with the camera moving, at resolution 2, reading the perf-log with a
120 s warm-up so the menus are excluded:

| | stock Qualcomm 512.676.0 | Turnip Balemuni Apex v2 |
| --- | --- | --- |
| FPS | 5.76 | 29.96 |
| seconds at target (30) | 0.0% | 100.0% |
| frame interval p99 | 187.91 ms | 43.43 ms |
| scenes per second | 81.6 | 424.5 |
| draws per scene | 39.91 | 40.04 |
| GPU busy, from the overlay | 99% | 93% |

Same scene, same draws, 5.2 times faster. Both saturate the GPU, so the stock
driver spends five times the GPU work per frame for the same picture.

The cause is the framebuffer-fetch path. Vita3K emulates programmable blending
by reading the colour attachment the same pass writes, and has three ways to do
it. The stock driver supports neither fast one:

| Path | Needs | stock | Turnip |
| --- | --- | --- | --- |
| rasterization order attachment access | `rasterizationOrderColorAttachmentAccess` | no | yes |
| shader interlock | `fragmentShaderSampleInterlock` | no | not needed |
| `direct_fragcolor` | nothing | **the only one available** | not used |

`direct_fragcolor` puts a `vkCmdPipelineBarrier` on the colour attachment
before every programmable-blending draw (`renderer/src/vulkan/scene.cpp:448`).
On a tile renderer that can force a store and a load out of tile memory.
`high-accuracy: true` gives the stock driver no second option; it was measured
at 6.39 FPS, inside the noise.

The stock driver does list the extension
`VK_EXT_rasterization_order_attachment_access`, but the code queries the
feature and the feature is off.

An earlier pass on ticket 00 measured the title screen and had the two drivers
the other way round, 60 FPS stock and 33 Turnip. That was wrong and is
recorded in the ticket, because it is the mistake this plan is most likely to
repeat.

Consequences:

- The stock driver is the slow path. Ticket 06 is the largest single
  performance difference on this device and it is one path, not three.
- Ticket 05's `disable-programmable-blending` row is the most interesting
  setting on the device, because turning it off removes the per-draw barrier.
- Ticket 05's `memory-mapping` row is a Turnip question, since the stock driver
  forces double-buffer.
- Ticket 16 targets the stock driver and matters less.
- The GPU never goes above its 680 MHz cap on either driver, while the hardware
  table lists 1000 MHz. Ticket 24. Turnip reaches 30 FPS at 93% GPU, so the cap
  is not what limits it.

## Success criteria

Ticket 04 records the baseline. Each title declares its target: 30 FPS for a
game the console locked to 30, 60 for an unlocked one. A title that targets 30
and a title that targets 60 are held to **different criteria**, and a change is
judged on each against its own target.

Measured on the second play of the scene, warm caches, on a cool device, in the
Ayaneo mode named in ticket 04, over the 60 second record of one run.

1. **Playability first, and it is a gate, not a score.** A change is only
   admissible if it holds both of these, on each title:
   - the target frame rate for that title, and
   - the steadiness of criterion 2, which is the part the emulator controls.

   A change that improves energy and misses either is **rejected**, whatever it
   saves. Energy never buys playability.

2. **Steady frames.** Over the 60 second record, the 99th percentile of frame
   intervals is at most 1.5 times the target frame time, so at most 50 ms for a
   30 FPS title, and the mean interval is within 5% of the target frame time.
   On a 60 Hz panel the quantum is 16.67 ms, so a 30 FPS frame that lands one
   vsync late measures 50 ms. Criterion 2 is therefore also a statement about
   how often a frame misses its vsync, so record the **fraction** of intervals
   above 1.5 times the frame time and not only the p99.

3. **Energy.** The mean device power draw in watts, from
   `tools/android/device_power_sample.sh`, over the same window, with the GPU
   clock, the GPU busy percentage and the CPU clock per cluster beside it. An
   A/B counts as a win only if the power difference is larger than the spread
   of the A runs. Device power is the battery current times the battery voltage
   and covers the display and the Android system as well as the emulator, so
   only the **difference** between two runs belongs to the emulator.

4. **No playability regression beyond the frame.** Input latency must not get
   worse, and the picture must not change. Both are checked by screenshot
   comparison at fixed moments. A picture change is a rejection even when the
   power saving is large: this target says "without compromising playability",
   and a wrong picture is not playable.

5. **No heat drop.** In a 20 minute run the average FPS of minutes 18 to 20 is
   within 3% of minutes 1 to 3, and the Android thermal status sampled
   **during** the run stays at 3 (SEVERE) or below. Values: 0 none, 1 light,
   2 moderate, 3 severe, 4 critical, 5 emergency, 6 shutdown. A run above 3 is
   invalid, not a slow run.

6. **The run is valid.** Every number comes from a run that
   `tools/android/run_is_valid.sh` passes. It rejects a run where the
   emulator's own watchdog fired, where the thermal status during the run was 3
   or above, where the frame data is too thin, or where the power samples
   covered a menu instead of play. The first attempt at an energy baseline in
   this plan was rejected by exactly this rule, which is why the rule exists.

7. **No desktop regression.** `container/vita3k-docker.sh test` passes, the
   format check passes, and a title is checked on a desktop build before the
   plan closes. That is a correctness check, not a speed check.

A change that holds 1 and 2 and cuts energy in criterion 3 is a win. A change
that holds 1 and 2 and cuts energy by less than the A spread is **rejected**:
it is inside the noise, and a change inside the noise is not worth the risk of
a merge.

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
3. **Driver.** The reference driver is **Turnip**. Name it in every record,
   with the file name and build, and record `perf-log: true` beside it. Stock
   appears only in ticket 30 and in the two labelled spot-checks ticket 04
   allows. Never mix drivers inside one result set: ticket 00 already lost a
   conclusion to exactly that.
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
   the first run's start temperature, or 3 minutes, whichever comes first.
   Ticket 01 found that the zones named `cpu-0-*` and `cpu-1-*` read without
   root, so the temperature is available. The zones named `pa`, `sdr*`, `mmw*`,
   `epm*` and `pmr735d*` return `Invalid argument` and are skipped.
7. **Validity.** The set is valid if the three A averages are within 3% of
   each other. B differs from A only if its difference from the A mean is
   larger than the A spread (maximum minus minimum). If the set is not valid,
   repeat it once, then write "not valid" and the numbers.
8. **Record.** Build commit, driver, every setting that differs from the
   defaults, the `perf_summary.py` output for each run, the KGSL
   `gpu_busy_percentage` mean, and the KGSL `gpuclk` mean.
9. **Measure in the game, not at a menu.** Use
   `tools/android/gameplay_scene.sh`, which walks into the saved chapter and
   then moves the camera, and read the result with a `--warmup` long enough to
   skip the boot and the menu walk. A frame rate measured on a title screen
   barely touches the renderer, and that mistake has already produced one
   wrong conclusion in this plan. Ticket 00 records it.
10. **Sample the power during the run.** Run
    `tools/android/device_power_sample.sh <csv> <seconds>` alongside the game,
    covering the movement window. Device power is the battery current times the
    battery voltage, read from `/sys/class/power_supply/battery`. It covers the
    display and the Android system as well as the emulator, so only the
    difference between two runs is the emulator's share. The sampler records the
    thermal status during the run, because a status read afterwards cannot say
    whether the run itself was limited.
11. **Reject an invalid run.** Run `tools/android/run_is_valid.sh <gameplay dir>
    <power csv>` before quoting any number. It rejects a run where the emulator's
    watchdog fired, where the thermal status during the run was 3 or above,
    where the frame data is too thin, or where the power samples covered a menu.
    The first energy baseline attempted in this plan failed this check: the
    emulator reported no flip for 8453 vblanks and the device was at thermal
    status 3.
12. **Raw files.** Keep logs, CSV files, power CSVs, traces and screenshots
    under `tmp/`. Do not commit them.

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