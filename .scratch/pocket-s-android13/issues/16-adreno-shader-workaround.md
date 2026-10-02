# 16: Test the Adreno shader compiler workaround

Status: open
Type: task
Label: ready-for-human
Blocked by: 01

## Problem

Upstream Vita3K PR 4141 works around a crash in the Qualcomm shader compiler.
The driver SIGSEGVs inside `libllvm-qgl` during `vkCreateGraphicsPipelines` on
Adreno 650 when a vertex shader partially writes lanes of a constant-indexed
element of a `Private` `vec4` array of 19 or more elements. Eighteen is safe,
nineteen crashes. The PR shrinks `outs` to 16 `vec4` on Adreno and adds an
`adreno-workaround` config setting. The PR is closed and was not merged.

`REG_O_COUNT` is `20 * 4` in `vita3k/shader/src/spirv_recompiler.cpp:64` on
this base.

## Scope after ticket 00: this is stock-driver compatibility, not a lever

The plan measures on Turnip. Ticket 00 measured the stock driver at 5.76 FPS
against Turnip's 29.96 in gameplay, so a fix that only helps the stock driver
cannot move a number in this plan's records.

This ticket is **compatibility work for the driver some users will still be
on**, not an optimisation. Run it only as a labelled out-of-protocol
compatibility check on the compile-heavy title, the way ticket 04 keeps one
stock spot-check. If the answer is "no crash on this GPU", the ticket is
`rejected` and that is the whole result.

Do not schedule it ahead of a ticket whose result reaches the plan's records.

## Steps

1. Confirm the shape. Read the declaration of `outs` and the `REG_O_COUNT`
   define, and check whether the Plus base changed them.
2. The crash is on the stock driver, in the driver's own compiler, not in ours.
   So the test is: run a game that compiles many vertex shaders on the stock
   driver, with and without the change, and see whether the process survives.
   Do not test this on Turnip. Turnip has its own compiler and does not have
   this bug.
3. Add the workaround behind a temporary config value, default off, and set the
   array size to 16 `vec4` when it is on. Use the driver check the
   tree already has, `state.is_adreno_stock`, which is
   `major_driver_version >= 100` (`renderer.cpp:665`) and is what the ported
   Plus log line uses (`pipeline_cache.cpp:1210`). Do not add a second, different
   driver test.
4. If it works, add a bit to the shader feature mask
   (`vita3k/renderer/src/vulkan/renderer.cpp:1537`) so the shader cache
   invalidates once. Leaving the old cache in place would load shaders that
   still have the 20-element array and crash anyway.
5. If the crash does not reproduce on the A32, set `Status: rejected` and write
   down what was run. The A32 may have a fixed compiler.

## Acceptance

- A verdict on whether this GPU has the compiler bug. Use the name ticket 01
  recorded for the part, not a name from a research document.
- If it does: the workaround behind a config value, the feature mask bit, and a
  note in the `uncharted_scene.sh` bullet of `CLAUDE.md`, which is where the
  driver advice already lives.
- If it does not: `Status: rejected` with the games and the driver used.

## Answer

## Comments
