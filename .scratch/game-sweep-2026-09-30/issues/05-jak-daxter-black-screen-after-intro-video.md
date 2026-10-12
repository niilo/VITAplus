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
5. If it is the shader or the blend, fix it in `vita3k/shader/`. If it is the
   texture, fix it in `vita3k/renderer/src/texture/`.

## Comments, 2026-10-10: step 4 done, the textures are not the cause

### The textures decode correctly

`export-textures: true`, one menu run on the Pocket S, Turnip, build
`v1.2.1-dev.23` (`e6bc600c`). Exports land in `textures/export/PCSA00080/`
as `<HASH>.png`, and `export-as-png` defaults to true. **278 textures, and not
one of them is black.** Measured over every pixel of every file:

| | value |
|---|---|
| fully black, max channel 0 | **0 of 278** |
| mean across all textures | 110.06 |
| median | 81.61 |
| darkest mean | 6.49 |
| textures with mean below 5 | 0 |

The sizes match what the third look saw: 65 files at 320x200, several 960x544
full-screen frames, 256x512, 272x528, 240x512, and a 196x36 UI strip.
`9D5A59991297D539.png` at 320x200 is the Jak and Daxter logo, fully decoded,
mean 234.34. So UBC and U8U8U8U8 decode to real images.

One oddity, not chased: one file is 961x545 where every other full-screen frame
is 960x544. A one-pixel-too-large dimension is a decode or stride question, but
it would show as a wrong image, not a black one.

### The renderer is not wedged

This contradicts the "wedged BEFORE scene render" reading of the watchdog. The
`perf-log` CSVs from the device hold PCSA00080 and say:

- 3174 scenes in 62.1 s, **51.1 scenes/s**, submitted continuously.
- 1863 frames, median interval **33.34 ms = 30.0 FPS**, p99 36.28 ms.
- 1895 presents.

The watchdog is a false positive for this title. Dead or Alive 5 (PCSE00235)
trips the same watchdog with 7 and 8 pipelines and renders correctly, and the
`only N pipelines` part is not a hang when a title reuses a few textured-quad
shaders. The menu uses exactly two fragment programs.

### 41 per cent of scenes draw nothing

`scenes.csv`, draws per scene: **0 draws in 1310 of 3174 scenes**, 9 in 1246,
2 in 551, and 28 or fewer in the rest. So there are stretches where the game
submits an empty scene, and stretches where it submits 9.

### The two menu shaders multiply three registers

From the device shader cache, `shaderlog/PCSA00080/eboot.bin/`, program
`86c963743c1a` is five instructions:

```
VPCKF32F32 i0.xyzw (pa4.xyzw pa6.xyzw)
VMUL       i0.xyzw pa0.xyzw i0.xyzw
VPCKF16F32 pa0.xyzw i0.xyzw
```

The output is `pa0 * pa4 * pa6`. For the whole screen to be exactly zero, one
of those three is zero everywhere: a uniform that is not uploaded, or a
sampled texel that is zero. The textures are not zero, so the uniforms or the
sampler path are.

### The screen is exactly zero, not dark

`tmp/black/g1..g4.png`, 2560x1440 each: max channel 0, mean 0.00, and 0.000
per cent of pixels above 8. Every pixel is `(0,0,0)`. A bad texture or a bad
blend would give wrong colours or near-black. Exactly zero means nothing
non-zero was written to the surface that was presented.

### Every present returns VK_SUBOPTIMAL

All 1895 presents in `presents.csv` have result `1000001003`, which is
`VK_SUBOPTIMAL`. None is `VK_SUCCESS`. The swapchain is 1280x720 while the
guest renders 960x544, so the present path is a `sampled draw (filter=Bilinear)`
blit.

`screen_renderer.cpp:650` handles `eSuboptimalKHR`, but rebuilds only when
`surface_matches_window_size()` is false, and the window does match the device.
So the result is swallowed on every present. Per the specification a
suboptimal swapchain still presents correctly, so this is not by itself the
fault. It is the one signal in the log nobody has followed, and a present blit
that samples the wrong image would black out the screen whatever the scene drew.

### Revised plan

1. ~~Step 4 above. Done: the textures are real.~~
2. Log the draw-time values of `pa0`, `pa4` and `pa6` for the two menu programs,
   and the colour target address. This says directly whether the inputs are
   zero. Same technique as the third look, and removed after.
