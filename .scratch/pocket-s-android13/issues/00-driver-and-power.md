# 00: Fix the driver and the power constraint before anything else

Status: open
Type: experiment
Label: ready-for-human
Blocked by: none

## Question

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
