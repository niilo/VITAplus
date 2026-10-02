# 12: Ask for huge pages on the JIT cache and the guest arena

Status: open
Type: task
Label: ready-for-agent
Blocked by: 01, 03

## Goal

Two large mappings are faulted in 4 KiB at a time: the dynarmic code cache and
the guest memory reservation. On a 4 KiB page mapping the instruction TLB
misses more often than on a 2 MB page mapping. `madvise(MADV_HUGEPAGE)` is one
syscall per mapping and nothing blocks it on Android.

## What is true on Android 13

- **Ticket 01 measured this device: `/sys/kernel/mm/transparent_hugepage/enabled`
  is `[always]`, not `madvise`.** The GKI defconfig sets
  `CONFIG_TRANSPARENT_HUGEPAGE_MADVISE=y` and leaves `ALWAYS` unset; this
  vendor kernel overrides that. So anonymous mappings already get 2 MB pages
  without any `madvise` call. That removes the main lever of this ticket.
- An app may read `/sys/kernel/mm/transparent_hugepage/enabled` and the other
  files in that directory. SELinux grants it.
- `MADV_COLLAPSE` needs Linux 6.1. This device runs 5.15, so the call returns
  `EINVAL`. Probe once with a 4096-byte region and only use it if the probe
  succeeds. Define the value as 25 if the NDK headers do not.
- `khugepaged` scans every 10 seconds and compacts 16 MB per cycle. A code cache
  that is written to continuously may never be collapsed in time. Measure
  rather than assume.

## Steps

0. **Read `AnonHugePages` for both mappings before changing anything.** THP is
   `always` here, so the guest arena and the JIT cache may already have 2 MB
   pages. If they do, this ticket is `rejected` with the numbers and nothing
   else in it is done. That is the likely outcome, and it is worth knowing
   before any code is written.

1. The dynarmic code cache. The emulator sets no code cache size
   (`vita3k/cpu/src/dynarmic_cpu.cpp:642-668`), so the `Dynarmic::A32` default
   applies. This is the ARMv7 JIT, not the arm64 backend, which is never
   compiled. Run `git submodule update --init --recursive` first:
   `external/dynarmic` is empty in this tree. Read the default in
   `external/dynarmic/include/dynarmic/interface/a32/config.h` and record its
   name, value and line. Then find where the A32 backend allocates the code
   cache and call `madvise(MADV_HUGEPAGE)` on it once, at creation. Log the
   address of that block as well, because step 5 needs it and the submodule's
   allocator chooses it. Record the file and line of the call in `## Answer`.
2. The guest reservation. `vita3k/mem/src/mem.cpp:97-106` maps
   `TOTAL_MEM_SIZE` with `PROT_NONE` at a preferred address. Call
   `madvise(MADV_HUGEPAGE)` on the whole range right after the mapping, before
   the first touch. The reservation is written page by page as the guest
   allocates, so a fault inside a `VM_HUGEPAGE` VMA has a chance of getting a
   huge page.
3. Pre-fault the code cache once with a memset over the first megabyte, so
   khugepaged has real pages to collapse later.
4. Add a temporary setting `huge-pages`, default on, so A and B run from one
   build. Log whether the `MADV_HUGEPAGE` call succeeded.
5. Verify. Read `AnonHugePages` and `KernelPageSize` from `/proc/self/smaps` and
   print the entry whose address range contains the address of each mapping from
   step 1. Log that address at creation, because the dynarmic code cache address
   is chosen by the submodule's allocator and the ticket cannot name it in
   advance. The shell cannot read `smaps` of a non-profileable release app, so
   reading it from inside the app is the method that works, and the manifest
   change from ticket 03 is what makes the profile commands in this ticket work
   at all.

## Measurement

After ticket 04 has a baseline, run A/B/A on the 60 FPS title. Record:

- `page-faults` per second from `simpleperf stat`, and minor faults from
  `/proc/<pid>/stat`.
- `cpu-cycles` per frame, and the frame interval 99th percentile.
- The `AnonHugePages` figure, so the reader knows whether the pages were
  actually obtained.

If the pages are not obtained, say so and set `Status: rejected`. An
unproven iTLB theory is not a result.

## Acceptance

- Both targets build and the format check passes.
- The measured `AnonHugePages` for both mappings, and the A/B/A numbers.

## Answer

## Comments
