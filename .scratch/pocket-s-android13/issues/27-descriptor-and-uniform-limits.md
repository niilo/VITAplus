# 27: Fit the descriptors and uniform buffers to the A7xx limits

Status: open
Type: research
Label: ready-for-human
Blocked by: 04

## Question

Does any pipeline on this device cross a hardware descriptor or uniform limit,
and does the bandwidth compression stay on for the color surfaces?

## The limits, from the vendor

Qualcomm documents the A7xx hardware limits as multiples of 16, not the values
the driver reports:

- more than 16 unique uniform buffers in one pipeline,
- more than 16 unique textures plus storage buffers in one pipeline,
- more than 16 unique samplers in one pipeline,

each cause a fill rate penalty. The sum of all uniform buffers used by one
shader should stay under 7372 bytes, and the buffer indices should be static,
because beyond that the driver can only map the accessed parts.

Turnip reports `maxPerStageDescriptorSamplers` and the rest as 16777216, so
nothing in the app stops it crossing 16. `minUniformBufferOffsetAlignment` is 64
and `maxUniformBufferRange` is 64 KiB.

## Context in the code

- `vkCmdBindDescriptorSets` is called with four sets and 2 or 4 dynamic offsets
  (`vita3k/renderer/src/vulkan/scene.cpp:273`).
- `vkUpdateDescriptorSets` for the render target set binds the color attachment
  and a raw alias, in one write of two descriptors
  (`vita3k/renderer/src/vulkan/context.cpp:537-541`).
- Texture descriptor sets are updated only when the texture count changes
  (`scene.cpp:244`, `:258`).
- The sampler cache holds `min(maxSamplerAllocationCount / 2, 512)` entries
  (`vita3k/renderer/src/vulkan/texture.cpp:313-318`).
- The color surface image is created with `eSampled` and `eInputAttachment`
  together (`vita3k/renderer/src/vulkan/surface_cache.cpp:768-771`), and the
  F16 path adds `eMutableFormat` (`surface_cache.cpp:756-757`).

## Steps

1. Count, per compiled pipeline, the number of unique samplers, unique
   textures, unique storage buffers and unique uniform buffers, and the sum of
   the uniform buffer sizes. Add the counters behind the `perf-log` setting
   from ticket 02. Report the maximum and the histogram, not only the maximum,
   because a few pipelines at the limit is a different problem from every
   pipeline at the limit.
2. Report how many pipelines cross 16 in any category and how many have a
   uniform buffer sum above 7372 bytes. Give the shader hash of each one that
   crosses, so it can be found again.
3. For each pipeline that crosses a limit, reduce it. The first thing to try is
   merging samplers that share a state, since `VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND`
   is supported on Turnip and lets unused slots stay empty.
4. Run A/B/A on the titles that had a crossing pipeline. Record FPS, the frame
   interval 99th percentile, GPU busy percentage and GPU clock.
5. UBWC: log whether the driver keeps the bandwidth compression on for the
   color surfaces. The driver decides this, so read it from the driver log or
   from a counter if one is exposed. Then test the two features the vendor
   document names as UBWC disablers and that this code uses: the mutable format
   flag on the F16 surfaces, and the combination of `eSampled` with
   `eInputAttachment` on one image. Measure before and after, and check the
   picture on an F16 title.

## Acceptance

- The per-pipeline counts, with the pipelines that cross a limit named by
  shader hash.
- A measured result for the sampler merging, or `Status: rejected`.
- A statement about UBWC for the color surfaces: on or off, and what the app can
  do about it.

## Answer

## Comments
