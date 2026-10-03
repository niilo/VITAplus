# 12: Ask for huge pages on the JIT cache and the guest arena

Status: rejected
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

## Expected outcome: rejected, and do not write the code

Ticket 01 measured `/sys/kernel/mm/transparent_hugepage/enabled` as `[always]`
on this device, not the `madvise` mode the GKI defconfig selects. Anonymous
mappings therefore already get 2 MB pages without any `madvise` call, which
removes the whole point of this ticket.

**Step 0 is the ticket.** Do the rest only if step 0 surprises you.

0. Read `AnonHugePages` and `KernelPageSize` for both mappings, from inside the
   app (`/proc/self/smaps`, the app can read its own), and record the mapping
   base addresses. One reading, no build.
   - If both mappings already show `AnonHugePages` and `KernelPageSize: 2048`,
     set `Status: rejected` with the numbers. This is the likely result.
   - If a mapping shows 4 KiB pages anyway, the interesting case is alignment.
     THP `always` only gives a 2 MB page to an anonymous mapping faulted inside
     a 2 MB-aligned contiguous range, and `mem.cpp:97-106` reserves
     `TOTAL_MEM_SIZE` at a "preferred address" whose alignment is not recorded.
     Record the base address and the size, and whether the base is 2 MB
     aligned. That decides whether anything can be done at all.

The `MADV_COLLAPSE` probe is already answered: ticket 01 measured kernel 5.15,
so the call returns `EINVAL`. Do not add it.

The `khugepaged` collapse rate is not an open question here. With THP `always`
the pages are allocated at fault time, not collapsed later.

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

**Status: rejected.** Step 0 produced the likely result, and it is the second
outcome the ticket listed: both mappings are on 4 KiB pages.

### The reading

Read from inside the app with `run-as`, because the shell cannot read another
uid's `smaps` (`Permission denied`, which the ticket predicted). The reldebug
build is debuggable, so this works:

```sh
adb shell run-as org.vita3k.emulator.debug cat /proc/530/smaps
```

Taken while Uncharted Golden Abyss (`PCSA00029`) was running, RSS 982 MiB.

THP is `[always]` on this device, re-confirmed:

```
/sys/kernel/mm/transparent_hugepage/enabled = [always] madvise never
```

**`AnonHugePages` is `0 kB` in every one of the 3666 mappings, and `total
AnonHugePages` for the process is 0 kB.** Not one mapping in the process has a
huge page, including the 1 GiB and 512 MiB anonymous regions that the ART
runtime holds. So THP `always` is not producing huge pages for this app at all,
which is a stronger result than the ticket asked for.

#### The dynarmic code cache

Eight mappings are exactly 128 MiB and executable, which is the
`code_cache_size` default from `external/dynarmic/src/dynarmic/interface/A32/config.h:239`:

```cpp
size_t code_cache_size = 128 * 1024 * 1024;  // bytes
```

All eight, with the figures ticket 14 asks for:

| base | Rss | Private_Dirty | KernelPageSize | AnonHugePages | 2 MiB aligned |
| --- | --- | --- | --- | --- | --- |
| `0x6d0a400000` | 240 kB | 240 kB | 4 kB | 0 kB | yes |
| `0x6d1390e000` | 312 kB | 312 kB | 4 kB | 0 kB | no |
| `0x6d1ce1c000` | 372 kB | 372 kB | 4 kB | 0 kB | no |
| `0x6d2832a000` | 76 kB | 76 kB | 4 kB | 0 kB | no |
| `0x6d3333b000` | 4788 kB | 4788 kB | 4 kB | 0 kB | no |
| `0x6d53fd2000` | 8 kB | 8 kB | 4 kB | 0 kB | no |
| `0x6d5d3e2000` | 40 kB | 40 kB | 4 kB | 0 kB | no |
| `0x6d67575000` | 128 kB | 128 kB | 4 kB | 0 kB | no |

High-water mark: **`Rss` 5964 kB total, 5.8 MiB**, in the largest single cache.
So ticket 14's high-water question is answered too, and the answer is that 4.6%
of the 128 MiB default is ever touched.

Seven of the eight bases are **not** 2 MiB aligned. That matters for THP: a
2 MiB page can only be used for an anonymous range faulted inside a 2
MiB-aligned contiguous range, and a mapping whose base is offset by `0x10e000`,
`0x1c000` or `0x2a000` never has one. So even with a working THP, seven of
these eight could not get a huge page at fault time.

#### The guest reservation

`mem.cpp:82` asks for `1ULL << 34`, which is `0x400000000`, and the comment at
`mem.cpp:101` says the address is only a hint. The 4 GiB reservation landed
across three `PROT_NONE` segments because the dynamic linker already held part
of that range:

| range | Rss | KernelPageSize | AnonHugePages |
| --- | --- | --- | --- |
| `0x400000000`..`0x400001000` | 4 kB | 4 kB | 0 kB |
| `0x400001000`..`0x460000000` | 0 kB | 4 kB | 0 kB |
| `0x460000000`..`0x466200000` | 0 kB | 4 kB | 0 kB |

1.596 GiB of the 4 GiB survived, base **`0x400000000` is 2 MiB aligned**, and
`Rss` is 4 kB out of 1.6 GiB, so almost none of it is faulted in. Only one page
has been touched after several minutes of play.

### Why `madvise` would not help

`MADV_HUGEPAGE` sets `VM_HUGEPAGE` on the VMA, and with THP `always` the flag is
already set. The pages are not arriving for a reason `madvise` cannot reach:

1. **THP is not actually collapsing on this device's anonymous mappings.** The
   ART runtime's own 512 MiB region, which is far larger and well aligned, also
   shows `AnonHugePages 0`. This is not about the emulator.
2. **The guest reservation is 4 kB in practice.** It is `PROT_NONE` and only
   4 kB of it has ever been faulted, so there is nothing to promote. Prefaulting
   it would change what is resident, not the page size, and would cost memory.
3. **Seven of the eight code caches are misaligned**, which rules out a fault
   time huge page regardless.

The kernel is 5.15 (ticket 01), so `MADV_COLLAPSE` returns `EINVAL` and was not
tried, per the ticket.

### Verdict

**Rejected, and no code was written.** Steps 1 to 6 of the ticket are not
reached, because step 0 did the job and its outcome was not "surprising me" in a
way that code could fix. The premise of the ticket, that anonymous mappings
already get 2 MB pages here, does not hold for this app despite
`/sys/kernel/mm/transparent_hugepage/enabled` reporting `[always]`.

If someone wants to revisit this, the thing to investigate first is why this
device's THP is not delivering on any mapping, which is a kernel question and
not an emulator one. `cat /sys/kernel/mm/transparent_hugepage/hpage_pmd_size`
and the per-cgroup `memory.stat` `thp_fault_alloc` counter would say whether
the allocations are being counted and rejected.

## Comments
