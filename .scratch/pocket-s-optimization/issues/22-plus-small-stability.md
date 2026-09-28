# 22: Take three small Plus stability fixes

Status: claimed
Claimed: 2026-09-28 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent
Blocked by: 19

## Context

Ticket 01: a validation error fix in the pipeline cache, a guard against
a crash on exit, and a small wake-up fix in the sync code.

Commits, in order: 3776c598, 6ecce835, 9f4c139f.

## Steps

1. Branch `pocket-s/22-plus-small-stability` from `master`.
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

Picked 3776c598, 6ecce835 and 9f4c139f on
`pocket-s/22-plus-small-stability` and merged to `master`. No conflicts.
The Linux build passes on the branch.

Linux build, `container/vita3k.sh test` and `format-check` pass on `master`
after the merge (33c47a42). The Android release build of that `master` passes.

Still to do on the device: boot one game and copy the Vulkan log lines.
