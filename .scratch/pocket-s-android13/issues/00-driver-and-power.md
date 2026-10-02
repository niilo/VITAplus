# 00: Fix the driver and the power constraint before anything else

Status: resolved
Type: experiment
Label: ready-for-human
Blocked by: none

## Question

Which Vulkan driver do the rest of this plan's measurements run on, and does the
GPU power constraint limit the clock?

## What ticket 01 already found

The installed `org.vita3k.emulator` 1.1 has `custom-driver-name:
Balemuni_Apex_v2_ULTIMATE_SD8Gen2`, and its log reports:

```
driverID: MesaTurnip  driverName: Turnip (Balemuni Apex v2 Ultimate)
driverInfo: Mesa 26.3.0-devel (SD 8 Gen 2 / Adreno 740 Apex v2 Ultimate by Balemuni)
conformance: 1.4.0.0
Present mode: Mailbox
Using the following memory mapping method: Page Table
```

So the device runs Turnip and has been running it. What is missing is the
measurement: the 7 FPS against 30 FPS number in `CLAUDE.md` came from an older
build and a different configuration. This ticket supplies that.

Consequence: `VK_EXT/ARM_rasterization_order_attachment_access` is present, so
the framebuffer-fetch path this device takes is the raster-order one, and
shader interlock is not used at all. `conformance: 1.4.0.0` means
`VK_KHR_dynamic_rendering_local_read` is core in 1.4 on this driver.

Which Vulkan driver do the rest of this plan's measurements run on, and does the
GPU power constraint limit the clock?

## Why this is first

The stock Qualcomm driver forces `double-buffer` no matter what the user
configures (`vita3k/renderer/src/vulkan/renderer.cpp:1112`), and
`double-buffer` `memcpy`s guest memory into the GPU-visible buffer on every
vertex stream, index buffer and uniform block
(`renderer.cpp:2396`, `:2415`, `:2474`). Every Vulkan ticket in this plan
measures a different cost on the two drivers, because the two paths are not the
same code. A plan that leaves this open measures nothing comparable.

## Steps

1. Install both drivers. Record the stock driver version from
   `adb shell dumpsys gpu --gpudriverinfo`, and the Turnip file name and build.
2. Run `tools/android/uncharted_scene.sh` on the Uncharted scene on both, at
   resolution 2, with the protocol from `../spec.md`. Record FPS, the frame
   interval 99th percentile, GPU busy percentage and GPU clock.
3. Decide. This repository already measured 7 FPS on the stock driver and
   30 FPS on Turnip for this scene (`CLAUDE.md`). If Turnip wins again, every
   ticket from here measures on Turnip, and every ticket writes one line in its
   `## Answer` saying what its result means for the stock driver. If it does
   not win, the rest of the plan measures on the stock driver.
4. Write the decision into `../spec.md` as one named driver, so every later
   ticket refers to it instead of repeating itself.
5. Then do ticket 24, the GPU power constraint, and write its result in the same
   place.

## Acceptance

- One driver named in `../spec.md`, with the numbers that named it.
- For every ticket that follows, a note in `../spec.md` of whether its result
  is expected to carry over to the other driver.
- Ticket 24 either `resolved` or `rejected` with its numbers.

## Answer

**The stock driver is 1.84 times faster than Turnip on this device.** The
plan, `CLAUDE.md` and the earlier research all assumed the opposite.

Measured on 2026-10-02 with the release build of this branch, Uncharted
Golden Abyss (PCSA00029), the same saved chapter, resolution 2, screen filter
Bilinear, `high-accuracy: false`, one run each, 20 s warm-up skipped:

| Run | Driver | FPS | frame interval p99 | scenes/s | draws/scene |
| --- | --- | --- | --- | --- | --- |
| stock-1 | Qualcomm 512.676.0 | 60.02 | 19.87 ms | 480.1 | 34.24 |
| stock-2 | Qualcomm 512.676.0 | 59.95 | 19.99 ms | 479.4 | 34.25 |
| turnip-1 | Turnip Balemuni Apex v2 | 32.84 | 33.47 ms | 262.7 | 34.25 |
| turnip-2 | Turnip Balemuni Apex v2 | 32.68 | 34.89 ms | 261.4 | 34.25 |

The draws per scene are identical to two decimals in all four runs, so the four
runs are the same scene and the same work. The scenes-per-second ratio,
480.1 / 262.7 = 1.83, matches the frame-rate ratio, so Turnip is not doing more
work per frame. It is slower at the same work.

Stock is at the 60 Hz panel cap, so 60 FPS is a floor on its capability, not a
measurement of it.

### GPU and CPU clocks during the runs

`tools/android/device_clock_sample.sh`, one sample per second:

| | GPU clock mean | GPU busy | CPU prime core (cpu7) |
| --- | --- | --- | --- |
| stock | 597 MHz | 62.1 % | 595 MHz in 19 of 22 samples |
| turnip | 501 MHz | 62.1 % | 595 MHz in 12 of 18 samples |

**The GPU never goes above 680 MHz on either driver**, which is the cap
`max_gpuclk` reports, while the hardware table lists 1000 MHz. So ticket 24's
question is live and the cap is real.

GPU busy percentage is the same on both, so the same fraction of time is spent
with the GPU busy; the difference is how much work fits into that time.

**The prime core sits at 595 MHz while its maximum is 3360 MHz.** The governor
is `walt` and it leaves cpu7 in a low bin for most of the run. That is a
separate finding from the driver one and it is worth a ticket of its own:
nothing in the app asks for the prime core, and the emulator has threads that
could use it.

### Why this happened, most likely

The stock driver forces `double-buffer` mapping, which the plan called the most
expensive frame path, and it is still 1.84 times faster. So the double-buffer
copies are not what limits the stock driver, and the mapping mode is not the
lever the plan assumed.

Turnip has `support_rasterized_order_access: true` and runs Page Table. Its
lower clock on the same busy percentage suggests its shaders or its binning
are the cost. The Balemuni build reports a 4 GB shader cache and 512 KB
suballocators and calls itself an instruction-level tuned build, so it is a
tuned build for other emulators and not for this one.

### What this changes

- Every Vulkan ticket in this plan measures on the **stock** driver from here.
  Ticket 05's `memory-mapping` row becomes a question about Turnip only.
- Ticket 19, the Turnip autotuner, is worth less. It becomes a question about
  whether a flag recovers the gap, not a source of wins.
- Ticket 06's interlock and subpass paths are **not used** on the stock driver,
  because it has no `rasterization_order_attachment_access` and
  `support_shader_interlock` is false there too. Its `direct_fragcolor` path is
  what runs. So ticket 06 measures one path on stock and a different one on
  Turnip, and stock is the one that matters.
- Ticket 16, the Adreno shader compiler workaround, targets the stock driver.
  It is now more relevant, because the stock driver is the one in use.
- Ticket 07's Turnip hang has to be checked on the stock driver too, since that
  is what runs now. Its extension list is in the answer of ticket 01.
- `CLAUDE.md` says the stock driver runs this scene at 7 FPS and Turnip at 30.
  That is now measured the other way round and the line is corrected.

### Still to do

- The A/B/A protocol from `../spec.md`, three runs each, to put a spread on
  these numbers. Two runs each already agree within 0.2%.
- One run at resolution 1, because a 60 FPS stock result may be limited by the
  panel rather than the GPU.
- Whether the Balemuni build in particular is slow, or Mesa Turnip in general.

## Comments

## Comments
