# 11: gxmSetUniformBuffers looks the region up with the wrong bound

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/modules/SceGxm/SceGxm.cpp:2471`:

```cpp
auto ite = gxm.memory_mapped_regions.lower_bound(buffers[i].address());
if ((ite != gxm.memory_mapped_regions.end()) && ((ite->first + ite->second.size) > buffers[i].address())) {
    // Bound the size
    bytes_to_copy = std::min<uint32_t>(ite->first + ite->second.size - buffers[i].address(), bytes_to_copy);
}
```

`memory_mapped_regions` is keyed by address. `lower_bound` returns the first
entry whose key is greater than or equal to the address. The entry that
contains the address is the one before it, unless the address is itself a key.

So when the containing region's start address is less than the buffer address,
`lower_bound` skips past it and returns the next region. If that next region
starts after this one ends, the condition is false and `bytes_to_copy` keeps
its unbounded value.

The bound exists to stop the copy running past the end of the mapped region
into unmapped host memory. When it fails to apply, the copy reads whatever
follows the mapping.

`:3120` has the same `lower_bound` for a different lookup, where the intent is
to find the region starting at an address. Leave that one alone.

EmuCoreV commit `5e4f7c58` change 11 uses `upper_bound` and then decrements.

## Plan

1. Replace `lower_bound` with `upper_bound` and step back one entry when it is
   not `begin()`, then test whether that entry contains the address. Keep the
   existing `bytes_to_copy` bound.
2. Confirm the case where the buffer address is exactly a region start. With
   `upper_bound` that entry is the one before the iterator, so it is still
   found.
3. Add a test in the GXM tests: map two regions with a gap, place a uniform
   buffer that starts inside the first and whose length runs past its end, and
   check that `bytes_to_copy` is clamped. A second case with a buffer that
   starts exactly on a boundary.

## Comments