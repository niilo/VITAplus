# 21: Every compiled shader writes a file, on the shader path, on Android

Status: claimed
Type: task
Label: needs-info
Claimed: 2026-10-20 Codex

## Context

Before the port below, `vita3k/renderer/src/shaders.cpp` contained:

```cpp
fs::create_directories(shaderlog_path);

auto shader_log_path = get_shaderlog_path("gxp");

// Dump gxp binary
fs_utils::dump_data(shader_log_path, &program, program.size);
```

`write_data_with_ext` at `:194` then writes the GXP, the SPIR-V and the GLSL for
every non-cache extension. This runs for every shader, every time a shader is
compiled.

On Android `shaderlog_path` is under the application's own storage. A cold
start writes diagnostic files on the thread compiling the shaders. Its share
of loading time has not been measured here.

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

## Code result

Implemented in `c2f81a34` using Plus 3576a576 and 6c568f82. Android debug
logging now controls diagnostic directory creation and dumps. The reusable
shader cache is still written. Desktop diagnostic dumps keep their existing
behaviour. Shader-generation messages use debug logging.

Linux and Android ARM64 compilation pass, as do all 103 C++ tests and the
APK packaging checks. The cold-start device comparison is still pending.
No startup or FPS improvement has been measured.

## Comments
