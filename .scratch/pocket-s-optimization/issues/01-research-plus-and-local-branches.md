# 01: List the Android changes in Vita3K-Plus and in the local branches

Status: resolved
Claimed: 2026-09-25 Claude Code session (Opus 5.5)
Type: research
Label: ready-for-agent
Blocked by: none

## Question

Which commits on `plus/all-enhancements` and on the three local `feat/*`
branches improve speed or stability on Android with an Adreno GPU, and
what does each one need?

## Context

- This repo has the Plus README but not the Plus code (`../map.md`, Notes).
- Remote `plus` is read-only for this work. Never push to it.
- Local branches with the user's earlier work: `feat/ayaneo-pocket-s-performance`,
  `feat/vulkan13-adreno`, `feat/vulkan-device-profiles`.

## Steps

1. `git fetch plus` and `git fetch origin`.
2. For each local branch: `git log --oneline master..<branch>` and
   `git show --stat <commit>`. Read each diff.
3. `git log --oneline --no-merges master..plus/all-enhancements`. Read each
   commit that touches Android, Adreno, Turnip, Qualcomm, page table,
   thread scheduling, deadlocks, memory, swapchain, or the pipeline and
   texture caches. Include at least: `f51a58ac`, `14521654`, `ffa287b4`,
   `5d25698c`, `e7f22c72`, `89496b8a`, `bf961274`, `4ee70305`.
4. For each commit, record: what it changes, the files, whether it mixes
   several fixes, and which earlier commits it needs.
5. Test whether it applies to `master`: create branch `tmp/plus-check` from
   `master`, run `git cherry-pick --no-commit <sha>`, record the result, run
   `git cherry-pick --abort`. Delete `tmp/plus-check` at the end.

## Output

Under `## Answer`:

1. A table with columns: source (branch or Plus), commit, subject, area,
   files, needs, applies cleanly (yes or no), expected effect on the
   Pocket S.
2. At most 10 lines of recommendation with two options for ticket 02:
   take single commits (list them), or base the fork on
   `plus/all-enhancements`. Say which local `feat/*` commits to keep in
   either option.

Do not change `master` in this ticket.

## Answer

Method: each commit was tested with `git cherry-pick --no-commit` on
`tmp/plus-check` (from `master` 3567ed76), then aborted. Pair and chain
checks used `git merge-tree --write-tree`. `tmp/plus-check` is deleted.
`master` is not changed. "applies cleanly" means the commit applies alone on
`master`, unless the needs column names a chain. No commit was built or
measured on the device.

