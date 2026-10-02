# 08: Measure the macroblock sync cost

Status: open
Type: research
Label: ready-for-human
Blocked by: 04

## Question

A render target created with `SCE_GXM_RENDER_TARGET_MACROTILE_SYNC` restarts the
render pass on every macroblock change (`vita3k/renderer/src/vulkan/context.cpp:699-736`).
In the fallback path it also loads and stores depth and stencil every time
(`context.cpp:707`). A 960x544 target with 4 macroblocks in each direction is
16 render pass restarts. What does that cost on a tile renderer, and does any
benchmark title use it?

## Context

- The flag is read at `vita3k/renderer/src/creation.cpp:125`.
- The macroblock count comes from the render target flags
  (`creation.cpp:128-133`), between 1 and 4 in each direction.
- When the scissor is larger than one macroblock the code takes a slow path:
  one scene per draw (`context.cpp:703`), with `ignore_macroblock = true`.
- When `use_texture_viewport` is on, the code stops the render pass without
  stopping and restarting recording (`context.cpp:727`).

## Steps

1. Add a counter for render pass restarts caused by a macroblock change and one
   for the slow path, behind the `perf-log` setting from ticket 02. Read them
   from `scenes.csv`.
2. Run each benchmark title and record both counters.
3. For any title with a non-zero counter, run the A/B/A protocol twice, with
   `high-accuracy` off and then on, and record both.
   The second half of the old step used `disable-programmable-blending`, which
   ticket 05 rejected: it buys nothing and it destroys the picture. So there is
   no way to separate `use_texture_viewport` from the fetch path on this device,
   because `high-accuracy` is the only knob and it moves both. **Report that as
   the limit of what this ticket can separate.**
   Record also whether `use_texture_viewport` was even on: with
   `high-accuracy: true` it is off, so the macroblock branch at
   `context.cpp:727` is not taken at all. If no benchmark title sets
   `SCE_GXM_RENDER_TARGET_MACROTILE_SYNC`, this ticket closes at step 2, which
   is the likely outcome.
4. Do not add a config value that turns macroblock sync off. It changes the
   picture. If the cost is large, write down what the cost is and let the
   later tickets decide.

## Acceptance

- Per title: render pass restarts per frame from macroblock sync, and the
  measured cost where the count is non-zero.
- A statement of whether this path matters for the benchmark set or only for
  one game.

## Answer

## Comments
