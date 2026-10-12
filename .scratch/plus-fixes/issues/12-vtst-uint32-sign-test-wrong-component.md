# 12: vtst_impl reads the wrong test-bank component for an unsigned 32-bit sign test

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/shader/src/translator/branch_cond.cpp:258-263`:

```cpp
int index_tb_comp = 0;
if (is_signed_integer_data_type(load_data_type)) {
    index_tb_comp = 1;
} else if (is_unsigned_integer_data_type(load_data_type)) {
    index_tb_comp = 2;
}
```

`index_tb_comp` selects which test-bank component the compare reads.
`is_unsigned_integer_data_type` is true for 8-bit, 16-bit and 32-bit unsigned
types alike, but a sign test on a 32-bit value has to read bit 31, which is
component 1. Component 2 is the wrong register for `UINT32`.

So every unsigned 32-bit sign test in every shader compares the wrong bits.

`vtstmsk` has a related defect in the same file. The mask form of the test
instruction on an `F16` or `F32` source currently returns 0.0 or 1.0
(`:481-527` sets `output_type = type_f32` and `:529` selects without a
bitcast). It has to produce a bit mask: `0xFFFF` for `F16` with the destination
retyped to `UINT16`, `0xFFFFFFFF` for `F32`, then bitcast back to `f32` so the
destination register's bookkeeping does not change.

Vita3K-Plus commit `dec3399a`, found on Super Hero Nation clouds and not a
per-title bug.

## Plan

1. Split the unsigned branch: `UINT32` uses component 1, the narrower unsigned
   types keep component 2.
2. Implement the `vtstmsk` float path: compute the mask constant for the source
   width, retype the destination to the matching unsigned type, store, and
   bitcast the result back.
3. `CURRENT_VERSION` in `vita3k/shader/include/shader/spirv_recompiler.h` needs
   a bump. Both changes alter generated code for programs already cached.
4. No reference output exists. Compare a fixed scene before and after and
   record what changed, as ticket 10 asks.

## Comments