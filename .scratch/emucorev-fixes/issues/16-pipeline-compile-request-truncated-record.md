# 16: CompileRequest stores a truncated GxmRecordState

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/renderer/src/vulkan/pipeline_cache.cpp:52`:

```cpp
constexpr size_t record_pipeline_len = offsetof(GxmRecordState, vertex_streams);
```

`CompileRequest` (`pipeline_cache.cpp:84`) copies that many bytes and then
casts the buffer to a `GxmRecordState *`:

```cpp
alignas(8) uint8_t record_data[record_pipeline_len];

const GxmRecordState *get_record() {
    // note: this object is only half defined, but we are only looking at the part that's defined
    return reinterpret_cast<const GxmRecordState *>(record_data);
}
```

The comment says the object is half defined and only the defined part is read.
That holds today. `compile_pipeline` (`:1096`) takes `has_color_surface_data`
as its own parameter (`:1073`, `:1540`) and reads only fields inside the
prefix: `color_base_format`, `cull_mode`, `two_sided`, the stencil ops, the
depth functions, `front_side_fragment_program_mode`, `is_maskupdate` and
`is_gamma_corrected`. `color_surface` is at
`vita3k/renderer/include/renderer/types.h:140`, past `vertex_streams` at
`:132`, and is never touched by the consumer.

So the defect is latent, not active. It matters for two reasons. It is a trap
for the next reader, and ticket 15 arms it: the raw framebuffer-fetch work
reads `record.color_surface.data` inside `compile_pipeline` in place of the
passed-in flag, which is past the end of the stored prefix.

Only reachable with `async-pipeline-compilation` on, which defaults to `false`
(`vita3k/config/include/config/config.h:176`). EmuCoreV commit `c50169e5`
makes the request own a full `GxmRecordState`.

## Plan

1. Replace `CompileRequest::record_data` with a full `GxmRecordState record`.
   Keep `record_pipeline_len` for the cache key: `hash_pipeline_record` at
   `:54` and the second `XXH3_64bits` at `:1429` both use the prefix on
   purpose.
2. `GxmRecordState` is not trivially copyable. It holds `std::shared_ptr`
   members at `vita3k/renderer/include/renderer/types.h:137-138`, so
   `memcpy` into it and a plain `.record = record` in the aggregate
   initialiser at `pipeline_cache.cpp:1534` both fail to compile. Those two
   members are `vertex_program_binding` and `fragment_program_binding`, and
   the request already carries them as separate `shared_ptr` fields. Drop them
   from the request rather than copying them.
3. Add the `std::is_trivially_copyable_v` static assert on whatever stays.
4. Do this before ticket 15 if both are taken. Ticket 15 is the reason this
   becomes reachable.
5. No test: the path needs a Vulkan device and a second host thread. Verify by
   building and running `tools/android/gameplay_scene.sh` with
   `async-pipeline-compilation: true` in the config, on one title.

## Comments