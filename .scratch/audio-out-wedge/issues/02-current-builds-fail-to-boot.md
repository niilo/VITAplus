# 02: Current builds stop Uncharted booting on the Pocket S, older builds do not

Status: closed
Type: experiment
Label: ready-for-human
Closed by: v1.3.7, cause found and fixed

## The conclusion below is superseded

This ticket recorded that no commit could be named. That was wrong, and it is
wrong in its first half. The stall had a cause and it was nine lines in
`_sceKernelWaitThreadEnd`:

```cpp
const bool woken = waiter->wait_for_run_precise(waiter_lock,
    timeout ? static_cast<int64_t>(*timeout) : -1);
if (!woken) {
    ...
    return RET_ERROR(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
}
```

`bc2d191a` made that function honour the guest's timeout, where the emulator had
always ignored it. Uncharted calls it during boot with a timeout the emulator now
treats as a deadline. The call returns `WAIT_TIMEOUT`, that thread is never
runnable again, and the main thread waits on it with the renderer idle and no
framebuffer flips. The conversion also had a defect of its own: the guest timeout
is a `SceUInt` in microseconds cast straight through, so a zero became a budget
of zero rather than no budget.

The whole rest of this ticket is kept as the measurement record. The counts, the
run table and the bisect refutation are all accurate for what was measured at the
time.

## Why the search failed, which is the transferable part

Nothing here was a hardware quirk and nothing was flaky. Three method errors, in
order of how much they cost:

**The window was wrong.** The bisect ran between `593351a9` and `a99ad6f6` and
returned `a99ad6f6`. That boundary is not where the stall is. Chronologically
`651f8a23` comes first and it boots, with a separate visual defect, then only
merges and docs, then `bc2d191a`, which stalls. Two unrelated defects sat in one
window and a bisect read the pair as one boundary.

**One sample per step is unsound for a stall.** The stall is deterministic, but
the *build* is a timing artefact, so a commit that shifts timing produces the
same kind of boundary as a commit that breaks something. Re-testing a step that
came back good does not distinguish the two, it only samples again.

**The verdict was assumed stable, so the search was one-sided.** Once a revision
was marked bad it stayed bad. A stall near the edge of a timing window reads
that way and the search has no way to notice.

What worked instead was rebuilding from a known-good base and removing one hunk
per build. Five device runs, one commit apart, each removing a single change:

| Build | Result |
| --- | --- |
| base, none of `bc2d191a` | boots |
| base, all of `bc2d191a` | stalls |
| base, minus `display.cpp` | stalls |
| base, minus the condvar ownership check | stalls |
| base, minus the thread-end timeout | boots |

That costs five runs and found it. The bisect cost about the same and did not.
The difference is that a single-hunk removal names the hunk, while a bisect only
names a boundary and cannot distinguish two defects sitting in the same window.

## What was asked and what was found at the time

The question was which commit degraded the quality of the device runs. The
answer given at the time was that no commit did, at least not one that a bisect
can name.

A build 25 commits behind master, `00514fb7`, boots Uncharted to the saved
chapter on this device every time it is tried. Builds at or after the current
work stop at boot nearly every time. The degradation is therefore real and it
is in that range of commits.

It is not a frame rate regression, and the numbers say so. With the warmup
`docs/agent-loop.md` asks for, 120 s:

| Build | Average FPS | draws per scene |
| --- | --- | --- |
| `bccd7768`, the one current run that booted | 29.93 | 39.56 |
| `00514fb7`, first run | 22.46 | 38.45 |
| `00514fb7`, second run | 27.73 | 32.38 |

The current build is the faster of the two where it works at all, and
29.93 FPS matches the 30 FPS `CLAUDE.md` records for Turnip at resolution
multiplier 2. What regressed is that the game boots, not how fast it runs.

## The runs

Driver `MesaTurnip` unless the row says otherwise, page-table mapping,
resolution multiplier 2, `log-level 1`. A run counts as booted when the audio
thread produced `[NGSRATE]` lines, which it does within seconds of the title
screen; a run that stalls produces none.

| Time | Build | Result |
| --- | --- | --- |
| 20:30 | `bccd7768`, stock driver | crashed 16 s into boot, `Refused read to INVALID guest address 0x17FFF` in `sceGxmSetUniformData` |
| 20:35 | `bccd7768`, stock driver | black, no crash |
| 20:40 | `bccd7768` | **booted**, 29.93 FPS, `run_is_valid.sh` VALID |
| 20:47 | `bccd7768` | stalled |
| 21:10 | `bccd7768`, Killzone Mercenary | stalled at the title screen |
| 21:20 | `c6c3983e` | stalled |
| 21:25 | `c6c3983e` | stalled |
| 21:31 | `c6c3983e` plus the renderer work | stalled |
| 21:35 | `c6c3983e` plus the renderer work | stalled |
| 21:42 | `c6c3983e`, Dariusburst | black |
| 21:57 | `c6c3983e` | stalled, with 18 GB free on the card |
| 22:13 | `c6c3983e`, after a reboot | stalled |
| 22:31 | `c6c3983e`, caches cleared | stalled |
| 23:37 | `00514fb7` | **booted**, 22.46 FPS |
| 23:5x | `00514fb7` | **booted**, 27.73 FPS |
| during the bisect | `593351a9` | **booted** |
| during the bisect | `a99ad6f6`, retested | **booted**, 3169 `[NGSRATE]` lines |
| three revert steps | master | stalled, see below |

