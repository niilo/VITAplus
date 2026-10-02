# 15: Narrow the color attachment layout

Status: open
Type: task
Label: ready-for-human
Blocked by: 05, 06

## Problem

`PipelineCache::retrieve_render_pass`
(`vita3k/renderer/src/vulkan/pipeline_cache.cpp:626-632`) uses
`vk::ImageLayout::eGeneral` as the initial layout for every non-transient color
target and as the final layout for every color target. The usage flags on that
image include `eSampled` and `eInputAttachment` at the same time
(`vita3k/renderer/src/vulkan/surface_cache.cpp:768-771`).

Qualcomm's best-practices document says that image layout specificity matters
more on Adreno than on most other GPUs, and that a layout which is too general
prevents the driver from keeping the target in tile memory.

This changes the layout of every non-transient color attachment, so it can
change the picture. The layout also interacts with the framebuffer fetch path
from ticket 06.

## Risk

This can produce a wrong picture, not only a slower one. The final layout is
what a later pass sees when it samples the target or uses it as an input
attachment. A final layout of `eColorAttachmentOptimal` without a matching
`pipelineBarrier` before each of those reads is a validation error, and on a
driver that does not check it, a stale read.

Run every choice once with the validation layer on before running the protocol.
The APK carries it and it is off by default (`vita3k/config/include/config/config.h:144`).
A run that reports any validation error or warning about a layout is not a
result. Record the message and drop that choice.

## Steps

1. Change three things together, because all three must agree or the render
   pass is invalid:
   - the `AttachmentDescription` initial and final layout
     (`pipeline_cache.cpp:631-632`);
   - `vk::AttachmentReference color_refs[2]` (`pipeline_cache.cpp:603-606`),
     which is hardcoded to `eGeneral` and is the layout the subpass input
     attachment is read with;
   - nothing else. Do not change the image usage flags in the same commit. The
     usages decide which layouts are legal, so changing both at once hides
     which one broke.
2. Put the choice behind a temporary config value with three values: both
   `eGeneral` (today), `eColorAttachmentOptimal` initial and `eGeneral` final,
   and both `eColorAttachmentOptimal`.
3. For each choice, run once with the validation layer on. Record every message.
4. Measure A/B/A per choice on each benchmark title. Record FPS, the frame
   interval 99th percentile, GPU busy percentage and GPU clock. GPU busy
   percentage is the point: if it falls and FPS rises, the driver is doing less
   work for the same picture.
5. Take one screenshot at each of three fixed save-slot moments per title in A
   and again in B, with `tools/android/device.sh screenshot`, and compare them
   under `tmp/pocket-s-android13/15/`. Write down the first visible difference,
   or `no difference seen`.
6. Load and store ops. `pipeline_cache.cpp:629-630` uses `eLoad` and `eStore`
   for every non-transient color attachment. The vendor guidance is to qualify
   with `LOAD_OP_CLEAR` and `LOAD_OP_DONT_CARE` early. Where the first draw
   covers the whole target, try `eClear` for loadOp behind the same config
   value, and where the target is never read after the pass, try `eDontCare`
   for storeOp. Record which titles each applies to. A loadOp of `eClear` where
   the first draw does not cover the whole target shows as garbage in the
   uncovered part, so check a screenshot before recording a verdict.
7. The depth and stencil attachment already uses narrow layouts:
   `initialLayout` is `eDepthStencilReadOnlyOptimal` when either aspect is
   loaded and `eUndefined` otherwise, and `finalLayout` is
   `eDepthStencilReadOnlyOptimal` (`pipeline_cache.cpp:653-654`). There is
   nothing to narrow there. Record that and stop.

## What not to do

Do not remove `eSampled` or `eInputAttachment` from the image usage in this
ticket, and do not remove the mutable format flag from the F16 path
(`surface_cache.cpp:756-757`). The vendor notes that
`VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT`, aliased images and several other
features disable UBWC, the hardware bandwidth compression. Ticket 27 owns that
question.

## Answer

## Comments
