# 12: Measure and reduce pipeline compile stutter

Status: open
Type: experiment
Label: ready-for-agent
Blocked by: 07

## Problem

Pipelines are compiled when a draw first needs them. Pipelines from the
disk cache are compiled on the render thread on purpose
(`vulkan/pipeline_cache.cpp:951`, `:978`). A new pipeline with async
compile on is skipped for that draw (`vulkan/scene.cpp:409-411`). A review
of the Pocket S reports stutter while Vita3K compiles shaders.

The disk cache holds only hashes and the Vulkan cache blob (`:281-330`).
It has no data to build pipelines before first use. A full precompile
needs a new file format and is not part of this ticket.

## Steps

1. **Measure.** Count synchronous compiles and their total time per
   second. Write them to a CSV when `perf-log` is on. Measure on a first
   play (cold cache: delete `cache/shaders/<title id>/` under the app's
   files folder; the path is built at `renderer/include/renderer/state.h:234`,
   check the real `cache_path` on the device first) and on a second play (warm cache), without the warm-up run.
2. **Gate.** Continue only if compiles take more than 2 ms in any frame on
   the warm cache, or cause frame intervals over 50 ms on the cold cache.
   Otherwise set `Status: rejected` with the numbers.
3. **Fewer pipelines.** Test Plus commit `bf961274` (`hash_pipeline_record`:
   it clears record fields that do not change the pipeline, so fewer
   duplicate pipelines are built). Take only that part of the commit.
4. **Worker count.** On the cold cache, test 2, 3 and 4 compile workers
   (`:228-239`), with a temporary config value.
5. Take one screenshot per run in the scene. The user checks for missing
   geometry in session H3.

## Output

Under `## Answer`: compiles per play and their time, frame intervals over
50 ms, cold and warm, for each change. Keep what lowers them without
missing geometry.

## Answer
