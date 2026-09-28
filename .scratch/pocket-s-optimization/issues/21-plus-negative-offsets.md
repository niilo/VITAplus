# 21: Take the Plus fixes for negative buffer offsets

Status: resolved
Claimed: 2026-09-28 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent
Blocked by: 19

## Context

Ticket 01: these commits prevent `VK_ERROR_DEVICE_LOST` under Double
Buffer, on the shader side and on the CPU side.

Commits, in order: d2ad1479, 6db85423, fc6048a4.

## Steps

1. Branch `pocket-s/21-plus-negative-offsets` from `master`.
2. `git cherry-pick -x` each commit, in the order below. Resolve conflicts
   so that the result matches the commit's intent. Record each conflict
   under `## Answer`.
3. Build both targets: `container/vita3k.sh build` and
   `container/vita3k.sh android release`. Run `container/vita3k.sh test`.
4. Merge to `master`. The code base decision (ticket 02) keeps these
   commits.
5. On the device: install the release APK and boot one game. Copy the
   Vulkan lines of `vita3k.log` under `## Answer`. If the game does not
   boot, find the commit with `git bisect` and revert it.

## Answer

Picked d2ad1479, 6db85423 and fc6048a4 on
`pocket-s/21-plus-negative-offsets` and merged to `master`. No conflicts.

fc6048a4 did not build: `Ptr(T *pointer, ...)` passes the pointer to
`host_to_guest(const void *)`, and a function pointer does not convert to
`const void *`. An extra commit adds a `reinterpret_cast`, as the old code
had. After that, the Linux build passes.

Linux build, `container/vita3k.sh test` and `format-check` pass on `master`
after the merge (33c47a42). The Android release build of that `master` passes.

Still to do on the device: boot one game and copy the Vulkan log lines.

Device check on 2026-09-28: the release APK from `master` (with
a778c289, see ticket 19) boots Ratchet & Clank (PCSF00484) on the stock
driver to the title screen at 30 FPS. Log lines:

```
Disabling Vulkan validation layers (may improve performance but provides limited error messages)
Vulkan device: Adreno (TM) 740
Driver version: 512.676.0
Qualcomm Vulkan driver classification: stock/other
Vulkan capability path: API 1.3.0, timeline semaphore yes, dynamic rendering yes, synchronization2 yes, extended dynamic state no, descriptor indexing yes, maintenance4 yes, pipeline cache control yes, subgroup size 64
Using a Vulkan timeline semaphore for render completion tracking
Present mode: Fifo (vSync enabled)
Using the following memory mapping method: Double buffer
Pipeline compiler worker policy: 8 logical CPU cores -> 3 workers
```