| source | commit | subject | area | files | needs | applies cleanly | expected effect on the Pocket S |
|---|---|---|---|---|---|---|---|
| feat/ayaneo-pocket-s-performance | 541499ee | ci: trigger artifact build test | CI | `.ci/common.sh` (comment only) | none | yes | None. Drop it. |
| feat/ayaneo-pocket-s-performance | 1838cd88 | perf(android): optimize Vulkan for Adreno handhelds | several fixes in one commit: validation default, present mode, pipeline cache key, Turnip detection | `config.h`, `state.h`, `EmulatorConfig.kt`, `pipeline_cache.cpp`, `renderer.cpp`, `screen_renderer.cpp` | none | yes | Turns the validation layer off by default on Android. This is the largest expected gain. Uses FIFO when v-sync is on and MAILBOX when it is off. Adds the driver identity to the pipeline cache file name and header. Detects Turnip by `driverID`. Logs a warning when the requested mapping mode is not supported. Resets turbo mode on exit. |
| feat/vulkan13-adreno | be6c7690 | perf(vulkan): add Adreno Vulkan 1.3 paths | Vulkan 1.2 and 1.3 features | `renderer.cpp`, `context.cpp`, `screen_renderer.cpp`, `screen_filters.cpp`, `pipeline_cache.cpp`, `state.h` | 1838cd88 | no alone; yes as a chain after 1838cd88 | Timeline semaphore for GPU completion waits. Dynamic rendering and synchronization2 for the final screen pass. Skips a full-screen clear in FSR. A mutex for async pipeline placeholders. The stock driver supports all of these. Expect a small gain in CPU wait time. |
| feat/vulkan13-adreno | d9f38adf | fix(vulkan): stabilize Adreno optimization paths | shader compile waits, driver properties | `pipeline_cache.cpp`, `renderer.cpp` | be6c7690 | no alone; yes in chain | Replaces a busy wait with a condition variable when two threads need the same shader. Reads driver properties through Vulkan 1.2 core. FSR needs only `shaderFloat16`. Expect fewer CPU spikes during shader compiles. |
| feat/vulkan-device-profiles | dfb60519 | feat(vulkan): add device profiles and FSR fallback | driver profiles, FSR, pipeline cache | `renderer.cpp`, `screen_filters.cpp`, `pipeline_cache.cpp`, `state.h` | d9f38adf | no alone; yes in chain | Adds `VulkanDeviceProfile` (stock or Turnip). Under Turnip, FSR renders to a private image and copies it to the swapchain. Loads the pipeline cache after all features are set. Stops the async workers while it saves the cache. |
| feat/vulkan-device-profiles | dccbcc6e | fix(vulkan): resolve cache and pipeline review findings | fixes to dfb60519 | `pipeline_cache.cpp`, `screen_filters.cpp` | dfb60519 | no alone; yes in chain | Fixes waiters that could stay blocked on a shader loaded from disk. Restarts the async compile workers on every exit path. Stability only. |
| feat/vulkan-device-profiles | 604ea188 | fix(vulkan): require Vulkan 1.1 and recover pipeline failures | minimum API, pipeline errors, FSR | `renderer.cpp`, `pipeline_cache.cpp`, `screen_filters.cpp`, `fsr_*.comp(.spv)` | dccbcc6e | no alone; yes in chain | A pipeline compile exception no longer ends the process. FSR uses an FP16 intermediate image. The 1.1 minimum has no effect on this Vulkan 1.3 device. |
| feat/vulkan-device-profiles | 64c694ae | fix(vulkan): address compatibility review findings | settings, pipeline cache | `settings.cpp`, `pipeline_cache.cpp`, `renderer.cpp` | 604ea188 | no alone; yes in chain | Per-game configs get the new validation default. A bad pipeline cache file is skipped instead of a crash. |
| feat/vulkan-device-profiles | 533f063f, feb21710 | fix(vulkan): gate FSR and device selection by compatibility; correct device compatibility enumeration | GPU selection, FSR | `renderer.cpp`, `screen_renderer.cpp` | 64c694ae | no alone; the full chain 1838cd88..feb21710 applies: yes | None on a device with one GPU. |
| feat/ayaneo-pocket-s-performance | 7526bdba | Merge master into the branch | merge | n/a | n/a | n/a | Not needed. 1838cd88 applies to `master` directly. |
| Plus | 7531b756, e5f0e276 | Fix typeless copies above 1.75x resolution (and review follow-up) | surface cache | `surface_cache.cpp`, `surface_cast_reinterpret.comp` | e5f0e276 needs 7531b756 | yes; pair yes | Correct output at 2x and 2.5x resolution. No speed change expected. |
| Plus | 3776c598 | Minor vulkan validation fix | pipeline cache | `pipeline_cache.cpp` | none | yes | Removes a validation error. No speed change. |
| Plus | 6ecce835 | Safeguard against crashing when exiting | memory | `mem.cpp` | none | yes | Fewer crashes on exit. |
| Plus | d2ad1479, 6db85423 | Decode negative buffer offsets; treat out-of-range offsets as 0 | shader translator | `usse_utilities.cpp`, `spirv_recompiler.cpp` | 6db85423 needs d2ad1479 | yes; pair yes | Prevents `VK_ERROR_DEVICE_LOST` under Double Buffer, the mode the stock driver uses. |
| Plus | fc6048a4 | Read negative buffer offsets correctly | memory | `mem.cpp`, `ptr.h` | none | yes | The same device-lost fix on the CPU side. |
| Plus | 9f4c139f | Sync fix | kernel sync | `sync_primitives.cpp` (10 lines) | none | yes | Small wake-up fix. Effect unknown. |
| Plus | 7814ed87 | Correct Killzone colours. Fix SyncObject crashes | a game fix and a crash fix | `transfer.cpp`, `creation.cpp`, `SceGxm.cpp` | none | yes | Fewer SyncObject crashes. |
| Plus | 14521654 | Fallback to Double Buffer on Qualcomm drivers rather than Turnip | mapping mode | `renderer.cpp` (6 lines) | none | yes | On the stock driver, Page Table or Native Buffer runs as Double Buffer. This avoids a driver crash. |
| Plus | f51a58ac | Switch to Page Table as default for Android | config default | `config.h`, `EmulatorConfig.kt` | 1d4d85e7, which does not apply. A manual two-line port is simpler. | no | No effect on stock (14521654). Page Table on Turnip. |
| Plus | 89496b8a | Fix for Page Table on Adreno 8xx + Turnip | mapping memory | `renderer.cpp` | 918102f7, 8be36fa1. The core change (one VMA allocation per mapping) can be ported by hand. | no | Only Turnip with Page Table. Not tested on the A32. |
| Plus | 4ecb21da | More compatible SPIR-V; stop Page Table crashes on Android | shader translator, texture cache | `usse_utilities.cpp`, `texture.cpp`, `data.cpp`, `texture/cache.cpp` | ed63438f, 9c044c8d, a CMake version change | no | Constant-index composite extract. An address range check before a texture hash. A log line before each pipeline compile on stock Adreno (costs log I/O). |
| Plus | 4ee70305 | Memory efficiencies (especially for Android) | memory, texture cache | `native_bootstrap.cpp`, `mem.cpp`, `texture.cpp`, `renderer.cpp` | 36d3ffff, 8be36fa1 | no | Frees cached textures on `onTrimMemory`. `madvise` on arena pages under an external mapping (not used under Double Buffer). Less risk of a low-memory kill; the device has 16 GB. |
| Plus | 8be36fa1 | Massive changes (NFS, COD, high-res Resistance) | many fixes: world stop for mapping changes, Page Table fallback, fault recovery | 53 files | most earlier Plus commits | no | Most later Plus memory and kernel commits need it. Cannot be taken alone. |
| Plus | 33926592 | Fix start-up hangs (and logging). Spiderman text | kernel, CPU, renderer | `dynarmic_cpu.cpp`, `mem/functions.h`, `renderer.cpp` | 8be36fa1 | no | Fewer start-up hangs. |
| Plus | ffa287b4 | Support for Sonic, thread scheduling, performance enhancements | a game fix, a thread-scheduling setting (off), dispatch tables, swapchain rebuild delay, freeze watchdog | `thread.cpp`, `display.cpp`, `batch.cpp`, `state_set.cpp`, `screen_renderer.cpp`, `surface_cache.cpp` | 8be36fa1, 33926592, d6df8059 | no | Array lookup instead of map lookup for command and state handlers (small CPU gain). No swapchain rebuild for a size mismatch shorter than 4 frames. A watchdog thread that wakes every 10 ms. |
| Plus | d6f60976, 1f7b73f0 | Android changes; settings defaults | config, Android UI | `EmulatorConfig.kt`, `config.h`, `thread.cpp` | ffa287b4 | no | Validation layer off by default. 1838cd88 already does this. |
| Plus | c7555734 | Fix signal semaphore validation errors and FSR | present, FSR | `screen_renderer.cpp`, `screen_filters.cpp`, `renderer.cpp` | 18e0290d (only the version line in `CMakeLists.txt` conflicts) | no | FSR without swapchain storage usage. One semaphore per swapchain image. Overlaps local dfb60519. |
| Plus | 34e8c9bb, 778e0927, b393dd26, 9c044c8d | Mali memory checks and fallback, update checker, SPIR-V 1.0 for Mali, crash log | Mali, config | `renderer.cpp`, `config.h`, `mem.cpp` | 8be36fa1 and later | no | None on Adreno, apart from crash logging. |
| Plus | 36d3ffff | Hang breaker for MGS3. Audio/Video. MSAA blending. Logging | a game fix, a watchdog, Android memory hook, audio and video | `display.cpp`, `kernel.cpp`, `native_bootstrap.cpp`, `SceVideodecUser.cpp` | 8be36fa1 | no | Adds the `onTrimMemory` hook and the Android thread-scheduling setting. |
| Plus | a87c97df, 26441d84, 8cb139f6, 53d78e23 | Memory protections and NGS; timing and memory; Page Table fixes; memory efficiencies | memory mapping, kernel waits, texture cache | `mem.cpp`, `creation.cpp`, `sync_primitives.cpp`, `io.cpp`, `texture/cache.cpp` | 8be36fa1, 36d3ffff, each other in date order | no | Deferred unmap, an event-flag deadlock breaker, reuse of native buffers. Mostly for Page Table and Native Buffer. |
| Plus | bf961274 | Optimisations and Page Table hardening | memory mapping, pipeline cache, IO | `creation.cpp`, `renderer.cpp`, `mem.cpp`, `io.cpp`, `pipeline_cache.cpp` | a87c97df, 8be36fa1, 26441d84, 53d78e23, abe2d71c | no | Unmap then remap skips the world stop and copy (128 MiB budget). The pipeline key ignores unused back-face state, so fewer duplicate pipelines are compiled; this can help on stock too. |
| Plus | abe2d71c | Android fixes | pipeline failure handling | `pipeline_cache.cpp`, `renderer.cpp`, `usse_utilities.cpp` | 6db85423 and later shader commits | no | Remembers pipelines the driver refused, so they are not compiled again on every draw. Prevents freezes when the stock driver rejects a pipeline. |
| Plus | e7f22c72 | Try to emulate the 3 cores of the Vita to stop freezing | thread scheduling, timed waits, hang watchdog | `thread.cpp`, `kernel.cpp`, `sync_primitives.cpp`, `display.cpp`, `h264.cpp`, `interface.cpp` | ffa287b4, a87c97df, 36d3ffff | no | 3 guest cores when scheduling is on. Timed waits use `wait_until`. Wakes all waiters when a stall is found. Also an H.264 change and Android file browsing. |
| Plus | 5ce6b856, 4fce7453 | Timed-out condvar waits take the mutex again; lighter lwmutex | kernel sync | `sync_primitives.cpp`, `thread.cpp` | 26441d84, e7f22c72 | no | A correctness fix for timeouts. A shorter fast path for the most frequent call; small CPU gain. |
| Plus | 918102f7, f8831394, 7adbc08e, 5e0e89d2, 0426045f | Page Table for GB3 and Tearaway; KZ Page Table crashes; hardening; faster deadlock breaker | memory mapping, kernel | `mem.cpp`, `renderer.cpp`, `creation.cpp`, `sync_primitives.cpp`, `SceGxm.cpp` | bf961274 and all before it | no | Page Table stability on Turnip. |
| Plus | 5d25698c | Deadlock-breaker fixes; accurate thread scheduling on by default | config, watchdog | `config.h`, `EmulatorConfig.kt`, `display.cpp`, `sync_primitives.cpp` | ffa287b4, e7f22c72, a87c97df, 36d3ffff | no | At most 3 guest threads run at once, by priority. This can stop some freezes. It can also lower FPS on the 8-core CPU. Measure it. |

