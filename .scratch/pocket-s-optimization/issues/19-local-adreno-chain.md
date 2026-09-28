# 19: Take the local Adreno Vulkan chain

Status: resolved
Claimed: 2026-09-28 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent
Blocked by: none

## Context

The local branches `feat/ayaneo-pocket-s-performance`,
`feat/vulkan13-adreno` and `feat/vulkan-device-profiles` form one chain.
Ticket 01 found that the chain applies to `master` with no conflict.

Commits, in order: 1838cd88, be6c7690, d9f38adf, dfb60519, dccbcc6e,
604ea188, 64c694ae, 533f063f, feb21710. Do not take 541499ee (CI comment)
or the merge 7526bdba.

After the merge, rebase the open branches of tickets 03 and 11 on the new
`master`:

- Ticket 03: 1838cd88 and 64c694ae already change the validation default.
  Keep only what the chain does not already do.
- Ticket 11: 1838cd88 has its own `select_present_mode()`. Keep the ticket
  11 version. Use the new rule only on Android (ticket 02 answer). Other
  systems keep the old order: MAILBOX, then FIFO_RELAXED, then FIFO.

## Steps

1. Branch `pocket-s/19-local-adreno-chain` from `master`.
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

Picked on `pocket-s/19-local-adreno-chain` and merged to `master`
(merge f74626e7). All 9 commits picked with `-x`.

- 1838cd88: conflict in `config.h`, because `perf-log` (ticket 05) sits
  next to `validation-layer`. Kept both lines, with the new default macro.
- The other 8 commits applied with no conflict.
- 604ea188 was not formatted for clang-format 22. An extra commit formats
  `renderer.cpp`.

Branch 03 is now empty after a rebase (see ticket 03). Branches 08 and 11
are rebased on `master` (see those tickets).

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

Problem found: in 2 of 9 starts, `createDevice` failed with
`ErrorFeatureNotPresent`, and the app stopped. The other 7 starts, with the
same APK and config, worked. With `validation-layer: true` the start
worked and the layer reported no feature error. Commit a778c289 (merge
886c16b4) retries with fewer features and logs each requested feature
that the driver does not report. The failure did not happen again in 7
starts with that build, so the cause is still not known. If it happens
again, the log names the feature.

Other finding: the version string in the log says
`4166-a52121af-niilo/pocket-s/11-present-mode-vsync` for APKs built from
`master`. The build folder keeps the git description from the last
configure. Record the build commit from `git`, not from the log.
