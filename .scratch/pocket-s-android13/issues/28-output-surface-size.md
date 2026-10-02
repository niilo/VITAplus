# 28: Shrink the output surface so the display hardware scales

Status: open
Type: experiment
Label: ready-for-human
Blocked by: 04, 20

## Question

Does giving the window a smaller fixed size than the panel change anything?

## Why it matters

The panel is 2560x1440. At a resolution multiplier of 2 the emulator produces
1920x1088 and something has to scale that to 2560x1440. Today the screen filter
does it on the GPU, in a second pass over the full panel size. The display
hardware can also do it, from a surface that is already smaller than the panel.

On a GPU capped at about 1.0 GHz, one full-screen pass at 2560x1440 is 3.7
million pixels. Whether removing it helps is a measurement, not a rule.

## Steps

1. Read what the app asks the window for today. `AndroidManifest.xml` and the
   `SurfaceHolder` calls in `android/app/src/main/java/org/vita3k/emulator/` set
   the size. Record the current values and the call sites.
2. Add a temporary setting `output-size`, default 0, meaning "use the panel
   size". A value of 1920 means a fixed width of 1920 with the aspect ratio kept.
3. Confirm with `dumpsys SurfaceFlinger --latency` that SurfaceFlinger is
   scaling rather than the app. Also log the swapchain `currentTransform` at
   session start. The vendor document and Google both state that a compositor
   which has to rotate or read back the framebuffer costs 1 to 3 ms per frame
   and raises GPU frequency about 40%, so confirm it is identity first.
4. Run A/B/A at the output size from ticket 20 and at the panel size, with the
   screen filter set to Nearest so the app does no scaling of its own. Record
   FPS, the frame interval 99th percentile, GPU busy percentage and GPU clock.
5. Check the picture. Scaling by the display hardware uses a different filter
   than the emulator's, so take screenshots at three fixed moments per title and
   write down the differences.
6. Separately, test `ANativeWindow_setFrameRate`. It is available on this API
   level, it has not been used anywhere in this repository, and on a 60 Hz
   panel with MAILBOX present mode it is the standard way to tell Android what
   the app produces. Call it with the target frame rate the game wants, once,
   when the swapchain is created and when the target changes. Record whether
   SurfaceFlinger changes behaviour, using `dumpsys SurfaceFlinger`.

## Acceptance

- A measured result for the smaller output surface, with the picture checked.
- A measured result for `ANativeWindow_setFrameRate`, or `Status: rejected`
  with the numbers.
- The swapchain transform recorded.

## Answer

## Comments
