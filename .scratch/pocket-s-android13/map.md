# Map: Android 13 performance for the Ayaneo Pocket S

The spec is `spec.md`. The tickets are in `issues/`. This map records the
facts the tickets were built from, so a reader does not have to re-derive
them.

## Ticket fields

Field meanings come from `docs/agents/issue-tracker.md` and
`docs/agents/triage-labels.md`. This table adds only what this plan needs.

| Field | Values |
|---|---|
| `Status:` | `open`, `claimed`, `resolved`, `rejected` |
| `Type:` | `research`, `grilling`, `task`, `experiment` |
| `Label:` | `ready-for-agent` means the code and tooling work is fully specified and needs no device. A ticket that runs the measurement protocol from `../spec.md` is `ready-for-human`, because it needs the unlocked device, the four benchmark titles from ticket 04, and a person to hold touches. |
| `Blocked by:` | ticket numbers, or `none`. Work may start when all are `resolved` or `rejected`. |

There is no `Measure after:` field here. A ticket that needs a baseline is
`Blocked by: 04`. A ticket that can be coded first and measured later says so
in its body and lists only the code dependencies in `Blocked by:`. Two rules
follow from that:

- An agent scanning the frontier skips tickets labelled `ready-for-human`.
- A ticket whose `## Acceptance` names A/B/A numbers stays `open` until a human
  supplies them, whatever its `Label:` says. Code merged with no numbers is not
  a resolved ticket, and ticket 22 must not take an unmeasured value from it.

## Facts from the code

Every line was read on `master` at `d9037c16`. Where a line number was off by
a few lines when this map was written, the sentence names the symbol instead.

`external/dynarmic` is not checked out in this tree. Any statement about
dynarmic internals below is either read from `vita3k/cpu/`, or it was read from
an upstream copy of dynarmic during research and is marked as unverified here.
Run `git submodule update --init --recursive` before reading the submodule.

### The frame path

The guest calls `sceGxmDisplayQueueAddEntry`
(`vita3k/modules/SceGxm/SceGxm.cpp:2400`). It pushes a display callback to the
display queue and sends a `NewFrame` command to the renderer. `should_display`
is set on the display-queue thread in `vita3k/renderer/src/sync.cpp`. The host
thread that runs the renderer is started in
`vita3k/renderer/src/batch.cpp:328`. Its loop is `render_loop`
(`batch.cpp:227`): process command batches, `render_frame`, `swap_window`.

Per scene the renderer records into two command buffers, `prerender_cmd` and
`render_cmd`, and does one `vkQueueSubmit` with both
(`vita3k/renderer/src/vulkan/context.cpp:639`). Then the present path does one
more submit and one `vkQueuePresentKHR`
(`renderer/src/vulkan/screen_renderer.cpp:574` and `:585`).

There is one queue. `select_queues` (`renderer.cpp:295`) picks a single family.
`transfer_queue` and `transfer_command_pool` are created (`renderer.cpp:939`,
`:954`) and never used for a submit.

Sync is fences and binary semaphores only. There is no timeline semaphore, no
`VK_KHR_synchronization2`, and no `VK_KHR_present_wait`.

The frame ring is 3 deep (`MAX_FRAMES_RENDERING`,
`vita3k/renderer/include/renderer/vulkan/types.h:34`). At the top of each frame
the renderer thread blocks on a condvar until the wait thread reports frame
`timestamp - 3` finished (`context.cpp:763`).

### What a frame costs on a tile-based GPU

Adreno is a tile renderer. It picks between GMEM (binning) and direct
rendering per render pass, and the choice is not visible to the app.
Qualcomm lists what pushes a pass out of GMEM: many texture samples in the
vertex stage, few vertices or draws, and any tessellation or geometry shader.
A Vita3K frame is many small passes with few vertices. That is the case the
driver sends to direct rendering.

Five code paths make the GPU or the CPU do more work than it needs to. Each has
its own ticket.

1. **Render pass layout.** The color attachment uses
   `vk::ImageLayout::eGeneral` as the initial layout for every non-transient
   target and `eGeneral` as the final layout for every target
   (`pipeline_cache.cpp:626-632`). The depth and stencil attachment already
   uses narrow layouts: `eDepthStencilReadOnlyOptimal` when either aspect is
   loaded, `eUndefined` otherwise (`pipeline_cache.cpp:653-654`). Qualcomm's
   guidance says layout specificity matters more on Adreno than on most other
   GPUs.
