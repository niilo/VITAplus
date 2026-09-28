# 19: Take the local Adreno Vulkan chain

Status: claimed
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
