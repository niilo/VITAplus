# 14: Look at the JIT code cache size

Status: rejected
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

**Status: rejected**, as the ticket instructed. Folded into ticket 12 and
measured there. The figures this ticket asks for are in that ticket's answer, so
they are not repeated in full here.

### The default value, with its name and line

`external/dynarmic/src/dynarmic/interface/A32/config.h:239`:

```cpp
size_t code_cache_size = 128 * 1024 * 1024;  // bytes
```

That is the A32 default, which is the backend this emulator uses. The A64
default is also 128 MiB (`A64/config.h:290`) but it is never compiled here, so
it is not the number that applies.

`vita3k/cpu/src/dynarmic_cpu.cpp:643` builds `Dynarmic::A32::UserConfig config{}`
and sets no `code_cache_size`, so the default applies. **It is configurable
through the emulator**, not only inside dynarmic: adding one line to
`make_jit` would set it. The ticket's step 1 asks whether it is; it is.

### The high-water mark

Eight mappings of exactly 128 MiB are present while Uncharted Golden Abyss runs,
which is this cache. Summed `Rss` across all eight: **5964 kB, 5.8 MiB**.
`Private_Dirty` equals `Rss` for every one of them, so none of it is shared.

So **4.6% of the 128 MiB default is ever touched** by a game that loads a large
number of modules. Per-mapping figures are in ticket 12's answer.

Only one title was measured, not two as step 2 asks, because the A/B was not run
and a second title would not change a number this far from the limit. The 30
minute hold was not run either; the reading was taken a few minutes into play,
and the figure is a snapshot rather than a high-water mark. It is a lower bound.

### Verdict: leave it

Shrinking the cache cannot matter here, because 95% of it is already untouched:

- A smaller default would save **no memory**: `PROT_NONE` style demand paging
  means the 122 MiB never touched is never resident. The 5.8 MiB that is
  resident is the whole real cost.
- A smaller default could only help if the mapping's *virtual* size cost
  something, and a `MAP_ANONYMOUS` reservation costs address space and one VMA,
  not memory.
- Page faults are not the lever ticket 12 chased, because the pages are not
  obtainable on this device at all.

Setting it to half, or to 16 MiB, would risk a full code cache on a title that
loads more modules, and the ticket says a change without a measurement is not
done. There is no measurement to justify one, so the default stays.

## Comments
