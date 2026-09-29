# 14: Find the best resolution, filter and output size

Status: open
Type: experiment
Label: ready-for-agent
Blocked by: 10

## Question

Which resolution multiplier, screen filter and output size give the best
picture on the 2560x1440 screen while all titles still meet their target?

## Context

- Multipliers step by 0.25. 2.5x = 2400x1360, 2x = 1920x1088.
- Filters: Nearest, Bilinear (default), Bicubic, FXAA, FSR
  (`renderer/include/renderer/vulkan/screen_filters.h:79-165`). FSR is a
  compute pass. With Turnip, the swapchain has no storage flag
  (`vulkan/screen_renderer.cpp:266-268`); check that FSR still works.
  `feat/vulkan-device-profiles` (`dfb60519`) adds an FSR fallback.
- The swapchain is the full screen size. `preTransform` is always
  Identity (`:288`). If the panel is portrait in its native orientation,
  the compositor must rotate each frame.
- A higher multiplier also makes surface sync copies larger, which costs
  CPU time, not only GPU time.

## Steps

1. Log `currentTransform` from the surface capabilities. If it is not
   Identity, create a ticket to use it as `preTransform`.
2. Use the driver and mapping mode from tickets 06 and 10.
3. Test on all 4 titles: 1.0 + FSR, 2.0 + Bilinear, 2.0 + FSR,
   2.5 + Bilinear, 2.5 + FSR. A = 1.0 + Bilinear.
4. Test a smaller output: set `SurfaceHolder.setFixedSize(1920, 1080)` on
   the emulator surface (temporary config value), so the display hardware
   scales to 1440p. Compare with the best pair from step 3.
5. Take one screenshot of the same frame for each case under `tmp/`.

## Output

Under `## Answer`: a table of case, title, average FPS, percent at target,
99th percentile ms. The user ranks picture quality in session H3. The
recommendation is the case the user ranks highest that meets the target on
all titles.

## Answer
