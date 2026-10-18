# 03: A uniform buffer size disagreement corrupts rendering from 651f8a23

Status: open
Type: task
Label: ready-for-agent

## Context

This is a better-identified regression than the boot stall in issue 01, and it
may be its cause. The boundary is one commit and was found by deploying
revisions to the Ayaneo Pocket S with a clean shader cache.

| Commit | Result |
| --- | --- |
| `e6bc600c` | boots, renders correctly |
| `651f8a23` | boots, **blinking textures, wrong colours, some of it flickering** |

`651f8a23`'s parent is `e6bc600c`, so the boundary is exact.

## What the commit changed

`651f8a23` "Size the default uniform buffer from the renderer, not the GXP
header". Every site that sized the default uniform buffer now reads
`renderer_data->uniform_buffer_sizes.at(SCE_GXM_DEFAULT_UNIFORM_BUFFER_CONTAINER_INDEX)`
instead of the GXP header field `default_uniform_buffer_count`, and
`sceGxmReserveFragmentDefaultUniformBuffer` and
`sceGxmReserveVertexDefaultUniformBuffer` stopped returning a null buffer with
success. Files: `vita3k/modules/SceGxm/SceGxm.cpp` and
`vita3k/shader/src/gxp_parser.cpp`.

The commit's own text names the hazard and one unresolved defect:

> The two ring-buffer advance sites have to change with them or the offsets
> desync from the reservation.

> `sceGxmProgramGetDefaultUniformBufferSize` multiplies a float-unit count by
> `sizeof(Ptr)` and so double-counts on a 64-bit host. Left alone, it is a
> guest-visible query.

## The mechanism this points at

`sizeof(Ptr)` is 8 on a 64-bit host. If the game sizes its uniform ring by
what `sceGxmProgramGetDefaultUniformBufferSize` returns, while the emulator
reserves and reads by `uniform_buffer_sizes`, then the game's advance and the
emulator's reservation disagree by a factor of two from the first frame.
Uniforms land in the wrong slots every frame. That is wrong colours, and
flickering textures as different draws use different programs.

This is read from the diff and the symptoms. It is not measured. The check is
small: for one program, log what the reserve hands the game against what the
draw reads, and the same program queried through
`sceGxmProgramGetDefaultUniformBufferSize`.

## Why it may be the cause of the boot stall

A shader fed garbage uniforms produces garbage at boot. A game that waits on a
boot-time render result will time out, which is the stall in issue 01:
`sceKernelWaitThreadEnd` returning `WAIT_TIMEOUT`, the renderer idle with
`SetFrameBuf calls=648 accepted=648`, no further flips.

If that is right, it explains three things the stall investigation could not:
why the newer builds stall while the older ones boot, why the bisect kept
landing on commits with nothing to do with the audio path, and why the stall
arrived without any audio file changing.

It is a hypothesis with a test: on a build that still boots, check whether a
boot stall and visual corruption move together. `651f8a23` is that build.

## Plan

1. Check the ring-buffer advance sites against the reservation. The commit says
   they must change together; confirm both changed and that the arithmetic is
   in the same units.
2. Check `sceGxmProgramGetDefaultUniformBufferSize`. If it still multiplies by
   `sizeof(Ptr)`, that is a guest-visible double count and a defect on its own,
   independent of everything else here.
3. Decide the single source of truth for the default uniform buffer size. Today
   the guest is told one size and the renderer reserves another. Whichever is
   right, the query and the reservation must return the same number.
4. Test on the device with a clean shader cache each time: deploy
   `651f8a23`, `53d34c60` and master, and compare the picture. A cache written
   by another build confounds the result, and `00514fb7` raises the cache
   version, so entries are not portable between revisions.
5. If fixing the size removes the boot stall, close issue 01 with that as the
   cause rather than with the audio path.

## The stall was not this, but the flicker is still open

Step 5 is answered: no. The boot stall was `_sceKernelWaitThreadEnd` taking a
deadline from the guest, found in issue 02 and fixed in `bb7086df`. This
regression is a separate defect and it is still open.

What changed since this ticket was written. `651f8a23` was reverted in v1.3.7,
by `09f207d3`, which reset master's tree to `bb7086df`. So the corruption is not
on the device any more, and steps 1 to 3 are the whole job.

The revert removed both halves of `651f8a23`, which means the `gxp_parser.cpp`
half is gone too. That half is the pa-register inference, and it is what fixes
the Jak and Daxter black screen recorded in
`.scratch/game-sweep-2026-09-30/issues/05`. So the correct end state is a size
that satisfies both, and it has to be built rather than picked: revert or keep
decides the flicker but also decides that black screen.

One correction to the reasoning above. This ticket treated `651f8a23` as
sufficient to explain the flicker on its own. It is not. Reverting it alone, in
`1aea6648`, did not stop the flicker on the device. After that revert
`SceGxm.cpp` still differed from `e6bc600c` by 24 lines, from `53d34c60`, and
`vita3k/shader/` by 11 files, from `2927abba`. So if this ticket is picked up,
the flicker may well be `53d34c60`, which touches `SceGxm.cpp` and both Vulkan
caches, rather than the size disagreement here. Re-land `53d34c60` first, with a
device run, before spending effort on this.

## Files

- `tmp/deploy-651f8a23` the run that showed the corruption
- `tmp/deploy-e6bc600c-clean` the parent, clean
- `tmp/deploy-593351a9-retry` the earlier good build, clean

## Comments

### 2026-10-18: found by deploying revisions on the device

Every comparison was made with `files/cache`, `files/shaderlog` and
`files/textures` emptied first. The earlier comparisons in issue 02 were not,
which is part of why their verdicts did not hold up.