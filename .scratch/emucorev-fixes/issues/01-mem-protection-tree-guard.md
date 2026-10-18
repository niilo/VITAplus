# 01: Guard the protection-tree lookup in remove_external_mapping

Status: open
Type: task
Label: ready-for-agent

## Context

`remove_external_mapping` (`vita3k/mem/src/mem.cpp:761`) does this:

```cpp
auto prot_it = mem.protect_tree.lower_bound(mapping.address);
if (prot_it->first + prot_it->second.size <= mapping.address) {
```

and dereferences `prot_it` without comparing it against `end()`.

`ProtectSegmentTrees` is `std::map<Address, ProtectSegmentInfo, std::greater<>>`
(`vita3k/mem/include/mem/state.h:59`). `lower_bound` on that comparator returns
`end()` when the tree is empty, and also when every key in the tree is above
the range being removed. Only `add_protect` inserts entries, from
`kubridge.cpp` and from the renderer's surface and texture caches, so the tree
is empty for much of a run.

Dereferencing `end()` is undefined behaviour. The read picks up a garbage
`first` and `size`, and the erase loop below then corrupts the tree.

`add_protect` already has the check, at `vita3k/mem/src/mem.cpp:623`:

```cpp
auto it = state.protect_tree.lower_bound(addr);
if (it == state.protect_tree.end() || it->first + it->second.size <= addr) {
```

So this is the same repair in the one function that lacks it. EmuCoreV commit
`ce756404` makes the identical change.

## Plan

1. Add `prot_it == mem.protect_tree.end() ||` to the condition at
   `vita3k/mem/src/mem.cpp:761`.
2. Extend `vita3k/mem/tests/external_mapping_tests.cpp`. The current test
   does an add and remove round trip but never populates `protect_tree`, so
   it never reaches this code. Add a case that removes a mapping while the
   tree is empty, and one where the tree holds a single entry whose address is
   above the removed range.
3. `container/vita3k.sh test`, then `container/vita3k.sh format-check`.

## Comments
### 2026-10-13: done, and the tests need a checked STL to fail

Fixed in `fix/mem-kernel-bookkeeping`. The two new cases in
`external_mapping_tests.cpp` pass against the old code in an ordinary
RelWithDebInfo build, because dereferencing `end()` reads the node header and
the resulting garbage address usually lands outside the erase loop's range. The
defect is real but it does not announce itself without instrumentation.

Built `mem-tests` a second time with `-D_GLIBCXX_DEBUG` to confirm it. Against
the old line the first new case aborts:

```
[ RUN      ] ExternalMappingTest.removing_a_mapping_survives_an_empty_protect_tree
Error: attempt to dereference a past-the-end iterator.
```

With the fix in, all 50 mem tests pass under the same build. Anyone changing
this line should do the same before believing a plain test run.

Note for whoever picks up the remaining unchecked STL dereferences: the image
has no ASan or UBSan runtime (`libasan.so.8.0.0` and `libclang_rt.asan.a` are
both absent) and no valgrind, so `-D_GLIBCXX_DEBUG` is the only cheap detector
available here. It catches container iterator abuse, which is what
`remove_external_mapping` does.