Game-specific Plus commits (not read in detail; "yes" marks the ones that
apply): 1a081a1c (yes), 7a68af48, 107c71c2, 5af094c5 (yes), 0632bfa6 (yes),
61522de2, a79fa2c2, 72e7506c, 8d04073b, c29baca6, e4c35d16, 7e428cd7,
8b051123 (yes), 9ed7effa (yes), 075b59f1 (yes), 01f6dc6e (yes), 66ef0310,
6fc25451, 98d04579 (yes), c8a42ce1 (yes), 4dbb1c3a, 3abff1b0, ba4ec343,
e1fec78f, 60bbf1ad, 0f0a7b51 (yes), 6cd1cb84, 98a65bff, 877fdc1b, d6df8059,
dfcc8066, 8f1f540c, 12850dfc, 9dc18643, 2227154f, f4e40524, b8f83474,
1d4d85e7, ed63438f, d508ff2b, 30036d3b, a3107bf2, 155500b2, 89ac3aa0,
8629832e, 64e11b23, 706ae33e, f3c7756d (yes), 53f5e415 (yes), 2f726111,
31b4fed8, 2738dcfa (yes), 21a1dab0, d74b53c7, 5165ec5a, 90379e8e, 82c3fe73,
efbe34ec, 8bad3303, 21853219, b8c562a1, d5ac2d6f, 937f7571 (yes), 64be7ddf,
adb22ff1, 099eb715, f573fbfd, 2cb4766e, 20588fbf (yes). UI, icon, version,
updater and README commits: 248516a1, b6a81b31 (yes), 81bbccd5 (yes),
18e0290d, 04836fa0 (yes), d4586321 (yes), 64db4ff0.

