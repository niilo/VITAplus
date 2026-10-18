# 14: f16 packing overflows to Inf instead of saturating

Status: open
Type: task
Label: ready-for-agent

## Context

`make_f16_pack_func` in
`vita3k/shader/src/usse_utilities.cpp:492-497`:

```cpp
spv::Id extracted = f16_pack_func->getParamId(0);

// use packHalf2x16
extracted = b.createBuiltinCall(type_ui32, utils.std_builtins, GLSLstd450PackHalf2x16, { extracted });
extracted = b.createUnaryOp(spv::OpBitcast, type_f32, extracted);
```

`GLSLstd450PackHalf2x16` rounds to nearest and lets anything above 65504 become
infinity. The hardware saturates. A legitimate shader result just above the half
float range therefore comes out as infinity, and the shader's own comparison
against the range goes the wrong way.

A NaN is a second case: it must survive the pack as NaN, which needs the
original value selected back in after the clamp.

Vita3K-Plus commit `105331d9`, found on MotoGP 14's bike and not a per-title
bug.

## Plan

1. Clamp the extracted pair to `-65504 .. 65504` before the call.
2. Compute `OpIsNan` on each component and select the original value where it
   is NaN, so the clamp does not turn a NaN into a finite number.
3. `CURRENT_VERSION` in `vita3k/shader/include/shader/spirv_recompiler.h` needs
   a bump.
4. Test with a shader whose colour exceeds 65504 in a channel, and one that
   produces NaN.

## Comments