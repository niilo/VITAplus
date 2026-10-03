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

1. ~~Test scaled vertex attributes off on Turnip~~ — **drop this.** Scaled vertex
   attributes do not exist in Vulkan Turnip (`grep` for
   `scaled_vertex_attribute` over `src/freedreno/` returns nothing). It is a
   Gallium/GL feature, so it cannot explain a Vulkan-only difference against the
   stock driver. The `support_scaled_attribute_formats=true` difference in
   `log_gpu_configuration` comes from the GL side and is not relevant here.
2. **A740 UBWC flag hint A/B — this is now the top lead.** In upstream Mesa,
   `enable_tp_ubwc_flag_hint = True` is set for **FD735** and **FD740v3** (Quest 3)
   but **not** for our **FD740** (`0x43050A01`). It sets
   `TPL1_DBG_ECO_CNTL1.TP_UBWC_FLAG_HINT`, a *texture processor* bit that controls
   how UBWC-compressed data is decoded. For a render-feedback loop Turnip rewrites
   the input-attachment descriptor to tiled mode with **UBWC zeroed**
   (`tu_desc_set_ubwc<CHIP>(dst, 0)` in `tu_emit_input_attachments`), so a
   reader/writer disagreement about UBWC format would return zero exactly where
   the box is. Build a variant that forces the hint on and compare. See
   `Banners-Turnip/docs/A740_BLACK_BOX.md`.
3. Try another Turnip build (for example an official Mesa release) to see
   if it is a regression of this driver build.
4. Capture a frame on the device (RenderDoc for Android, or AGI) at the spot
   to find the draw that makes the box, and compare its shader output on
   Turnip and stock.
5. Look for the effect in the game: a head fade or a near-camera occluder
   is likely, because the box covers the head only when the camera is
   close.

The debug logs (transfers, surface reads) and the `preserve-f16-nan` setting
are on the branch `debug/transfer-log`, not on `plus-master`.

## Comments

2026-10-03 (later): **the UBWC-hint hypothesis is now falsified.** Built two
drivers from the same Mesa `8fc4981` differing only in
`enable_tp_ubwc_flag_hint` on the FD740 entry, and tested both on the device:

- `Turnip_a740-sr1.zip` — hint off → box present
- `Turnip_a740-ubwc-hint-ON.zip` — hint on → **box still present**

So `TP_UBWC_FLAG_HINT` is not the variable. The tiled/UBWC-zeroed descriptor
rewrite in `tu_emit_input_attachments` is still a real code path, but the flag
that looked like it controlled it does not change the outcome. Do not spend more
time on that knob.

That kills the cheapest-looking explanation for the stock-vs-Turnip difference.
What survives is that the difference is somewhere in *how the feedback read is
issued*, not in a decode-mode flag.

Also settled in this pass: `support_scaled_attribute_formats` is a red herring.
It does not exist anywhere in Vulkan Turnip (`grep` over `src/freedreno/` finds
nothing); it is a GL/Gallium concept, so it cannot explain a Vulkan-only
difference. Dropped from the next steps above.

Remaining leads, in order of cost:

1. **Capture the offending draw** on device (RenderDoc for Android, or AGI) and
   compare the input-attachment descriptor and shader output between Turnip and
   stock. This is now the *only* un-tried approach: every flag-level hypothesis
   has been tested and falsified (see the closing note below).
2. Emulator-side work, if a workaround is wanted rather than a fix: the three
   blending strategies (`direct_fragcolor` -> `support_shader_interlock` ->
   `support_texture_barrier`) are an all-or-nothing ladder, and the fallback
   makes the scene black. A graceful "keep the feature, avoid the feedback loop"
   path would need a copy-instead-of-feedback workaround.

### Closing note (2026-10-03, end of day)

Investigation **stopped at the user's direction**. Three hypotheses were built
and tested on the device, all falsified:

| # | Hypothesis | Test | Result |
| --- | --- | --- | --- |
| 1 | `enable_tp_ubwc_flag_hint` | same Mesa, flag off vs on | no effect |
| 2 | `support_scaled_attribute_formats` | source inspection | not a Vulkan Turnip feature |
| 3 | SUBPASS_FENCE replacing CACHE_INVALIDATE | pre-2026-09-04 driver (`2b6602eb`) | no effect |

The black box survived all three. Diagnosis is left open rather than guessed at;
`Banners-Turnip/docs/A740_BLACK_BOX.md` has the full record so the work is not
repeated. The two diagnostic drivers were removed from the device; the baseline
`Turnip_a740-sr1.zip` (Mesa `8fc4981`, current upstream) remains for normal use.

