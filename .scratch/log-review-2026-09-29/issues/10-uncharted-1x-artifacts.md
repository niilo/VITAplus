# 10: Uncharted shows artifacts at resolution multiplier 1

Status: open
Type: task
Label: needs-triage

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

## Comments