2. **Shader interlock restarts the render pass per draw.** With
   `high-accuracy` on and `VK_EXT_fragment_shader_interlock` present, every
   draw that changes framebuffer-fetch state does `vkCmdEndRenderPass` then
   `vkCmdBeginRenderPass` (`scene.cpp:463` and `:473`). **Neither driver on this
   device reaches this path.** The stock driver reports
   `support_shader_interlock=false`, and Turnip has rasterization order access,
   which forces `support_shader_interlock = false` at `renderer.cpp:863`. So
   this is documented behaviour that does not run here, and `high-accuracy` can
   only turn texture viewport off. Ticket 05 measured it at 6.39 against 5.76
   FPS on stock, inside the noise.
3. **Macroblock sync restarts the render pass per macroblock.** A render
   target with `SCE_GXM_RENDER_TARGET_MACROTILE_SYNC` restarts the pass on
   every macroblock change (`context.cpp:699-736`) and, in the fallback path,
   asks for a render pass that loads and stores depth and stencil every time
   (`context.cpp:709`).
4. **Double Buffer copies guest memory into the GPU buffer on every draw
   (stock only).** In `double-buffer` mode, `BufferTrapping::access_buffer`
   (`renderer.cpp:2396`) is called per vertex stream (`scene.cpp:310`), per
   index buffer (`scene.cpp:615`) and per uniform block (`scene.cpp:63`). For a
   range under 12 KiB it does a host `memcpy` from guest memory into the
   GPU-visible buffer on the draw path (`renderer.cpp:2415`). For a larger range
   it `mprotect`s the guest range read-only (`renderer.cpp:2468`) and `memcpy`s
   guest memory into the trapped buffer on the way in (`renderer.cpp:2474`).
   There is no write-back copy in `BufferTrapping`. The dirty flag only
   decides whether the inbound copy happens.
5. **End of scene write-back (stock only; the copy into the GPU buffer is
   double-buffer only).** `can_mprotect_mapped_memory` defaults to `true`
   (`surface_cache.h:323`) and is reassigned only on non-Android Linux
   (`renderer.cpp:1134`), so on Android it stays `true` and every surface takes
   the `protect_surface()` branch (`surface_cache.cpp:817-821`).
   `need_surface_sync` is then set for small writeback surfaces only
   (`surface_cache.cpp:819-820`). Separately, `state.disable_surface_sync`
   gates whether `perform_surface_sync()` runs at all
   (`renderer/src/vulkan/context.cpp:610`). When it runs it ends the scene with
   a `vkCmdCopyImageToBuffer` (`surface_cache.cpp:2608`) and pushes a
   `BufferSyncRequest` whose handler `memcpy`s the result back into guest
   memory on the wait thread (`context.cpp:137`).

### Stock Adreno downgrades the mapping mode (off the measured path)

Everything in this subsection describes the stock driver. The plan measures on
Turnip, which runs Page Table, so none of it is on the path being optimised.

`renderer.cpp:1112` logs and runs Page Table and Native Buffer as Double Buffer
when the driver is the stock Adreno one, because the driver crashes on the
host-visible device-local mapping those modes need. The saved config value is
left alone. So on the stock driver the user cannot leave Double Buffer, and
item 4 above applies to every frame.

### Settings the Plus base changed, with no measurement on this device

| Setting | Default here | Default on `pre-plus-master` | Where it is read |
|---|---|---|---|
| `high-accuracy` | `true` | `false` | `renderer.cpp:1058` |
| `memory-mapping` | `page-table` on Android | `double-buffer` | `renderer.cpp:1092` |
| `disable-surface-sync` | `false` | `true` | `app_init.cpp:646` |
| `guest-cores` | `3` | not present | `interface.cpp:448` |
| `accurate-thread-scheduling` | `true` | not present | `interface.cpp:441` |
| `async-pipeline-compilation` | `false` | `true` | `app_init.cpp:651` |
| `log-level` | `2` | `0` | `interface.cpp:465` |

Two of the old defaults are corroborated inside this repository, by
`.scratch/pocket-s-optimization/map.md`, which was written against
`pre-plus-master`: `memory-mapping: double-buffer` and
`async-pipeline-compilation: true`. The other five are not corroborated
anywhere in the tree. Ticket 01 confirms each one with `git show`.

