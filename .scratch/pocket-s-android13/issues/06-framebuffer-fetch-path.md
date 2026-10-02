# 06: Make the framebuffer fetch path cheap on a tile renderer

Status: open
Type: research
Label: ready-for-human
Blocked by: 04

## Question

Vita3K emulates programmable blending by reading the color attachment the same
pass writes. It does that three ways. Which one is cheapest on Adreno, and can
a fourth way do better?

## The three ways today

1. `VK_EXT/ARM_rasterization_order_attachment_access`, when present. Chosen
   first (`renderer.cpp:858-863`), and it turns shader interlock off. **Ticket
   01 found it present on this device under Turnip**, so this is the path the
   device takes. Paths 2 and 3 below are not the ones in use, so measure them
   on the stock driver or not at all. On the stock driver the extension is not
   documented, so read ticket 00 for which driver the benchmark runs on.
2. Shader interlock, when `high-accuracy` is on and the extension is present.
   Costs an end and a begin of the render pass on every draw that changes
   fetch state (`scene.cpp:463`, `:473`). On a tile renderer that is a store
   and a load of the tile.
3. Subpass input (`direct_fragcolor`). A single subpass with one input
   attachment bound to the color attachment (`pipeline_cache.cpp:616-624`), plus
   a `vkCmdPipelineBarrier` per draw (`scene.cpp:458`) because the write and the
   read are in the same subpass.

Turning all three off is what `disable-programmable-blending` does
(`renderer.cpp:1080`). NetherSX2-Turnip documents that turning framebuffer fetch
off fixed a PS2 game on Snapdragon.

## What ticket 00 already measured, on this device

This is now the largest performance difference measured on the device, and it
is one path, not three. Uncharted in gameplay, camera moving, resolution 2:

| Path in use | FPS | p99 interval |
| --- | --- | --- |
| `direct_fragcolor`, stock driver | 5.76 | 187.91 ms |
| rasterization order access, Turnip | 29.96 | 43.43 ms |

The draws per scene are the same, 39.91 against 40.04, so it is the same work.
The stock driver's feature log says why it has no choice:

```
FeatureState: support_shader_interlock=false support_texture_barrier=false
              direct_fragcolor=true programmable_blending=true
```

So on this device `direct_fragcolor` costs about 5 times what the correct path
costs. Steps 2 to 4 of this ticket are now about that one number.

`high-accuracy: true` does not help the stock driver. Measured: 6.39 FPS
against 5.76, inside the noise of two runs, and `direct_fragcolor` is still
true. Do not spend a run on it again.

## Risk

Every path here decides whether programmable blending is emulated. A wrong
implementation does not crash. It draws blended geometry with the wrong source
colour, which looks like a transparency bug and is easy to miss in one
screenshot.

Steps 2 to 4 are behind config values with today's behaviour as the default, so
a failed experiment costs a config value and not a merge. Take three screenshots
at fixed moments per title per configuration, with
`tools/android/device.sh screenshot`, and compare them before recording a
verdict.

## What the hardware says

- Qualcomm: early-Z and LRZ are disabled by blending, stencil, a depth compare
  change, masked writes, discard, reading the framebuffer, and a framebuffer
  fetch. Adreno rejects occluded pixels at up to 4 times the drawn fill rate.
  So the cost of any of these paths is not only the fetch. It is the loss of
  early-Z for that draw.
- Qualcomm: subpass merging needs more than one subpass, an input target, each
  resolve used in exactly one subpass, `srcAccessMask` without `SHADER_WRITE`
  and `dstAccessMask` without `SHADER_READ`. The code has one subpass, so the
  merge does not apply today. Qualcomm puts the gain at over 10% of frame time
  when the conditions hold.
- Turnip exposes `VK_KHR_dynamic_rendering_local_read`, which is the portable
  way to read an attachment written earlier in the same dynamic rendering scope.
  It does not need a render pass object, an input attachment descriptor, or the
  per-draw self-dependency barrier that path 3 needs.

## Steps

1. Count how many draws per frame on each benchmark title take each path. Add
   counters behind the `perf-log` setting from ticket 02 and read them from
   `scenes.csv`. A game with 40 draws and no programmable blending gets none of
   this and can be skipped.
2. Measure the per-draw barrier and the per-draw render pass restart directly.
   Make each one conditional on a temporary config value, run the A/B/A
   protocol, and record the numbers even if the result is that they are free.
3. Test `VK_KHR_dynamic_rendering_local_read` as a fourth path behind a
   temporary config value. It is a large change. Only build it if steps 1 and 2
   show the fetch path costs more than 5% of frame time on any title.
   This path needs an extension the plan has not confirmed on either driver.
   Turnip exposes it. The stock Qualcomm driver is a 2023 build and a 2024
   extension is unlikely to be in it, so read ticket 01 step 4 first. If the
   stock driver does not have it, the answer for the stock driver is no, and
   that goes in the record.
4. Test whether a two-subpass render pass makes the merge rule apply, behind a
   temporary config value, if the current pass has an input attachment and
   could be split. Record the result either way.
5. Record the LRZ effect: run each benchmark with `disable-programmable-blending`
   on and off and read `gpu_busy_percentage` and GPU clock, not only FPS. If
   FPS rises and GPU busy rises too, the GPU is doing more work per frame for
   the same picture, and that belongs in the record.

## Acceptance

- A table of draws per frame and path per title, and a measured cost for the
  barrier and for the render pass restart.
- A verdict on `disable-programmable-blending` for this device, with the
  visible-artifact risk written down.

## Answer

## Comments
