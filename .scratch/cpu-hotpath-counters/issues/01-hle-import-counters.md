# 01: Count HLE import calls per NID

Status: open
Type: task
Label: ready-for-agent

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