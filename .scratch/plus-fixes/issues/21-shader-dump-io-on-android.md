# 21: Every compiled shader writes a file, on the shader path, on Android

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/renderer/src/shaders.cpp:188-194`:

```cpp
fs::create_directories(shaderlog_path);

auto shader_log_path = get_shaderlog_path("gxp");

// Dump gxp binary
fs_utils::dump_data(shader_log_path, &program, program.size);
```

`write_data_with_ext` at `:194` then writes the GXP, the SPIR-V and the GLSL for
every non-cache extension. This runs for every shader, every time a shader is
compiled.

On Android `shaderlog_path` is under the application's own storage. A cold start
compiles thousands of shaders and writes a file per shader, on the thread doing
the compiling. On a device with slow storage that is the dominant cost of a cold
launch.

Two things must be preserved when this is gated:

- `fs::create_directories(shaderlog_path)` has to stay inside the guard.
- The `ext == shader_type_str` branch writes `shader_path`, which is the shader
  cache, not a log. That one must still be written, or every launch recompiles.

The gate is debug logging, not a new setting.
`spdlog::should_log` is already available at
`external/spdlog/include/spdlog/spdlog.h:78`.

Vita3K-Plus commit `3576a576`. `129cc7b2` and `82de10e5` in EmuCoreV carry the
same change.

## Plan

1. Wrap the dump path in a check for debug logging, using `spdlog::should_log`
   rather than a new configuration key.
2. Keep the cache write on the `ext == shader_type_str` branch outside the
   guard. Check `write_data_with_ext` and confirm which branch writes what
   before moving anything.
3. This only shows on a cold cache. A warm-cache run is a null result, so the
   measurement has to start from a cleared `shaders/` directory.
4. Measure with `tools/android/gameplay_scene.sh` and
   `tools/android/device_power_sample.sh` from a cleared cache, and record time
   to first frame.

## Comments