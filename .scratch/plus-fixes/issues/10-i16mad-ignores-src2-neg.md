# 10: i16mad decodes src2_neg and then ignores it

Status: open
Type: task
Label: ready-for-agent

## Context

`src2_neg` appears exactly once in
`vita3k/shader/src/translator/ialu.cpp`: on line 362, as the parameter
declaration.

The immediate is decoded off the instruction and never read again. The add at
`:444-445` is unconditional:

```cpp
auto add_result = m_b.createBinOp(spv::OpIAdd, source0_type, mul_result, source2);
```

So an `IMAD16` whose third source is negative adds instead of subtracting. Every
multiply-add in an affected program computes the wrong value. The surrounding
code handles the same bit correctly for other instructions, which is why it
looks like an omission rather than a decision.

Fifa 15 is where this was found, and it is not a per-title bug: the
instruction is used by any shader that multiplies and accumulates with a negated
addend.

Vita3K-Plus commit `42edce31`.

## Plan

1. Select the opcode on the flag:
   `auto add_result = m_b.createBinOp(src2_neg ? spv::OpISub : spv::OpIAdd, source0_type, mul_result, source2);`
2. Check the neighbouring instructions in the same file for the same
   omission. `i32mad2` and `i8mad` are named in the comparison report as
   candidates; confirm each by reading rather than assuming.
3. The disassembler path builds the instruction text for the CPU debugger. If
   it prints the operand with a hardcoded `+`, make it follow the flag too, so
   a shader dump shows what the emulator actually did.
4. `CURRENT_VERSION` in `vita3k/shader/include/shader/spirv_recompiler.h` does
   not need a bump for this one alone, because the generated code changes only
   for programs that use the flag and those are already cached with the wrong
   result. It does need one. Take the next unused number.
5. Test with a shader that uses `IMAD16` with a negated source, and compare the
   picture before and after.

## Comments