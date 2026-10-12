# 13: sop2m drops the lanes its write mask does not cover

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/shader/src/translator/alu.cpp:1236-1238`:

```cpp
result = utils::convert_to_int(m_b, m_util_funcs, result, DataType::UINT8, true);

// Final result. Do binary operation and then store
store(inst.opr.dest, result, wmask, 0);
```

When the written lanes are not a packed run starting at x, the lanes the
instruction does not write must keep their previous value. The check for that
shape is `(wmask & (wmask + 1)) == 0`, and nothing in the tree uses it.

`SOP2M` is the masked two-operand float instruction, used in blending and
lighting maths. A lane that should have been preserved but was zeroed is a
wrong pixel.

Vita3K-Plus commit `b87189eb`, found on Gran Turismo 3 and not a per-title bug.

## Plan

1. When `wmask` is not a packed run from x, call `finalize` with
   `SWIZZLE_CHANNEL_4_DEFAULT` before the store, so the unwritten lanes carry
   their previous value. Both are already in scope at that point.
2. Check the other masked instructions in `vita3k/shader/src/translator/alu.cpp`
   for the same omission, and record what you find here.
3. `CURRENT_VERSION` in `vita3k/shader/include/shader/spirv_recompiler.h` needs
   a bump.
4. Test on a shader that uses `SOP2M` with a non-contiguous mask.

## Comments