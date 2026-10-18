# 09: The USSE PN predicate is decoded as NEGP2

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/shader/include/shader/usse_types.h:43-53`:

```cpp
enum class ExtPredicate : uint8_t {
    NONE,
    P0,
    P1,
    P2,
    P3,
    NEGP0,
    NEGP1,
    NEGP2,
    PN
};
```

The field decoded from the instruction is 3 bits, so the values are 0 to 7.
`NONE` is 0, `P0` to `P3` are 1 to 4, and `NEGP0` to `NEGP2` are 5 to 7.
`PN` is 8, which the field cannot hold.

So the hardware value 7, which means `PN`, the result-negation predicate, is
translated as "negate p2". Every shader that uses it takes the wrong branch.
That is not a shading difference, it is control flow.

Two other places depend on the numeric layout:

- `vita3k/shader/src/usse_program_analyzer.cpp:568` computes a negated flag
  with `predicate_writed_to + 5` arithmetic, which is correct for the current
  order and wrong after the swap.
- `get_condition_value` at
  `vita3k/shader/src/usse_translator_entry.cpp:902` assumes the negated
  predicates are a contiguous run at the end.

`ext_vec_predicate_to_ext` and `short_predicate_to_ext` map by name, so they
travel with the change.

Vita3K-Plus commit `7451f9f4`, from Vita3K-nx. Its subject is about logging
`Pn`, and the fix to the decode came in the same change.

## Plan

1. Reorder the enum so `PN` is 7 and `NEGP2` is 8.
2. Replace the `+ 5` arithmetic at `usse_program_analyzer.cpp:568` with a
   `negated[]` lookup table indexed by the predicate.
3. Update `get_condition_value` at `usse_translator_entry.cpp:902` to split
   `NEGP0` to `NEGP1` from `NEGP2`.
4. Bump `CURRENT_VERSION` in
   `vita3k/shader/include/shader/spirv_recompiler.h:40`. Every shader in the
   on-disk cache was generated with the wrong decode, so without the bump the
   fix does not take effect for a user with a warm cache. It is 16 now.
5. Test on a title whose shaders use the predicate. No reference output exists,
   so compare the picture before and after on a fixed scene with
   `tools/android/gameplay_scene.sh`.

## Comments