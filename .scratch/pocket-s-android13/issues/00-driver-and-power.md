# 00: Fix the driver and the power constraint before anything else

Status: open
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

## Comments
