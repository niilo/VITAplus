# 02: Count translator and fast-path events

Status: resolved
Type: task
Label: ready-for-agent
Blocked by: 01

## Answer

All four counters are in, sharing 01's flags and its interval.

- `count_translated_instruction` at
  `vita3k/cpu/src/dynarmic_cpu.cpp:377`, first thing in
  `PreCodeTranslationHook`. Written to `jit.csv` as
  `translated_instructions`, and the comment at the call site says it is
  translation time rather than execution time.
- `count_cache_invalidation` at `dynarmic_cpu.cpp:878`, with the byte length,
  written as `cache_invalidations` and `cache_invalidation_bytes`.
- `count_page_table_read` and `count_page_table_write` at `dynarmic_cpu.cpp:440`
  and `:489`, written as `page_table_reads` and `page_table_writes`.
- `count_invalid_access_recovery` at `dynarmic_cpu.cpp:398`, on the transient
  branch only, written as `invalid_access_recoveries`.

### One decision the plan did not settle

The page-table counters are guarded by `if (parent->mem->use_page_table)` in
`dynarmic_cpu.cpp:439` and `:488`. Without that guard they would also count
every access under `fastmem`, where the JIT resolves addresses itself and never
calls back. That number would be a total access count, not a fast-path miss
count, and it would answer a different question. The guard costs one load on a
path that is already a callback.

### Acceptance

- Nothing is counted when both flags are off. Covered by
  `NothingIsWrittenWhileDisabled`.
- `jit.csv` carries all four rows per interval under the names above. Covered by
  `TranslatorCountersGetTheirOwnRows`, which also pins that a zero counter is
  left out instead of written as 0.
- `./format.sh` is clean.

Build, ctest and the format check are clean.

## Goal

Four counters on branches that already exist. Each one measures whether a fast
path is working, which no current measurement answers.

## Steps

1. **Instructions handed to the translator.** Increment once per
   `PreCodeTranslationHook` call (`vita3k/cpu/src/dynarmic_cpu.cpp:373`). This
   is the translation-time instruction count, not an execution count, and the
   name in the CSV must say so.
2. **JIT cache invalidations.** Increment in `invalidate_jit_cache`
   (`dynarmic_cpu.cpp:866`), with the byte length. Feeds ticket 14, which found
   the 128 MiB code cache is 4.6% touched and left the default alone; an
   invalidation count says whether cache pressure is real.
3. **Page-table callback entries.** Increment in `MemoryRead` and `MemoryWrite`
   (`dynarmic_cpu.cpp:431` and `:485`) when `use_page_table` is set. These are
   the accesses the page table could not resolve, so the count is the fast-path
   miss rate. Feeds ticket 10 on memory mapping modes.
4. **Invalid access recoveries.** Increment in `confirm_invalid_access`
   (`dynarmic_cpu.cpp:414`) on the transient-recovery path, where the address
   turns out to be valid after all. That distinction is currently visible only
   in the log.
5. Write all four to `jit.csv` through `perf_log`, on the same interval and the
   same start and stop as 01.

Counting `PreCodeTranslationHook` calls costs one increment per translated
instruction, which happens once per instruction compiled rather than once per
execution. It is not the rejected per-instruction counter from the spec: it
measures translation work, and it stops once a block is compiled.

## Acceptance

- Nothing is counted when both flags from 01 are off.
- `jit.csv` carries all four rows per interval with the names the steps give.
- A test covers that a disabled component counts nothing.
- `./format.sh` is clean.