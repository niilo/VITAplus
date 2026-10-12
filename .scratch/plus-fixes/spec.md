# Emulation fixes available in Vita3K-Plus

Vita3K-Plus (`https://github.com/nckstwrt/Vita3K-Plus`) is the fork VITAplus
was built on top of. `.scratch/plus-base/spec.md` records the move: the base
became `plus/all-enhancements` at `20588fbf`, and this repository's own work
went on top of it.

That move happened. It is not a plan. `git merge-base HEAD
remotes/plus/all-enhancements` is `20588fbf`, which is the commit named in that
spec, and VITAplus has 104 commits on top of it.

What that leaves is the other direction: **51 commits that Plus gained after
`20588fbf` and that VITAplus has never taken.** A local checkout at
`/home/pielinen/src/Vita3K-Plus` is at `e2331f80`, which is exactly what
`remotes/plus/all-enhancements` points at in this repository, so the comparison
below uses current data and nothing needs fetching.

Nothing in this directory has been implemented. These are tickets only.

## Method

The 51 commits are read with `git -C /home/pielinen/src/VITAplus show <hash>`.
For each, the question was asked in this order:

1. What does the Vita hardware or the documented API contract require?
2. Does this repository contain the same code on the same path?
3. If it does, is the same input reaching it?

Every file:line below was read in this repository before it was written down.

## Where the 51 commits come from

| Range | Origin | Commits |
| --- | --- | --- |
| `75c7b17a` to `abca8776` | Vita3K-nx, NaGaa95's emulator, imported by Plus | 14 |
| `d5de4bed` to `e2331f80` | Plus's own per-title and general fixes | 37 |

The nx block is four bulk imports of 26, 35, 19 and 8 files. Those four
commits carry most of the general correctness work and most of the risk. The
37 later commits are mostly per-title, but several of them, named after a game,
turn out to be general fixes.

## Confirmed present in this repository

22 tickets.

| Area | Tickets |
| --- | --- |
| `vita3k/kernel` and the display interface | 01 to 07 |
| `vita3k/ngs` | 08 |
| `vita3k/shader` | 09 to 14 |
| `vita3k/codec` | 15 |
| `vita3k/ctrl` | 16, 17 |
| `vita3k/io` | 18, 19 |
| `vita3k/config`, `vita3k/renderer` | 20 to 22 |

## These answer existing tickets

Four tickets in `.scratch/emucorev-fixes/` were written against EmuCoreV
without knowing about Plus. Plus has the answer for each. They have been
revised to point at the commit.

| Ticket | Plus commit | Note |
| --- | --- | --- |
| 05 `simple_event_delete` wrong map | `987bfea4` | The same one-word change |
| 10 `sceAudiodecPartlyDecode` offset | `e2331f80` | The same one-line change |
| 15 raw half-float subpass fetch | `24702197` | The whole fix, seven files |
| 17 uniform copy past the SA bank | `ca3ad408` | The exact line the ticket quotes |
| 14 partial surface write-back | `e530c4e8` | First half only |
| 18 vertex stride alignment | `cef53700` | Supersedes what the ticket planned |

Ticket 18 changed the most. It was written to take `35cbde8d`, which is on
`remotes/plus/all-enhancements` and was not merged. `cef53700` replaces that
commit's `stride % 4` test with the rule Vulkan actually states: each vertex
attribute address, `stride * index + offset`, must be a multiple of the
component size of the attribute's own format. That test returns true less
often, so fewer streams get repacked.

`35cbde8d` also cannot be taken alone. It relies on a lazy
`vertex_stream_ring_buffer.create()`, and
`vita3k/renderer/src/vulkan/creation.cpp:62` only creates that buffer when
memory mapping is off, so a restrided stream under memory mapping would
allocate against a null handle.

## Reached on the Android build

- Tickets 01, 02, 03, 04, 05, 07, 08, 15, 16 and 18 run whenever the guest
  calls the function.
