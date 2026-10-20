# 14: A partly drawn surface is written back in full, and the row sync is unbounded

Status: claimed
Type: task
Label: needs-info

## Context

Two defects, both in the surface write-back path.

**The partial path is gated to macroblock sync.** At
`vita3k/renderer/src/vulkan/context.cpp:691`:

```cpp
if (render_target->has_macroblock_sync && state.res_multiplier != 1.0f
    && rendered_rect_x1 > rendered_rect_x0 && rendered_rect_y1 > rendered_rect_y0) {
```

A small linear colour surface that was only partly drawn takes the full
write-back path, which copies the whole surface into guest memory and clobbers
the part the CPU wrote in the untouched area. `ColorSurfaceCacheInfo`
(`vita3k/renderer/include/renderer/vulkan/surface_cache.h:93`) has no field for
a partial rectangle, and `note_scene_draw_rect`
(`vita3k/renderer/src/vulkan/surface_cache.cpp:560`) keeps only a 32-pixel
rounded union and returns early when the rectangle is empty, so the exact drawn
rectangle is not recorded.

`.scratch/game-sweep-2026-09-30/issues/07-jak1-renders-dark-terrain-sky-water-black.md`
logs a case of this shape: `transfer_copy: no surface sync for
src=0x6312BB40 -> dst=0x630FDB40 fmt=0x60000 64x352`.

**The row-wise sync handler has no bounds arithmetic.** At
`vita3k/renderer/src/vulkan/context.cpp:138`:

```cpp
if (mem_it == state.mapped_memories.end() || mem_it->first + mem_it->second.size < request.location + request.size) {
    ...
}
...
if (request.row_stride != 0) {
    uint8_t *dst = reinterpret_cast<uint8_t *>(Ptr<void>(request.location).get(mem));
    for (uint32_t row = 0; row < request.row_count; row++) {
        memcpy(dst, src, request.row_bytes);
        src += request.row_stride;
        dst += request.row_stride;
    }
}
```

`request.location + request.size` can overflow. `row_bytes`, `row_stride` and
`row_count` are never checked against the mapped range or against the
destination, so a row layout that does not describe the request reads past the
mapped buffer and writes past the destination. Both values come from the
renderer, so this is not guest-reachable today, but it is the handler that
guest memory is copied through.

EmuCoreV commit `43e8dc20` adds a self-contained `surface_sync.h` with
`intersect_surface_rects`, `surface_sync_rows_span` and a bounds-checked
`copy_surface_sync_rows`, and records the true scene rectangle.

## Plan

Vita3K-Plus commit `e530c4e8` implements step 2 and step 3. Use that commit's
surface-cache work as the reference for those two steps. Step 4 is not in it.

1. Port `surface_sync.h` as it stands. It has no Vulkan headers and no
   dependencies.
2. Record the exact drawn rectangle. `note_scene_draw_rect` must stop returning
   before it has stored anything, and the stored rectangle must be the one
   intersect is applied to, not a rounded union. Keep the 32-pixel union as
   well: `surface_sync_clamp_rt` uses it at `surface_cache.cpp:2437-2458`.
3. Re-derive the eligibility test against this repository rather than copying
   it. `is_small_writeback_surface` here (`surface_cache.cpp:76`) takes
   `(tiling, width, height)` and covers `Linear` or `Tiled`, where Plus's
   version is linear only. `perform_surface_sync` also has paths that Plus's
   predicate does not account for: `needs_copy_buffer` also covers
   `surface_sync_needs_f10_repack` and `se5_repack` (`:2334`, `:2375`), there
   is a `state.surface_sync_clamp_rt` clamp at `:2411`, and there is a
   `gpu_read_sync_only` path. Decide for each whether a partial write-back is
   correct or whether the existing "skip the write-back" decision at
   `surface_cache.cpp:2427` should stand. That is a policy choice, not a
   mechanical fix.
4. Add the bounds checks to the row loop: reject a request whose
   `location + size` overflows, whose `row_stride` is 0 with a non-zero
   `row_count`, or whose `row_bytes` exceeds `row_stride`, and clamp the
   number of rows to the mapped range.
5. Fix the producer. `context.cpp:703` passes `surface_info->total_bytes` as the
   request size while `rect_start` is already offset into it, so the handler's
   mapped-range check at `:138` rejects the request and the partial write-back
   is dropped as "not fully mapped". Plus sends
   `(rect_row_count - 1) * row_stride_bytes + rect_row_bytes`.
6. Clamp against the bound size, not the render target size. The cache at
   `surface_cache.cpp:647` only invalidates when the new surface is bigger, so a
   surface re-bound smaller keeps the larger entry, and `perform_surface_sync`
   then uses `original_width` and `original_height`: a guest heap overflow. Plus
   adds `bound_width` and `bound_height` to `ColorSurfaceCacheInfo` for this.
   The existing clamp uses `rendered_w` and `rendered_h`, which are the render
   target size and can be larger than the surface, so it does not catch this.
7. This path runs on the Android build by default: the mapping default is
   `page-table` (`vita3k/config/include/config/config.h:24`), so
   `context.cpp:680` takes it on every submit.
8. Measure after: `container/vita3k.sh android release`, install, and run
   `tools/android/gameplay_scene.sh` on Uncharted. Report FPS and device power
   with `tools/android/device_power_sample.sh`, because narrowing the copy can
   change both.

## Comments

### 2026-10-12: Vita3K-Plus has the first half

VITAplus merged Vita3K-Plus at `20588fbf`. `e530c4e8` is one of the 51 commits
since. It records `scene_x0/y0/x1/y1` before the early return in
`note_scene_draw_rect`, and `perform_surface_sync` intersects the sync rect
with it instead of returning `nullptr` when a small linear surface was not
covered in full.

Three things in this repository must survive the port, because Plus does not
have them: `surface_sync_clamp_rt` at `surface_cache.cpp:2411`, the
`needs_copy_buffer` guard, which is wider here because it also covers
`surface_sync_needs_f10_repack` and `surface_sync_needs_se5_repack`, and
`carry_raw_cast_in_unorm`. Dropping the `needs_copy_buffer` guard would put a
U2F10F10F10 or SE5F9M9M9 surface down the partial path and skip the repack.

Steps 4, 5 and 6 are not in that commit. `.scratch/plus-fixes/spec.md` has the
detail and the commit each came from.

### 2026-10-20: partial linear write-back and bounded row copies implemented

Commit 6ba65c00 adapts Plus e530c4e8 and 7c52fc78. Record the unscaled scene
bounds. Permit partial updates for eligible small linear surfaces. Keep the
render-target clamp, the wider packed-format guard and the tiled path. The
new partial path excludes CPU-swizzled surfaces. Smaller rebinds skip whole
CPU conversion paths instead of writing beyond the bound allocation.

Double Buffer now copies the exact rows copied by the GPU, with the actual
row span rather than the old total surface size. The wait handler validates
row layout and uses 64-bit address bounds before any copy. Five surface tests
cover retained CPU bytes, invalid layouts, spans, bounds and CPU presentation.
The Linux build and all 110 C++ tests pass. See the
[follow-up review](../../plus-visual-fixes-2026-10-20/review.md).

The mapped range is validated as a whole. Invalid requests are rejected;
rows are not silently shortened. Packed conversion and CPU swizzle loops
still need separate bounded partial support. Device rendering, FPS and power
have not been measured, so the ticket remains needs-info.
