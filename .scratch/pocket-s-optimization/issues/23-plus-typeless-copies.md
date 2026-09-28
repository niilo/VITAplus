# 23: Take the Plus fix for typeless copies above 1.75x

Status: claimed
Claimed: 2026-09-28 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent
Blocked by: 19

## Context

Ticket 01: without it, the output is wrong above 1.75x resolution.
Ticket 14 tests 2x and 2.5x.

Commits, in order: 7531b756, e5f0e276.

## Steps

1. Branch `pocket-s/23-plus-typeless-copies` from `master`.
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