`EmulatorConfig.kt` and `config.h` disagree on `log-level`: the Kotlin default
is `0` (`EmulatorConfig.kt:95`) and the native default is `2`
(`config.h:197`). `native_config.cpp` fills the field from `current_config`, so
the native value is what reaches the app.

Plus states in its README that its changes were built for higher-end devices.

### Settings that are declared but not read

- `hashless-texture-cache` is not read from `Config`. `renderer.cpp:1141`
  passes a hardcoded `true` to `texture_cache.init`.
- `texture-cache` has one behavioural read in the tree, and it is in the
  OpenGL backend (`renderer/src/gl/sync_state.cpp:418`).
- `cpu-pool-size` has no read site in `vita3k/`.
- `v-sync` does not reach the Vulkan present mode. `pending_vsync`
  (`vita3k/renderer/include/renderer/state.h:135`) is read only by the OpenGL
  backend (`renderer/src/gl/renderer.cpp:756`). The present mode is chosen at
  `screen_renderer.cpp:208-229` in the order MAILBOX, FIFO_RELAXED, FIFO, with
  `eImmediate` as the initial value.
- `guest-cores`, `cpu-pool-size` and `hashless-texture-cache` have no field in
  `EmulatorConfig.kt`, so they cannot be set from the Android settings screen.

### Frame pacing

`vita3k/display/src/display.cpp:411-413` sleeps to the next multiple of
`TARGET_MICRO_PER_FRAME`, which is `1000000 / 60` microseconds
(`display.cpp:55`), computed from `std::chrono::system_clock`. That clock is not
monotonic. The period is 16666 us, while the Vita runs at about 59.94 Hz
(16683 us).

The vblank thread is separate from the present. Nothing ties the tick to the
host display.

The swapchain image count comes from `minImageCount` in the surface
capabilities (`screen_renderer.cpp:259-261`), not from the present mode. So
changing the present mode does not change the image count by itself.

### Host allocation

VMA is used with `eExternallySynchronized` (`renderer.cpp:969`). Page table
mappings set `eDedicatedMemory`, from `page_table_dedicated_device_memory`
(`renderer.cpp:1623`, applied at `:1866`). `VK_EXT_global_priority` is set to
high on every queue (`renderer.cpp:836`). `VK_EXT_memory_priority` is absent.
Vulkan objects are created on the renderer thread, except when
`async-pipeline-compilation` is on, in which case `createGraphicsPipeline` runs
on a compile worker thread (`pipeline_cache.cpp:1546`, `:1219`).

### CPU side

The emulator builds the `Dynarmic::A32` frontend and backend
(`vita3k/cpu/src/dynarmic_cpu.cpp:642`, `:667`) for ARMv7 guest code. The
arm64 dynarmic backend is not compiled. `config.arch_version` is
`ArchVersion::v7` (`:644`).

Dynarmic uses `all_safe_optimizations` when `cpu-opt` is on
(`dynarmic_cpu.cpp:664`) and one shared global monitor sized
`MAX_CORE_COUNT` = 150 (`vita3k/cpu/include/cpu/common.h:38`, assigned at
`dynarmic_cpu.cpp:656`). The emulator sets no code cache size, so the
`Dynarmic::A32` default applies. That default is in the submodule and has not
been read in this tree.

Guest memory is one large `PROT_NONE` reservation
(`vita3k/mem/src/mem.cpp:97-106`) with no huge pages requested anywhere. There
is no `madvise(MADV_HUGEPAGE)`, no `setpriority`, and no `sched_setaffinity` in
the tree.

Guest threads are detached `SDL_CreateThread` calls
(`vita3k/kernel/src/kernel.cpp:192`), one per guest thread, with no host
priority and no host affinity.

## Facts from research

Each line names its source class. A line marked "vendor" is a Qualcomm or
AYANEO claim. A line marked "proxy" was measured on a different part.

### Snapdragon CPU

- The cores report ARMv9.0-A. LSE2, SB, BTI and PAuth are mandatory in that
  version. `dc ZVA` is mandatory from Armv8.2 and i8mm from Armv8.6. SSBS is
  optional in every Arm version. FP16 arithmetic, DotProd and FlagM2 are
  optional. MTE is optional. SVE is not expected. Source: the Arm A-profile
  feature table, mandatory and optional columns.
