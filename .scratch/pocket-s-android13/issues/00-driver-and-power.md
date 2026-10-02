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
   `## Answer

**CORRECTED 2026-10-02, second pass. The first answer was wrong.** It was
measured on the Uncharted title screen, which barely touches the renderer. In
actual gameplay the order is the other way round: Turnip is 5.2 times faster
than the stock driver. The user reported this before the second pass, and the
user was right.

### Gameplay measurement

`tools/android/gameplay_scene.sh` walks the Uncharted menus into the saved
chapter, then holds the right stick and the movement keys so the camera turns
and the world has to be re-rendered. The right stick is bound to I, J, K and L
in the emulator config. Read with `--warmup 120`, which skips the boot and the
menu walk and leaves the movement window.

| | stock, `high-accuracy: false` | stock, `high-accuracy: true` | Turnip |
| --- | --- | --- | --- |
| FPS | 5.76 | 6.39 | 29.96 |
| seconds at target | 0.0% | 6.6% | 100.0% |
| frame interval p99 | 187.91 ms | 169.84 ms | 43.43 ms |
| intervals over 50 ms | 287 | 654 over the whole run | 4 |
| scenes per second | 81.6 | 90.5 | 424.5 |
| draws per scene | 39.91 | 39.43 | 40.04 |
| max draws per scene | 337 | 337 | 337 |

Draws per scene and the maximum are the same to within rounding in all three
runs, so it is the same scene and the same work. Turnip does it 5.2 times
faster.

The emulator's own overlay agrees and adds the reason it is hard to miss:
**GPU 99% on the stock driver and 93% on Turnip, at 5.76 and 29.96 FPS.** Both
saturate the GPU. The stock driver spends five times the GPU work per frame for
the same picture.

### Why: the stock driver has no fast framebuffer-fetch path

This is the finding, and it is not what the plan expected. From the app log on
the stock driver:

```
FeatureState: support_shader_interlock=false support_texture_barrier=false
              direct_fragcolor=true programmable_blending=true
```

Vita3K emulates programmable blending by reading the color attachment that the
same pass writes. It has three ways to do that, and the stock driver supports
none of the two fast ones:

| Path | Needs | Stock | Turnip |
| --- | --- | --- | --- |
| rasterization order attachment access | `rasterizationOrderColorAttachmentAccess` | no | yes |
| shader interlock | `fragmentShaderSampleInterlock` | no | not needed |
| `direct_fragcolor` | nothing, always available | **yes, the only one** | not used |

`direct_fragcolor` uses a subpass input attachment and puts a
`vkCmdPipelineBarrier` on the color attachment before **every draw that uses
programmable blending** (`renderer/src/vulkan/scene.cpp:448`). On a tile-based
renderer that barrier can force a store and a load out of tile memory.

The stock driver does list `VK_EXT_rasterization_order_attachment_access` and
`VK_ARM_rasterization_order_attachment_access` among its extensions. The code
queries the **feature** `rasterizationOrderColorAttachmentAccess`, not the
extension, and the feature is off. So the extension being present does not help.

`high-accuracy: true` does not give the stock driver a second option. It was
measured and it changes nothing about the path: `direct_fragcolor=true` still,
6.39 FPS against 5.76, inside the noise of two runs. The stock driver has one
path and the plan cannot choose another.

### What this changes

- **The plan measures on Turnip.** The earlier correction in this file, which
  named the stock driver, was itself wrong and is replaced by this answer.
- `CLAUDE.md` is corrected again. Its current text says 60 FPS on stock and 33
  on Turnip. The truth is 5.8 and 30.
- Ticket 06 is no longer a question about three paths. It is a question about
  one path that the stock driver forces, and it is the largest single
  performance difference measured on this device.
- Ticket 16, the Adreno shader compiler workaround, targets the stock driver.
  That is now the slow path, so it matters less.
- Ticket 05's `disable-programmable-blending` row becomes the most interesting
  setting on the device, because turning it off removes the per-draw barrier.
  It loses blending accuracy, so it needs the picture check the ticket already
  requires.
- The 680 MHz GPU cap from the first pass still stands and is still ticket 24.
  Turnip reaches 30 FPS with the GPU at 93%, so the cap is not what limits it.
- The prime core at 595 MHz is ticket 29 and is unchanged. The overlay shows
  CPU 43% on Turnip and 23% on stock, so the CPU is not the limit either.

### Method note, because it nearly caused a wrong answer

The first pass launched the game and read the FPS after 70 seconds. That is the
title screen. `perf_summary.py` averaged over the whole file by default, which
mixed the menus and the level, and gave 25 FPS for the stock driver when the
gameplay figure is 5.76.

The fix is `tools/android/gameplay_scene.sh`, which reaches the level and moves
the camera, plus reading the result with a `--warmup` that skips the menus.
Every measurement in this plan has to do that. A frame rate measured anywhere
other than in the game is not a result.

## Comments
