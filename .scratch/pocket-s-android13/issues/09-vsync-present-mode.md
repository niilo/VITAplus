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

## Why this matters more after the retarget to energy

A 30 FPS game on a 60 Hz panel presents every other vsync. MAILBOX lets the
renderer run free and discard frames; FIFO ties each present to a vsync, so the
GPU can idle between presents instead of being woken and then throwing the work
away. Waking a GPU for 60 frames a second to use 30 of them is energy spent for
nothing.

That is a whole-device effect, not only the emulator's, which makes it larger
than anything in the GPU-side tickets. Combine it with
`ANativeWindow_setFrameRate` from ticket 28: telling SurfaceFlinger that the app
produces 30 FPS can move the panel and the system scheduling as well as the
renderer.

Steadiness is now a gate rather than a score, so the late-frame fraction from
criterion 2 of `../spec.md` belongs in the record next to the power figure.


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

## The present mode list is not collected yet

Ticket 01 step 7 asked for one `LOG_INFO` in
`vita3k/renderer/src/vulkan/screen_renderer.cpp` that prints the whole
`getSurfacePresentModesKHR` result, and its answer does not report it. So the
list this ticket needs is not on record. The only mode on record is
`Present mode: Mailbox` under Turnip, from tickets 00 and 01. The stock driver's
is not recorded.

Add that one log line as step 0, behind the `perf-log` setting from ticket 02,
and read the list from `vita3k.log` before the A/B. It is one line of code and
no run is wasted on it.

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

The port from `a52121af` is done, adapted to this base. `v-sync` now chooses the
Vulkan present mode, and the present mode list is logged.

### What this base looked like before

The old code was inline in `setup()`, not in a function, and it started from
`eImmediate` and walked the list:

```cpp
present_mode = vk::PresentModeKHR::eImmediate;
for (const auto &mode : present_modes) {
    if (mode == eMailbox) { present_mode = mode; break; }
    if (mode == eFifoRelaxed) { present_mode = mode; }
    ...
}
```

`v-sync` was read by nothing in the Vulkan backend. Confirmed: before this
change, `grep pending_vsync vita3k/renderer/src/vulkan/` returned nothing.

### What changed

`select_present_mode()` is a new function at `screen_renderer.cpp:226`, called
from `setup()` where the old inline loop was. On Android, `v-sync` on gives
FIFO and `v-sync` off gives MAILBOX when the surface offers it. Other systems
keep the order this base always used, MAILBOX then FIFO_RELAXED then FIFO, and
`v-sync` does not change it. FIFO is the fallback everywhere because Vulkan
requires it.

The other points the ticket lists:

- **A `v-sync` change while a game runs rebuilds the swapchain**
  (`screen_renderer.cpp:763-772`). `ensure_swapchain()` compares the pending
  value against the mode the swapchain was created with and sets
  `need_rebuild` with the reason `"v-sync changed"`. `pending_vsync` is read
  with a plain load there, not an exchange, so the value survives for
  `select_present_mode()` to consume. The exchange in `select_present_mode()`
  (`screen_renderer.cpp:232`) is what clears it.
- **`vita_surface` is resized** when the swapchain comes back with a different
  image count (`screen_renderer.cpp:338-345`). This is the bug the ticket calls
  out: `rebuild_swapchain_if_visible()` calls `create_swapchain()` and never
  calls `create_surface_image()`, and `vita_surface` is indexed per swapchain
  image.
- **The mode and the image count are logged** on every swapchain creation
  (`screen_renderer.cpp:335-336`).
- **`swapchain-extra-images`,** default 1, so 0 gives `minImageCount`. It is
  clamped to 0..3 at `renderer.cpp:1042`.
- **The present mode list is logged** under `perf-log`
  (`screen_renderer.cpp:257-265`). This is the step 0 the ticket asked for, and
  it is behind the setting so it costs nothing when it is off.

The default value of `v-sync` is unchanged.

### A note on other devices, as the ticket's risk section asks

A device whose preset turns `v-sync` **off** now gets MAILBOX on Android where it
previously got whatever the walk picked, which was also MAILBOX on most drivers,
so the common case does not change. A device that turns `v-sync` **on** now gets
FIFO where it previously got MAILBOX. That is the intended change and it is not
measured on any device other than this one. Ticket 22 owns the preset and should
know that flipping `v-sync` now also flips the present mode.

### Verified on the device

Built, installed and run with Uncharted. From `vita3k.log`:

```
[select_present_mode]: Present modes the surface offers: Mailbox, Fifo
[create_swapchain]: Present mode: Fifo (v-sync on), swapchain images: 4 (minimum 3, plus 1)
[vblank_sync_thread]: Vblank period: 16666 us
```

Then with `v-sync: false` in `config.yml`, same build, same title:

```
[create_swapchain]: Present mode: Mailbox (v-sync off), swapchain images: 5 (minimum 3, plus 1)
```

Three facts the ticket wanted and did not have:

- **The present mode list is `Mailbox, Fifo`.** Nothing else. No
  `FifoRelaxed`, no `Immediate`. So the old walk always ended at MAILBOX on this
  device, and the new default of FIFO with `v-sync` on is a real change.
- **`minImageCount` is 3**, so the default `swapchain-extra-images: 1` gives 4
  images, and the setting at 0 would give 3.
- **The mode and the image count are logged on every swapchain creation**, so an
  A/B does not need the config file to know what a run used.

**The image count also moved from 4 to 5 with MAILBOX**, which the ticket did not
predict. `minImageCount + 1` was asked for and the driver returned 5 when the
mode was Mailbox, so `vita_surface` had to be resized. That is exactly the case
`screen_renderer.cpp:338-345` handles, and it is why that resize is in the port
rather than being an optional tidy-up.

### A second bug the port did not fix on its own

The first device run with `v-sync: false` still logged `Present mode: Fifo
(v-sync on)`. The port moves the mode selection into `select_present_mode()` and
reads `pending_vsync` there, but **nothing in the Vulkan backend ever seeded
`vsync` from `config.v_sync` at startup**, so the member kept its default of
`true` and only `set_vsync_state()` could change it. The setting was inert from
launch, which is the bug the ticket set out to fix.

Fixed at `renderer.cpp:1043-1046`, next to where `extra_images` is set:

```cpp
screen_renderer.vsync = config.v_sync;
```

After that the same run logged `Mailbox (v-sync off)` and the default run logged
`Fifo (v-sync on)`. `v-sync` was left at `true` afterwards.

### What was not done

**No A/B/A.** The ticket says not to start until ticket 04 has a baseline, and
it does not. So the three measurements it lists are not here:

- FIFO against MAILBOX.
- `minImageCount` against `minImageCount + 1`.
- The frame interval 99th percentile, present count, GPU busy and GPU clock.

Acceptance item 3, "the A/B/A numbers are in `## Answer`", is **not met.**

### Acceptance, honestly

- Both targets build and the format check passes: yes.
- The mode follows `v-sync`: **yes, from `config.yml` on a fresh launch, both
  directions, verified above.** The live switch through the settings UI while a
  game runs was not exercised. That path (`ensure_swapchain()` setting
  `need_rebuild` when `pending_vsync` disagrees with the current mode) is in
  place but unverified.
- A/B/A numbers: not done.

## Comments
