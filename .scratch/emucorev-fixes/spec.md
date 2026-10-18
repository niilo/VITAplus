# Emulation fixes found in EmuCoreV

EmuCoreV (`/home/pielinen/src/EmuCoreV`, package `com.sbro.emucorev`) is an
Android PlayStation Vita emulator. Its README states that it uses a Vita3K
core, and the source confirms it: `app/src/main/cpp/vita3k/` is a Vita3K
checkout with app-specific changes around it.

That matters because a fix in EmuCoreV is a fix in the same codebase, not a
second emulator that solved the same problem differently. Each candidate was
read as a diff, then checked against this repository.

Nothing in this directory has been implemented. These are tickets only.

## Method

83 commits in EmuCoreV touch its bundled core
(`git log -- app/src/main/cpp/vita3k`). Commits that only change its Compose
UI, its installer flows or its own save-state feature were dropped, because
this repository has no equivalent feature to break.

For the rest, the question was asked in this order:

1. What does the Vita hardware or the documented API contract require?
2. Does this repository contain the same code on the same path?
3. If it does, is the same input reaching it?

Every file:line in the tickets was read in this repository before it was
written down. Where a ticket states a defect, that line was confirmed by
reading the code, not inferred from a commit subject.

## Confirmed present in this repository

Seventeen tickets, grouped by subsystem.

| Area | Tickets |
| --- | --- |
| `vita3k/mem` | 01, 02 |
| Sync primitives | 03, 04, 05 |
| Threads and process exit | 06, 07 |
| NGS | 08, 09 |
| Modules and codecs | 10, 11, 12 |
| Renderer | 13, 14, 15, 16, 18 |
| Shader | 17 |

The shortest are one line: 01 adds a missing `end()` comparison, 05 changes
one map name, 09 adds one assignment. The longest are 15 and 18, and both
need a decision recorded in their own ticket before any code is written.

## Reached on the Android build, or reachable

These are not theoretical. The Android package targets SDK 35
(`android/app/build.gradle:59`), arm64 is the release ABI, and the emulator
runs on an Adreno part with either the stock Qualcomm driver or Mesa Turnip.

- 13 is enabled unconditionally: `renderer/src/transfer.cpp:45` sets
  `use_gpu_depth_transfer = true` and `:196` gates only on the Vulkan
  backend. Both drivers take that path.
- 15 is on by default: `renderer/src/vulkan/renderer.cpp:877` reduces
  `preserve_f16_nan_as_u16` to `independentBlend`, which both drivers report.
- 14 is on by default: the Android default mapping is `page-table`
  (`vita3k/config/include/config/config.h:24`).
- 11 and 12 are in the guest-facing path and run whenever a game calls them.

Tickets 02, 06, 17 and 18 need a specific configuration to bite: an NGS
allocation that splits a block, a shutdown that races a thread creation, a
non-memory-mapped shader, or `memory-mapping: disabled`. The code is wrong
in every case; the frequency differs.

## Checked and not taken

Recorded so the same comparison is not repeated later.

- **Visibility query reset** (`646400d9`). Already fixed here at
  `renderer/src/vulkan/context.cpp:625-628`, which keeps the extra element
  that `sync_state.cpp:241` allocates. EmuCoreV's change is the same
  one-liner. `.scratch/pocket-s-android13/issues/07-query-pool-flags.md`
  records the original fix.
- **`SceLibLocation` in the init list** (`6d87f43a`). Present at
  `vita3k/modules/include/modules/library_init_list.inc:18-23`.
- **The Plus game fixes in `7c7b450c`.** All 16 emulation changes are
  already here: `SceAudiodecUser.cpp:316-322`, `SceVideodecUser.cpp:626-630`,
  `SceNgs.cpp:389-412`, `load_self.cpp:127-178`, `packages/src/sfo.cpp`,
  `mem/src/mem.cpp:168-201`, the `surface_cache.cpp` raw-cast and scene-rect
  work, `texture.cpp`, `SceSysmem.cpp`. Two need a check by hand: the
  texture-buffer slot resolution in `shader/src/translator/data.cpp` and the
  raw-cast rebuild in `shader/src/translator/texture.cpp`.