- Tickets 09 to 14 are in the shader translator and run on every shader that
  uses the affected instruction.
- Ticket 22 runs on every frame.
- Tickets 19, 20 and 21 are load-time and configuration.

## Do not take

- **`3f1859ae` display queue.** It moves the display-queue pop to after the
  guest flip callback for every mapping method except Double Buffer, and adds a
  compensating wait in `sceGxmDisplayQueueFinish`. This repository pops before
  the callback in all cases (`vita3k/modules/SceGxm/SceGxm.cpp:955`) and does
  not need the compensation. Turnip runs Page Table on the measured device, so
  a literal port removes the early release from the fast driver.
- **`997654b3` compressor.** This repository's compressor is a deliberate
  rewrite with a resolved ticket behind it
  (`.scratch/log-review-2026-09-29/issues/05-ngs-compressor.md`). Plus's hunk
  is one line against an older implementation.
- **`10ff6e86` wake gate.** It adds up to 50 ms of artificial delay per wake on
  the contended path, on a device whose frame rate is measured.
- **`ccd2e203` guest code patch.** It rewrites 58 bytes of one library by exact
  pattern. This repository has no `guest_code_fixes` subsystem, and the same
  class of bug belongs in the kernel wait layer.
- **`abca8776`.** Logging only. No behaviour change.
- **`ad93d9c7` Java device poll.** The Kotlin method it calls does not exist in
  this repository, so the C++ half alone is a silent no-op.
- **The double-buffer cluster** `f02b3c6a`, `ed03ee36`, `a67e20ee`, `092f3db6`,
  `cef53700`. It is a chain in ancestry order. `f02b3c6a` alone copies vertex
  streams at draw time and returns a pointer into the snapshot ring, so the
  renderer can read bytes the guest has already overwritten; `ed03ee36` deletes
  that. `a67e20ee` copies 16 KiB of slack for a size heuristic and `092f3db6`
  replaces it with static analysis. Take from `cef53700` backwards or not at
  all. Only `cef53700`'s `needs_restride` is a ticket, and it is ticket 18.

## Blocked

**`ca3ad408` does not link.** It calls `Atrac9DecodeBounded` at
`vita3k/codec/src/atrac9.cpp:94`. That function does not exist in either
repository, and `external/LibAtrac9` has only `Atrac9Decode`. The Plus checkout
does not initialise that submodule, so the failure was never seen there.

Take the hunks, not the commit. The uniform-copy hunk is ticket 17. The
underlying defect is real: `InitBitReaderCxt` takes no length, so the last
frame of a short superframe reads past the guest buffer. Fixing that needs the
bounded decoder written in the submodule first, and this repository cannot take
a submodule pointer from Plus.

## Wrong for this device

**`e0f9d51a`** gates a pipeline-change barrier on
`(deviceID >> 24) == 6`. The device log
(`tmp/gameplay/turnip-gameplay/vita3k.log`) reports
`Adreno (TM) 740 ... device 0x43050A00`, so the shift is `0x43`. A literal port
compiles and does nothing. It would also add a barrier to every pipeline switch
on the driver that reaches 30 FPS, so it needs a measurement either way.

## Verified on 2026-10-13

Every row below was opened and read in this repository. Reported line numbers
had drifted; the corrected ones are given. No ticket has been written for any of
them yet.

Three corrections to the table this replaces. `renderer/src/vulkan/batch.cpp`
is `renderer/src/batch.cpp`, and `renderer/src/vulkan/types.h` is
`renderer/include/renderer/vulkan/types.h`. The `092f3db6` row on
`data.cpp:806` was inverted: this repository already subtracts `0x1000000`, and
`092f3db6` changes that to a `0xFFFF` mask, so the missing work is the mask plus
a `thread_buffer_f32_count` clamp, not the offset.

