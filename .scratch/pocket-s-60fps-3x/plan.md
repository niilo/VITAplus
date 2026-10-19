# Pocket S: 60 FPS at 3x, with Steam Deck support

Planning date: 2026-10-18. Inspected revision: `09f207d3`.

This is an implementation plan, not a claim that the target has been reached.
This session inspected code and recorded findings. It did not measure these
games on a Pocket S or Steam Deck. The raw device captures cited by older
tickets are not present in this cloud checkout, so their numbers below are
reported historical results, not independently reprocessed measurements.

The user's new priority is correct, sustained 60 FPS at a true 3x internal
resolution. This changes the priority of the earlier plan, which optimized
energy at each game's existing frame limit. Preserve those records. Record
power and thermals, but do not reject a necessary performance improvement merely
because it uses more power than the old 30 FPS configuration.

## 1. Define the result per game

| Game | Required work before its 60 FPS result is meaningful |
| --- | --- |
| Uncharted: Golden Abyss | Reproduce a moving gameplay scene. Check a correct 60 FPS unlock separately from rendering performance; the existing reference targets 30. |
| Killzone: Mercenary | Establish its game/version-specific frame limit, unlock feasibility, and a combat scene with effects. There is no comparable performance baseline in the reviewed records. |
| Jak and Daxter Collection | Track Jak 1, Jak II, and Jak 3 separately. Recheck rendering, boot, and audio correctness before performance claims. Old menu captures and reported 20 FPS sessions are not a gameplay baseline. |
| Sly 2: Band of Thieves | Interprets the user's “Sky 2.” Establish the actual installed version, frame limit, and an outdoor traversal/combat scene. |
| Ratchet & Clank | Record the exact installed Vita entry/version. If the collection is used, track its games separately. Use combat with weapons, particles, and traversal. |
| WipEout 2048 | Verify the game's limiter and any 60 FPS patch. Use a reproducible race section; do not assume the older ticket's “unlocked” candidate label proves native 60 FPS. |

For each entry, record title ID, region, game update, executable/module hashes,
save, input sequence, and patch version. A 30 FPS fallback remains available
where a correct unlock is unavailable. That entry has not met the 60 FPS goal.

The existing `fps-hack` forces display waits to one vblank in
[`SceDisplay.cpp`](../../vita3k/modules/SceDisplay/SceDisplay.cpp). The Android
UI already warns that this can double game speed. Test it per game; it is not a
general 60 FPS solution. Check movement, game timers, physics, animation,
cutscenes, input, and audio against wall time. Investigate a title-specific
limiter/timestep patch when necessary. Reuse the patch loader where appropriate,
but require matching executable/version and expected original bytes before
applying a patch. An unlock changes rendering demand, so every optimized result
must state whether the patch was enabled.

For a full-size 960x544 target:

| Multiplier | Internal target | Pixels per image |
| --- | --- | ---: |
| 1x | 960x544 | 522,240 |
| 2x | 1920x1088 | 2,088,960 |
| 3x | 2880x1632 | 4,700,160 |

Log actual target sizes and sample counts: games can use smaller or changing
targets. Keep internal resolution separate from the swapchain/output surface.
The Pocket S panel is 2560x1440, so a full-size 3x image is downsampled to it.
Lower internal resolution plus upscaling is an alternative quality mode; it
does not meet this true-3x target.

Moving from 2x at 30 FPS to 3x at 60 FPS multiplies output-pixel throughput by
`(3/2)^2 * (60/30) = 4.5`. This is a workload comparison, not a prediction of
required total speedup. Fixed costs, overdraw, samples, passes, clocks, and
overlap must be measured. Precompilation does not remove the steady pixel cost.

## 2. Keep the useful work already present

| Current work or record | Consequence for the new plan |
| --- | --- |
| Dynarmic A32 frontend with ARM64 and x86-64 host backends | Guest CPU instructions already become native host code during execution. Do not propose native compilation as a feature that is wholly absent. |
| Safe Dynarmic optimizations, including existing block optimizations | Verify actual emitted code and costs before proposing flags that are already enabled. |
| Shader hash list, cached SPIR-V, and a loading-time shader-module pass | Extend the missing pipeline stage instead of replacing shader caching. |
| Pipeline hash list plus `VkPipelineCache` blob | It lacks complete, replayable pipeline descriptions. Known hashes are still recreated on first use. |
| Full `GxmRecordState` owned by `CompileRequest` | The latest pull already fixed the truncated-record issue. Old open-ticket status is not evidence the fix is missing. |
| Perf logs, HLE/JIT counters, simpleperf/Perfetto commands, baseline reports, touch record/replay | Extend the existing tools. Do not build another benchmark harness from scratch. |
| Texture per-scene hash memoization | Already implemented. Count remaining hashes and bytes before changing invalidation. |
| Android present-mode selection, `swapchain-scale`, ADPF support | Already implemented. Their presence does not prove a performance gain. ADPF's earlier prime-core test did not establish one. |

