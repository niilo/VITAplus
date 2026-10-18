# 11: The internal repeat offset is applied to every FPINTERNAL operand

Status: open
Type: task
Label: ready-for-agent

## Context

`get_repeat_offset` (`vita3k/shader/include/shader/usse_translator.h:143`):

```cpp
const int get_repeat_offset(Operand &op, const std::uint8_t repeat_index, RepeatMode repeat_mode, RegisterBank bank) {
    if (repeat_mode == RepeatMode::INTERNAL || repeat_mode == RepeatMode::BOTH) {
        if (bank == RegisterBank::FPINTERNAL) {
            return repeat_index;
        }
    }
```

With `RepeatMode::INTERNAL` the repeat offset belongs only to operand index 3,
the internal repeat register. Every other FPINTERNAL operand should get 0.
This code gives the offset to all of them, so the wrong source registers are
read. For a shader that packs a vector, that produces white or garbage.

The comment below the check explains that the intent was to test INTERNAL and
BOTH against every FPINTERNAL, including non-GPI. That is a different rule,
about GPI operands, and it does not cover the index.

`Operand::index` already exists (`vita3k/shader/include/shader/usse_types.h:318`),
so nothing else is needed.

Vita3K-Plus commit `d2cadf35`, found on Epic Mickey 2 and not a per-title bug.

## Plan

1. Guard the early return on `op.index == 3`, so the offset reaches only the
   internal repeat register.
2. Read the rest of the function and check the GPI path still returns the
   offset for the operands it is meant to cover. The comment says the GPI check
   is deliberately placed after this one; keep that placement.
3. `CURRENT_VERSION` in `vita3k/shader/include/shader/spirv_recompiler.h` needs
   a bump, for the same reason as ticket 10.
4. Test on a shader that packs a vector with an internal repeat.

## Comments