| Commit | Verdict | Corrected location | Finding |
| --- | --- | --- | --- |
| `75c7b17a` | **Confirmed** | `modules/SceSysmem/SceSysmemForDriver.cpp:289` | The driver free path erases from `blocks` only, so the uid stays in `vm_blocks` (added at `SceSysmem.cpp:112`) and `sceKernelSyncVMDomain` then invalidates JIT on freed pages. The `mappedSize` half is `.scratch/plus-fixes/issues/07`, already written. |
| `75c7b17a` | **Confirmed** | `modules/SceGxm/SceGxm.cpp:4504-4513` | `component_to_copy_remain_per_elem` is never clamped to `component_left_to_copy`, so the first iteration copies a whole element and both pointers advance past the buffers. Guest-controlled `componentCount`. |
| `75c7b17a` | **Partial** | `renderer/src/vulkan/pipeline_cache.cpp:501-508` and `:1426-1445` | `output_register_format` is in neither key. The surface base format is already in the pipeline key: `color_base_format` sits at `renderer/include/renderer/types.h:106`, before `vertex_streams`, and `hash_pipeline_record` does not zero it. Only the shader key is missing it. |
| `129cc7b2` | **Confirmed** | `display/src/display.cpp:490-498` | No `display.mutex`, although register and unregister take it (`SceDisplay.cpp:218`, `:229`), and `Callback::execute` runs guest code inside the iteration. This is the second half of `.scratch/plus-fixes/issues/01`. |
| `129cc7b2` | **Confirmed** | `renderer/src/vulkan/context.cpp:703-708` | `location` is the rect origin and `size` is the whole surface, so the check at `:138` rejects the request and the partial write-back is dropped as not fully mapped. Already step 5 of `.scratch/emucorev-fixes/issues/14`. |
| `129cc7b2` | **Confirmed, not a Plus delta** | `modules/SceAppUtil/SceAppUtil.cpp:259` | `files[0].mode` is read with no `files` check and no `fileNum > 0` test. The function is identical in Plus, so `129cc7b2` is not the commit that fixes it. Belongs to an upstream bug list. |
| `129cc7b2` | **Confirmed** | `io/src/state_functions.cpp:67-80` and `io/src/io.cpp:403-411` | `can_write_file()` tests mode bits only (`io/include/io/state.h:51`), and `create_shared_file` returns an empty `FilePtr` on `fopen` failure (`io/include/io/filesystem.h:39`). `open_file` hands back a valid fd regardless, so `fwrite` and `ftruncate` get a null `FILE*`. |
| `363285b8` | **Confirmed** | `kernel/src/sync_primitives.cpp:2450-2451` | `while (std::atomic_load(&msgpipe->remainingThreads) != 0) // FIXME busy loop bad`. Spins only while threads drain. |
| `363285b8` | **Confirmed** | `ngs/src/rate_resampler.cpp:145-152` | Every `needs_reset` frees and rebuilds the `SwrContext` and reruns filter design. This repository already counts creations at `:66` with an `[NGSRATE]` log, so it is measurable rather than speculative. |
| `7451f9f4` | **Partial** | `renderer/src/vulkan/pipeline_cache.cpp:1565-1596` | `compile_pipeline` does catch `vk::SystemError` at `:1218` and `:1246`. `precompile_shader` has no handler at all, and `createShaderModule` at `:1589` runs on the guest thread. |
| `7451f9f4` | **Confirmed, high** | `renderer/src/vulkan/pipeline_cache.cpp:510`, `:523-535`, `:557-564` | The compiling sentinel is `~0ULL` at `:510`, set at `:535`, and waiters spin on it at `:529`. Translation and module creation at `:557` and `:564` are unguarded, so a throw leaves the sentinel forever and every later draw of that shader spins. An unrecoverable hang. |
| `7451f9f4` | **Confirmed** | `renderer/src/batch.cpp:326-371`, `renderer/src/vulkan/screen_renderer.cpp:824-856` | Neither the render loop nor the swapchain rebuild has a `try`. Only `renderer.cpp:1860`, `:2383` and `pipeline_cache.cpp` catch `vk::SystemError`. A lost device reaches `std::terminate`. Needs a policy for what happens after a device loss. |
| `7451f9f4` | **Confirmed** | `modules/SceGxm/SceGxm.cpp:3062-3082` | `sceGxmInitialize` resets the queue at `:3077` but never `display_entries_done`, which is only written at `:957`. A re-init after `sceGxmTerminate` inherits the old count. One line. |
| `d4d7cdd7` | **Confirmed** | `kernel/src/kernel.cpp:191-194` | Neither `SDL_CreateSemaphore` nor `SDL_CreateThread` is checked. A null semaphore is dereferenced; a null thread leaves the caller waiting on a semaphore nobody signals, forever. Same severity class as the `strlen` rows and the same size of fix. |
| `d4d7cdd7` | **Confirmed** | `modules/SceKernelThreadMgr/SceThreadmgr.cpp:1084-1094` | Callbacks are serviced once before the sleep in `delay_thread` at `:1071`, never during it. |
| `cdc40165` | **Confirmed** | `modules/SceNgsUser/SceNgs.cpp:271-272` and `:322` | Both read the guest `Patch` struct instead of searching live racks. At `:322` the address is not validated at all. |
| `cdc40165` | **Confirmed** | `ngs/src/ngs.cpp:483-507` | `release_rack` runs `v->~Voice()` and `rack->~Rack()` and erases only its own rack. Nothing sweeps `patch->dest` and `patch->source` held by voices in the other racks. The fix has to hold each source voice's `voice_mutex`, so it is not a one-liner. |
| `cdc40165` | **Confirmed** | `ngs/src/ngs.cpp:339-340`, `ngs/src/scheduler.cpp:410` | `kernel.get_thread()` returns `lock_and_find` (`kernel.cpp:167`), so null is possible, and both dereference it. Null deref on the audio thread. |
| `cdc40165` | **Confirmed** | `ngs/src/modules/atrac9.cpp:110`, `player.cpp:223` | `next_buffer_index` is a guest `SceInt16` tested only against `-1`, then used to index a four-element array (`atrac9.h:43`, `player.h:37`). The same file does bound-check elsewhere (`player.cpp:135`, `:148`), so the omission is local. One helper fixes both modules. |
| `181aa153` | **Confirmed, memory safety** | `modules/SceGxm/SceGxm.cpp:2871-2879` | Guest-owned `Command` nodes are linked into the immediate list that `sceGxmEndScene` at `:2845` submits. The deferred context still owns them and frees them in `free_command_list` at `:1075-1100` when the guest recycles the VDM range, before the render thread reaches them. `renderer::append_command_list` does not exist here. |
| `181aa153` | **Confirmed** | `modules/SceGxm/SceGxm.cpp:2560-2576` | No `frag_texture_count` clamp, so all 16 entries of a precomputed texture array are read when the program declares fewer. |
| `181aa153` | **Confirmed** | `modules/SceGxm/SceGxm.cpp:2547-2553`, function ends `:2634` | Only `sceGxmDrawPrecomputed` sets `last_precomputed`, at `:2777`. After a precomputed draw through the general path the next draw skips `set_program`. Same function as the previous two rows, so one ticket. |
| `7c52fc78` | **Confirmed** | `renderer/src/vulkan/surface_cache.cpp:645-653` | Only growth invalidates. `bound_width` and `bound_height` do not exist anywhere in the tree. Already step 6 of `.scratch/emucorev-fixes/issues/14`. |
| `ca3ad408` | **Confirmed** | `renderer/src/vulkan/surface_cache.cpp:91-105` | The nibbles are the wrong way round for `eR4G4B4A4UnormPack16`: R goes to bits 0-3 and A to 12-15. The loop also walks `pixel_stride` rather than `width`, and the function has no width parameter. |
| `ca3ad408` | **Confirmed** | `renderer/src/vulkan/surface_cache.cpp:864` | `stride_bytes * original_height` with no linear special case. A region starting part way along a row measures one stride longer than it spans. |
| `ca3ad408` | **Confirmed** | `renderer/src/vulkan/pipeline_cache.cpp:319-372` | No `VkPipelineCacheHeaderVersionOne` vendorID, deviceID or `pipelineCacheUUID` check, so a cache written for another GPU is passed to the driver as initial data. The overflow guard at `:346` is also missing. |
| `779751e5` | **Confirmed** | `mem/src/mem.cpp:280-306`, `:284`, `:294` | Only `remnant_front` is returned to the allocator. The tail past `align_addr + size` stays allocated, bounded by the alignment on every aligned allocation. |
| `779751e5` | **Confirmed** | `renderer/src/vulkan/pipeline_cache.cpp:286-314`, called from `:414-417` | The sentinel is appended after every pending request and workers use FIFO `wait_dequeue`, so each worker compiles the whole backlog before exiting. A device that discards the surface makes `cleanup()` wait seconds for nothing. |
| `779751e5` | **Confirmed** | `shader/src/translator/branch_cond.cpp:348`, `:403` | `test_wben` is declared at `:348` and never referenced in `vtst_impl`; the only other use is `if (!test_wben)` in `vtstmsk` at `:469`. The ALU result computed at `:305` is dropped and `:405` stores the predicate only. |
| `987bfea4` | **Confirmed** | `renderer/src/vulkan/gxm_to_vulkan.cpp:396-399` and `:701-704` | The format is fixed to `eA1R5G5B5UnormPack16` at `:459`, which is alpha-first, and both the colour and texture paths use `translate_swizzle4_abgr`. A TODO is already at `:457`. |
| `44775966` | **Confirmed** | `modules/SceGxm/SceGxm.cpp:2114-2118` | `UNIMPLEMENTED()`. `surface->downscale` is read at `context.cpp:219,229,238,651` and `surface_cache.cpp:1550,1587`, so the field matters and nothing can set it. |
| `092f3db6` | **Inverted, see above** | `shader/src/translator/data.cpp:806-811` | This repository already subtracts `0x1000000`. Missing is the `0xFFFF` mask and the `thread_buffer_f32_count` clamp, neither of which is in the tree. |
| `e0f9d51a`, `24702197` | **Confirmed** | `renderer/src/vulkan/pipeline_cache.cpp:661-668` | `dependencies[0]` has no `eInputAttachmentRead` and no `eShaderRead` in `dstAccessMask`, and lacks `eEarlyFragmentTests` in `srcStageMask` and `eFragmentShader` in `dstStageMask`. |
| `39be7875` | **Confirmed absent** | `renderer/src/vulkan/texture.cpp:37-60` | `is_depth_stencil_compatible_format` returns false for `U8U8U8U8`, with a comment at `:37` saying the depth alias is not implemented yet. No `depth_alias_pack.comp` exists in `shaders-builtin/vulkan/`, and there is no repack in `surface_cache.cpp`. A missing feature, not a defect. |
| `96cdc106` | **Confirmed absent** | decision site is `renderer/src/vulkan/texture.cpp:145-152` | The comment there reads that surface-cache views are always 2D, so a cube texture must skip it. `retrieve_color_surfaces_as_cube` does not exist and `surface_cache.cpp` has no cube or face handling. The cube path at `texture.cpp:415-520` is the uploaded-texture route only. A missing feature. |
| `93639d0b` | **Confirmed** | `codec/src/mjpeg.cpp:79`, `:117-127` | No destination pitch parameter, and `dst_strides[]` is derived from `width`. `SCE_JPEGENC_PITCH_HW_CSC` exists as an enum at `codec/include/codec/types.h:40` and is referenced nowhere in the tree. |
| `93639d0b` | **Confirmed, feature absent** | `modules/SceJpegEnc/SceJpegEncUser.cpp:52-77` | The buffer is read with no flush. No `sync_guest_range`, no `sync_surfaces_for_cpu_read`, and no such `CommandOpcode`: `renderer/include/renderer/commands.h` ends at `MemoryUnmapFlush`, which is used only before unmapping (`SceSysmem.cpp:158`). |
| `e3147337` | **Confirmed** | `shader/src/usse_utilities.cpp:1334-1335` | `!raw_move && is_float_data_type(dest.type)` always stores false for a raw move. `store` has no parameter for the case, so this needs the per-word store-type tracking the commit adds. |
| `121ad4ea` | **Confirmed** | `ngs/src/modules/player.cpp:187-211` | A zero-length buffer breaks out of the loop or returns end-of-data. `next_buffer_index` is only read at `:223`, after `loop_count` expires, so the chain stalls instead of advancing. Fold into the buffer-index ticket. |
| `d5de4bed` | **Confirmed** | `renderer/src/vulkan/surface_cache.cpp:2518` | `sync_filter` has no case for `F32` or `F32F32`, so a float surface whose sync is not from the raw image is written back with `eLinear`, which averages the raw words. One line. |
| `b87189eb` | **Confirmed** | `shader/src/translator/alu.cpp:1236-1239` | No `finalize` before the store, and `store` compacts components into masked lanes (`usse_utilities.cpp:1517-1536`), so a non-contiguous `wmask` writes the wrong lanes. The same commit fixes two more spots in these files: `alu.cpp:263` indexes `ExtPredicate` out of range for VMAD2's 3-bit predicate, and `usse_program_analyzer.cpp:72-73` orders the VMAD2 case above VMAD. |
| `dec3399a` | **Confirmed** | `shader/src/translator/branch_cond.cpp:484-491`, `:529`, `:261-263` | The float case at `:484` sets `output_type = type_f32` and selects 0.0 or 1.0 instead of `0xFFFFFFFF`, while the integer branch at `:494-524` is correct. And `:261-263` still sends an unsigned sign test to component 2, which is `.scratch/plus-fixes/issues/12`. |
| `092f3db6`, `cef53700` | **Confirmed** | `renderer/include/renderer/vulkan/types.h:273-278`, `vkutil/include/vkutil/objects.h:93-99`, `vkutil/src/objects.cpp:204-213` | `vertex_stream_ring_buffer` has no marks, and `RingBuffer::allocate` at `:204` wraps with `if (cursor + data_size > capacity) cursor = 0;`. A scene that submits more vertex data than the ring holds overwrites bytes an in-flight submission still reads. |

Not checked: the `24702197` Turnip LRZ workaround in `sync_state.cpp`. It is a
per-driver behaviour change and needs a measurement on both drivers, which no
amount of reading answers.

### Three that are worth a ticket before the rest

Ranked by what each would do to a running game, across both specs.

1. `181aa153`, the command-list splice at `SceGxm.cpp:2871`, and the two
   adjacent rows at `:2560` and `:2547`. A use-after-free of guest-owned
   command nodes, and all three are in one function.
2. `7451f9f4`, the `retrieve_shader` sentinel at `pipeline_cache.cpp:510`. A
   throw leaves `~0ULL` in the map and every later draw of that shader spins on
   the render thread. A small `try` that nulls the slot closes an
   unrecoverable hang.
3. `d4d7cdd7`, the unchecked `SDL_CreateThread` at `kernel.cpp:191`, and the
   `strlen` on a null name in the eight `*_create` functions. Both are guest
   reachable and both are a couple of lines each.

Then `129cc7b2` for the null `FILE*`, `cdc40165` for the three NGS null
dereferences and the dangling cross-rack patches, `75c7b17a` for the uniform
copy overrun, and the shader translator rows last, because they are real but
their effect is per-title visuals and they need a picture comparison to
verify.

## Comments