Counted: 16 stalls and 3 boots on builds at or after `bccd7768`, against 4 boots
and 0 stalls on `00514fb7` and the two older recorded runs in `tmp/gameplay/`
and `tmp/uncharted-scene/`.

Three titles fail the same way, which rules out one game's savedata:
PCSA00029 Uncharted, PCSA00107 Killzone Mercenary, PCSE00792 Dariusburst.

## The method, and why the bisect answer is wrong

Each bisect step was judged by building the reldebug APK for that revision,
installing it, launching Uncharted, waiting 100 s and reading the emulator's own
log. No taps and no menu walk, so a step costs about four minutes.

`git bisect start master 00514fb7` returned `a99ad6f6` as the first bad commit,
with its parent `593351a9` good. The boundary is adjacent, which is what makes
it convincing, and it is wrong:

1. Retesting `a99ad6f6` on this device boots Uncharted: 3169 `[NGSRATE]` lines,
   no hang watchdog. A commit cannot be the deterministic cause of a stall that
   a build containing it did not hit, and this build did not hit it here, nor
   at 20:40 today when `bccd7768` booted at 31.21 FPS.
2. Reverting it on master does not restore the boot, in three steps: the five
   `if (!system) return SCE_NGS_OK` returns; then those plus
   `sceNgsPatchCreateRouting` and `sceNgsVoiceGetOutputPatch`; then all nine
   returns the commit introduced, including the `if (!patch) return SCE_NGS_OK`
   in the three `sceNgsVoicePatchSetVolume*` exports. All three still stalled.

A commit that shifts timing produces the same kind of bisect boundary as a
commit that breaks something, and one sample per step cannot tell them apart.
That is the whole result of this ticket: the method is not sound for a stall
whose probability is below 1.

## What was open, and what happened to it

Which change makes the stall likely. Two ways forward were offered, and both
were the wrong shape:

- Repeat the search with two or three runs per step, thirty device runs, which
  still only narrows the range. It would have narrowed it to the same window the
  first bisect already had, and the cause is in that window.
- Work the code plan in issue 01 and find what the emulator waits on inside
  `SDLAudioAdapter::audio_output`. That plan assumed the stall was the audio
  port. It was not, and no amount of time inside `audio_output` would have found
  it, because `audio_output` was waiting for a guest thread that had already
  given up. The symptom looked like an audio hang because the audio thread was
  where the guest was parked, not because audio was the cause.

The stall itself is recorded in issue 01, with the thread dumps. Those dumps are
consistent with the real cause and were the evidence that pointed at it: the main
thread's `last_import_nid` is `sceKernelWaitThreadEnd`, which is the function
that was changed. Reading the dump as "who is waiting on whom" rather than "what
broke" is what eventually named the commit.

Issue 01 stays open. `sceAudioOutOutput` not returning may still be a real
defect in its own right; it is simply not this stall.

## Files

- `tmp/gameplay/ngs-keyoff-turnip` the only current build that booted
- `tmp/gameplay/old-00514fb7`, `old-00514fb7-b` the older build, twice
- `tmp/gameplay/after-reboot`, `space-turnip` current builds that stalled
- `tmp/kz-walk` the Killzone walk and the cache-clear run
- `/tmp/opencode/bisect_judge.sh` the per-revision judge, one APK, one launch,
  one log read

## Comments

### 2026-10-15: recorded after the session that found it

Every number above was read from a log or from `perf_summary.py` output that is
in `tmp/`. Nothing here is estimated. The FPS figures use `--warmup 120`; the
per-run `perf.txt` files in those directories use the default warmup and read
higher, which is why the same runs appear as 31.21 and 29.93.

### 2026-10-18: the cause, found by removing one hunk at a time

Closed. `_sceKernelWaitThreadEnd`, taking a deadline from the guest. Fixed in
`bb7086df`, in master as part of `09f207d3`, released as v1.3.7.

Rebuilt from `e6bc600c`, the last revision that boots, and removed one hunk from
`bc2d191a` per build rather than bisecting:

| Build | Result |
| --- | --- |
| base, none of `bc2d191a` | boots |
| base, all of `bc2d191a` | stalls |
| base, minus `display.cpp` | stalls |
| base, minus the condvar ownership check | stalls |
| base, minus the thread-end timeout | boots |

Three of the four hunks turned out to be fine, and the lock-order fix in the
same function was kept. Only the deadline was the stall.

The one thing worth carrying to the next stall: the thread dump recorded the
function to change. `PCSA00029 main` sitting in `sceKernelWaitThreadEnd` while
the renderer was idle was the whole answer, and it had been in issue 01 from the
first session. What was missing was reading a hang dump as a statement about
which call changed behaviour, rather than as a statement about audio.

One more thing that cost time. A dirty shader cache on the device produced a
false failure early on, and `files/cache`, `files/shaderlog` and `files/textures`
have to be emptied before every deploy. Two of the runs in the table above are
from before that was established, and the counts in "The runs" include at least
one run that should not be counted at all.