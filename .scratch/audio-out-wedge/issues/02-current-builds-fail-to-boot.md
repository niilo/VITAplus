# 02: Current builds stop Uncharted booting on the Pocket S, older builds do not

Status: open
Type: experiment
Label: ready-for-human

## What was asked and what was found

The question was which commit degraded the quality of the device runs. The
answer is that no commit did, at least not one that a bisect can name.

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

## What is open

Which change makes the stall likely. Two ways forward, and they cost different
amounts:

- Repeat the search with two or three runs per step. That is about thirty
  device runs and it still only narrows the range.
- Work the code plan in issue 01, which does not need to know which commit
  changed the timing: find what the emulator is waiting on inside
  `SDLAudioAdapter::audio_output`, which `sceAudioOutOutput` calls. Its
  `cond_var.wait_for` is bounded, so the suspicion falls on
  `SDL_PutAudioStreamData` blocking because nothing drains the stream, and on
  `reset_android_session_audio` closing and reopening the device behind a port
  the guest still holds.

The stall itself is recorded in issue 01, with the thread dumps. This ticket is
the measurement record and the answer to the question that was asked.

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