- **`CURRENT_VERSION 15 to 16`** (`82de10e5`). Already 16 at
  `vita3k/shader/include/shader/spirv_recompiler.h:40`.
- **The condvar mutex reacquisition** (`a853ed5b`, `5e4f7c58`). Present at
  `vita3k/kernel/src/sync_primitives.cpp:1459-1481`.
- **NGS state flags on play, pause, init and rack release** (`7c7b450c`).
  Present at `vita3k/modules/SceNgsUser/SceNgs.cpp:389-412`, `:929-931`,
  `:981-987`, `:1130-1136`, `:1166-1169`.
- **Thread state released before the delete condition is signalled**
  (`d50bee99`). Not present here. `vita3k/kernel/src/kernel.cpp:73` and
  `:86-91` hold the local `ThreadStatePtr` while `thread_deleted_cond` is
  notified. Folded into ticket 06, which rewrites the same function.
- **Save-state work** (`0a0ddcba`, `c853fa56`, `1f609cd6`, `be541417`,
  `9cb6a593`, `0127152e`, `4bd70ca7`, `58c76c65`). This repository has no
  save states. `grep` for `savestate` under `vita3k/` returns nothing.
- **App plumbing** (`1aa67c78`, `f8e39b33`, `3ab58501`, `205300f0`,
  `72b84dfe`, `ade08286`, `faab1609`, `8f730643`, `0691e7fd`, `44663f7d`,
  `65d1ad79`, `514af5cc`). Kotlin, Gradle and installer changes with no
  counterpart.
- **Game profiles and the Game DB** (`6e5866b5`, `c6e695bf`). This
  repository has no Game DB: `config/include/config/` holds `config.h`,
  `functions.h`, `settings.h`, `state.h` and `version.h`, and a search for
  `game-db.xml` finds nothing. `high-accuracy` exists as a setting
  (`config.h:159`) and defaults to `true`, so the Borderlands 2 profile is a
  per-title config entry, not a new subsystem.
- **The kernel wait rewrite** (`41bada11`). Renames `ThreadStatus` and
  replaces the wait layer wholesale. About 1600 lines of
  `sync_primitives.cpp`, plus new headers. Not worth the conflict.

## Verified on 2026-10-13

Every row below was opened and read in this repository. The reported line
numbers had drifted; the corrected ones are given. No ticket has been written
for any of them yet.