The older Uncharted reference reports 29.96 FPS at 2x with 93% GPU busy on
Turnip, versus 5.76 FPS on the stock driver. The recorded Turnip identity is:

```
driverID: MesaTurnip  driverName: Turnip (Balemuni Apex v2 Ultimate)
```

This is evidence for selecting a verified Turnip build as the Pocket S
reference, not a fresh measurement or a requirement to keep that old build
forever. The later driver review records `driverID: QualcommProprietary`
without rasterization-order attachment access, and `driverID: MesaTurnip`
with it. Query the actual features each run and preserve the stock fallback.
Do not force an unsupported feature or infer the driver from a folder name.

Other records change the priorities:

- An older Jak HLE recording reports about **2.15 million
  `sceGxmWaitEvent` calls/s**, consuming about 190 ms/s within that HLE entry.
  The current export still returns `UNIMPLEMENTED`. This is a concrete CPU and
  correctness investigation, not a proven speedup.
- An older Jak JIT recording reports roughly 28,600 page-table read callbacks/s
  and 9,809 translated instructions/s. These have different denominators.
  They do not establish a memory-access miss percentage, repeated compilation,
  or translation time. Measure addresses, reasons, and durations.
- The black-menu investigation later measured continuing submissions and
  approximately 30 FPS despite a watchdog warning. It identified a false
  watchdog inference. A black screen with counted frames is not playable.
- Earlier pipeline refusals and SPIR-V errors have subsequent fixes and
  counter-evidence. Reproduce on this revision before opening duplicate fixes.
- The audio cost reported as 13.31% was not reproduced in a later recording;
  that recording found 1.77% for the callback thread and no filter-design churn.
  Neither recording is a current baseline for all six games.
- The xxHash record identifies a possible CPU cost, but the proposed per-scene
  cache already exists. Samples per second are not function calls per second.
- Some old notes confuse the A32 guest frontend with the ARM64 host backend,
  or treat one GPU-bound title as proof CPU work cannot help another title.
  Use the current source and per-title critical path instead.
- Do not carry forward an assumption that compression is unavailable merely
  because the driver is Turnip. Inspect the actual driver's image/compression
  behavior. Vendor guidance alone does not prove the measured path's cost.

## 3. Establish measurements that can select the next change

Start with Uncharted, Jak 1, and WipEout as diagnostic cases, then cover every
requested entry. This is execution order, not a reduction of the game list.

1. Use a release build with symbols retained separately for profiling. Record
   build ID/artifact hash, commit, effective per-game settings, driver ID,
   version and cache UUID, mapping method, and enabled device features. Old
   records found stale version banners, so the displayed version is not enough.
2. Record a repeatable gameplay sequence per entry with the existing touch/input
   tools. Use one controlled scene first, then add a second scene with different
   effects or geometry. Verify the screenshot and audio before accepting data.
3. Run 1x, 2x, and 3x reconnaissance. Test a verified unlock initially at 1x to
   separate game timing from rendering capacity, then repeat at 3x. Compare
   the current configuration with each candidate in separately labelled cold-
   and warm-cache experiments. Keep stock-driver fallback comparisons separate
   from the Turnip reference results.
4. Collect frame-time distributions, new guest-frame IDs, present IDs, CPU
   running/blocked time per critical thread, GPU elapsed time, pipeline compile
   events, submission counts, transfer bytes, and memory high-water marks.
   Repeated presentation of one guest frame must not count as reaching 60 FPS.
5. Add **GPU timestamp queries** after checking `timestampValidBits` and
   `timestampPeriod`. `scenes.csv` currently measures host recording time per
   submission, not GPU execution time. Read query results after completion in a
   later frame, without adding synchronous query waits or device-idle calls.
   If timestamps are unavailable, use a supported GPU capture/profiler and mark
   the missing measurement explicitly; host recording time is not a substitute.
