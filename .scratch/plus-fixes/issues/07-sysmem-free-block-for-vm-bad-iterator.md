# 07: sceKernelFreeMemBlockForVM erases with the wrong map's iterator

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/modules/SceSysmem/SceSysmem.cpp:165-171`:

```cpp
const Blocks::const_iterator block = state->vm_blocks.find(uid);
assert(block != state->vm_blocks.end());

free(emuenv.mem, block->second->mappedBase.address());
state->allocated_user -= block->second->size;
state->blocks.erase(block);
state->vm_blocks.erase(block);
```

`Blocks` is `std::map<SceUID, KernelMemBlockPtr>`
(`vita3k/modules/include/modules/sysmem_state.h:36`). `block` is an iterator
into `vm_blocks`. `state->blocks.erase(block)` unlinks a node owned by the
other tree, and `state->vm_blocks.erase(block)` then unlinks it again. That is
undefined behaviour, and the outcome is a corrupted map.

Two more faults in the same function:

- It subtracts `block->second->size`, while `sceKernelAllocMemBlockForVM` at
  `:110` sets that field to `sizeof(SceKernelMemBlockInfo)`. The allocator adds
  the real size. So `allocated_user` drifts upward on every alloc and free
  pair, and `sceKernelGetFreeMemorySize` reports a number that only grows.
- The two `assert`s at `:164` and `:166` abort the process in a debug build. A
  guest that frees a UID it was never given should get an error code.

The driver path has the same accounting fault.
`vita3k/modules/SceSysmem/SceSysmemForDriver.cpp:271-287`
(`ksceKernelFreeMemBlock`) subtracts `size` rather than `mappedSize` and never
removes the UID from `vm_blocks`, so a block freed through the driver can be
freed a second time by `sceKernelFreeMemBlockForVM`.

Vita3K-Plus commit `75c7b17a`, from Vita3K-nx.

## Plan

1. Look the UID up again in `blocks` and erase from each map with an iterator
   that belongs to it.
2. Subtract `mappedSize`, not `size`, and check `sceKernelAllocMemBlockForVM`
   for the matching add.
3. Replace the asserts with `return SCE_KERNEL_ERROR_ILLEGAL_BLOCK_ID`.
4. Fix the same two faults in `ksceKernelFreeMemBlock`.
5. Test: allocate for VM, free for VM, and check `allocated_user` returns to its
   starting value and both maps are empty for that UID. Then check the double
   free through the driver path returns an error rather than corrupting the map.

## Comments