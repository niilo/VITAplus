# Agent loop for the Android 13 performance tickets

How an agent implements and verifies a ticket from
`.scratch/pocket-s-android13/` without a person. Read
`.scratch/pocket-s-android13/spec.md` first, then the ticket itself.

## What can be done without the device

Every ticket splits into a code part and a measurement part. The code part is
what an agent does. The measurement part needs the Ayaneo Pocket S, so it stays
`open` until a human runs it.

| Ticket | Code part an agent can do | Needs a device |
|---|---|---|
| 02 | Port `perf-log`, the channel API, `perf_summary.py` | No |
| 03 | The manifest flag, `device.sh` commands, `docs/adr/` | No |
| 09 | Port the present mode change, behind a setting | Yes, for the A/B |
| 10 | Port the steady clock and absolute deadlines | Yes, for the A/B |
| 11 | `setpriority` in the two threads, behind a setting | Yes, for the A/B |
| 12 | `madvise(MADV_HUGEPAGE)` in both mappings | Yes, to see the gain |
| 13 | The ADPF session, behind a setting | Yes, to see the gain |
| 14 | Read the dynarmic default, make the size a setting | Yes, for the A/B |
| 15 | The three layout choices, behind a setting | Yes, for the A/B |
| 16 | The shader array size, behind a setting | Yes, to see the crash |
| 25 | The four flags, one setting each | Yes, for the A/B |
| 07 | Read the flags, add the setting | Yes, for the stall |
| 26 | The four-way counters | Yes, for the split |
| 27 | The per-pipeline counters | Yes, for the counts |
| 31 | Count the resampler initialisations, behind a temporary setting | Yes, for the power A/B |
| 32 | Name the unnamed samples and attribute each host cost | No |

A ticket that names its blockers as **code** dependencies is unblocked once that
code has merged, even if the ticket is still `claimed` because a device
measurement is outstanding. `.scratch/pocket-s-android13/map.md` records which
tickets that applies to. A mechanical scan of `Status:` will skip them, so read
that section before deciding the frontier.

The remaining tickets are measurement only, or need a change that no
measurement yet justifies.

## The loop

For each ticket:

1. **Claim.** Set `Status: claimed` and add `Claimed: <date> <agent>`.
   Commit and push before any work, so another agent sees the claim. If the push
   fails because another agent pushed first, rebase and re-read.
2. **Read.** The ticket, then the code it names. Confirm the `path:line`
   references still point where the ticket says. A reference that moved means
   the ticket was written against another base; note it in `## Comments`.
3. **Branch.** `git switch -c <ticket-slug>`, for example
   `perf-log` for ticket 02 or `huge-pages` for ticket 12.
4. **Implement.** Only what the ticket says. Every behaviour change goes behind
   a config value that defaults to today's behaviour, so the change is a
   measurement and not a merge. Add the setting to `config.h`, `native_config.cpp`
   and `EmulatorConfig.kt` when the ticket says the Android app needs it.
5. **Verify, in this order.** Each step must pass before the next:
   ```sh
   container/vita3k-docker.sh format     # format what you touched
   container/vita3k-docker.sh build      # configure if needed, then build
   container/vita3k-docker.sh test       # mem-tests, module-tests, ngs-tests
   container/vita3k-docker.sh format-check
   container/vita3k-docker.sh android reldebug   # the APK builds too
   ```
   `python3 tools/android/test_perf_summary.py` and
   `python3 tools/android/test_fps_sample.py` run on the host, not in the
   container. Run them when the ticket touches those files.
6. **Test the change itself.** A ticket that adds a counter or a CSV column
   needs a test. `tools/android/test_perf_summary.py` covers the summary. For
   anything else, add a googletest in the suite the code belongs to
   (`vita3k/mem/tests`, `vita3k/module/tests`, `vita3k/ngs/tests`) and register
   it in ctest as the existing suites are.
7. **Review.** Read your own diff as a reviewer who did not write it. Ask:
   - Does the default path behave exactly as before? Prove it, do not assume it.
   - Does the new setting reach the Android app? Check all three files.
   - Is any line reference in a comment now wrong?
   - Does the code follow the writing standard in `CLAUDE.md`?
   Then get a second opinion from a subagent before merging.
8. **Merge.** `git switch master && git merge --no-ff <branch>`, then delete
   the branch.
9. **Update the ticket.** Move the code result into `## Answer`. If a
   measurement is still missing, leave `Status: claimed` and say which number a
   human has to supply. Do not set `resolved` on code alone.

## Rules

- Do not change behaviour on any other device. A default that moves is a
  breaking change for every user.
- Do not touch the files in `.signing/`, and never put a password from them on
  a command line.
- One ticket per branch. Do not fix a second ticket in the same branch; open
  another branch.
- If a measurement shows a change does not help, that is a result. Write it in
  `## Answer` and set `Status: rejected`. A rejected ticket counts as done.
- Commit messages follow the writing standard in `CLAUDE.md`. State what
  changed and why, and name the ticket.

## The environment

`container/vita3k-docker.sh` needs no device and no person. It builds the Linux
image from `container/linux.Containerfile` and the Android image from
`container/android.Containerfile`, both on `linux/amd64`. The repo is mounted at
`/src`, so build output lands in the normal `build/` folder on the host.

```sh
container/vita3k-docker.sh image linux      # build the image once
container/vita3k-docker.sh build           # Linux build, preset container-linux
container/vita3k-docker.sh test            # ctest
container/vita3k-docker.sh format-check    # the same clang-format check as CI
container/vita3k-docker.sh android release # arm64 release APK
container/vita3k-docker.sh run <cmd...>    # any command in the Linux container
container/vita3k-docker.sh shell           # an interactive shell
```

`VITA3K_CONTAINER_CPUS` and `VITA3K_CONTAINER_MEMORY` set the container size.
The container default of 1 GiB is too small to link, so the script passes 16G
by default. The ccache and Gradle caches live in volumes, so a clean container
reuses earlier compiles.