- No host ARM instruction saves clock cycles in JIT output, because the guest
  is ARMv7 and the host instruction set never appears in the emitted code. The
  host features can only help the emulator's own C++ code, which is shader
  translation, texture decode and format conversion.
- The one shared `Dynarmic::ExclusiveMonitor` is the cost that matters for
  guest exclusive access. Whether any dynarmic optimization flag removes it is
  ticket 25.
- Android 13 THP is `madvise` mode in the GKI defconfig, but this device is
  `always`, so anonymous mappings already get 2 MB pages. Ticket 01 measured
  it. An app may still call `madvise(MADV_HUGEPAGE)`.
- `MADV_COLLAPSE` needs Linux 6.1. This device runs 5.15, so the call returns
  `EINVAL`. Ticket 01 measured the kernel version.
- `setpriority` on the calling thread passes the capability check, but the
  kernel allows a lower nice value only with `CAP_SYS_NICE` or a nonzero
  `RLIMIT_NICE` soft limit, and an app process has neither. So an app cannot
  raise its own priority by lowering the nice value. `pthread_setschedparam`
  with `SCHED_FIFO` returns `EPERM`. `sched_setaffinity` can only narrow the
  current mask. Source: `is_nice_reduction()` in `kernel/sched/core.c`, and
  the RLIMIT_NICE section of `getrlimit(2)`. Ticket 11 measured the refusal.
- Android's own guidance says an app should not set CPU affinity, because
  devices often ignore it. ADPF exists so the system picks the core type.
  Source: `source.android.com/docs/core/perf/performance-hint-api`.
- `Process.setThreadPriority` only sets a nice value. It does not change the
  cgroup. A native `std::thread` does not inherit the creator's nice value.
- ADPF API levels, from the official reference: `PerformanceHintManager` and
  `Session.createHintSession` are API 31. `reportActualWorkDuration(long)` and
  `updateTargetWorkDuration` are API 31. `Session.setThreads` is API 34 and is
  not available. The NDK `APerformanceHint_*` functions are API 33.
  `setPreferPowerEfficiency` is API 35.

### Measurement on Android 13, unprivileged

- simpleperf needs `<profileable android:shell="true" />` in the manifest for a
  release build. With it, the on-device `simpleperf` runs inside the app's own
  uid and needs no root.
- The unstripped `libVita3K.so` is under
  `android/app/build/intermediates/cxx/<build>/<hash>/obj/arm64-v8a/`.
  `binary_cache_builder.py` plus `report.py` symbolise a pulled `perf.data`.
- `dumpsys SurfaceFlinger --latency <layer>` works from adb and gives desired,
  actual and ready timestamps for 128 frames.
- `dumpsys gfxinfo` reports nothing for a native Vulkan surface, because HWUI
  never sees those frames. Do not use it.
- `dumpsys gpu --gpustats` gives global frame and GPU workload statistics.
- `dumpsys thermalservice` gives the current thermal status. `cmd
  thermalservice override-status` and `reset` work on Android 13. There is no
  `get-current-status` and no `headroom` subcommand.
- KGSL files under `/sys/class/kgsl/kgsl-3d0/` readable without root are
  `gpuclk`, `gpu_busy_percentage`, `gpubusy`, `max_gpuclk`, `throttling` and
  `idle_timer`. `gpuclk_khz` and `busclk_khz` do not exist here, and `gpuclk`
  is in Hz. Ticket 01 measured this.
- `tools/android/device.sh` already has a `thermal` command
  (`device.sh:212-219`) that samples `/sys/class/kgsl/kgsl-3d0/gpuclk` and
  `/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq`. Ticket 03 extends
  it rather than adding a second command, because one of the two sets of paths
  is wrong and only one command should exist.
- Perfetto from adb: `sched`, `freq`, `gfx`, `view`, `hal`, `sync`, `idle`,
  `power`, `thermal`, `membus` are atrace categories. There is no `vulkan` or
  `egl` atrace category. `power/gpu_frequency` and `gpu_mem/gpu_mem_total` are
  ftrace events.
- `ANativeWindow_setFrameRate` is API 30 and is available.
  `Choreographer.postFrameCallback64` is not in the public SDK.
  `Surface.setProducerThrottlingEnabled` is API 37. `MADV_COLLAPSE` is not in
  the Android 13 kernel.

