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

## Found in the comparison, no ticket written

Each of these was reported by reading the EmuCoreV diffs. The line numbers
below are the ones that diff work produced and **have not been confirmed by
reading this repository**. Confirm before writing a ticket.

| EmuCoreV | Reported location here | Claim |
| --- | --- | --- |
| `a853ed5b` #2 | `kernel/src/load_self.cpp:616` | SELF segment bytes read at `header_len + p_offset`; the decompress path at `:623` uses `seg_infos[i].offset` |
| `a853ed5b` #4 | `modules/SceGxm/SceGxm.cpp:955` | display queue entry popped before the guest callback runs |
| `a853ed5b` #5, #6 | `modules/SceGxm/SceGxm.cpp:2887`, `modules/SceNgsUser/SceNgs.cpp:322` | `sceGxmFinish` returns `INVALID_THREAD` for a null context; `sceNgsPatchRemoveRouting` dereferences a guest handle |
| `5e4f7c58` #1 | `camera/src/camera.cpp:335` | YUV422 conversion refuses the whole frame when only the chroma planes are short |
| `5e4f7c58` #6, #7, #8 | `kernel/src/sync_primitives.cpp:1173`, `kernel/src/thread.cpp:861`, `modules/SceAppUtil/SceAppUtil.cpp:249` | `strlen` on a null name in the `*_create` functions; `resume` does not clear `suspend_requested`; no savedata slot id bound |
| `5e4f7c58` #10 | `modules/SceCommonDialog/SceCommonDialog.cpp:1147` | `selected_save` not compared against the vector size |
| `82de10e5` #3, #5, #7, #8 | `modules/SceKernelThreadMgr/SceThreadmgr.cpp:1308`, `kernel/src/sync_primitives.cpp:1371`, `renderer/include/renderer/gxm_types.h:39` | `sceKernelOpenSimpleEvent` unimplemented; `semaphore_cancel` checks the count after waking; `SceGxmColorSurface` has no clip fields |
| `82de10e5` #9 | `modules/SceNetCtl/SceNetCtl.cpp:160` | adhoc thread writes the common dialog status without the mutex or a dialog type check |
| `82de10e5` #11, #13, #15 | `renderer/src/vulkan/gxm_to_vulkan.cpp:396`, `shader/src/usse_utilities.cpp:492`, `vkutil/src/objects.cpp:318` | `U1U5U5U5` routed through the ABGR swizzle table; `packHalf2x16` overflows to Inf instead of saturating; no `HANDLE_DESTROY(DescriptorPool)` |
| `d50bee99` #4, #5, #6 | `gxm/include/gxm/state.h:79`, `kernel/src/load_self.cpp:313` | one immediate GXM context only; `unload_func_exports` erases plain NIDs another module still owns |
| `3c0fbd61` codec half | `codec/src/aac.cpp:49`, `codec/src/aac.cpp:102` | the resampler is configured once from the core rate, and `swr_convert` capacity is `frame->nb_samples` with the return value discarded. The 7-line compressor smoothing hunk is **confirmed**: `vita3k/ngs/src/modules/compressor.cpp:145-151` applies `target_gain` with no smoothing on the gain, although the detector envelope is smoothed at `:136` and `:140` |

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