Recommendation for ticket 02:

1. Option 1, single commits on `master`: the local chain 1838cd88,
   be6c7690, d9f38adf, dfb60519, dccbcc6e, 604ea188, 64c694ae, 533f063f,
   feb21710 (applies cleanly as a chain). Then the Plus commits that apply:
   14521654, 3776c598, 6ecce835, d2ad1479 + 6db85423, fc6048a4, 9f4c139f,
   7531b756 + e5f0e276. Port by hand only when a measurement needs them: the
   pipeline key change from bf961274, the refused-pipeline list from
   abe2d71c, the lwmutex fast path from 4fce7453.
2. Option 2, base on `plus/all-enhancements` (branched from 5b040dc4). A
   merge of `master` conflicts in 10 files. The local chain conflicts in 15
   more. Keep from 1838cd88 only the present mode, cache key and Turnip
   detection parts. Keep the Vulkan 1.3 and profile chain, and merge its FSR
   fallback with c7555734.
3. Both options: drop 541499ee and the merge 7526bdba.
4. On the stock driver, Plus runs Double Buffer (14521654), so its Page
   Table and Native Buffer work does not apply there. Plus commits mix
   several fixes, add log lines in code that runs per draw, add a 10 ms
   watchdog thread, and turn on a 3-core thread limit that is not measured.
5. Recommended: option 1. Also build a Plus release APK and measure it
   against the ticket 06 baseline. Move to option 2 only if it gives fewer
   freezes and no FPS loss.
6. Ticket 03 (validation default) is written on branch
   `pocket-s/03-validation-layer-default`. It overlaps the validation part
   of 1838cd88.
