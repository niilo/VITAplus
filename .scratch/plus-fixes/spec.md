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

## Found in the comparison, no ticket written

Reported from reading the diffs. The line numbers are what that reading
produced and **have not been confirmed by reading this repository**. Confirm
before writing a ticket.

| Commit | Reported location here | Claim |
| --- | --- | --- |
| `75c7b17a` | `modules/SceSysmem/SceSysmem.cpp:165` | Also subtracts `size` where the allocator adds `mappedSize`; the driver path at `SceSysmemForDriver.cpp:271` never removes the uid from `vm_blocks` |
| `75c7b17a` | `modules/SceGxm/SceGxm.cpp:4502` | `sceGxmSetUniformDataF` writes up to three floats past the requested range when `componentCount` is not a multiple of four |
| `75c7b17a` | `renderer/src/vulkan/pipeline_cache.cpp:502` | The pipeline and shader cache keys omit `output_register_format` and the surface base format |
| `129cc7b2` | `display/src/display.cpp:491` | `wait_vblank` iterates `vblank_callbacks` unlocked and runs guest code inside the iteration |
| `129cc7b2` | `renderer/src/vulkan/context.cpp:703` | A partial-rect `BufferSyncRequest` carries the whole surface size, so the partial write-back is dropped as not fully mapped |
| `129cc7b2` | `modules/SceAppUtil/SceAppUtil.cpp:251` | `sceAppUtilSaveDataDataRemove` reads `files[0]` when `fileNum` is 0 and does not check `files` |
| `129cc7b2` | `io/src/state_functions.cpp:67` | `FileStats::write` and `truncate` use a null `FILE*` when the host open failed, and `open_file` does not report it |
| `363285b8` | `kernel/src/sync_primitives.cpp:2437` | `msgpipe_delete` busy-waits on `remainingThreads`; the comment already says FIXME |
| `363285b8` | `ngs/src/rate_resampler.cpp:145` | The `SwrContext` is freed and recreated on every reset |
| `7451f9f4` | `renderer/src/vulkan/pipeline_cache.cpp:1073` | `compile_pipeline` and `precompile_shader` let an exception escape; a lost device reaches `std::terminate` |
| `7451f9f4` | `renderer/src/vulkan/pipeline_cache.cpp:522` | `retrieve_shader` does not clear its compiling sentinel if translation throws, so later draws spin forever |
| `7451f9f4` | `renderer/src/batch.cpp:326`, `screen_renderer.cpp:824` | The render loop and the swapchain rebuild do not survive a `vk::SystemError` |
| `7451f9f4` | `modules/SceGxm/SceGxm.cpp:3078` | `sceGxmInitialize` does not reset `display_entries_done` |
| `d4d7cdd7` | `kernel/src/kernel.cpp:191` | `create_thread` does not check `SDL_CreateSemaphore` or `SDL_CreateThread`, so a failed create blocks the caller forever |
| `d4d7cdd7` | `modules/SceKernelThreadMgr/SceThreadmgr.cpp:1084` | `sceKernelDelayThreadCB` does not service callbacks during the delay |
| `cdc40165` | `modules/SceNgsUser/SceNgs.cpp:271`, `:322` | `sceNgsPatchGetInfo` and `sceNgsPatchRemoveRouting` dereference a guest handle instead of finding the owning live voice |
| `cdc40165` | `ngs/src/ngs.cpp:483` | Releasing a rack leaves other racks' patches pointing at destructed voices |
| `cdc40165` | `ngs/src/ngs.cpp:339`, `ngs/src/scheduler.cpp:410` | `invoke_callback` and the deferred path use `kernel.get_thread()` without a null check |
| `cdc40165` | `ngs/src/modules/atrac9.cpp:110`, `player.cpp:223` | Guest buffer indices are tested only against `-1` before indexing a four-element array |
| `181aa153` | `modules/SceGxm/SceGxm.cpp:2869` | `sceGxmExecuteCommandList` splices the guest's command nodes into the render thread's queue, which frees them |
| `181aa153` | `modules/SceGxm/SceGxm.cpp:2562` | A precomputed state's texture count is not the bound, so the draw reads past its texture array |
| `181aa153` | `modules/SceGxm/SceGxm.cpp:2547` | `gxmDrawElementGeneral` does not set `last_precomputed`, so the next draw skips rebinding the programs |
| `7c52fc78` | `renderer/src/vulkan/surface_cache.cpp:645` | The cache only invalidates on a larger surface, so a shrink writes back at the cached size |
| `ca3ad408` | `renderer/src/vulkan/surface_cache.cpp:102` | `pack_rgba8_to_r4g4b4a4` puts R and A in the wrong nibbles and walks the stride instead of the width |
| `ca3ad408` | `renderer/src/vulkan/surface_cache.cpp:864` | A linear texture region is measured one stride too long |
| `ca3ad408` | `renderer/src/vulkan/pipeline_cache.cpp:319` | A cache file written for another GPU is fed to the driver as initial data |
| `24702197` | `renderer/src/vulkan/sync_state.cpp` | Turnip LRZ workaround, per-driver, needs a measurement on both drivers |
| `779751e5` | `mem/src/mem.cpp:280` | `alloc_aligned` frees only the front remnant, so the back one leaks per allocation |
| `779751e5` | `renderer/src/vulkan/pipeline_cache.cpp:301` | Compile workers drain the whole backlog at shutdown instead of abandoning it |
| `779751e5` | `shader/src/translator/branch_cond.cpp:403` | `VTEST` with write-back-enable does not write the ALU result to the destination |
| `987bfea4` | `renderer/src/vulkan/gxm_to_vulkan.cpp:396` | `U1U5U5U5` routes through the ABGR swizzle table for an alpha-first format |
| `44775966` | `modules/SceGxm/SceGxm.cpp:2114` | `sceGxmColorSurfaceSetScaleMode` is `UNIMPLEMENTED()` |
| `092f3db6` | `shader/src/translator/data.cpp:806` | A thread-buffer register offset is masked with `0xFFFF`, not offset by `0x1000000` |
| `e0f9d51a`, `24702197` | `renderer/src/vulkan/pipeline_cache.cpp:661` | The subpass-0 dependency omits `eInputAttachmentRead` |
| `39be7875` | `renderer/src/vulkan/texture.cpp:41` | A `U8U8U8U8` texture over a depth-stencil surface is not served from a GPU alias; a compute repack is missing |
| `96cdc106` | `renderer/src/vulkan/surface_cache.cpp` | Six colour surfaces rendered as a cube cannot be sampled; no cube surface path exists |
| `93639d0b` | `codec/src/mjpeg.cpp:81` | The YUV pitch cannot be padded, so `SCE_JPEGENC_PITCH_HW_CSC` is ignored |
| `93639d0b` | `modules/SceJpegEnc/SceJpegEncUser.cpp:77` | A HLE module reads guest memory that aliases a render target without syncing it first |
| `e3147337` | `shader/src/usse_utilities.cpp:1334` | A raw `VMOV` to the fragment output is always treated as not holding the declared type |
| `121ad4ea` | `ngs/src/modules/player.cpp:186` | A zero-byte player buffer stalls the chain instead of following `next_buffer_index` |
| `d5de4bed` | `renderer/src/vulkan/surface_cache.cpp:2518` | A float colour surface is written back with linear filtering |
| `b87189eb` | `shader/src/translator/alu.cpp:1236` | `sop2m` drops the lanes its write mask does not cover |
| `dec3399a` | `shader/src/translator/branch_cond.cpp:481` | `vtstmsk` on a float source returns 0.0 or 1.0 instead of a bit mask |
| `092f3db6`, `cef53700` | `renderer/src/vulkan/types.h:273`, `vkutil/include/vkutil/objects.h:93` | The vertex ring has no fence tracking, so a large scene overwrites data an in-flight submission reads |

## Comments