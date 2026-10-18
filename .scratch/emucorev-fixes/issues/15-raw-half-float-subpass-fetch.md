# 15: The subpass fetch reads float, so f16 blending loses the low bits

Status: open
Type: task
Label: needs-triage

## Context

`preserve_f16_nan_as_u16` is on by default.
`vita3k/renderer/src/vulkan/renderer.cpp:877`:

```cpp
features.preserve_f16_nan_as_u16 = static_cast<bool>(physical_device_features.independentBlend) && (!raw_preserve_needs_interlock || features.support_shader_interlock);
```

`raw_preserve_needs_interlock` is false at `:876`, so this reduces to
`independentBlend`, which both the stock Qualcomm driver and Turnip report.
The flag says the real GXM framebuffer fetch reads the raw 16 bits per
channel, and that the host must not lose them.

The fetch does not do that. `vita3k/shader/src/spirv_recompiler.cpp:942`:

```cpp
source = b.createOp(spv::OpImageRead, v4, { b.createLoad(last_frag_data, spv::NoPrecision), coord_0 });
```

`last_frag_data` is a `f32` subpass input at input attachment 0. The shader
reads the colour attachment as float. A value that round-trips through the
float conversion is not the value the guest wrote, so each programmable
blending step compounds the difference. A raw 16-bit read does not.

The rest of the machinery is single-image. `pipeline_cache.cpp:162-169`
declares the raw alias as `eStorageImage` unconditionally,
`pipeline_cache.cpp:622` sets `subpass.inputAttachmentCount = 1` even when
`with_raw_attachment` is true, `context.cpp:552` hardcodes the same descriptor
type for binding 2, `surface_cache.cpp:794` and `renderer.cpp:1009` create the
raw image without `eInputAttachment`, and `scene.cpp:448` builds a one-image
barrier for the fetch. `renderer/src/shaders.cpp:162` keys the disk shader
cache on `get_shader_hash(program)` alone, so two variants of one program
would share a cache entry. There is no `fragment_shader_variant_hash` in this
repository.

The write side is already there: `spirv_recompiler.cpp:1822` emits `out_color_ui`
at location 1, so the raw attachment is populated. Only the read is missing.
The shader-interlock branch at `spirv_recompiler.cpp:965-1011` already has a
raw path, but neither Adreno driver takes it: on Turnip
`support_shader_interlock` is forced false at `renderer.cpp:863`, so
`use_shader_interlock` is false and `with_raw_attachment` is true
(`pipeline_cache.cpp:1164`).

**This conflicts with two open tickets.**
`.scratch/pocket-s-android13/issues/06-framebuffer-fetch-path.md` step 2 and
`.scratch/pocket-s-android13/issues/15-attachment-layouts.md` step 1 both want
an unused input attachment removed, to keep the pass in GMEM. This ticket adds
one. Doing both without a decision will undo one of them.

EmuCoreV commit `e2d42f6c` adds a second `SubpassData` image of `uint` at
input attachment 1, binding 2, branches on a `use_raw_image` uniform, and tags
the shader hash so the variants do not share a disk cache entry.

Vita3K-Plus commit `24702197` does the same and more, and should be the
reference instead. It adds the `uint` subpass input, propagates
`eInputAttachment` to binding 2, sets `inputAttachmentCount` to 2, and adds
`current_color_raw_image` alongside the existing `current_color_raw_view` so
the fetch barrier covers both images. For the disk cache it tags the in-memory
hash with `hash[6] ^= 0x75` and adds a `"u"` version suffix, rather than adding
a separate hash function, so there is no `fragment_shader_variant_hash` to
introduce.

## Plan

1. Decide the conflict first and write it here. The question is whether the
   fetch path stays on `direct_fragcolor` with two input attachments, or moves
   to the interlock path. Answer it with a measurement on the device, not from
   the specification, because the two options trade GMEM residency against
   bit accuracy. If this cannot be decided, close the ticket as `superseded`
   by whichever of 06 and 15 goes first, and do not start the port.
2. Add `raw_color_attachment_input` to `Hints` in
   `vita3k/renderer/include/renderer/shader_variant.h`, and derive it from
   `preserve_f16_nan_as_u16` and `with_raw_attachment`.
3. In `spirv_recompiler.cpp`, add the raw `uint` read next to the float one at
   `:942`, and select between them with a uniform.
4. Propagate the descriptor type. Binding 2 must be `eInputAttachment`, not
   `eStorageImage`, and `subpass.inputAttachmentCount` at `pipeline_cache.cpp:622`
   must become 2 when the raw attachment is in use.
5. Size the input attachment pool for two per set.
   `vita3k/renderer/src/vulkan/context.cpp:427` sizes it at
   `DESCRIPTOR_PACK_SIZE * MAX_FRAMES_RENDERING`, one per set.
6. Set `current_color_raw_image` alongside `current_color_raw_view`
   (`context.cpp:319`) and add the second image to the fetch barrier in
   `scene.cpp:448`.
7. Add `fragment_shader_variant_hash` and use it at
   `renderer/src/shaders.cpp:162`, so the two variants do not collide in the
   disk cache. Bump `CURRENT_VERSION` in
   `vita3k/shader/include/shader/spirv_recompiler.h`.
8. Measure on both drivers with `tools/android/gameplay_scene.sh`, and record
   the `driverID` line with any result, per the rule in `CLAUDE.md`.

## Comments

### 2026-10-12: Vita3K-Plus has the whole change

VITAplus merged Vita3K-Plus at `20588fbf`. `24702197` is one of the 51 commits
since, and it is the complete change this ticket asks for across seven files.
Steps 2 to 6 are all in it. Use it as the reference, not the EmuCoreV version.

Step 1 still stands and is still the reason this ticket cannot start. The
conflict with `.scratch/pocket-s-android13/issues/06-framebuffer-fetch-path.md`
step 2 and `issues/15-attachment-layouts.md` step 1 is real and Plus did not
resolve it either.

`24702197` also carries a per-driver Turnip LRZ workaround in
`renderer/src/vulkan/sync_state.cpp`. Take that as separate work, not with this
ticket: it is keyed on `vk::DriverId` and needs a measurement on both drivers,
per the rule in `CLAUDE.md`. `.scratch/plus-fixes/spec.md` has the detail.