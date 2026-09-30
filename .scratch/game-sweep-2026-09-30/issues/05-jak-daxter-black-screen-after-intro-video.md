# 05: Jak and Daxter Collection: the start menu is drawn black (it reacts to input)

Status: open
Type: task
Label: ready-for-agent

## Symptom

Jak and Daxter Collection (PCSA00080) on the Ayaneo Pocket S (Adreno 740),
release APK with the fix of issue 01. The game no longer crashes, but the
screen stays black (the FPS counter shows 30) at 60 seconds after the start.
Found on 2026-09-30.

## Evidence

- The watchdog reports, at 12 seconds: "STUCK-SCENE WATCHDOG: presenting (358
  SetFrameBuf accepted) but only 2 pipeline(s) ever compiled, frozen for 720
  vblanks (~12s) - wedged BEFORE scene render".
- The main thread (`PCSA00080`, thread 8) is running at `pc 0x81189424`, the
  import stub for NID `0x8BD94593`.
- Threads `SndStreamThread` (78, 81), `avPlayer FileStreaming` (105) and
  `avPlayer VideoDec` (194) wait. `WAIT_BLOCK` and `WAIT_DONE` lines repeat
  with result `0x80028005` (timeout) for threads 193, 194 and 81.
- One error before the wait: `_sceKernelLockLwMutex returned
  SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID (0x80028181)`, 1 ms after
  "Video on Stream 1 - Width 960 Height 544".
- "Game is using unimplemented distortion audio module" (open issue
  `log-review-2026-09-29/06`).
- The log of this run: pull with `tools/android/device.sh log
  org.vita3k.emulator <dir>`. The session starts at the last "Game started"
  line.

## Next steps

1. Find out what NID `0x8BD94593` is (`vita3k/nids/include/nids/nids.inc`)
   and what the main thread waits for.
2. Check the `SceAvPlayer` HLE: the game uses its own player library
   (`simple_mp4_player`) on top of the avplayer imports. Look at which
   avplayer calls the game makes after the "READY" event and which one does
   not return the expected data (frame, state change).
3. Find the source of the invalid lw mutex ID.
4. Compare with Vita3K-Plus (`org.vita3kplus.emulator`).

## Comments

2026-09-30, second look:

- Vita3K-Plus 20588fbf has the crash of issue 01 too, so the black screen
  cannot be compared with Plus yet. Do not use Plus as a reference here.
- The screen is still black at 100 seconds. Pressing Start and A does not
  change it (both screenshots are identical, 25700 bytes).
- The main thread is not stuck. It runs the frame loop: `r0` in the hang dump
  is a frame counter (0x165 = 357, and 358 `SetFrameBuf` calls are accepted)
  and the loop calls `sceGxmWaitEvent` each frame. `sceGxmWaitEvent` is an
  empty stub (`vita3k/modules/SceGxm/SceGxm.cpp:6052`, it returns at once).
  It is a suspect: the real call may block until an event, and the game may
  depend on that wait.
