# 02: The memspace block allocator appends a phantom block on every split

Status: open
Type: task
Label: ready-for-agent

## Context

`MemspaceBlockAllocator::alloc` (`vita3k/mem/include/mem/mempool.h:68`) does
this:

```cpp
if (blocks.capacity() == blocks.size()) {
    blocks.resize(blocks.size() + 1);
}

blocks.insert(blocks.begin() + i + 1, division);
```

The resize runs before the insert. It appends one value-initialised element,
`{size = 0, offset = 0, free = false}`, and then the insert places
`division` in front of it. The allocator therefore grows by two elements
where it should grow by one, and leaves behind a zero-length block that is
marked allocated.

After that the vector no longer tiles the memspace. `free()` searches with
`std::lower_bound` on `offset` (`mempool.h:83`), and a block that is not part
of the tiling shifts what the search finds.

Two smaller faults in the same struct:

- `init` (`mempool.h:39`) pushes the main block without clearing `blocks`
  first, so a second `init` leaves the old blocks in place.
- `alloc` has no size guard. `alloc(0)` aligns to 0, matches any free block,
  and returns an offset with no usable bytes. The same happens when
  `align(size, 4)` wraps to 0.
- The comment on `blocks` (`mempool.h:32`) says the blocks are sorted by
  size. They are sorted by address offset, which is what `free()` assumes.

This allocator backs NGS. `Rack` and `System` inherit `MempoolObject`
(`vita3k/ngs/include/ngs/system.h:342` and `:371`) and allocate through it at
`vita3k/ngs/src/ngs.cpp:400`, `:434`, `:454`, `:467` and `:470`.

EmuCoreV commit `6ef3759f` fixes all of these and adds `mempool_tests.cpp`.

## Plan

1. Drop the `resize` at `mempool.h:68`. `insert` already grows the vector, and
   `blocks.begin() + i + 1` is the correct position with the current code
   because the loop index refers to the element before the insertion point.
2. Reject `size == 0` and any size that aligns to 0 at the top of `alloc`.
   Return the same value `alloc` returns when it fails, or a new error code
   if the callers can be audited to take one.
3. Clear `blocks` at the start of `init`.
4. Fix the comment on `blocks`.
5. Add `vita3k/mem/tests/mempool_tests.cpp`. Cover a split, two splits in the
   same block, `alloc(0)`, a second `init`, and an allocation that exhausts
   the pool. Assert that `blocks` tiles the memspace exactly after each call.
6. `container/vita3k.sh test`, then `container/vita3k.sh format-check`.

## Comments