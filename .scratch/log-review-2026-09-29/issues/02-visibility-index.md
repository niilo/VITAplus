# 02: Initialize the visibility index and check it against a new buffer

Status: resolved
Claimed: 2026-09-29 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent

## Context

WipEout 2048 logged `Using visibility index 808858157 which is too big for
the buffer` (`vita3k/renderer/src/vulkan/sync_state.cpp:239`). 808858157 is
0x3036372D. That is the text "-760" read as a number. So the value is
uninitialized memory, not a value from the game.

Causes found in the code:

1. `GxmContextState::visibility_index` has no initial value
   (`vita3k/renderer/include/renderer/gxm_types.h:277`). The two fields next
   to it have one. `sceGxmSetFrontVisibilityTestEnable` and
   `sceGxmSetFrontVisibilityTestOp` send this field to the renderer. If the
   game calls one of them before `sceGxmSetFrontVisibilityTestIndex`, the
   renderer gets the uninitialized value.
2. `sync_visibility_index` checks the index only when a visibility buffer is
   set. With no buffer, it stores the index without a check. When
   `sync_visibility_buffer` then sets a buffer, `scene.cpp:378` uses the
   stored index to write `queries_used[index]` and to call `beginQuery`.
   An index past the buffer size writes outside the vector. That can
   corrupt memory or crash.

## Plan

1. Set `uint32_t visibility_index = 0;` in `gxm_types.h`.
2. In `sync_visibility_buffer`, after the new buffer is set: if
   `current_query_idx` is not -1 and is not less than the buffer size, log
   the same warning and set it to 0. This is the same rule that
   `sync_visibility_index` uses.
3. Build and run the tests in the container.

## Answer

Done on branch `log-review/01-02-nids-visibility`:

- `visibility_index` now starts at 0.
- `sync_visibility_buffer` checks the stored index against the new buffer
  size, logs the warning and uses 0. It skips the check while a query is
  open, because the open query belongs to the old buffer.

Linux build, `container/vita3k.sh test` and `format-check` pass. Not yet
checked on the device with WipEout 2048.

## Comments