### Adreno and Turnip

Sources: the Qualcomm mobile best practices document (vendor), the Mesa
Freedreno documentation, and the Mesa `freedreno` source.

- Vendor: qualify every change with `LOAD_OP_CLEAR` and `LOAD_OP_DONT_CARE`
  early, keep layouts exact, merge passes with the same color format, drop
  unused depth and stencil. Vendor: subpass merging is worth more than 10% of
  frame time when the conditions hold. The conditions need more than one
  subpass, an input target, each resolve in exactly one subpass, `srcAccessMask`
  without `SHADER_WRITE` and `dstAccessMask` without `SHADER_READ`. The code
  has one subpass, so the merge does not apply today.
- Vendor: UBWC, the hardware bandwidth compression, is disabled by compute
  shaders, linear tiling, host readback of a render target, aliased images,
  `VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT` and sparse residency. The color surface
  request includes `eSampled` and `eInputAttachment` on the same image
  (`surface_cache.cpp:770`), and the F16 path sets the mutable format flag
  (`surface_cache.cpp:751-757`). Ticket 27 owns this.
- Vendor: early-Z and LRZ are disabled by blending, stencil, masked writes,
  discard, a depth compare change, reading the framebuffer, and a framebuffer
  fetch. Adreno rejects occluded pixels at up to 4 times the drawn fill rate.
- Proxy: fp16 in a fragment shader runs at twice the rate on Adreno 640 and 730.
  No A32 measurement exists. Turnip lowers mediump fragment varyings to 16-bit
  on gen6 and up, and its own source says `RelaxedPrecision` still matters in
  the Vulkan path.
- Vendor: keep the sum of uniform buffers used by one shader under 7372 bytes,
  and use static indices. Vendor: using more than 16 unique uniform buffers,
  textures or samplers in one pipeline costs fill rate on A7xx. Ticket 27 owns
  both.
- Vendor: the Adreno shader instruction cache on A7xx is 127 instructions, and
  a performance drop appears at every multiple of 2000 shader instructions on
  the graphics queue.
- Vendor: Qualcomm's own swapchain advice for Android is
  `VK_PRESENT_MODE_FIFO_KHR` with `minImageCount = 3`.
- Vendor and Google: a compositor that has to rotate or read back the
  framebuffer costs 1 to 3 ms per frame and raises GPU frequency about 40%.
  Verify that the swapchain transform is identity.
- Turnip: `VK_EXT_external_memory_host` is absent. `VK_KHR_present_wait`,
  `VK_KHR_dynamic_rendering_local_read`,
  `VK_EXT_pipeline_creation_cache_control` and
  `VK_EXT_pipeline_creation_feedback` are present. `VK_KHR_pipeline_binary` and
  `VK_EXT_memory_priority` are absent. `minUniformBufferOffsetAlignment` is 64
  and `maxUniformBufferRange` is 64 KiB. Advertised descriptor limits are
  16777216 per stage, which hides the hardware limits.
- Stock: ticket 01 recorded the full list from the app log. It has
  `VK_EXT_pipeline_creation_cache_control`, `VK_EXT_pipeline_creation_feedback`,
  `VK_QCOM_image_processing`, `VK_QCOM_render_pass_transform`,
  `VK_QCOM_tile_properties`, `VK_KHR_timeline_semaphore` and
  `VK_KHR_synchronization2`. So pipeline cache control is available on both
  drivers, and only UBWC is a Qualcomm-only feature. Ticket 27's UBWC half
  therefore has no answer on the measured driver.
- Turnip: `TU_DEBUG` and `TU_AUTOTUNE_*` are read through `os_get_option()`,
  which on Android checks the system property `debug.mesa.tu.debug` before the
  environment variable. The environment variable is used when the property is
  empty.
- Turnip: concurrent binning is off by default. `TU_DEBUG=forcecb` or driconf
  `tu_allow_concurrent_binning=true` turns it on.
- Turnip: `MESA_SHADER_CACHE_DIR` points at the Mesa disk cache, which
  Balemuni's build raises from 1 GB to 4 GB to cut compile stutter.

### What other projects already did

- Upstream Vita3K PR 4141 works around an Adreno shader compiler crash: the
  driver SIGSEGVs in `libllvm-qgl` when a vertex shader partially writes a
  constant-indexed element of a `Private` `vec4` array of 19 or more elements.
  `REG_O_COUNT` is 20 in `vita3k/shader/src/spirv_recompiler.cpp:64`. The PR
  is closed and not merged. Ticket 16.