| EmuCoreV | Verdict | Corrected location | Finding |
| --- | --- | --- | --- |
| `a853ed5b` #2 | **Drop** | `kernel/src/load_self.cpp:618` and `:623` | The two offset expressions are equal for every SELF this repository builds. Its own repacker writes `segment_info.offset = HEADER_LEN + phdr.p_offset` (`packages/src/sce_utils.cpp:1068`) with `header_len = HEADER_LENGTH` (`sce_types.h:30`), so `:618` and `:623` agree. Needs a hardware reference before anyone acts. |
| `a853ed5b` #4 | **Confirmed** | `modules/SceGxm/SceGxm.cpp:955` | `display_queue.pop()` at `:956` runs before `run_guest_function` at `:971`. `Queue::wait_empty()` (`queue.h:118`) and the push gate at `:2437` only see the queue, so `sceGxmDisplayQueueFinish` (`:2458`) can return while the callback still runs. |
| `a853ed5b` #5 | **Confirmed** | `modules/SceGxm/SceGxm.cpp:2889-2892` | `assert(context)` then `RET_ERROR(SCE_GXM_ERROR_INVALID_THREAD)` for a null context. Wrong error code, and the assert compiles out in release. |
| `a853ed5b` #6 | **Partial** | `modules/SceNgsUser/SceNgs.cpp:320-324` | The null check is present. The range check is not: `Ptr::get` (`mem/include/mem/ptr.h:52`) returns `page_table[addr/KiB(4)] + addr` for any non-zero address. `Ptr::valid` exists at `:71` and the neighbouring `sceNgsPatchGetRouting` uses it (`:255`). |
| `5e4f7c58` #1 | **Partial, claim inverted** | `camera/src/camera.cpp:343-361` | It does not refuse the frame. It returns early only when the luma buffer is short (`:335-342`), then writes `width*height` to Y and `(width/2)*height` to U and V with no clamp against `sizeUBase`/`sizeVBase`. The defect is an unbounded write, not a refusal. |
| `5e4f7c58` #6 | **Confirmed, eight sites** | `kernel/src/sync_primitives.cpp:256, 457, 687, 994, 1174, 1389, 1598, 2091` | `strlen(name)` with no null check in every `*_create`. The name arrives unchecked from the guest, for example `_sceKernelCreateSema` (`SceThreadmgr.cpp:134`). This is the only row on this list that can kill the host process outright. |
| `5e4f7c58` #7 | **Confirmed** | `kernel/src/thread.cpp:861-870` | `resume` sets `single_stepping` and `external_suspend` but never `suspend_requested`. The sibling paths do clear it (`resume_if_suspended` `:877`, `resume_from_world` `:906`). Reachable from `KernelState::resume_threads` (`kernel.cpp:242`) and from the early return at `thread.cpp:850`. |
| `5e4f7c58` #8 | **Confirmed** | `modules/SceAppUtil/SceAppUtil.cpp:249, 259, 260, 309, 324` | `slot->id` goes straight into a filename through `construct_slotparam_path` (`:245`) with no range check, `slot` is dereferenced without `valid()`, and `:259` reads `files[0].mode` without checking `files` or `fileNum`. |
| `5e4f7c58` #10 | **Drop** | `modules/SceCommonDialog/SceCommonDialog.cpp:1147` and six sites | `selected_save` is emulator-side, written only by the overlay (`overlay/src/common_dialog.cpp:1586`, `:1635`) from an index bounded by `slot_list_size`, while the vectors are sized to `slotListSize` (`SceCommonDialog.cpp:1357`). `sceSaveDataDialogGetResult` already guards with `empty()` (`:1274`). Not reachable from the current UI. |
| `82de10e5` #3 | **Confirmed** | `modules/SceKernelThreadMgr/SceThreadmgr.cpp:1308-1311` | `UNIMPLEMENTED()`, and the sibling `sceKernelOpenSema` at `:1303` is implemented. Already written up as `.scratch/plus-fixes/issues/05-sce-kernel-open-simple-event-unimplemented.md`. |
| `82de10e5` #5 | **Confirmed** | `kernel/src/sync_primitives.cpp:1355-1373` | `semaphore_cancel` runs the wake loop at `:1355-1369` and only then checks `semaphore->val < setCount` at `:1371`. On the error path every waiter has been woken and `pNumWaitThreads` is never written. |
| `82de10e5` #7, #8 | **Confirmed** | `renderer/include/renderer/gxm_types.h:39-57`, `modules/SceGxm/SceGxm.cpp:1929`, `:2047` | `SceGxmColorSurface` has no clip fields, and the file's own comment at `:40` says the content does not match the hardware. Both accessors are stubs and never write the out-pointers, so a guest scissor on a colour surface is discarded. Visual only. |
| `82de10e5` #9 | **Confirmed** | `modules/SceNetCtl/SceNetCtl.cpp:160-164` and `:290-292` | `adhoc_thread` writes `emuenv.common_dialog.status` and `.result` with no `common_dialog.mutex` (`dialog/include/dialog/state.h:104`) and no `type` check, although the netctl state above it at `:151` does take `emuenv.netctl.mutex`. Every other writer locks. |
| `82de10e5` #11 | **Confirmed** | `renderer/src/vulkan/gxm_to_vulkan.cpp:396-399` and `:701-704` | `U1U5U5U5` routes through `translate_swizzle4_abgr`, but the format is fixed to `eA1R5G5B5UnormPack16` at `:459`, which is alpha-first. A standing TODO is already at `:457`. |
| `82de10e5` #13 | **Confirmed** | `shader/src/usse_utilities.cpp:486-502` | No clamp on the input vector anywhere in `make_f16_pack_func`, and `GLSLstd450PackHalf2x16` at `:495` is the only half-pack in the tree. Already written up as `.scratch/plus-fixes/issues/14-f16-pack-overflows-to-inf.md`. |
| `82de10e5` #15 | **Confirmed, latent** | `vkutil/src/objects.cpp:318-324` | `DestroyQueue::destroy_objects` handles `ImageView`, `Sampler`, `Fence`, `Semaphore` and `Framebuffer`, and everything else falls to `LOG_ERROR("Unknown object type {}")`. No call site adds a `DescriptorPool` today, so nothing leaks yet. |
| `d50bee99` #4 | **Confirmed** | `gxm/include/gxm/state.h:79`, `modules/SceGxm/SceGxm.cpp:2125-2127` | One `immediate_context` slot, and a second immediate context gets `SCE_GXM_ERROR_ALREADY_INITIALIZED`. Deferred contexts have a map at `:80`; immediate ones do not. |
| `d50bee99` #5, #6 | **Confirmed** | `kernel/src/load_self.cpp:313-322` | `unload_func_exports` erases the plain-NID entry at `:321` with no owner test, while the binding loop at `:325` does check `library_nid`. `load_func_exports` at `:288` uses `emplace`, so the first exporter wins and its unload drops the entry for both modules. |
| `3c0fbd61` codec half | **Confirmed** | `codec/src/aac.cpp:50-53` and `:106-112` | Input and output rate are both fixed to the constructor argument, so no rate conversion can happen. `swr_convert`'s output capacity is `frame->nb_samples`, its return value is only asserted at `:107` and never used, and `:111` reports the input count to the guest regardless of what was written. |
| `3c0fbd61` compressor | **Confirmed** | `vita3k/ngs/src/modules/compressor.cpp:150-151` | Already recorded under "Related work below". |

