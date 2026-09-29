# 15: Reduce CPU texture work, if the profile shows it

Status: open
Type: experiment
Label: ready-for-agent
Blocked by: 07

## Gate

Do this ticket only if ticket 07 shows texture decode, texture upload or
fence waits among the top 10 functions of a title, or more than 5% of the
render thread time. Otherwise set `Status: rejected` with the numbers.

## Context

- PVRTC is always decoded on the CPU, because Adreno has no PVRTC
  (`texture/cache.cpp:460-473`).
- The texture cache hashes or protects textures (`cache.cpp:645-790`).
  Vulkan always uses protection (`vulkan/renderer.cpp:983`).
- Uploads use 16 staging buffers. When all are used in one scene, the
  code waits on a fence (`vulkan/texture.cpp:204,225`). There is also a
  per-frame wait at `vulkan/context.cpp:579`.

## Steps

1. From the profile, name the part that costs time: decode, swizzle,
   hashing, or fence wait.
2. Fence waits: test 32 staging buffers instead of 16 (`types.h:33`),
   with A/B/A.
3. PVRTC decode: count PVRTC textures per scene and decode time. If decode
   is large, write a design note for a cache of decoded textures on disk
   or a compute decoder, as a new ticket. Do not build it in this ticket.

## Output

Under `## Answer`: the measured cost of each part, the result of step 2,
and any new ticket created.

## Answer