- Upstream PR 4173 reports that a texture spanning four or more host pages skips
  hashing, and only fully interior pages are write-protected, so a guest write
  to the head or tail page never marks it dirty. Android 13 on this part uses
  4 KiB pages, so the exposure is small but the case is real. Ticket 26.
- RPCS3 PR 19561 forces strict query scopes on Adreno and Turnip, after games
  hung on `vkCmdCopyQueryPoolResults` with `VK_QUERY_RESULT_WAIT_BIT` waiting
  for an availability bit that never arrived. Vita3K calls
  `copyQueryPoolResults` per scene with `eWait` (`context.cpp:599-601`).
  Ticket 07.
- NetherSX2-Turnip documents that turning framebuffer fetch off fixed a PS2
  game, and that turning readbacks off was a large win on a Snapdragon 865. Same
  levers as `disable-programmable-blending` and `disable-surface-sync`.
- WinNative's Performance driver variant sets `KGSL_PROP_PWR_CONSTRAINT` to
  `PWR_MAX` at queue creation and re-asserts it every 1000 submissions.
  Ticket 24.
- The renderer calls `adrenotools_set_turbo` when `turbo-mode` is on
  (`renderer.cpp:2388`). It only exists for the stock Qualcomm driver.
  Ticket 05.

## What to run first, after the measurements

Tickets 00, 01 and 02 are done. Ticket 03 is the gate on everything else, so
it is first. Then, with the GPU known to be the limit at 93 to 99% busy, the
order changes from what the first draft assumed:

1. **03**, the profiling harness. Gates 04 and everything below it.
2. **04**, the baseline, on Turnip only, in gameplay.
3. **05**, the settings that still have an open question: the surface write-back
   rows and the thread rows. The framebuffer-fetch rows are answered.
4. **24**, the GPU clock. Under the energy target this is the largest single
   question: what is the lowest clock that still holds a steady frame rate. Test
   the three Ayaneo power modes before writing any code.
5. **09, 28**, pacing and the frame rate hint. For a 30 FPS game on a 60 Hz
   panel, FIFO and `ANativeWindow_setFrameRate` can move the whole device, not
   only the renderer.
6. **20**, resolution. Resolution 1.0 is a quarter of the pixels of 2.0, so
   "how low can we go and still hold steady" is now the question.
7. **18, 19, 15, 06**, the GPU-side work, for the same frame at less energy:
   a second full-panel pass, GMEM versus direct rendering, attachment layouts,
   and the unused input attachment on the raster-order path.
8. **30**, which needs no baseline and can run at any point.
9. **07, 08, 09, 10, 16, 17, 21, 23, 25, 26, 27, 29** as their blockers
   clear.

Tickets **11, 12, 14 and 25** are expected to close as rejected or flat, from
the kernel rule and the CPU-busy numbers already measured. Do not spend a
measurement run on them before recording the reason they close.

## Decisions so far

- 2026-10-02: the base for this plan is the Plus base, and the plan replaces
  `.scratch/pocket-s-optimization/`. Reason: six settings changed their
  defaults when the base moved, and the old plan's measurements were taken on
  the old base.
- 2026-10-02: `perf-log` is ported once, in ticket 02, and the old
  `pocket-s/05-frame-timing-log` and `plus-base/04-perf-log` tickets close
  against it.
- 2026-10-02, ticket 00: **the plan measures on Turnip.** The stock driver is
  the slow path. The gap is the framebuffer-fetch path: the stock driver has
  neither `rasterization_order_attachment_access` nor shader interlock, so it
  falls back to `direct_fragcolor`, which puts a pipeline barrier on the colour
  attachment before every programmable-blending draw.
- 2026-10-02, ticket 00: **the GPU is the limit.** 99% busy on stock and 93% on
  Turnip, against CPU 23% and 43%. Every CPU ticket is therefore expected to
  close flat.
- 2026-10-02, ticket 05: **`disable-programmable-blending` is rejected.** No
  gain on Turnip, 29.86 against 29.96 FPS, and the picture is destroyed.
- 2026-10-02, ticket 05: **`high-accuracy: true` is inert on the stock
  driver.** 6.39 against 5.76 FPS, and `direct_fragcolor` stays true because the
  driver has no shader interlock to switch to.