Ranked by what each would do to a running game: the eight `strlen` sites
(`5e4f7c58` #6), the display queue pop (`a853ed5b` #4), `semaphore_cancel`
(`82de10e5` #5), the AAC resampler, `ThreadState::resume`, `unload_func_exports`,
the adhoc dialog write, the camera chroma write, then the visual-only ones.

## Related work already tracked elsewhere

- **`.scratch/plus-fixes/`** is a later comparison, against Vita3K-Plus itself,
  which VITAplus merged at `20588fbf`. Plus has the answer for six of the
  tickets here: 05, 10, 14, 15, 17 and 18. Those six carry a dated note under
  their `## Comments` with the commit to use. Read that directory first; for
  those six it holds a reference this one does not.
- `.scratch/log-review-2026-09-29/issues/09-ngs-key-off-release.md` is the
  parent of tickets 08 and 09. Its plan steps 1 and 3 are what EmuCoreV
  implements.
- `.scratch/log-review-2026-09-29/issues/05-ngs-compressor.md` records plan
  step 3, "attack and release smoothing of the gain", as done. The detector
  is smoothed. The gain is not. Confirmed at `compressor.cpp:145-151`.
- `.scratch/pocket-s-android13/issues/06-framebuffer-fetch-path.md` and
  `issues/15-attachment-layouts.md` both want an unused input attachment
  removed to keep the pass in GMEM. Ticket 15 adds one. They cannot both be
  done without a decision.
- Ticket 16 is latent unless `async-pipeline-compilation` is turned on. It
  defaults to `false` (`config/include/config/config.h:176`). Ticket 15
  would arm it.

## Comments