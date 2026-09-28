# 02: Decide the code base

Status: resolved
Claimed: 2026-09-28 Claude Code session (Opus 5.5)
Type: grilling
Label: ready-for-human
Blocked by: 01

## Question for the user (session H2)

Which code does the Pocket S work start from?

1. **Keep `master` and cherry-pick** the Plus commits and local `feat/*`
   commits that ticket 01 lists. Smaller change, easier upstream merges.
   Each pick needs its own build and check.
2. **Base the fork on `plus/all-enhancements`,** plus the chosen local
   `feat/*` commits. Gets all Plus fixes at once, including page table
   fixes and thread scheduling. Upstream merges become harder.

## Agent steps

1. Show the user the ticket 01 table and recommendation. Ask the question.
2. Write the user's answer under `## Answer`, word for word.
3. Create task tickets for the answer, numbered after the last ticket:
   - Option 1: one ticket per commit or group of commits to pick. Each one
     uses `git cherry-pick -x`, builds both targets, and boots one game on
     the device.
   - Option 2: one ticket for the rebase or merge, with the same steps.
4. Add every new ticket number to the `Blocked by:` line of ticket 06, so
   the baseline is taken on the final code base.

## Answer

The question was asked on 2026-09-28 with two decisions: (1) the code base,
options 1 and 2 as above; (2) for ticket 11, use FIFO as the v-sync default
(1) only on Android, or (2) everywhere. The user's answer, word for word:

> 1

The agent reads this as option 1 for both decisions: keep `master` and
cherry-pick, and use the ticket 11 present mode change only on Android.
Both were the recommended options.

New tickets for option 1: 19, 20, 21, 22, 23. Ticket 06 is now also
blocked by them.
