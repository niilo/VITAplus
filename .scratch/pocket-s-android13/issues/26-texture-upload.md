# 26: Measure the texture upload path

Status: open
Type: research
Label: ready-for-human
Blocked by: 04

## Question

How much of a frame goes into uploading textures, and what limits it?

## Context

- `NB_TEXTURE_STAGING_BUFFERS` is 16
  (`vita3k/renderer/include/renderer/vulkan/types.h:35`). It sizes a fixed
  array, so changing the count at run time needs a container that can grow.
  That is more work than putting a number behind a config value, so read the
  array before deciding how to do it.
- `VKTextureCache::prepare_staging_buffer`
  (`vita3k/renderer/src/vulkan/texture.cpp:209`) waits on a fence when the
  next staging buffer is still in flight, with
  `std::numeric_limits<uint64_t>::max()` as the timeout.
- `support_pvrt` is read from the device format features at
  `vita3k/renderer/src/vulkan/texture.cpp:334` and defaults to `false`
  (`vita3k/renderer/include/renderer/texture_cache.h:143`). When it is false,
  PVRTC is decoded on the CPU by `vita3k/renderer/src/texture/pvrt-dec.cpp`.
  Read the value from `vita3k.log` rather than assuming, because a GPU path
  exists.
- The Vulkan texture cache always uses the protecting path.
  `hashless-texture-cache` is not read from `Config` anywhere, because
  `vita3k/renderer/src/vulkan/renderer.cpp:1141` passes a hardcoded `true` to
  `texture_cache.init`.
- The texture cache hashes guest texture bytes on the renderer thread, with a
  per-scene memo (`vita3k/renderer/src/texture/cache.cpp`).

## Steps

1. Split renderer thread time into decode, format swizzle, hashing and fence
   wait, per title. Two passes, because one report cannot give all four:
   - a normal call-graph profile, `simpleperf record --app <pkg> -g`, gives
     decode, swizzle and hashing as three symbols;
   - `simpleperf record --app <pkg> -e task-clock:u --trace-offcpu` gives the
     time the thread spent not running, which is the fence wait.
   Name which of the four is largest. This decides whether the rest of the
   ticket is worth anything.
2. Write to `scenes.csv` from ticket 02: bytes uploaded per scene, the number
   of staging buffer fence waits, and the total time blocked on them.
3. Fence waits: put `NB_TEXTURE_STAGING_BUFFERS` behind a temporary config
   value, default 16, and run A/B/A at 16 and 32. Check first that 32 does not
   push peak memory past what ticket 01 step 5 recorded for this device.
4. Hashing: wire `hashless-texture-cache` to the Vulkan backend behind a
   temporary config value, default off, and run A/B/A. A wrong picture here
   shows as a stale or a wrong texture, and it does not crash, so take
   screenshots at three fixed moments per title per configuration before
   recording a verdict.
5. PVRTC decode: report the share from step 1. If it is under 2% of renderer
   thread time, set `Status: rejected` for that part and say so.

## Acceptance

- The four-way split of texture time per title, from a profile, with the
  largest one named.
- A measured result for the staging buffer count and for the hashless cache,
  or `Status: rejected` with the numbers for each.
- The bytes uploaded per scene per title, which also tells the next plan
  whether the write-back path in ticket 05 is worth another look.

## Answer

## Comments
