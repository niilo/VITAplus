# 17: Reduce pipeline compile stutter

Status: open
Type: experiment
Label: ready-for-human
Blocked by: 02, 04

## Problem

Pipelines are compiled when a draw first needs them. The disk cache holds only
hashes and the `VkPipelineCache` blob
(`vita3k/renderer/src/vulkan/pipeline_cache.cpp:320`, `:389`), so it holds no
data to build pipelines before the first use. There is a shader hash list
(`vita3k/renderer/src/shaders.cpp:60-86`) and SPIR-V is cached on disk per
stage (`shaders.cpp:164-167`), and a precompile pass runs from that list
(`renderer.cpp:2342`), but the precompile stops at the SPIR-V stage.

## What the hardware and the drivers say

- Qualcomm: do all `vkCreateGraphicsPipelines` calls during initialization.
  The framerate hitches from shader compilation go away.
- Qualcomm: the Adreno shader instruction cache on A7xx is 127 instructions,
  and a performance drop appears at every multiple of 2000 shader instructions
  on the graphics queue.
- Turnip exposes `VK_EXT_pipeline_creation_cache_control` and
  `VK_EXT_pipeline_creation_feedback`. Upstream Vita3K and Plus use neither.
  Cache control is the extension that lets the app say "fail instead of
  compile".
- `VK_KHR_pipeline_binary` is not exposed by Turnip. There is no way to store a
  compiled pipeline and load it back.
- Turnip's own disk cache is what `MESA_SHADER_CACHE_DIR` points at. Balemuni's
  Mesa build raises it from 1 GB to 4 GB for this reason.
- Plus adds a log line before every pipeline compile on the stock Adreno
  (`vita3k/renderer/src/vulkan/pipeline_cache.cpp` and the `4ecb21da` change),
  and it is compiled in unconditionally. Check the cost of that log line at
  info level.

## Steps

1. Measure the stutter first. From ticket 02, `frames.csv` gives the frame
   interval distribution. Compare a cold run against a warm run on the same
   scene and count intervals over 1.5 times the target frame time. This is the
   number to reduce.
2. Count compiles per scene with a counter behind `perf-log`, and log the
   compile duration per pipeline. The longest single compile sets the worst
   hitch.
3. Check `async-pipeline-compilation`. It defaults to `false` on this base and
   `true` upstream. Upstream PR 3169 says to turn it off for the few games with
   long-standing graphical problems. Run A/B/A on the benchmark titles and
   record FPS, the frame interval 99th percentile, and the maximum interval.
4. Use `VK_EXT_pipeline_creation_cache_control` where it is present: mark
   pipelines that are already in the disk cache as fail-fast, so a cache hit
   never enters the driver. Combine with the refused-pipeline list that Plus
   already keeps. Measure. The extension is absent from the stock driver's
   documented set, so read ticket 01 step 4 before building anything on it.
5. Time the per-compile log line that Plus adds on the stock Adreno driver.
   It is unconditional and not behind a config value: the `LOG_INFO` at
   `vita3k/renderer/src/vulkan/pipeline_cache.cpp:1211` is guarded only by
   `state.is_adreno_stock`. Put it behind a config value,
   default on, and run A/B/A. At info level there are other unconditional lines
   in the frame path as well, so compare against a run at `log-level: 1` as
   well, since ticket 05 covers that setting for the same reason.
6. A full precompile that builds pipelines before the first frame needs a new
   file format. Only plan it if steps 1 to 5 leave a large number and there is
   time. Say so in the answer rather than starting it silently.

## Acceptance

- Cold-run stutter measured, with the count and duration of compiles.
- A measured result for `async-pipeline-compilation` and for cache control.
- A statement of what is left after these changes, in numbers.

## Answer

## Comments