6. Split renderer time into recording, compilation, hashing/decode, uploads,
   readbacks, fence waits, and present waits. Split guest-thread time into JIT
   execution, translation, HLE work, and legitimate blocking. CPU and GPU can
   overlap; their times must not simply be added as the frame time.
7. Preserve A/B/A/B/A, matching settings, valid gameplay windows, cooldowns,
   and cold/warm separation. Measure instrumentation overhead with counters
   off/on. Repair confirmed watchdog false positives using actual progress;
   do not silently ignore invalid-run reports.

Use simpleperf with matching symbols and Perfetto on Pocket S. Use host CPU
profiling plus Vulkan GPU timing on the physical Steam Deck. GPU busy and
aggregate CPU usage are supporting signals: one saturated guest thread or a
blocked render thread can limit FPS while whole-device utilization is low.

At this stage produce one table per entry: cap/unlock state, correctness,
1x/2x/3x FPS and p99, CPU critical-path time, GPU time, compile hitches, dominant
wait, and confidence. That table determines the next implementation task.

## 4. Ordered implementation work

| Order | Work package | Concrete change or investigation | Exit condition |
| --- | --- | --- | --- |
| P0 | Correct baseline and unlocks | Recheck boot, rendering, audio, patch timing, and previous pipeline failures on current code. Establish each entry's real 60 FPS feasibility. | Valid gameplay captures; correct timing; current failures separated from stale records. |
| P1 | Missing timing and attribution | Extend existing telemetry with GPU queries, compile durations, wait categories, hash bytes, and frame/submission association. | Each slow entry has an identified critical path; instrumentation overhead is known. |
| P2a | GXM event behavior | Trace Jak's high-frequency `sceGxmWaitEvent` callers and establish the actual event contract, then implement the missing behavior if confirmed. | Correct waits/wakes, cancellation and shutdown; no lost guest scheduling tokens; reduced measured polling without progression regressions. |
| P2b | Renderer idle wakeups | Replace the `top(3)` three-microsecond timeout path with event/predicate-driven waits for commands, display work, async flips, and shutdown. Reuse the existing `Queue::wake()`. | No lost wakes, no present delay, bounded recovery, fewer idle wakeups; both devices behave correctly. |
| P3 | Complete pipeline preparation | Persist stable pipeline recipes; pre-create known pipelines before gameplay; retain the driver cache. | Same scene's warm run has measured reductions in first-use pipeline stalls, with unchanged output and bounded memory/startup cost. |
| P4 | GPU frame cost at 3x | Optimize the largest measured pass, attachment traffic, synchronization, or shader cost, one change per comparison. | GPU time and late-frame rate fall at true 3x on the same scene; correctness and Deck gates pass. |
| P5 | Memory/texture and CPU cost | Address measured page-table callback causes, texture hashing/upload waits, HLE dispatch, or JIT execution costs. | The critical thread gets cheaper, or measured power/thermal headroom improves, without memory-coherence regressions. |
| P6 | Sustained operation and profiles | Tune only the effective per-device/per-game choices, then run the full six-game matrix and collection entries. | Sustained target met per entry, or a named remaining limit is recorded. No universal preset is inferred from one game. |

P2a and P2b are separate changes. A GXM wait must not be made blocking based
only on its name: derive the condition and semantics first. Likewise,
`sceKernelGetThreadId` already returns `thread_id` directly, so the old note's
suggestion to cache its result is not an implementation task.

For P2b, `Queue::top(0)` currently waits only for a nonempty command queue.
Switching to that call alone would ignore a display-only wake. Include pending
display/flip/shutdown state in the wait predicate or a consistent wake
generation, and test notification-before-wait as well as notification-during-wait.

## 5. Finish GPU precompilation first, then consider CPU prewarming

| Layer | Current behavior | Recommendation |
| --- | --- | --- |
| Emulator/HLE C++ | Compiled into native ARM64 or x86-64 as part of the build. Release LTO is already configured. | Verify release flags are active. Consider profile-guided builds only for measured host hot paths. |
| Vita CPU code | Dynarmic translates blocks on demand into the host ISA. Each CPU instance owns JIT state and cache. | Instrument compile time and duplicated block translation first. Consider targeted prewarming only if it explains late frames. |
| Vita GPU shaders | Translated to cached SPIR-V; the loading pass creates shader modules. SPIR-V is an intermediate representation. | Keep this cache and its variant/version handling. It is not the missing whole solution. |
| Vulkan graphics pipelines | Created on first draw from state; the driver cache can help but known hashes alone cannot reconstruct them. | Record full stable recipes and ask the driver to create known pipelines during loading. This is the first precompilation project. |
| Static texture conversion | Some formats can require host decode/conversion when uploaded. | Predecode/cache only if profiling shows repeated static conversion is material. Do not preload the entire game's assets speculatively. |

