# 20: Settle the resolution multiplier and the mapping mode

Status: open
Type: experiment
Label: ready-for-human
Blocked by: 05

## Goal

The panel is 2560x1440 and the Vita output is 960x544. The pixel count scales
with the square of the multiplier. This ticket changes no code. It finds the
value the preset should set for this device.

| Multiplier | Output | Pixels against 960x544 |
|---|---|---|
| 1.0 | 960x544 | 1.00x |
| 1.5 | 1440x816 | 2.25x |
| 2.0 | 1920x1088 | 4.00x |
| 2.5 | 2400x1360 | 6.25x |

## Steps

1. Run the A/B/A protocol at 1.0, 1.5, 2.0 and 2.5 on each benchmark title.
   Record FPS, the frame interval 99th percentile, GPU busy percentage and GPU
   clock.
2. Read the result with the pixel table above, not with FPS alone. A title that
   holds 30 FPS at 2.5x is spending six times the GPU work for the same frame
   rate.
3. Find the largest multiplier each title holds its target at, with the frame
   interval 99th percentile still inside criterion 2 of `../spec.md`.
4. Check that the resolution multiplier and the memory mapping mode interact.
   `renderer.cpp:1404` and `context.cpp:225` use the multiplier, and the
   surface cache allocates at it. A higher multiplier with
   `disable-surface-sync` off means more bytes copied back per scene.
5. Record whether the renderer's own resample path
   (`vita3k/renderer/src/vulkan/surface_cache.cpp:2519`) is cheaper than
   letting the screen filter do it. Note that this blit scales the rendered
   target down to guest resolution for the write-back, and it only runs when
   `disable-surface-sync` is off. Ticket 28 covers giving the window a smaller
   size so the display hardware scales instead.

## Acceptance

- A table per title: the largest multiplier that meets criteria 1 and 2, and
  what each step costs.
- A recommended default for this device, and what a user who wants a sharper
  picture should expect to pay.

## Answer

## Comments
