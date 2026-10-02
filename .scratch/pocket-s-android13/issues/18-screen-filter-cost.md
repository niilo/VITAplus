# 18: Measure the cost of the screen filter

Status: open
Type: experiment
Label: ready-for-human
Blocked by: 05, 20

## Question

The panel is 2560x1440. At a resolution multiplier of 2 the renderer produces
1920x1088 and the screen filter scales that to 2560x1440 in a second pass.
Which filter costs the least, and is the second pass worth it at all?

## Context

- `screen-filter` defaults to `Bilinear`.
- Nearest, Bilinear, Bicubic and FXAA are a single full-screen quad
  (`vita3k/renderer/src/vulkan/screen_filters.cpp:313-321`).
- FSR is two compute dispatches plus a clear plus, on some paths, a blit
  (`screen_filters.cpp:615-694`). Two compute dispatches over 2560x1440 at
  1.0 GHz move at least 7.4 MB each. Measure the cost; do not assume it.
- The present render pass clears to black every frame
  (`vita3k/renderer/src/vulkan/screen_renderer.cpp:528-531`). On a tile renderer
  a clear is cheap, but with a filter pass in front of it, the clear covers the
  full 2560x1440 tile set.

## Steps

1. Run A/B/A per filter at the baseline resolution multiplier, and again at the
   multiplier ticket 20 settles on. Record FPS, the frame interval 99th
   percentile, GPU busy percentage and GPU clock.
2. Report GPU busy percentage for each. The filter's cost shows there before it
   shows in FPS, because a slower GPU can hold the frame rate by raising its
   clock.
3. Check whether the app ever skips the filter pass. Grep for
   `fullscreen_hd_res_pixel_perfect`. The field is declared at
   `vita3k/config/include/config/state.h:95` and read at
   `vita3k/app/src/app_init.cpp:650` and `:743`, which call
   `stretch_hd_pixel_perfect`. Read every write site of that function and say
   which one decides the swapchain size. State the file and the line, or state that nothing
   writes it. No multiplier turns 960x544 into 2560x1440 exactly.
   Write down the closest multiplier and whether the filter pass still runs at
   it.
4. Check the swapchain transform while you are in there. Qualcomm and Google
   both state that a compositor which has to rotate or read back the framebuffer
   costs 1 to 3 ms per frame and raises GPU frequency about 40%. Log
   `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` pre-rotation. It must be
   identity on this panel.

## Acceptance

- A table of FPS, frame interval 99th percentile, GPU busy percentage and GPU
  clock per filter.
- The swapchain transform recorded.
- A recommended default, and a statement of whether the filter pass can be
  skipped at some multipliers.

## Answer

## Comments
