# 18: Vertex streams are only restrided on Apple hosts

Status: open
Type: task
Label: needs-triage

## Context

`vita3k/renderer/src/vulkan/pipeline_cache.cpp:1044`:

```cpp
#ifdef __APPLE__
        const uint32_t stride = align(stream.stride, 4);
#else
        const uint32_t stride = stream.stride;
#endif
```

and `restride_stream` in `vita3k/renderer/src/vulkan/scene.cpp:128` is inside
the same guard, with the call site at `:327` compiled out to match.

Vulkan requires every vertex attribute fetch address, `stride * vertex +
offset`, to be a multiple of the format's component size. A stride that is not
a multiple of 4 misaligns three of every four vertices on a 32-bit format,
which reads the wrong bytes and produces garbled geometry.

The upstream guard is `#ifdef __APPLE__` because Metal forbids a stride that is
not a multiple of 4. The requirement is from Vulkan, so it applies on Android
and Linux too.

Reachability: only when memory mapping is off
(`vita3k/renderer/src/vulkan/renderer.cpp:1127`,
`mapping_method != MappingMethod::Disabled`). The Android default is
`page-table` (`vita3k/config/include/config/config.h:24`), so the default build
keeps the streams mapped and does not take the path.

**Do not port EmuCoreV's version.** The equivalent change already exists in
this repository as commit `35cbde8d` on `remotes/plus/all-enhancements`, titled
"Mali needs the same alignment treatment as Apple Metal". It is not merged into
`HEAD`; `git merge-base --is-ancestor 35cbde8d HEAD` fails. That branch also
has `renderer::stream_snapshot_get` in `bind_vertex_streams`, which `master`
does not, so a cherry-pick will not apply cleanly.

EmuCoreV commit `326184b5` is an adaptation of the same commit with three
additions worth taking: it applies to all non-mapped streams rather than Apple
only, it guards the restride with "every active attribute must fit inside the
original stride", and it pre-simulates the ring allocations for the whole draw
so an expanded stream cannot overlap another stream in the same draw.

## Plan

1. Take `needs_restride` from Vita3K-Plus commit `cef53700`, not `35cbde8d`.
   This replaces step 1 below and is better than it. `35cbde8d` widens any
   stride that is not a multiple of 4. `cef53700` checks each active attribute
   against the component size of its own format, which is what Vulkan
   actually requires, and it returns true less often, so fewer streams are
   repacked. It also threads the same predicate through `get_vertex_input_state`,
   which is step 5 here, so that part is done too.
2. Do not take `35cbde8d`. It relies on a lazy
   `vertex_stream_ring_buffer.create()`. `vita3k/renderer/src/vulkan/creation.cpp:62`
   only creates that buffer when memory mapping is off, so a restrided stream
   under memory mapping would allocate against a null handle. That lazy create
   comes from a different commit and costs a 64 MiB buffer
   (`creation.cpp:34`) that the current Android configuration never allocates.
3. Move the restride out of `#ifdef __APPLE__` and gate it on
   `mapping_method != MappingMethod::Disabled` plus the condition that the
   stream is not mapped.
4. Take EmuCoreV's fit check. Without it, widening a stride for alignment can
   push an attribute past the end of its buffer.
5. Take the ring pre-simulation. `bind_vertex_streams`
   (`vita3k/renderer/src/vulkan/scene.cpp:317`) currently returns `void` and
   the caller draws unconditionally, so it has to return `bool` and `draw()`
   must skip when the expansion does not fit. `RingBuffer` in
   `vita3k/vkutil/include/vkutil/objects.h:93` exposes only `handle()`, so it
   needs `capacity_bytes()` and `cursor_bytes()`.
6. Verify with `memory-mapping: disabled` on a title that uses a non-multiple
   of 4 stride. Find one by logging `stream.stride` from
   `pipeline_cache.cpp:1044` while a scene is set up. Then confirm no
   regression with the default `page-table` setting.
7. This ticket is worth little in the shipped configuration. The Android
   default is `page-table` (`vita3k/config/include/config/config.h:24`), and
   `features.enable_memory_mapping` is true for every mapping method except
   `Disabled` (`renderer.cpp:1127`). The mapped path is always taken, so this is
   a no-op unless the user sets `memory-mapping: disabled`. Keep it low in the
   queue for that reason.

## Comments

### 2026-10-12: supersede 35cbde8d with cef53700

VITAplus merged Vita3K-Plus at `20588fbf`. `35cbde8d` and `cef53700` are both
among the 51 commits since, `cef53700` later. Step 1 above is replaced.

Nothing else in this ticket changes. The reachability note in Context is
correct and was confirmed against the device configuration.