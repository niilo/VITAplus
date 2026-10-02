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

## Why it matters, after ticket 01

The iTLB argument is dead. Transparent huge pages are `always` on this device
(ticket 01), so the code cache gets 2 MB pages and the instruction TLB pressure
the ticket was built on does not exist.

What is left is first-touch page faults, because the cache is not prefaulted.
That is a one-line `memset` and it belongs in ticket 12's single `smaps`
reading rather than in a separate ticket. With the GPU at 93% busy there is no
frame rate at stake: ticket 00 measured CPU 23% and 43%.

**Fold this ticket into ticket 12.** Read the mapping size and its
`Rss`/`Private_Dirty` alongside `AnonHugePages`, and set this one to `rejected`
with the numbers.

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
