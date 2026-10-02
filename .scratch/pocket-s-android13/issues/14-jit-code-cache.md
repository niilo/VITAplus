# 14: Look at the JIT code cache size

Status: open
Type: research
Label: ready-for-agent
Blocked by: 01, 03

## Question

The emulator sets no dynarmic code cache size, so the `Dynarmic::A32` default
applies. How much of it is used, and does the size cost anything?

## Context

`vita3k/cpu/src/dynarmic_cpu.cpp:642-668` builds a `Dynarmic::A32::UserConfig`
and sets no code cache size. This is the ARMv7 JIT for the Vita guest, not the
arm64 backend. The default value is in the dynarmic submodule, which is empty in
this tree, so read it after `git submodule update --init --recursive` and
record the name, value and line. Do not quote a default from another backend.

## Why it matters

Two costs grow with the reservation:

- First-touch page faults, if the mapping is not prefaulted.
- Instruction TLB pressure, if the pages are 4 KiB. Ticket 12 addresses the
  page size.

Nothing else, because the mapping is lazy and a 64-bit address space is not
the constraint. If the games use a small part of the reservation, a smaller
cache does not help. If they use most of it, the faults and the iTLB are real,
and ticket 12 is the fix rather than a smaller cache.

## Steps

1. Read the default and record it. Then find the allocator for the code cache
   and whether the size is configurable through the emulator's
   `DynarmicCPU::make_jit` or only inside dynarmic.
2. Measure the high-water mark. Log the mapping's `Rss` from
   `/proc/self/smaps` after 30 minutes of play in a game that uses many
   modules, and again in a game that uses few. Do the same for `Private_Dirty`.
3. If the cache size is configurable, put it behind a temporary setting with
   the current default, and run A/B/A at the default and at half of it.
4. Record `page-faults` per second from simpleperf and `cpu-cycles` per frame
   from the same run, so the two costs can be told apart.

## Acceptance

- The default value, its name and its line in the dynarmic submodule.
- The high-water mark for two games, and the A/B/A numbers.
- A verdict: leave it, or set it and say to what.

## Answer

## Comments
