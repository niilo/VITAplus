# 09: Make v-sync choose the Vulkan present mode

Status: open
Type: task
Label: ready-for-agent
Blocked by: 00, 01

## Problem

The `v-sync` setting does nothing on Vulkan. The present mode is chosen at
`vita3k/renderer/src/vulkan/screen_renderer.cpp:208-229` in the order
MAILBOX, FIFO_RELAXED, FIFO, with `eImmediate` as the initial value, and
`v-sync` is not read. `pending_vsync`
(`vita3k/renderer/include/renderer/state.h:135`) is read only by the OpenGL
backend (`renderer/src/gl/renderer.cpp:755`). Ticket 01 step 7 records which
modes this device actually offers.

This is the same work as `.scratch/pocket-s-optimization/issues/11-present-mode-vsync.md`,
claimed, with the code on `origin/pocket-s/11-present-mode-vsync` (`a52121af`)
built on the old base. Port it.

## Risk

`v-sync` defaults to `true`, so a change that keys the present mode on `v-sync`
changes the mode on every Android device that has not changed it. MAILBOX to
FIFO can lower the frame rate on a device where the emulated Vita runs above
the panel refresh rate, and it can raise it on a device where the compositor
was dropping frames. Neither direction is measured here.

`../spec.md` says not to change behavior on other devices unless it is measured
there too. So keep the existing order for every device, put the new selection
behind the `v-sync` value itself, and record in `## Answer` that a device whose
preset turns `v-sync` off now gets a different mode than it did on the stock
driver. Ticket 22 owns the preset.

## Steps

1. `git show a52121af` and port the change to this base. Read it first; the old
   base and the Plus base differ in `screen_renderer.cpp`.
2. One function chooses the mode, keyed on the `v-sync` value: v-sync on gives
   FIFO on Android, v-sync off gives MAILBOX if it is offered and FIFO
   otherwise. Other systems keep the existing order. Do not change the default
   value of `v-sync` here.
3. Read `pending_vsync` where `ScreenRenderer::ensure_swapchain()`
   (`vita3k/renderer/src/vulkan/screen_renderer.cpp:720`) decides whether to
   rebuild, and set `need_rebuild` when the value differs from the mode the
   current swapchain was created with. There is no `pending_vsync` read in the
   Vulkan backend today; only the OpenGL backend reads it
   (`renderer/src/gl/renderer.cpp:755`). `set_vsync_state` already exists at
   `vita3k/renderer/include/renderer/state.h:204` and is what sets it.
4. The image count comes from `minImageCount` in the surface capabilities
   (`screen_renderer.cpp:259`), not from the present mode, so changing the mode
   does not change the count by itself. The count still has to be right, because
   `vita_surface` is indexed per swapchain image
   (`renderer/src/vulkan/renderer.cpp:1423`). Resize `vita_surface` in
   `create_swapchain()` when the image count changes, so the index cannot pass
   the end. Note that `create_surface_image()` sizes it
   (`screen_renderer.cpp:708-709`) and `rebuild_swapchain_if_visible()` calls
   `create_swapchain()` (`screen_renderer.cpp:770`) without calling
   `create_surface_image()` again,
   so this is the place to fix that.
5. Log the mode and the image count on every swapchain creation.
6. Add a temporary setting `swapchain-extra-images`, default 1, so 0 gives
   `minImageCount`.

## Measurement

Do not start until ticket 04 has a baseline. Run A/B/A on the 60 FPS title and
the 30 FPS title:

- FIFO against MAILBOX.
- FIFO with `minImageCount` against `minImageCount + 1`.

Record the frame interval 99th percentile, the present count per second, GPU
busy percentage and GPU clock from ticket 03. Keep the combination with the
lowest 99th percentile as the v-sync-on default. Qualcomm's own advice for
Android is FIFO with `minImageCount = 3`, so start there.

## Acceptance

- Both targets build and the format check passes.
- Switching v-sync while a game runs changes the logged mode, with no crash.
- The A/B/A numbers are in `## Answer`.

## Answer

## Comments