- The game plays its intro video with the system module
  `vs0:sys/external/libscemp4.suprx` (loaded as real code, not HLE). The log
  shows one `receive_h264_frame` error (`AVERROR(EAGAIN)`, "Requires Another
  Call") at the start and no more video lines after `Changing from Buffering
  mode to previous state`. The `avPlayer VideoDec`, `AudioDec` and `Demux`
  threads then wait on conditions with timeouts. So the video may never
  deliver a frame.
- Only 2 pipelines compile. The game draws nearly nothing.

Next: (a) log the `sceVideodec*` calls and results for this game, (b) find out
what the game waits for in `sceGxmWaitEvent`, (c) check whether the video
frames arrive (`sceAvPlayer` is not used, so look at `SceMp4` and
`SceVideodec` in `vita3k/modules/`).

2026-09-30, third look (temporary debug logs in `sceAvcdecDecode` and
`sceGxmSetFragmentTexture`, since removed):

- The video is not the cause. `sceAvcdecDecode` works: 960x544, pixel type
  `0x20` (YUV420 packed), a frame at every call, about 30 per second, and a
  mean luma of 47 to 54 (dark, not empty). The clip loops (the media is set
  to loop), so this is a looping menu background and not an intro that ends.
- The game does draw. `sceGxmSetFragmentTexture` shows full screen 960x544
  textures in the formats UBC1, UBC2 and U8U8U8U8, and UI textures (280x200
  and 196x36, UBC3) that change every frame. So the game logic runs and
  issues draws. The screen is black anyway.
- So the fault is in rendering or in presentation, not in the video or in the
  game logic. The "only 2 pipelines compiled" line is not a sign of a hang:
  all draws probably share two simple textured-quad shaders
  (varying masks `0x0003` and `0x0013`).
- These settings were tried one at a time (each 40 to 100 seconds on the
  device) and the screen stayed black in all of them: `memory-mapping`
  double-buffer, `resolution-multiplier` 1, `disable-surface-sync`,
  `high-accuracy`, `force-full-precision`, `disable-raster-order`,
  `disable-programmable-blending`, `surface-sync-clamp-rt` off and
  `shader-cache` off. `memory-mapping: native-buffer` gave a different
  screenshot size (24202 bytes) and was not checked further. All values are
  back at the defaults.
- One `transfer_copy` was logged: surface-synced `0x607299B0` to
  `0x601FE000`, 960x544, format `0x60000`.

Next: capture a frame with RenderDoc or AGI on the device and look at the
last draw before the present. Check if the draws write to the color target,
and if the presented image is the same surface that the game rendered to.
Also check the UBC texture decode for these textures (the texture cache and
`vita3k/renderer/src/texture/`). Try the Turnip driver versus the stock
driver (the box bug in issue 08 depends on the driver).

2026-09-30, test by the user on the Pocket S with the built-in controller
(release APK with the fixes up to `901695f7`). This changes the picture:

**The black screen is the start menu of the collection. It is not a hang.**

- Log, first session: `Jak Collection main...`, `Starting main loop...`,
  `MBB initVideo load`, then the video player starts (looping menu background,
  already seen in the third look above) and `StartMenuMusic()` plays the menu
  music. The menu runs and waits for input.
- The user pressed X. The menu took it: 39 seconds later the app loads
  `app0:Jak1.self` (`Main executable Jak1_retail_psp2 (Jak1.self) loaded`) and
  "Game started" appears a second time in the same log. So the collection
  starts Jak 1 by loading a second executable inside the same app.
- So the menu logic, the input and the audio work. Only the drawing of the
  menu is black: the looping video background and the menu items are not
  visible. This matches the third look: the game draws textures (UBC and
  U8U8U8U8, 960x544 and UI sizes), and the screen stays black.
- My earlier tests pressed Start and A through adb, which did nothing. That
  did not mean that the game hung. The menu wants X.
- Automation: in the config, cross is the keyboard key `KeyX` and start is
  `Enter`. `adb shell input keyevent KEYCODE_X` did not start Jak 1 in one try
  (45 seconds after the launch, then 25 seconds wait). `KEYCODE_BUTTON_A`,
  `_X`, `_B`, `_START`, `KEYCODE_DPAD_CENTER` and `KEYCODE_ENTER`, also with
  `input gamepad keyevent`, did not start it either. A way to send a cross
  press to the app is needed for device tests. Try a longer key press
  (`input keyevent --longpress` or `input swipe` as a touch), the overlay
  buttons of the app (touch at their screen position), or a key binding
  that adb can reach.

Because the menu and Jak 1 both draw with the same renderer, issue 07 covers
the in-game picture. The two issues may have one cause.

Revised plan for this issue:

1. Get a reliable input route for device tests (see above).
2. Capture a frame of the menu (RenderDoc or AGI) and look at the last draws:
   target, shader, blend state, textures, and their output.
3. Set `log-active-shaders: true` and read the fragment shaders of the menu
   draws (the folder `shaderlog` on the device). Find out if their output is
   zero.
4. Compare with the texture export (`export-textures: true`) to see if the
   menu textures decode to real images.
5. If it is the shader or the blend, fix it in `vita3k/shader/`. If it is the
   texture, fix it in `vita3k/renderer/src/texture/`.