3. Capture a frame with RenderDoc or AGI and read the last draw before the
   present, then the present blit.

## Addendum 2026-10-10: the fragment uniform buffer is never bound

### What was measured

Two temporary logs, both behind a `constexpr bool` and both removed with the
rest of this instrumentation, run on the device against commit `e6bc600c`:

1. `[FRAGUNIF]` in `vulkan::set_uniform_buffer`, printing the guest uniform
   address and the `pa` registers for every fragment-program uniform set.
2. `[FRAGSIZE]` in `renderer::create`, printing the per-program uniform buffer
   sizes and offsets. `[FRAGCALL]` in the three `sceGxm*Fragment*UniformBuffer`
   exports, printing the game's calls.

| run | draws logged | fragment uniform sets |
| --- | --- | --- |
| Jak menu, PCSA00080 | 48 | **0** |
| Uncharted, PCSA00029 | — | 12 |

The Uncharted control reads real values, for example
`pa2=(1,0,1,1) pa6=(-0.666667,0,0.707107,0.707107)`, so the log works. Jak
logs none at all in 80 seconds, across three render targets.

The game is not at fault for not supplying the buffers. It supplies them:
81 calls to `sceGxmReserveFragmentDefaultUniformBuffer` and 14 to
`sceGxmSetFragmentUniformBuffer` (indices 0 through 4, buffer `0x6061A420`).

### Why the emulator drops them

`[FRAGSIZE]` for the two menu programs:

```
prog=497ca1682200 buffer_count=0 sizes= [0..14]=0 offsets= [0..14]=4294967295 textures_used=0x0 texture_count=0
prog=86c963743c1a buffer_count=0 sizes= [0..14]=0 offsets= [0..14]=4294967295 textures_used=0x1 texture_count=1
```

`uniform_buffer_sizes` is empty for both, so `gxmSetUniformBuffers`
(`SceGxm.cpp:2465`) skips every entry and the fragment uniform descriptor is
never written. The recompiler is handed `buffer_count = 0`
(`spirv_recompiler.cpp:1222`), so the shader has no uniform buffer binding at
all and `pa0`, `pa2`, `pa4` and `pa6` read as zero. Every fragment writes zero.
The surface stays at its cleared value, which is what the screenshots show.

### The GXP headers say why

Read from `tmp/jak-menu-shaders/eboot.bin/*.gxp`:

| program | type | parameter_count | primary_reg_count | default_uniform_buffer_count | uniform_buffer_count | container_count |
| --- | --- | --- | --- | --- | --- | --- |
| `08d0a2b163c0` | vertex | 6 | 12 | 24 | 0 | 1 |
| `497ca1682200` | fragment | 0 | 4 | **0** | 0 | 0 |
| `86c963743c1a` | fragment | 1 | 8 | **0** | 0 | 0 |

The vertex program declares a 24-float default uniform buffer and the menu's
geometry is transformed correctly. Both fragment programs use `pa` registers
(`primary_reg_count` 4 and 8, and the disassembly reads `pa0`, `pa2`, `pa4`,
`pa6`) but declare no default uniform buffer, no named uniform buffer and no
container.

`get_program_input` (`gxp_parser.cpp:232`) only adds the index-14 default
buffer when `program.default_uniform_buffer_count != 0`. It is 0, there are no
parameters to recover it from, so `program_input.uniform_buffers` comes back
empty. That is the whole fault: a header field the hardware does not need,
which the analyzer does.

### Correction

An earlier draft of this addendum said shader `497ca1682200` had an empty draw
rect. That was a bug in my own log, which read `context.draw_rect_*` before the
block that computes it. The draws reach the screen with `rect=(0,0)-(1920,1088)`.
There is no empty-rect problem.

### What a fix has to decide

The size of the inferred default uniform buffer is not readable from these
headers. `default_uniform_buffer_count` is in float units, and the register
format decides the stride: the vertex program's 24 floats over 12 `pa`
registers is 2 floats per register, an f16 vec4, while the fragment programs
use `VPCKF16F32` and `VPCKF32F32` on the same registers. Guessing the stride
wrong means either truncating the colour the menu draws or uploading garbage
past the end of the game's buffer, so this needs a decision rather than a
guess. Two candidates:

1. Size it from `primary_reg_count` and the program's own output register
   format, matching what the vertex program reports.
2. Use `SCE_GXM_MAX_UB_IN_FLOAT_UNIT`, the same fallback already applied to
   stripped-symbol shaders at `gxp_parser.cpp:222`. Safe but uploads the whole
   window per draw; `SceGxm.cpp:2470-2478` bounds it to the memory mapped
   region.

## Addendum 2026-10-11: the fix is real but does not clear the screen

### The rule, and how it was checked

`primary_reg_count` is a safe bound on the `pa` registers a program reads. The
register bank the recompiler emits is a `uvec4` per register (`%12 = uvec4[32]`
in every generated module), so register `N` sits at byte `N * 16` and the bank
is `primary_reg_count * 4` floats.

Checked over the 272 GXP programs in the shaderlog of three games (PCSA00029,
PCSA00080, PCSE00052), comparing each header against its disassembly:

- 84 programs have `default_uniform_buffer_count == 0` with
  `primary_reg_count > 0`. For every one of them the highest `pa` register the
  disassembly reads is below `primary_reg_count`; in almost all of them it is
  exactly `primary_reg_count - 2`. Four of the 84 are empty shaders with no
  instructions at all.
- This is not a Jak quirk. The three games are 80 fragment and 12 vertex
  programs between them, all patched programs with a stripped symbol table.

### The change

`gxp_parser.cpp`, the `default_uniform_buffer_count != 0` test that gates the
synthesised index-14 buffer, now also fires on `primary_reg_count != 0`, sizing
the buffer from `primary_reg_count * 4` when the header declares nothing.

### It works, and it is not enough

The recompiled module for `497ca1682200` gains the `uvec2[1] buffer_addresses`
array it previously lacked, which is the array the shader needs to reach guest
memory at all. So the change reached the shader.

The screen does not move. Measured over the game area, `y > 120`, on three
screenshots 85 seconds into the menu:

| build | max | mean | pixels above 8 |
| --- | --- | --- | --- |
| before the change | 0 | 0.00 | 0.00% |
| after the change | 0 | 0.00 | 0.00% |

The only lit pixels are rows 26 to 86, which is the Android status bar, and it
accounts for the whole difference in the full-frame mean (0.40 before, 0.46
after). The game area is byte-for-byte zero either way.

### Where the path still stops

With the change in, `vulkan::set_uniform_buffer` is still never called for a
fragment program during a Jak menu session: 0 calls, against 24 across the
Uncharted sessions in the same log file. The log was checked by slicing at the
last `Vita3K session start` and confirming the session is PCSA00080.

`gxmDrawElementGeneral` takes the uniform buffers from
`pre_frag->uniform_buffers` when the draw uses a precomputed fragment state,
sized `pre_frag->buffer_count`, and falls back to
`context->state.fragment_uniform_buffers` otherwise. The skip in
`gxmSetUniformBuffers` is `!buffers[i] || sizes.at(i) == 0`. Sizes are now
non-zero, so the remaining candidate is `buffers[0]` being null, which is the
game's own default fragment uniform buffer never reaching the draw.

The next measurement is one line: at `SceGxm.cpp:2538`, print `pre_frag`,
`pre_frag->buffer_count`, and `frag_buffers[0]`, alongside
`sizes.at(0)`.

### State

The `gxp_parser.cpp` change is in the working tree, uncommitted. The device
carries an APK built from it. `backend_renderer` was set on the device during
this work and is not a valid config key, so nothing was written to config.yml.

## Addendum 2026-10-11 (evening): a second zero-size site, and a dead end

The previous addendum blamed the analyzer alone. That was incomplete. There is a
second site reading the same zero header field, and it is worse: it does not
merely under-report, it hands the game a null pointer and reports success.

### Site two: the reserve

`SceGxm.cpp:3872` sized the reservation from the header directly:

```cpp
const size_t size = (size_t)program->default_uniform_buffer_count * 4;
if (size == 0) {
    *uniformBuffer = Ptr<void>();          // null, and success is returned
    ...
    return 0;
}
```

For these programs `default_uniform_buffer_count == 0`, so the emulator gave the
game a null uniform buffer every frame and the game wrote its menu uniforms
through guest address 0. This is why the earlier run showed 81 calls to
`sceGxmReserveFragmentDefaultUniformBuffer` and no fragment uniform ever
reaching `vulkan::set_uniform_buffer`.

