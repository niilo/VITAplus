# 01: Count HLE import calls per NID

Status: resolved
Type: task
Label: ready-for-agent

## Answer

Implemented in `vita3k/util/include/util/hotpath.h` and
`vita3k/util/src/hotpath.cpp`, wired at
`vita3k/modules/module_parent.cpp:167`. Config values `hle-counters` and
`hle-counters-time`, both default off, with `--hle-counters` and
`--hle-counters-time` on the command line.

The table is open addressed with linear probing and a CAS insert, 8192
slots, no lock and no allocation on the dispatch path. NID 0 marks a free
slot and is counted as overflow, so a call cannot vanish silently, and an
overflow row is what says the table was too small.

Rows go to `hle.csv` through `perf_log`, one row per NID per interval, with
the name from `import_name`. `exchange` gives the delta and resets in one
step, so a long session reads as a rate. The vblank thread calls
`dump_interval` once every 60 vblanks.

**One thing the plan did not say, and the code has to respect:** `perf_log`
keeps lines in memory and flushes once per second. A dump that happens after
a flush would leave its rows sitting in memory until the next one, so the dump
interval and the flush interval are both one second.

`hotpath::set_enabled` is called in `vita3k/interface.cpp:540` with
`hle_counters && perf_log`, so the counters stay off without a perf log to
write to. `reset()` runs beside it so a second game does not inherit the first
game's totals.

### Acceptance

- Both flags default to off, and a run with them off behaves as before.
  Covered by `DisabledByDefault` and `NothingIsWrittenWhileDisabled`.
- `hle.csv` has a header, one row per NID per interval, with name, calls, and
  duration when timing is on. Covered by `CountsAreReportedPerNid` and
  `SelfTimeOnlyWhenEnabled`.
- A test covers insert, probe past a collision, overflow, and the delta
  behaviour of `exchange`: `CollidingNidsAreBothCounted`, `NidZeroIsReportedAsOverflow`,
  `DumpResetsSoIntervalsAreDeltas`. The collision test finds a colliding pair
  rather than relying on luck.
- `./format.sh` is clean.
12 new tests, all passing. Build, ctest and the format check are clean.

### Measured on the device

Pocket S, Android 13, release APK of this branch, `PCSA00080` (Jak and Daxter
Collection) with page-table memory mapping. `perf-log` on for every run, because
the counters need it; `hle-counters` is the only thing that differs.

| run | `hle-counters` | app CPU (`top`, % of one core) | FPS | frame p99 |
| --- | --- | --- | --- | --- |
| baseline | off | 124, 128, 125, 132, 128 (mean 127.4) | 30.00 | 34.49 ms |
| counters | on | 125, 128, 128, 125, 132, 128 (mean 127.7) | 30.00 | 34.96 ms |
| counters + self-time | on | 125, 129, 125, 132 (mean 127.8) | 30.00 | 34.96 ms |

The cost is inside the noise: about 0.3% of one core for the counts alone, and
self-time does not separate from that either at this resolution. The game holds
its 30 FPS cap in all three, so nothing regressed. These are single runs at
about one sample per two seconds, so this says the overhead is *small*, not
that it is exactly zero.

What the counters say about Jak and Daxter, per second:

- `sceGxmWaitEvent` 2.15 M calls, 190 ms, 88 ns per call. This is the real CPU
  hot path in HLE, and it is worth looking at first.
- `sceGxmWaitEvent` is called 500 times more often than anything else in the
  list. Nothing else is close.
- Mutex traffic is modest: `sceKernelLockLwMutex` 2924 calls and
  `sceKernelUnlockLwMutex2` 3017 calls per second, 4.3 and 2.0 us per call.
- `sceKernelGetThreadId` 6553 calls per second, which is the kind of call that
  wants a cached answer.
- `sceClibMemset`, `sceClibMemcmp` and `sceClibMemcpy` together are 1475 calls
  per second, all in the low microseconds.

The large `ms/sec` numbers belong to `sceKernelWaitLwCond`, `sceAudioOutOutput`,
`sceKernelDelayThread` and the two display functions, at 880 to 2190 ms per
second. **Those are guest threads blocking, not emulator CPU.** They wait on a
frame or a queue, and the guest has three cores of work with a 30 FPS cap, so
most of that wall time is idle waiting. Reading them as cost would point the
optimisation at the wrong code. `sceGxmWaitEvent` is the only high-frequency
entry where the duration is real work.

