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
3. For any title with a non-zero counter, run the A/B/A protocol twice. Once
   with `high-accuracy` off, and once with `high-accuracy` on and
   `disable-programmable-blending` on. The first run changes two things at once:
   it turns `use_texture_viewport` on
   (`vita3k/renderer/src/vulkan/renderer.cpp:1086`) and it switches
   programmable blending from shader interlock to subpass input
   (`renderer.cpp:1068`). The second run holds the texture viewport off, so the
   difference between the two runs is the texture viewport branch at
   `context.cpp:727`. Report both. `disable-programmable-blending` also loses
   blending accuracy, so take screenshots at three fixed moments per title in
   A and B for the second run and write down every difference.
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