P3 should implement these steps:

1. Capture the actual state needed to reconstruct a pipeline: shader variants
   and content, specialization values, vertex input, topology, blend/depth/
   stencil/sample state, descriptor/pipeline layout, render-pass compatibility,
   formats, and relevant renderer features. Serialize values and stable IDs,
   not pointers or `GxmRecordState` memory bytes.
2. Version the recipe format and namespace cache use by game/module content,
   shader-generator version, options/features, GPU and driver compatibility.
   Check the Vulkan blob's device/driver cache identity before reuse. Separate
   Pocket S and Deck native driver caches.
3. Load known recipes and create pipelines before the game starts rendering.
   Start with the recorded startup/first-scene set; bound compile concurrency
   and memory. Measure both sequential and limited parallel creation. Avoid
   spending all cores or memory preparing pipelines the session will not use.
4. Query and enable pipeline-creation feedback/cache-control features where
   supported. A compile-required result must lead to real compilation, not a
   permanently failed entry. Time cache hits and misses rather than assuming a
   known hash means creation is free.
5. Preserve correct first-use behavior for unknown pipelines. The current async
   path returns no pipeline and the draw can be skipped. Do not count missing
   draws as a performance improvement or enable that path globally as the fix.
   Check worker lifetime, shared shader synchronization, shutdown, and failure
   handling as concurrency grows.
6. Validate the same scene across process restart, renderer option changes,
   driver update, different GPU, and a truncated/incompatible cache. A cache
   miss or rebuild must preserve normal game behavior.

This improves cold/warm startup and first-use stutter. New levels and unseen
state still create new recipes. It does not make expensive pixel shaders or
attachment transfers cheaper after they have compiled.

A later CPU prewarming experiment should first record guest block descriptors,
module-relative locations, execution/compilation frequencies, and compile
durations. Use ARM/Thumb state and relevant CPU state in identity, validate
module content after loading/relocation, and preserve code invalidation.
The current Dynarmic public API exposes execution and invalidation, but no
general persistent-AOT or explicit precompile interface. An extension is real
engineering work, not a config switch.

Prefer rebuilding selected blocks from validated descriptors or reusable
intermediate data before attempting a raw native-code disk cache. Native code
can embed callback/context addresses and links that change between processes;
cache identity, relocation, instruction-cache maintenance, concurrency, and
invalidation must be designed. ARM64 and x86-64 products remain separate.
Do not translate blocks by executing guest code during a preparation pass.

Do not start whole-game AOT unless measured translation costs remain important
after P2-P5. It cannot remove guest work, HLE waits, or GPU work. Module loading,
computed branches, ARM/Thumb state, relocations, and writable code also make
complete ahead-of-time discovery unreliable without a dynamic fallback.

## 6. GPU/Vulkan optimization choices after attribution

- **Attachment traffic and pass boundaries:** count loads/stores, pass restarts,
  macroblock work, feedback barriers, and bytes copied per rendered frame. Merge
  only compatible work where guest-visible ordering permits it. Do not remove
  programmable blending or macroblock synchronization to improve an FPS number.
- **Image layouts:** track real image use and transitions. A color image read
  as an input attachment while it is written needs a legal feedback path.
  Blindly replacing `GENERAL` with `COLOR_ATTACHMENT_OPTIMAL` or dropping the
  input attachment is not a safe global optimization. Preserve raw/F16 and
  aliasing semantics. Validate candidate changes before release measurements.
- **3x fragment cost:** inspect the slowest shaders for texture traffic,
  overdraw, register pressure, conversions, and excessive generated operations.
  Use supported GPU tools/counters where available, plus controlled shader
  experiments. Preserve floating-point and blending correctness; the old shader
  correctness fixes are not disposable performance costs.
- **Transfers and surface synchronization:** count actual uploaded/read-back
  bytes and fence stalls. Avoid redundant copies or dirty ranges only when
  guest memory, aliases, and GPU-produced surfaces remain coherent. Check the
  existing surface-sync clamp before proposing it again.
