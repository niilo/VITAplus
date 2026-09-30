# 07: Jak and Daxter: The Precursor Legacy (in the collection) renders dark: terrain, sky and water are black, 18 FPS

Status: open
Type: task
Label: ready-for-agent

## Symptom

Found by the user on 2026-09-30 on the Ayaneo Pocket S (Adreno 740, stock
driver, release APK with the fixes up to `901695f7`). After X in the start menu
(issue 05), Jak and Daxter Collection (PCSA00080) loads Jak 1 and plays. The
user reports: "a few more black screens and incorrect rendering".

The screenshots (two, 6 seconds apart, `FPS: 18`) show a scene that moves (the
eggs bob) with these parts:

- Visible, textured and lit: pink eggs, two chests, grass tufts, rocks.
- Black: the ground, the terrain between the objects, the sky, the water (if
  any). The objects float in black.
- Not seen: Jak and Daxter, the HUD, the camera fade.

The picture looks like the same fault as issue 05: some draws produce
black and others work. It may also be the fault of
`.scratch/log-review-2026-09-29/issues/11-uncharted-dull-colors.md` (colors
that are too dark).

## Evidence from `vita3k.log` (the Jak 1 session)

- No crash. No `[CRASH]`. Normal errors only: missing `SlotParam_0.bin`, missing
  `TROPUSR.DAT`, NID `0xE87D1777` (`SceSblACMgrForDriver`) not found.
- `TTY: Texture streaming complete (18.69 MB, 0.50 sec)`, `Remaining
  available VRAM 18.22 MB`.
- `[cmd_handle_transfer_copy]: transfer_copy: no surface sync for
  src=0x6312BB40 -> dst=0x630FDB40 fmt=0x60000 64x352`.
- `[format_supports_linear_filter]: format D32SfloatS8Uint lacks linear-filter
  support: samplers for it will use nearest`. The game samples a depth
  texture.
- `sceGxmMidSceneFlush` is stubbed ("Flags ignored").
- 102 pipelines compile in about 4 minutes (`stock-Adreno` path). The frame rate
  is 18 while the scene is static, so the compile work does not explain it
  alone. Note: the first seconds after a level load will stutter.
- `FRAGOUT` lines: 37 fragment programs with `output_in_declared_format=false`,
  7 with `true`.
- Settings at the time: `memory-mapping: page-table`, `resolution-multiplier: 2`,
  `screen-filter: Bilinear`, `v-sync: true`, Vulkan, texture cache on.

## Next steps

1. Find which draws are black. Capture a frame in the Jak 1 scene (RenderDoc or
   AGI). Note the shader, the blend state, the depth state and the textures of
   the black draws (terrain, sky).
2. Check the depth texture path: a draw that samples `D32SfloatS8Uint` may be the
   sky or the fog. Test with nearest versus linear, and read the shader.
3. Check the `transfer_copy` without surface sync: the copy source may be a
   render target that was not synced.
4. Test the settings in the Jak 1 scene (only the menu was tested in issue 05):
   `disable-surface-sync`, `high-accuracy`, `force-full-precision`,
   `disable-programmable-blending`, `disable-raster-order`,
   `memory-mapping` (double-buffer, external-host), `resolution-multiplier: 1`.
   Change one at a time and use `tools/android/device.sh config-set`.
5. Compare with the Plus app after the `vargs.h` fix is applied to it, or with
   Vita3K upstream, to see if the picture is the same.
6. Find the cause of the 18 FPS: a perf-log run (`setting perf-log`) and
   `tools/android/device.sh pull-perf`.
7. Ask the user when the "few more black screens" happen (for example, at level
   loading or at cutscenes), and add the moments here.

## Comments
