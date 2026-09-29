# 10: Uncharted shows artifacts at resolution multiplier 1

Status: resolved
Claimed: 2026-09-29 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent

## Context

2026-09-29, Ayaneo Pocket S, build `master` fda1b822 (release APK), Turnip
driver Balemuni Apex v2, memory mapping Double buffer, texture viewport on.

At resolution multiplier 1, Uncharted: Golden Abyss (PCSA00029) shows
heavy artifacts in the top part of the screen, and parts of the screen
often blink black. At multiplier 2 the problem is gone (the user checked).
At multiplier 3 it was not seen either. The log has no error for it.

1x is the native Vita size, so this is not expected. The problem is
probably in the scaling code of the renderer (surface cache or texture
viewport). It is not in the NGS changes of this review, because those
change only audio.

## Plan

1. Check if the problem happens on desktop at 1x. If it does, capture a
   frame with RenderDoc.
2. On the device, check with texture viewport off.
3. Compare the surface cache paths that depend on the resolution
   multiplier (`vita3k/renderer/src/vulkan/surface_cache.cpp`).

## Answer

Cause: the byte reinterpret path of `retrieve_color_surface_as_texture`
(`vita3k/renderer/src/vulkan/surface_cache.cpp`) had no barrier between
its two transfer commands. `copyImageToBuffer` writes the transition
buffer, and `copyBufferToImage` reads it right after. Without a buffer
barrier, the GPU can start the read before the write ends and copy stale
rows. At resolution multiplier 1 every typeless copy takes this path,
because the compute de-interleave (ticket 23 of pocket-s-optimization)
runs only above 1. Upstream Vita3K has the same bug.

The same path also used `eTransferSrcOptimal` for an image in `eGeneral`.
That was a bug from the pick of Plus 6839971c. It is fixed too, but it was
not the cause of the artifacts.

How it was found:

1. Tests with the user: high accuracy on (texture viewport off), the
   layout fix, the Plus fix 1a081a1c, the Plus LOD and alu fixes,
   anisotropic filtering 1, surface sync on, depth clamp on: all still had
   artifacts.
2. The old baseline 04401a79 had the artifacts too. So the bug is older
   than our picks.
3. Vita3K-Plus 20588fbf was clean with the same settings.
4. A bisect of the Plus history on the device, with an automatic title
   screen test (the artifacts show on the "Touch to Start" screen): commit
   39 (abe2d71c) bad, commit 40 (8be36fa1) clean. File groups of commit 40
   then showed that the renderer files fix the artifacts. In them,
   8be36fa1 adds this `BufferMemoryBarrier`.

Commits on `log-review/10-typeless-copy-layout`: 92df8755 (layout),
132fce5f (pick of Plus 1a081a1c: unsigned cast format for F16 stores and
zero-initialized shader register banks; kept because Plus has it for
Uncharted eye speckles), bb44d95c (the barrier). With the barrier, 20
title screen frames at 1x are clean, and the user confirmed it in game.

Notes for the next Plus bisect:

- Old Plus commits do not build as they are. The helper
  `plus-step.sh` (session scratchpad) takes `android/` from the Plus tip,
  sets the app ID `org.vita3kplus.emulator`, applies our bf32c3b6 when it
  applies, and renames `SCHED_DEADLINE` (a Linux macro) in `thread.cpp`.
- Plus commits 22 to 35 fail on Turnip with
  `createGraphicsPipeline: ErrorOutOfHostMemory`.
- The install gave `INSTALL_FAILED_TEST_ONLY` after a Plus build in the
  shared Gradle cache. `adb install -t` works.

## Comments