- 2026-10-02, ticket 00: **a measurement outside the game is not a result.**
  The first ticket 00 pass read a title screen and got the driver order
  backwards. The protocol now requires `tools/android/gameplay_scene.sh`.
- 2026-10-03: **the target is energy per played frame, not frame rate.** Steady
  30 FPS on a locked game, 60 where the rate is unlocked, and the lowest power
  that holds both. Energy never buys playability: criterion 1 is a gate, and a
  change that saves power and misses the frame rate or the steadiness is
  rejected.
- 2026-10-03: **the 29.96 FPS reference run does not meet the new
  playability gate.** Its p99 interval is 43.43 ms against a 33.33 ms frame
  time, so the emulator is missing vsyncs on a game that is meant to be locked
  to 30. Fixing that is now a correctness issue, not a speed one.
- 2026-10-03: **every measurement carries power and passes a validity check.**
  `tools/android/device_power_sample.sh` reads device power as the battery
  current times the voltage, and `tools/android/run_is_valid.sh` rejects a run
  where the emulator's watchdog fired, where the thermal status during the run
  was 3 or above, or where the samples covered a menu. The first energy
  baseline attempted was rejected by that rule, which is why the rule exists.
- 2026-10-03, ticket 03: **the harness is built and three of its four commands
  run on the device.** `device.sh` gains `perf`, `trace`, `clocks` and
  `latency`; `clocks` replaces `thermal` so there is one set of sysfs paths. The
  four host threads now carry short names, so a report separates them. Measured
  on the device: `trace` produced a 3.5 MB trace carrying `sched_switch`,
  `power/gpu_frequency`, `gpu_mem_total` and `thermal_temperature`; `clocks`
  reads every column; `latency` finds the SurfaceFlinger layer. `perf` is not
  verified, because an APK cannot be built from a git worktree.
- 2026-10-03, ticket 03: **the Android APK cannot be built from a git
  worktree.** SDL's `GetGitRevisionDescription.cmake` requires a `.git`
  *directory* with `HEAD` and `packed-refs`, and a worktree has `.git` as a
  file. Confirmed pre-existing by building with the branch's only manifest
  change stashed. `container/vita3k-docker.sh android release` has to run in the
  main checkout. The same applies to `git submodule update --init --recursive`,
  which reports success and leaves the working trees empty.
- 2026-10-03, ticket 03: **the GPU is not pinned to its cap.** With the emulator
  up and the GPU idle, `gpuclk` reads 220 MHz against a 680 MHz `max_gpuclk`,
  and it read 680 MHz only while a game was running. So the cap is a ceiling,
  not a floor, and the energy question for ticket 24 is about what holds the
  frame rate, not about raising a fixed clock.

## Fog

- Whether the stock driver lists `VK_EXT_/ARM_rasterization_order_attachment_access`
  and can be made to enable its feature. Ticket 30. This is the largest open
  question in the plan.
- Whether the Vulkan hex device ID, which ticket 01 did not record, is needed
  for the preset's device matching. Ticket 22, with ticket 01's step 3.
- Whether `timestampValidBits` and `timestampPeriod` are usable, which decides
  whether `scenes.csv` can carry GPU timestamps. Ticket 26 now owns it; ticket
  02 left it open and closed itself.
- Whether the device is GPU-bound on a title other than the one ticket 00 used.
  Ticket 05. The prior is GPU-bound: 99% and 93% busy.
- Whether the device honours an ADPF hint, and whether that moves the prime
  core out of its low bin. Tickets 29 and 13 together. Ticket 11 found the
  nice value is refused, so this is the only remaining placement route.

Closed by ticket 00 and 01, kept here so the mistake is not repeated:

- The driver is Turnip. The stock driver is 5.2 times slower in gameplay.
- The GPU is the limit: 93 to 99% busy, CPU 23 to 43%.
- The KGSL files are `gpuclk`, `gpu_busy_percentage`, `gpubusy`, `max_gpuclk`,
  `throttling`, `idle_timer`. `gpuclk_khz` and `busclk_khz` do not exist.
- The cpufreq files are under `cpu*/cpufreq`, not `policy*`.
- Transparent huge pages are `always`, not `madvise`.
- The temperature zones `cpu-0-*` and `cpu-1-*` read without root.