- **Descriptors and allocations:** count real updates and allocation churn.
  Reuse compatible state and frame storage when the profile shows a cost.
  Old Adreno threshold claims from other GPUs are not measured limits here.
- **Submission modernization:** consider synchronization2, timeline semaphores,
  or transfer-queue work only after submission/wait costs justify it. Query
  features, retain the existing fallback, and prove useful overlap. A newer
  Vulkan API alone does not predict an FPS gain.
- **Output:** keep panel-native output for the reference. `swapchain-scale`
  changes output, not the guest resolution multiplier. Lowering it is a labelled
  diagnostic/quality experiment, not evidence that true-3x rendering got cheaper.

## 7. CPU optimization choices after attribution

- Re-measure the GXM wait stub first on Jak. HLE self-time that includes a real
  sleep is not CPU work to eliminate. Separate guest spin, host processing,
  and legitimate event waits.
- Inspect page-table callback addresses, access sizes, alignment, permissions,
  and mapping transitions. Separate translation-time code reads from execution
  misses. Compare only supported mapping modes with full rendering/audio checks;
  a mode that runs faster through stale memory is invalid.
- Count texture hash calls, bytes, scene IDs, and dirty reasons. Extend reuse
  across scenes only with sound write/alias invalidation. Preserve the current
  within-scene memo; simply adding it again changes nothing.
- Profile guest scheduling and JIT execution on the critical threads. Preserve
  guest priorities, waits, preemption, exclusives, and memory ordering. Do not
  disable the global exclusive monitor or accurate scheduling as a broad preset.
- Audit generated ARM64 for measured hot instruction patterns, memory callbacks,
  spills, and call overhead. The ARMv7 guest ISA does not prohibit a better
  ARM64 host implementation. Host-specific improvements need correct semantics
  and a separate x86-64 path or portable fallback.
- Verify existing release LTO, then evaluate PGO or SIMD for demonstrated
  emulator-side work such as decode/conversion. Use runtime/target feature
  selection; do not distribute one globally host-specific binary to both targets.
- Treat affinity/ADPF as measured scheduling experiments after finding a
  runnable critical thread. ADPF is already present and its earlier result did
  not establish a speedup. Root-only governor controls and a supposed free
  prime-core speedup are not prerequisites for this plan.
- Keep the 128 MiB JIT reservation unless measured eviction/translation pressure
  justifies a change. Reserved virtual address space is not resident memory.

## 8. Gates for Pocket S and Steam Deck

For a title with a validated 60 FPS mode, the proposed acceptance target is:

- Correct gameplay, audio, rendering, input, and wall-clock game speed.
- True 3x internal targets and the declared output settings.
- At least 99% of measured gameplay seconds within 1 FPS of the target
  60/59.94 Hz cadence; report the full distribution and late-frame fraction.
- Present/frame-interval p99 at or below 20 ms on a controlled 60 Hz output,
  and fewer than 1% of intervals above 25 ms. Correlate presents with new guest
  frames. A repeated-frame 60 Hz swapchain alone does not pass.
- As an engineering headroom target, critical CPU work and GPU work each fit
  within approximately 14 ms at p95. They overlap; this is not a 28 ms budget.
- The final three minutes of a 20-minute run remain within 3% of the first
  stable three minutes, with no severe-or-worse Android thermal status. Also
  check a longer traversal that discovers new shaders rather than only one loop.

Record cold loading/first-use behavior separately from the warm steady target.
If the target fails, state whether the remaining limit is the game cap,
correctness, CPU execution, GPU work, compilation, memory, or synchronization.
Do not turn an improvement into a claim that all six games meet the goal.

Keep mode, brightness, refresh rate, driver, clocks, and charging state
controlled. Battery voltage times battery current is not whole-device power
while a charger supplies the system. Use a consistent on-battery protocol or
an appropriate external measurement before claiming power savings.

Steam Deck preservation is a gate throughout the work:

1. Build and test Linux x86-64 and build Android ARM64 for each relevant code
   change. Run the current full CTest set, including `ngs-scheduler-tests`
   introduced by the latest pull; old reports of three suites are not the
   current complete target list. Add focused tests for actual new cache,
   wait/wake, invalidation, or patch behavior.
2. Run shared renderer, timing, memory, and JIT changes on a physical Deck with
   RADV and recorded TDP/refresh settings. Cloud compilation and Xvfb startup
   establish neither GPU performance nor game compatibility on the Deck.
