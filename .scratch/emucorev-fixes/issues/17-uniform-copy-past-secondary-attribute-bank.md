# 17: A uniform copy can run past the secondary attribute bank

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/shader/src/spirv_recompiler.cpp:1359`, the branch taken when memory
mapping is off:

```cpp
const uint32_t reg_block_size_in_f32v = std::min<uint32_t>(buffer.reg_block_size + 3, REG_SA_COUNT) / 4;
const auto spv_buffer = utils::create_access_chain(b, spv::StorageClassStorageBuffer, spv_params.buffer_container,
    { b.makeIntConstant(spv_params.buffers.at(host_idx).index_in_container) });
copy_uniform_block_to_register(b, spv_params.uniforms, spv_buffer, ite_copy, buffer.reg_start_offset, reg_block_size_in_f32v);
```

Two faults in those three lines.

`reg_block_size + 3` rounds the block up to whole vec4s, so the copy writes up
to three scalars past the range the GXP header asked for.

`reg_start_offset` is never clamped against the bank. The destination is
`vec4[32]` (`spirv_recompiler.cpp:1105`, with `REG_SA_COUNT = 32 * 4` at
`:59`). A block that starts at offset 124 with an eight-float size gives
`reg_block_size_in_f32v = min(11, 128) / 4 = 2`, and
`copy_uniform_block_to_register` then writes `sa[31]` and `sa[32]`. Index 32
is one past the end of the array, so the generated `OpAccessChain` is out of
range.

The mapped branch at `:1356` already clamps:

```cpp
const uint32_t copy_size = std::min(buffer.reg_block_size, REG_SA_COUNT - buffer.reg_start_offset);
```

so only the non-mapped branch is wrong. The buffer index is also unguarded
here, while `:1142` checks `buffer.index >= SCE_GXM_REAL_MAX_UNIFORM_BUFFER`
205 lines earlier. An index of 14 maps through `convert_buffer_idx_to_host`
(`:1135`) to container 0, and `spv_params.buffers.at(0)` throws
`std::out_of_range` when no buffer landed there.

This is compiled out of the default Android build, which uses
`memory-mapping: page-table` (`vita3k/config/include/config/config.h:24`). It
is in the shipped shader compiler and runs for any user who sets
`memory-mapping: disabled`.

EmuCoreV commit `fb3fcd90` clamps the copy to
`min(reg_block_size, REG_SA_COUNT - reg_start_offset)`, copies the scalar tail
element by element, and skips buffers with an out-of-range index.

## Plan

Take the hunk from Vita3K-Plus commit `ca3ad408`, not the EmuCoreV version.
Plus computes the clamp once for both branches rather than only the non-mapped
one:

```cpp
const uint32_t copy_size = std::min(buffer.reg_block_size, REG_SA_COUNT - buffer.reg_start_offset);
```

and skips the buffer when `buffer.index >= SCE_GXM_REAL_MAX_UNIFORM_BUFFER` or
`buffer.reg_start_offset >= REG_SA_COUNT`. It also changes
`copy_uniform_block_to_register` to take a float count instead of a vec4 count,
write `count / 4` whole vectors, then copy the `count % 4` remainder scalar by
scalar. The current code rounds the count up and copies the tail inside the
vector, which is the overrun.

1. Clamp the copy to `min(reg_block_size, REG_SA_COUNT - reg_start_offset)` and
   skip a buffer whose `reg_start_offset` is already past the bank.
2. In `copy_uniform_block_to_register` (`:1055`), copy the tail scalars one at
   a time rather than rounding the length up. The fast path at `:1066` stores a
   whole vec4 per iteration when `start % 4 == 0`.
3. Add the `buffer.index >= SCE_GXM_REAL_MAX_UNIFORM_BUFFER` check that `:1142`
   already has. The constant is in `vita3k/gxm/include/gxm/types.h:28`.
4. Bump `CURRENT_VERSION` in
   `vita3k/shader/include/shader/spirv_recompiler.h:40` to invalidate cached
   pipelines. It is 16 now; EmuCoreV's value does not carry over, so take the
   next unused number.
5. The tail handling matters more here than upstream.
   `.scratch/game-sweep-2026-09-30/issues/05-jak-daxter-black-screen-after-intro-video.md`
   records that this repository synthesises the default uniform buffer from
   `primary_reg_count * 4` floats when `default_uniform_buffer_count` is 0,
   which is 84 of 272 programs across three games. That makes a
   `reg_block_size` that is not a multiple of 4 the normal case.
6. Verify with `memory-mapping: disabled` on one title that compiles the
   affected programs, and confirm no regression with the default setting.

## Comments

### 2026-10-12: the Plus commit does not link, so take the hunk

VITAplus merged Vita3K-Plus at `20588fbf`. `ca3ad408` is one of the 51 commits
since, and it carries the fix for steps 1 to 3.

Do not cherry-pick it. It calls `Atrac9DecodeBounded` at
`vita3k/codec/src/atrac9.cpp:94`, and that function does not exist in either
repository. `external/LibAtrac9` has only `Atrac9Decode`, and
`InitBitReaderCxt` takes no length, so a bounded decode cannot be written
without work in the submodule. The Plus checkout does not initialise that
submodule, which is why the failure was never seen there.

Copy the two hunks by hand. The underlying atrac9 defect is real and is not
covered by this ticket; it needs the bounded decoder first, and this repository
cannot take a submodule pointer from Plus.

Same note applies to `.scratch/emucorev-fixes/issues/15`, which points at a
different Plus commit that does not have this problem.