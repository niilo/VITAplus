# Selected Vita3K-Plus fixes

Reviewed on 2026-10-20 for the Pocket S 3x/60 FPS work and continued Linux
x86-64 support. This is a selective port, with device testing still required.

Source branch: [nckstwrt/Vita3K-Plus/all-enhancements](https://github.com/nckstwrt/Vita3K-Plus/tree/a86b5f93db43e4c4640ad1527e2a4695aad0f35b),
pinned to `a86b5f93db43e4c4640ad1527e2a4695aad0f35b`.
Destination before these changes: `d71bfc1f8c18425c8ae8b48de525016bf8f8e3ea`.
Common ancestor: `20588fbf52370356b8e5e8ca52ed3dd59ba8ddad`.

`git rev-list --left-right --count HEAD...a86b5f93` returned `134 96` before
this port. These are commit ancestry counts. Several Plus changes already
have independent implementations here; 96 does not mean 96 missing fixes.
The earlier comparison in [plus-fixes/spec.md](../plus-fixes/spec.md) ends
at an older Plus snapshot. Its claim that nothing needs fetching is stale.

## Adopted changes

| Source | Change used here | Reason and limits |
| --- | --- | --- |
| [766e0d2b](https://github.com/nckstwrt/Vita3K-Plus/commit/766e0d2bf3c6041e2480bcc65f62444a72b55fa3) | Read variadic doubles as IEEE bit patterns from the ARM ABI's integer argument locations; pass the guest va_list address to sceClibVprintf. | The Jak collection fix contains these general ABI corrections. Keep our existing 8-byte va_list alignment. Real CPU register and stack tests reproduce the old double conversion failure. This does not implement the whole Jak commit. |
| [97c8e91f](https://github.com/nckstwrt/Vita3K-Plus/commit/97c8e91f5ee96ca31af7753240676eeecca68c25) | Translate both packed IMAD16 lanes, preserve scalar INDEX/internal destinations, and honour source-2 subtraction. | Relevant to Ratchet & Clank: Full Frontal Assault vertex calculations. The user confirmed both Trilogy and Full Frontal Assault as targets. Trilogy needs separate validation. Cache version 19 avoids reusing our older version 17 and Plus version 18 products. |
| [93639d0b](https://github.com/nckstwrt/Vita3K-Plus/commit/93639d0baa5b4a9ea2a2bd68a14375bc607e837a) | Respect hardware CSC plane pitch, crop JPEG output without moving the input planes, implement sceJpegEncoderSetValidRegion, and select YVU texture plane order. | Relevant to Uncharted photos and planar video/texture colours. Preserve our ARGB/ABGR byte-order fix. Real conversion and JPEG round-trip tests check colour and crop behaviour. GPU read-back is a separate part of the source commit and remains pending. |
| [ca3ad408](https://github.com/nckstwrt/Vita3K-Plus/commit/ca3ad408) | Validate saved Vulkan cache headers against vendor, device and pipelineCacheUUID. | Use the Plus compatibility check and strengthen length checks. Parse before inserting keys; retain the current cache if the driver rejects the replacement. Tests cover truncation, overflow, header lengths and changed identities. This is groundwork for reliable precompilation, not pipeline recipe preparation. |
| [3576a576](https://github.com/nckstwrt/Vita3K-Plus/commit/3576a576383a84c7f0e7681895bbaa048c92b20b), [6c568f82](https://github.com/nckstwrt/Vita3K-Plus/commit/6c568f82f6c21e0edf8d75ebadc4654243698662) | Write Android shader diagnostic dumps only at debug logging level. Move shader-generation messages to debug. | Avoid diagnostic filesystem work during shader compilation while retaining the reusable shader cache. Desktop diagnostic dumps remain available. Debug logging deliberately enables the dumps again. No FPS benefit has been measured. |
| [80f888e2](https://github.com/nckstwrt/Vita3K-Plus/commit/80f888e268fdd33094da7fc4efe83b9334ffe139) | Preserve destination bytes when pre-touching file-read buffers. | The Dariusburst fix addresses general short-read and EOF corruption. Both new regression tests fail on the destination branch's old implementation. Memory write traps are still triggered. |
| [9e855eaa](https://github.com/nckstwrt/Vita3K-Plus/commit/9e855eaa38337c6b0ebc95b1149d69b87a08c06a) | Use the generic command allocator for sceGxmFinish. | The synchronization command must not depend on a context's guest-owned command storage. Preserve renderer abort checks and the Vulkan request-queue drain. Do not adopt the unrelated 65,536-slot command-ring increase without measurements. |

## Existing work retained

- The optional renderer event wait, GPU timestamps, compile timings and queue
  telemetry from phase 1 remain in place. Their settings and defaults do not change.
- NGS voices still use guest addresses rather than persistent host pointers.
  Keep our key-off release handling and scheduler tests. Killzone's pooled-voice
  envelope reset is already present in EnvelopeModule::on_key_on. The newer Plus
  audio commit also changes host priorities, exclusives and thread timing; its
  title is not evidence that all those changes should be adopted together.
- Keep the current Uncharted JPEG byte-order interpretation, shader precision,
  display queue ordering, device feature checks and Steam Deck paths.
- Do not change dependency submodule checkouts, signing keys, app identity or
  release workflows as part of this review.

## Relevant work needing another implementation and validation pass

| Source | Finding and next step |
| --- | --- |
| b00f61ae, pipeline compilation | Warms program pairs using default or frequently seen surface states, rather than a complete recorded pipeline recipe. It also moves cache saving to a worker and changes texture-content reuse and ring allocation. The code tests a few alternative translation hints to infer shader independence. That is not a proof for all possible draw state. Review shader variant keys, worker access, lifetime and cache synchronization before adoption. Prefer the full recipe capture and preparation project already specified in the 3x plan. |
| 766e0d2b, 388d0e82, remaining Jak changes | GXM waiting uses a process-global generation counter, thread-local history and a 5 ms timeout. JPEG writes wait up to 20 ms on open scenes. Memory pinning and NGS object placement also change. Validate the wait contract, guest scheduling and memory transitions separately; the ABI port alone does not establish that Jak gameplay works. |
| 928e5917, vertex alignment | Relevant to both Adreno and AMD. It changes the vertex binding descriptions and the data packing together, including misaligned attribute offsets. Its source also assumes the newer stream snapshot and ring management code. Port and test the complete matching binding/packing path, including mapped streams and zero strides. Taking only its stride predicate would leave inconsistent pipeline and buffer layouts. |
| 93639d0b, GPU photo read-back | The source selects recent cached surfaces and synchronizes them for CPU reads. Check range overlap, CPU/GPU ownership and transfer completion on both mapping modes. Colour/pitch/crop fixes do not establish that the first Uncharted photo is no longer black. |
| 7a8b219b, 4d41056c, 1cacdf99, audio and scheduling | Changes include host nice values, guest core affinity, timing floors and synchronization contracts. Preserve the existing tested waits and guest-pointer handling. Reproduce any missing Killzone audio failure and port its narrow cause first. Host scheduling changes require Pocket S and Deck measurements. |
| a86b5f93, streamed audio | Null-buffer termination depends on the newer initial-buffer and reached-data state machines. Port that complete contract with refill/callback/loop tests before changing the current player and ATRAC9 paths. |
| f02b3c6a through cef53700, double buffer | Snapshot, synchronization and shader stream analysis form a dependency chain. Check the current Page Table and Double Buffer paths and retain guest-visible ordering. Earlier review notes explain why taking the first commit alone is wrong. |
| c00750d3, dd145806, Android platform handling | Android 17 negotiation and rotation handling are separate platform changes. They do not explain the Android 13 Pocket S performance target and are not prerequisites for this port. |
| Other title, UI, network and controller commits | Leave unrelated integration changes out of the performance branch. Revisit a specific commit when a target game reproduces its failure or the user requests that feature. |

No new WipEout 2048 or Sly 2 speedup has been established by this review.
No game-specific 30-to-60 FPS unlock is included.

## Device checks

Run the same checkpoints on Pocket S and Steam Deck. Record the actual driver,
mapping mode, resolution, output settings, frame timings and audio/rendering
correctness. Separate the first cache rebuild from warmed gameplay.

| Game | Required check after this port |
| --- | --- |
| Uncharted: Golden Abyss | First and later photos, hardware CSC and crop paths, channel order, save/load, Page Table versus Double Buffer. A black first photo may still need the deferred read-back work. |
| Jak and Daxter Collection | Each collection entry: boot, menus, JPEG animation and gameplay. The GXM wait and memory placement changes were not imported. |
| Ratchet & Clank Trilogy | Each collection entry: geometry and animation with rebuilt shaders, then a repeated 1x/3x scene. |
| Ratchet & Clank: Full Frontal Assault | Vertex geometry affected by packed IMAD16, signed operands and repeated gameplay after a cache restart. |
| Killzone Mercenary | Repeated weapon fire, voice reuse, audio transitions and sustained gameplay. Retain our NGS lifetime and release handling. |
| Sly 2: Band of Thieves; WipEout 2048 | Rendering/audio correctness, cold shader loading, warmed 3x frame budgets. No title-specific speedup is claimed. |

## Verification

The unpatched baseline was built and tested with the new ABI and EOF tests.
Two VargsTest cases and both FileReadTest cases failed on the old code. The
same focused suites pass with the fixes. Cache parser tests also pass.

The Linux x86-64 RelWithDebInfo build passed. All 103 C++ tests across eight
CTest entries passed, including real JPEG round trips and YVU conversion.
clang-format 22 checks passed on all 20 changed C++ files, and git diff --check
passed. Android ARM64 verification is pending.
Physical Pocket S and Steam Deck gameplay, GPU timing, power and thermals
remain unmeasured. This review does not establish 60 FPS at true 3x.

The GitHub APK tagged dev-pockets3x-p1-20261019 predates these changes.
It remains the phase 1 test build.
