# 11: Make v-sync choose the Vulkan present mode

Status: superseded
Superseded by: .scratch/pocket-s-android13/issues/09-vsync-present-mode.md
Claimed: 2026-09-25 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent
Blocked by: 05
Measure after: 06

## Problem

- The `v-sync` setting does nothing on Vulkan (`../map.md`, Notes).
  Vulkan always takes MAILBOX when offered
  (`vulkan/screen_renderer.cpp:208-228`).
- Android offers only FIFO and MAILBOX. There is no IMMEDIATE.
- The swapchain has `minImageCount + 1` images (`:258`). Under FIFO this
  can add up to about 4 frames of latency.
- `feat/ayaneo-pocket-s-performance` (`1838cd88`) already has a
  `select_present_mode()` that reads `pending_vsync`. Start from it if
  ticket 02 did not bring it.

## Steps

1. Choose the mode in one function: v-sync on gives FIFO; v-sync off gives
   MAILBOX, else FIFO.
2. Read `pending_vsync` in `ensure_swapchain()` (`:696-708`). If the mode
   changes, set `need_rebuild`.
3. MAILBOX can give more images than FIFO. `vita_surface` is sized once
   (`create_surface_image`, `:684-685`) and indexed per swapchain image
   (`vulkan/renderer.cpp:1159`). Resize `vita_surface` when the swapchain
   is created again, so the index cannot go past the end.
4. Log the present mode and image count in `create_swapchain()`.
5. Add a temporary config value for the image count: `minImageCount` or
   `minImageCount + 1`.

## Acceptance

- Both targets build.
- On the device, switching v-sync while a game runs changes the logged
  mode, with no crash.
- A/B/A on the 60 FPS and the 30 FPS title: FIFO against MAILBOX, then
  FIFO with `minImageCount` against `minImageCount + 1`. Record the frame
  interval 99th percentile and presents per second (ticket 05). Keep the
  combination with the lowest 99th percentile as the v-sync-on default.

## Answer

Code done on branch `pocket-s/11-present-mode-vsync`, commit abf7c1de, rebased on
`master` after ticket 19 as a52121af. Linux and Android release builds
pass. `container/vita3k.sh format-check` passes.

- `select_present_mode()` runs in `create_swapchain()`. On Android, v-sync
  on gives FIFO. v-sync off gives MAILBOX, else FIFO.
- `ensure_swapchain()` sets `need_rebuild` when `pending_vsync` differs
  from the current mode.
- `vita_surface` is resized in `create_swapchain()` when the image count
  changes.
- The log line is `Present mode: <mode> (v-sync on|off), swapchain images:
  <n> (minimum <m>)`.
- The temporary setting is `swapchain-extra-images` (default 1). 0 gives
  `minImageCount`.
- Ticket 02 answer: the new rule is used only on Android. Other systems
  keep the old order (MAILBOX, then FIFO_RELAXED, then FIFO), and v-sync
  does not change it there.
- The rebase replaces the `select_present_mode()` of 1838cd88 (ticket 19).
  That version also changed desktop and took IMMEDIATE when v-sync was
  off.

Still to do: the v-sync switch test and the A/B/A runs on the device after
ticket 06.
Update 2026-10-03: this ticket is `superseded` by
`.scratch/pocket-s-android13/issues/09-vsync-present-mode.md`, which carries the
same work over to the Vita3K-Plus base. That ticket keeps
`select_present_mode()`, `swapchain-extra-images` and the log line, and adds
that under the energy retarget this is the largest whole-device lever in the
plan: a 30 FPS game on a 60 Hz panel presents every other vsync, so MAILBOX
wakes the GPU 60 times a second to use 30 of them.

## Comments

- 2026-10-03: closed as `superseded`. The line references in `## Problem`
  (`screen_renderer.cpp:208-228`, `:258`, `:696-708`, `:684-685`) and in
  `vulkan/renderer.cpp:1159` were against the old base. On this base the
  present-mode order is `vita3k/renderer/src/vulkan/screen_renderer.cpp:211-231`,
  the initial value `eImmediate` is at `:214`, the image count is
  `swapchain_size` at `:261`, `create_swapchain()` is at `:247`, `create_surface_image()` at `:713` and
  `ensure_swapchain()` at `:725`. `vita_surface` is indexed per swapchain image
  at `vita3k/renderer/src/vulkan/renderer.cpp:1424` (was `:1159`), and it is
  sized from `swapchain_size` at `screen_renderer.cpp:714`. So the resizing
  step 3 asks for belongs in `create_swapchain()`, next to `:261`.
- 2026-10-03: the android13 ticket records that the present mode list this
  ticket needs was never collected, so the A/B still needs a device run that
  logs `getSurfacePresentModesKHR`.
