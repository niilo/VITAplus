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
- **Overhead is not measured.** That needs a device, which this run does not
  have. It is the one acceptance item still open, and it belongs with a
  gameplay run rather than here.

12 new tests, all passing. Build, ctest and the format check are clean.

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