### How it was located

One log line at `SceGxm.cpp:2538`, at the ternary that picks the buffer source,
reporting `pre_frag`, its `buffer_count`, `frag_buffers[0]`,
`context->state.fragment_uniform_buffers[0]` and `sizes.at(0)`. One build, one
run, and it settled it:

```
[FRAGUBSRC] pre_frag=0x0 pre_buf_count=0 pre_ub=0x0 span_len=15 span0=0x0
            ctx_default=0x0 sizes0=16 sizes1=0 rb_count=1
```

`pre_frag` null rules out the precomputed-state hypothesis. `sizes0=16` shows
the analyzer fix had landed, so the surviving skip had to be `!buffers[0]`, and
both `span0` and `ctx_default` were null. Confirmed cause: the reserve.

### The change

Four sites now read the size from `renderer_data->uniform_buffer_sizes`
instead of the raw header, so the reservation and the draw cannot disagree:

- `sceGxmReserveFragmentDefaultUniformBuffer` and
  `sceGxmReserveVertexDefaultUniformBuffer` (the reservation itself)
- the two ring-buffer advance sites in `gxmDrawElementGeneral` and
  `sceGxmDrawPrecomputed`, which had to change in the same commit or the ring
  offsets would have desynced from the reservation

Verified working on device. Before, every reservation was `size=0` with a null
pointer. After:

```
[FRAGRES] size=64  ring_used=0   frag_uniform_buffers0=0x0
[FRAGRES] size=128 ring_used=64  frag_uniform_buffers0=0x70A20000
[FRAGRES] size=64  ring_used=192 frag_uniform_buffers0=0x70A20040
```

Real pointers, advancing through the 512 KiB ring.

### And the screen is still exactly zero

Game area (`y > 120`), three screenshots 85 s in:

| build | max | mean | pixels above 8 |
| --- | --- | --- | --- |
| baseline | 0 | 0.00 | 0.00% |
| analyzer fix | 0 | 0.00 | 0.00% |
| reserve fix | 0 | 0.00 | 0.00% |

Byte-for-byte zero. Rows 26 to 86 light up; that is the Android status bar and
it accounts for the entire full-frame mean difference.

### Where this leaves the ticket

Two real bugs, neither of which is the black screen. The uniform path is now
consistent end to end and bound to real guest memory, and the output is still
exactly zero. That is evidence against the uniform path being the cause at all.

The observation that should drive the next step: **neither menu fragment
program samples a texture.** Both are `count=4` quads that multiply uniform
registers:

```
497ca1682200  out = pa0 * pa2
86c963743c1a  out = pa0 * pa4 * pa6
```

Two flat quads cannot paint a game-selection menu with a video background and
logo artwork. The likely reading is that these two draws only clear or tint,
and that whatever paints the menu is a path not yet found. That would also
explain why the 278 correctly-decoded textures never appear on screen.

The remaining unexplained fact, unchanged: 6 distinct draw shapes in the entire
session, all `count=4`, alternating across three 960x544 targets, with
`rect=(0,0)-(1920,1088)` and `res_multiplier=2`.

### Next step, deliberately not uniform-tracing

Stop following uniforms. Establish what is supposed to paint the menu, because
everything so far has assumed the draws are the menu without evidence:

1. Capture the frame (RenderDoc or AGI) and read the last draw before present,
   then the present blit. This separates "nothing was drawn" from "drawn and
   lost in present", the fork everything else has been sliding past.
2. Failing a capture, dump guest memory at the present and check whether the
   pixels exist in the source surface before the blit.

### Also noted, not fixed

`sceGxmProgramGetDefaultUniformBufferSize` (`SceGxm.cpp:3628`) returns
`default_uniform_buffer_count * sizeof(Ptr<const void>)`. That multiplies a
float-unit count by pointer size, so on a 64-bit host it reports twice the
bytes. It is a guest-visible query and was left alone deliberately; it deserves
its own ticket if it turns out to matter.

### State at handoff

Both fixes are uncommitted on `fix/ngs-voice-released-rack-exports`, build
clean, ctest 3/3, format-check 0. Neither is verified against the symptom it
was written for, so neither should be landed as a black-screen fix. All
temporary instrumentation has been removed from the source and the clean build
re-verified afterwards.