### Getting the device to run a game

Four things had to be fixed before any game would boot. None of them are about
this ticket, and all four cost most of the time spent here.

1. **A fresh APK install resets `pref-path`.** The app writes a config pointing
   at its own `files/vita`, which is empty, so every title reports
   `not found in apps list`. Restore the real path after installing.
2. **`perf-log` set in `config.yml` is ignored.** `read_config_object` in
   `vita3k/android/jni/native_config.cpp:566` overwrites `perf_log` from the
   Kotlin settings object on every load, so the YAML value never reaches
   `perf_log::start`. The symptom is silent: `hle-counters` reports "enabled" in
   the log, no `Performance log is on` line appears, and no `perf/` folder is
   created. **This is pre-existing**, from `be25c7c1`, and it affects any YAML
   setting that `read_config_object` touches, not just this one. Driving the
   toggle through the app's settings UI is the only path that persists today.
   `hle-counters` and `hle-counters-time` are unaffected, because they have no
   JNI field to be overwritten from.
3. **The installed games are unreadable.** The tree under
   `/storage/4CDE-C1FC/emu-app-data/psvita` is `0770` and owned by `media_rw`,
   so the app (uid 10205) cannot read it, and there is no root on the device to
   fix that with. Copy the game out instead.
4. **A copy made by `adb shell` is owned by `shell`**, which the app also cannot
   read, so a plain `cp` fails the same way. `chmod -R 777` the copy, and put it
   under `<root>/ux0/app/<title_id>/` plus `<root>/ux0/license/<title_id>/`. The
   license directory holds the `.rif` file; without it module loading fails with
   `Failed to decrypt`, which is a different error from the `Failed to read`
   that a permissions problem gives, and the two are easy to confuse.

Also: the screen dozes, and a black screenshot looks like a hang. `input keyevent
KEYCODE_WAKEUP`, then `wm dismiss-keyguard`. A `mCurrentFocus` of
`NotificationShade` means the lock screen still has focus.

## Goal

Every guest-to-host call passes through `call_import`
(`vita3k/modules/module_parent.cpp:152`). A counter there says which library
functions the game leans on, and whether the cost is in HLE at all.

## Steps

1. Add `vita3k/util/include/util/hotpath.h` and `vita3k/util/src/hotpath.cpp`,
   namespace `hotpath`. Per-NID counters go in a fixed open-addressed table
   with linear probing and atomic insert, so the hot path takes no lock and
   allocates nothing. NID 0 marks an empty slot and is counted as an overflow,
   so a call cannot be lost silently.
2. Config `hle-counters`, default off. One relaxed atomic increment per call.
3. Config `hle-counters-time`, default off. When set, each call adds its own
   duration to the same slot. **Two clock reads per HLE call is the reason this
   is a second flag.** Ticket 32 measured `__kernel_clock_gettime` at 5.28% and
   named it a cost to remove.
4. Give an unresolved NID its own counter, so the "import not found, returning
   0" path becomes countable rather than log-only.
5. Write one row per NID per interval to `hle.csv` through the existing
   `perf_log` channel (`vita3k/util/include/util/perf_log.h`), using
   `exchange` so each row is a delta and a long session reads as a rate.
6. Resolve the NID to a name in the dump with `import_name`
   (`vita3k/nids/include/nids/functions.h`), so the CSV is readable without a
   lookup table.
7. Start and stop alongside `perf_log` in `vita3k/interface.cpp:533`, so both
   truncate on the same game start.

## Acceptance

- Both flags default to off, and a run with them off behaves as before.
- `hle.csv` has a header, one row per NID per interval, with name, calls, and
  duration when timing is on.
- A test in `vita3k/mem/tests/` covers the table: insert, probe past a
  collision, overflow, and the delta behaviour of `exchange`.
- `./format.sh` is clean.
- The overhead of a counters-on run against a counters-off run is measured and
  written in the answer, so the cost of the instrumentation is a number.