3. Preserve shader precision, image layouts, synchronization, and cache misses
   across both drivers. Select paths using queried capabilities. Scope Android
   scheduling/window behavior appropriately; do not copy Snapdragon settings
   into a global default.
4. Keep native shader/pipeline/JIT products separated by their compatibility
   identities. Transferable recipe/content data must still be checked against
   each renderer's features and formats.
5. Reject a shared optimization if Deck correctness fails or its performance
   regresses beyond repeated-run variation. Maintain a per-game device table.
   The user requested Deck support; this plan does not silently impose the same
   Pocket S 3x/60 performance target on every Deck model.

## 9. First implementation sequence

1. Make current release builds identifiable and reproduce Uncharted and Jak 1
   gameplay on the reference Turnip build. Classify correctness failures before
   timing them. Record title limits/unlock behavior for the whole requested list.
2. Add the missing GPU and compilation/wait measurements to the existing tools.
   Capture the 1x/2x/3x table. Extend recorded input walks beyond Uncharted.
3. Investigate `sceGxmWaitEvent` semantics and the renderer idle timeout as
   independent CPU changes. Measure each separately; neither is a promised FPS
   multiplier.
4. Implement versioned pipeline recipe capture and loading-time pipeline
   preparation. Compare cold, warm, and newly encountered state, with correct
   rendering. Keep startup time and memory in the result.
5. Optimize the largest measured 3x GPU pass or transfer cost, while measuring
   CPU-bound entries independently. Run the Deck gate for every shared change.
6. Reassess targeted CPU prewarming from compile-duration and block-reuse data.
   Defer whole-game AOT while those data do not justify it.
7. Complete all game/collection-entry runs, correct unlock validation, and
   sustained tests. Publish an achieved/blocked result for every entry.

The first deliverable is a trustworthy per-game frame budget and a correct
baseline. The first substantial compilation project is full Vulkan pipeline
preparation. Sustained 3x/60 requires reducing whichever CPU/GPU work still
exceeds the frame budget after that preparation.

## Source references

- [Prior Android plan](../pocket-s-android13/spec.md),
  [driver measurement](../pocket-s-android13/issues/00-driver-and-power.md),
  [corrected driver finding](../pocket-s-android13/issues/30-stock-driver-raster-order.md).
- [HLE counters and Jak call rates](../cpu-hotpath-counters/issues/01-hle-import-counters.md),
  [JIT counters](../cpu-hotpath-counters/issues/02-translator-counters.md),
  [Jak session findings](../cpu-hotpath-counters/issues/05-jak-daxter-performance.md),
  [later black-menu evidence](../game-sweep-2026-09-30/issues/05-jak-daxter-black-screen-after-intro-video.md).
- [Audio finding](../pocket-s-android13/issues/31-audio-thread-cost.md),
  [corrected texture/host attribution](../pocket-s-android13/issues/32-host-cpu-cost.md),
  [JIT cache reservation](../pocket-s-android13/issues/14-jit-code-cache.md),
  [ADPF implementation/result](../pocket-s-android13/issues/13-adpf.md).
- [Current CPU configuration](../../vita3k/cpu/src/dynarmic_cpu.cpp),
  [host backend selection](../../external/dynarmic/src/dynarmic/CMakeLists.txt),
  [current public JIT API](../../external/dynarmic/src/dynarmic/interface/A32/a32.h).
- [Current GXM exports](../../vita3k/modules/SceGxm/SceGxm.cpp),
  [render command waiting](../../vita3k/renderer/src/batch.cpp),
  [queue wake implementation](../../vita3k/threads/include/threads/queue.h).
- [Pipeline storage and creation](../../vita3k/renderer/src/vulkan/pipeline_cache.cpp),
  [shader cache](../../vita3k/renderer/src/shaders.cpp),
  [Vulkan shader preload](../../vita3k/renderer/src/vulkan/renderer.cpp),
  [draw behavior](../../vita3k/renderer/src/vulkan/scene.cpp),
  [submit timing](../../vita3k/renderer/src/vulkan/context.cpp).
- [Texture cache](../../vita3k/renderer/src/texture/cache.cpp),
  [output and present modes](../../vita3k/renderer/src/vulkan/screen_renderer.cpp),
  [profiling ADR](../../docs/adr/0001-android-profiling-tools.md).
