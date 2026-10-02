# 27: Fit the descriptors and uniform buffers to the A7xx limits

Status: open
Type: research
Label: ready-for-human
Blocked by: 04

## Question

Does any pipeline on this device cross a hardware descriptor or uniform limit,
and does the bandwidth compression stay on for the color surfaces?

## The limits, from the vendor

Ticket 01 measured `gpu_model = AdrenoA32`, and Mesa maps that to the `FDA32`
bucket. Every number below was measured on Adreno 640, 730 or 750, not on this
part, so treat them as a shape rather than a threshold. Re-measure rather than
assume, and record where each number came from.

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
   color surfaces. **UBWC is a Qualcomm driver feature and Turnip has no
   equivalent**, so this step has no answer on the measured driver. Keep it as a
   stock-driver compatibility note and say so in the answer, rather than leaving
   it looking open.

   What survives on Turnip is the same question as ticket 06's new step 2: the
   mutable format flag on the F16 surfaces (`surface_cache.cpp:756-757`) and
   `eSampled` together with `eInputAttachment` on one image (`:770`) are the two
   features this code uses that a tile-based driver may pay for. Do not measure
   that here; ticket 06 owns it and this ticket should cross-reference it.

## Acceptance

- The per-pipeline counts, with the pipelines that cross a limit named by
  shader hash.
- A measured result for the sampler merging, or `Status: rejected`.
- For the stock driver only: a statement about UBWC for the color surfaces. Say
  plainly that the measured driver has no UBWC, so the item does not apply to
  it.

## Answer

## Comments
