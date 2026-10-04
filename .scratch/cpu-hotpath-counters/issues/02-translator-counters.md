# 02: Count translator and fast-path events

Status: open
Type: task
Label: ready-for-agent
Blocked by: 01

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