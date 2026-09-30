# 08: Black box over Drake's head in Uncharted on Turnip

Status: open
Type: task
Label: needs-info

## Symptom

Uncharted: Golden Abyss (PCSA00029), Ayaneo Pocket S (Adreno 740). When the
camera is close to Drake and looks up at him, a black rectangle covers his
head. It is aligned with the screen and flickers (7 of 8 screenshots, one
clean). Screenshots from 2026-09-29 are in the session scratchpad
(`auto/blink`, `auto/blink2`).

## What we know

- Vita3K-Plus 20588fbf (the Plus app) shows the same box. So it is not
  caused by our changes on `plus-master`.
- It shows at resolution multiplier 1 and 2.
- It shows with page-table and double-buffer memory mapping.
- **The stock Qualcomm driver shows no box.** The Balemuni Turnip driver
  (Mesa 26.3.0-devel) shows it. So it depends on the driver.
- Uncharted draws its lighting with programmable blending (the shader reads
  the pixel color already in the framebuffer, `direct_fragcolor=true`,
  through a subpass input). With `disable-programmable-blending: true` the
  box is gone, but all objects are black (only the sky is visible). So
  programmable blending is needed, and the box is an area where the read
  gives 0 or the draw result is lost.
- The game does no `sceGxmTransferCopy` or downscale during the box (a log
  of the first 48 transfers had none).
- The only special surface reads (a 64-bit F16 surface 0x60A9FEC0 read as a
  32-bit texture at the second half of each texel) happen at game start,
  not during the box.

## What was tested (all still show the box on Turnip)

| Test | How |
| --- | --- |
| Texture viewport off | `high-accuracy: true` |
| No raw F16 attachment | `preserve-f16-nan: false` (setting added on `debug/transfer-log`) |
| No rasterization order access | `disable-raster-order: true` (log: "disabled by config") |
| 32-bit shader math | `force-full-precision: true` (features_mask 0x1CDC) |
| Turnip LRZ off | `tu-debug: nolrz` (log: "TU_DEBUG set to 'nolrz'") |
| Turnip safe modes together | `tu-debug: nolrz,sysmem,noubwc,flushall,syncdraw` |

Differences between the drivers in `log_gpu_configuration`: Turnip has
`support_rasterized_order_access=true` (stock false) and
`support_scaled_attribute_formats=true` (stock false). The first one is
ruled out by the test above. The second one is not tested yet.

## Next steps

1. Test scaled vertex attributes off on Turnip (needs a setting or a build
   that forces `support_scaled_vertex_attribute = false`).
2. Try another Turnip build (for example an official Mesa release) to see
   if it is a regression of this driver build.
3. Capture a frame on the device (RenderDoc for Android, or AGI) at the spot
   to find the draw that makes the box, and compare its shader output on
   Turnip and stock.
4. Look for the effect in the game: a head fade or a near-camera occluder
   is likely, because the box covers the head only when the camera is
   close.

The debug logs (transfers, surface reads) and the `preserve-f16-nan`
setting are on the branch `debug/transfer-log`, not on `plus-master`.

## Comments
