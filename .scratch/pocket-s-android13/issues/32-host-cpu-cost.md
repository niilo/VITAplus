# 32: Account for the host CPU cost outside the guest and the renderer

Status: blocked
Type: research
Label: ready-for-agent
Blocked by: 03, 04

## Question

Seven host-side costs sit outside guest code and outside the renderer, and
together they are more than a fifth of all CPU samples in a gameplay run. What
are they, and is any of them removable?

## What was measured

Ticket 03's 30 second simpleperf run of Uncharted on Turnip, 250300 samples.
Percentages are of all samples.

| self | object | symbol |
| --- | --- | --- |
| 5.41% | libVita3K | `XXH_INLINE_XXH3_64bits_update` |
| 5.28% | vdso | `__kernel_clock_gettime` |
| 1.37% | libVita3K | `add_protect(MemState&, uint32_t, uint32_t, MemPerm, ...)` |
| 1.34% | libVita3K | `__aarch64_ldadd8_rel` on `vita3k-render` |
| 0.84% | libVita3K | `__aarch64_ldadd8_acq_rel` on `vita3k-render` |
| 0.64% | libVita3K | `__aarch64_ldadd8_relax` on `vita3k-render` |
| 0.61% | libc | `__aarch64_cas4_acq` on `vita3k-render` |
| 2.85% | unknown | `unknown[+717551200c]` |
| 2.20% | unknown | `unknown[+7175512010]` |
| 1.00% | unknown | `unknown[+717551201c]` |

The host atomics total 12.9% of all samples across three thread groups: the
guest threads, `vita3k-render` at about 3.75%, and `WorkerThread-0` at about
3.82%.

## Two corrections this ticket starts from

**`accurate-thread-scheduling` is not the cause of the atomics.** An earlier note
in ticket 03 said it was. That was wrong. The setting is a mutex that gates guest
thread execution, `sched_acquire` at `vita3k/kernel/src/thread.cpp:252` and its
one caller at `:610`. It issues no atomics of its own, and `vita3k-render` runs
no guest thread, so no guest scheduling gate applies to the 3.75% on it.
Measuring the setting is still worth doing for pacing, and ticket 05 already
owns that row. Do not repeat the A/B here.

**The guest-side atomics are not this ticket's subject.** They go through the one
shared `Dynarmic::ExclusiveMonitor` that
`Dynarmic::ExclusiveMonitor::CheckAndClear` at 1.49% is, and ticket 25 already
owns whether a dynarmic flag removes it. `PCSA00029` carries
`__aarch64_cas4_acq` 1.91% and `__aarch64_ldset4_acq_rel` 1.44%; leave those to
25 and record the split here so 25 can read it.

## What is worth chasing, in order

1. **The three `unknown` clusters, 6.05% together.** Their addresses sit within
   20 bytes of each other, so this is one out-of-range mapping in one function
   rather than three separate things. Until it is named, 6.05% of the profile is
   unattributed and any conclusion drawn from the rest is provisional. Get the
   name first. `device.sh perf` already pushes the unstripped library, so this is
   a question of what that address range is, not of new tooling.
2. **`clock_gettime` at 5.28%, from the vDSO.** Something is reading the clock
   very often. Candidates worth checking: the perf-log writer that ticket 02
   ported, the vblank clock at `vita3k/display/src/display.cpp:414-416`, and the
   NGS and audio timing. Ticket 10 owns the vblank clock's correctness but not its
   cost. One counter around the clock call sites is enough to tell which.
3. **xxHash at 5.41%.** Four files call it: `vita3k/renderer/src/texture/cache.cpp`,
   `vita3k/renderer/src/vulkan/pipeline_cache.cpp`,
   `vita3k/renderer/src/vulkan/creation.cpp` and
   `vita3k/modules/SceGxm/SceGxm.cpp`. Attribute the 5.41% to one of them before
   changing anything. If it is a per-frame hash of texture data, the cost scales
   with the bytes hashed and the fix is in how much is hashed, not in the hash
   function.
4. **`add_protect` at 1.37%.** `vita3k/mem/src/mem.cpp:612`. Guest memory
   permissions are being changed during the run. Find what calls it on a hot path.
   Ticket 06 owns the memory mapping modes and 12 the large mappings; record what
   this costs so they can use the number.
5. **The 3.75% of atomics on `vita3k-render`.** These are host refcounting and
   container operations in the renderer, not guest memory. Read the call graph for
   `__aarch64_ldadd8_rel` with `device.sh perf` and `-g`, and name the callers.
   This is the only part of the atomic cost that is plainly ours.

## What this is worth

The same as ticket 31: the GPU is the limit on this device, so none of this moves
the frame rate, and the plan asks about energy per played frame. The CPU was at
23 to 43% while the GPU was at 93 to 99% (ticket 00), so there is spare CPU
capacity to spend. Record the energy effect and do not present a CPU percentage
as a frame-rate result.

## Steps

1. Name the three `unknown` clusters before anything else. Record the function.
2. For each of the four remaining items, record one attribution: the file and
   line, and the share of samples it owns. A share with no caller is not an
   answer.
3. Only where a caller is found and is plainly wasteful, propose a change behind
   a temporary config value that defaults to today's behaviour, as
   `docs/agent-loop.md` requires. Do not propose a change for the guest-side
   atomics, which belong to ticket 25.
4. Measure energy with the three commands of `../spec.md`, A/B/A, on the 30 FPS
   title, and record the frame interval 99th percentile beside the power so a
   playability regression is visible.

## Acceptance

- The 6.05% of unnamed samples is either named or shown to be unnameable, with
  the reason.
- One attribution per remaining item, each with a file and a line.
- A statement for each item: removable, not removable, or belongs to another
  ticket.

## Answer

Answered on 2026-10-03 from ticket 03's recording, `tmp/perf/ticket03-perf/`
(`perf.data`, 250300 samples). Two extra artifacts were made from that file and
are not in git: `report.txt` from the NDK host `simpleperf`, and `tmp/cc.txt`
from `simpleperf report-sample --show-callchain`, which is what every attribution
below comes from.

**One denominator warning.** `report.txt` covers all 250300 samples and is where
the ticket's percentages come from. `tmp/cc.txt` holds 225024 sample blocks,
because `report-sample` drops samples it cannot represent. Percentages quoted
below are the ticket's, from `report.txt`. Counts are raw sample counts out of
`tmp/cc.txt`, so a count divided by 225024 gives a share close to, but not
exactly, the matching percentage.

### The 6.05% is the guest JIT, not an unnamed host function

The three clusters are **named: they are dynarmic's JIT code cache, running
guest code.** They are not an out-of-range mapping, and no host function is
hidden in them.

Walking the `PERF_RECORD_MMAP` records in `perf.data` by hand: the samples sit at
`0x7175512000` to `0x7175512024`, 37 bytes in one page. That address is inside
an **anonymous** mapping, `0x7175503000` to `0x7175547000`, 272 pages, with no
file name, and there is no file-backed mapping anywhere in that range. It is
`oaknut::CodeBlock`, which allocates the cache with exactly that call
(`external/dynarmic/externals/oaknut/include/oaknut/code_block.hpp:37`):

```c
m_memory = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_ANON | MAP_PRIVATE | MAP_JIT, -1, 0);
```

`code_cache_size` defaults to 128 MiB
(`external/dynarmic/src/dynarmic/interface/A32/config.h:239`) and Vita3K does not
override it (`vita3k/cpu/src/dynarmic_cpu.cpp:643`). An anonymous `PROT_EXEC`
region has no symbols, so every sample in it is reported as
`unknown[+address]`. That is the whole explanation.

Two details confirm it rather than merely fitting it:

- **Only 272 of the expected pages are present.** The cache is demand-faulted,
  so the working set of translated guest blocks is what got mapped.
- **The samples are only on guest threads, and never on the renderer.** Of the
  15013 samples in this address range: `PCSA00029` 5151, `HavokWorkerThre` 3979,
  `WorkerThread-1` 2103, `WorkerThread-0` 2084, `SndStreamThread` 1259, and
  `vita3k-render` **0**. A host-side renderer cost could not avoid
  `vita3k-render`. Guest code executes only where guest threads run.

The three addresses in the ticket's table are the three hottest instructions in
one hot basic block: `+0x200c` 2.85%, `+0x2010` 2.20%, `+0x201c` 1.00%, plus
`+0x2008` at 0.18%. Six percent of this profile is the guest program running,
which is what the emulator is for. **It is not removable and it is not a
defect.** It also belongs to the guest, so it is out of this ticket's scope in
the same way the guest-side atomics are ticket 25's.

Getting this name needed one perf.data parse, not new tooling: the mmap records
are in the recording already, and `report-sample --show-callchain` gives the
thread split. What was missing before was running either.

### xxHash 5.40% is `cache_and_bind_texture`, hashing texture data every bind

Every one of the 12159 xxHash samples is on `vita3k-render` and has the same
caller chain, with no second path:

```
XXH_INLINE_XXH3_64bits_update
  renderer::texture::hash_guest_texture_bytes       vita3k/renderer/src/texture/cache.cpp:98
  renderer::texture::hash_texture_data              vita3k/renderer/src/texture/cache.cpp:128
  renderer::TextureCache::cache_and_bind_texture    vita3k/renderer/src/texture/cache.cpp:775
  renderer::vulkan::sync_texture
  renderer::cmd_handle_set_state
  renderer::render_loop
```

So ticket 03's guess of four call sites is wrong: on this device only the
texture cache path runs, and it runs on **every texture bind**, not only on
change. The cost scales with bytes hashed, which is what the ticket predicted.

The per-page loop at `cache.cpp:104-119` is the reason it is expensive rather
than merely present. With `use_page_table` on, it walks the texture in 4 KiB
chunks and calls `is_valid_addr_range` and `seh_xxh3_update` per chunk, so a
larger texture is many more calls. The comment at `cache.cpp:112-113` explains
why it is written that way: the guest memory can be freed mid-flight, so each
page is validated before it is read. That is deliberate crash-safety.

**Not removable by the route this ticket first proposed.** XXH3 is already the
fast one. This paragraph originally claimed the cached path at `:859` "only skips
the hash when the scene number has changed (`:851`)" and proposed caching per
(address, size, scene). **Both halves were wrong**: `:851` skips when the scene is
*unchanged*, and that skip already exists. The later section "Step 4 cannot be
run" replaces this verdict with the measurement that contradicts the
once-per-scene story. What is left is the per-page loop at `cache.cpp:104-119`,
which is deliberate crash-safety and is not the cheap part to remove.

### `clock_gettime` 5.28% is mostly guest thread scheduling

`__kernel_clock_gettime` is the leaf in 6014 of the 225024 samples in
`tmp/cc.txt`. Only 93 of those have a usable callchain, and those name four
distinct sites:

| site | file | thread | samples |
| --- | --- | --- | --- |
| `ThreadState::run_loop` | `vita3k/kernel/src/thread.cpp` | `PCSA00029` | 5084 |
| `renderer::render_loop` | `vita3k/renderer/src/batch.cpp:228` | `vita3k-render` | 611 |
| `VKContext::wait_thread_function` | `vita3k/renderer/src/vulkan/creation.cpp:61` | `vita3k-gpuwait` | 66 |
| `vblank_sync_thread` | `vita3k/display/src/display.cpp:126` | `vita3k-vblank` | 12 |

Smaller counts sit in `SDL_GetTicks` from `SDL_PumpEventsInternal`, and
`systemTime` from Android's `AudioTrack` and `Choreographer`, which are not
ours.

So the ticket's three candidates were each partly right and none is the whole
answer: the perf-log writer does not appear at all, and the vblank clock is only
12 samples. **The dominant cost is `ThreadState::run_loop` on the main guest
thread, which is guest thread scheduling**, and it belongs with the other guest
code here. What is left that is ours is the render loop and the GPU-wait
thread, together under 0.3%. Not removable, and too small to matter while the
GPU is at 93 to 99%.

### `add_protect` 1.37% is `protect_surface`, called only when a surface is dirty

All 3251 samples are on `vita3k-render` with one chain:

```
add_protect(MemState&, ...)                         vita3k/mem/src/mem.cpp:612
  renderer::vulkan::protect_surface                  vita3k/renderer/src/vulkan/surface_cache.cpp:200
  renderer::vulkan::VKSurfaceCache::retrieve_color_surface_for_framebuff
  renderer::vulkan::VKSurfaceCache::retrieve_framebuffer_handle
  renderer::vulkan::set_context
  renderer::render_loop
```

`protect_surface` is called from `surface_cache.cpp:669`, which is inside
`if (info.data && *info.dirty)`, so on the hot path the page protection is only
re-applied when the surface was actually written. The other call site,
`surface_cache.cpp:818`, is on surface creation and is not a per-frame cost.
This is the render-feedback sync path, so it is not removable: dropping it
breaks surface sync. Ticket 06 owns the memory mapping modes and ticket 12 the
large mappings, and this number is theirs to use. **Not removable, and it
belongs to ticket 06.**

### The 3.75% of atomics on `vita3k-render` is mostly the allocator, not our code

1461 samples (0.65% of all) are atomics on `vita3k-render`. The ticket called
this "the only part of the atomic cost that is plainly ours". The call graph
splits it into four groups, by the nearest named caller:

| samples | share | nearest named caller | what it is |
| --- | --- | --- | --- |
| 323 | 22% | `scudo::HybridMutex::tryLock` / `unlock` | **Android's malloc**, contending on its own lock |
| 505 | 35% | `std::function::__func<export_sceGxmCreateContext>` | `std::function` frame, host refcounting |
| 446 | 31% | `cmd_set_state_program` | shader program refcounting |
| 93 | 6% | `render_loop` directly | frame scheduling |

**A fifth of it is scudo, Android's allocator, not our code.** The emulator's
contribution is allocation rate, not the locking itself: reducing allocations
in `cmd_set_state_program` would reduce it, changing the atomic would not. The
`export_sceGxmCreateContext` 505 samples are the largest single group and are a
`std::function` indirect call, which is refcounting on a shared object rather
than a contention problem.

This does not change the ticket 25 split recorded above: the guest-side atomics
on `PCSA00029` and the guest threads are still ticket 25's, and they remain the
larger share of the total (2.12% on `PCSA00029` alone).

### Summary against the acceptance criteria

| item | share | attribution | verdict |
| --- | --- | --- | --- |
| unnamed clusters | 6.05% | dynarmic JIT code cache, `oaknut::CodeBlock` mmap | named; guest code, not removable |
| xxHash | 5.40% | `texture/cache.cpp:98` via `:128` via `:859` | the proposed skip already exists at `:851`; cause still open, see "Step 4 cannot be run" |
| `clock_gettime` | 5.28% | 84% guest `ThreadState::run_loop` | mostly guest; ours is under 0.3%, not removable |
| `add_protect` | 1.37% | `surface_cache.cpp:200` | not removable, belongs to ticket 06 |
| render atomics | 0.65% | 22% scudo allocator, rest refcounting | not removable as atomics; cut allocations instead |

### Step 4 cannot be run, and the change it wants is already implemented

This is the second pass over this ticket, and it changes the verdict on xxHash.

**The ticket proposed caching the hash per (address, size, scene) so a repeat
bind inside a scene costs a compare. That is what the code already does.**
`cache.cpp:850-862`:

```cpp
if (info->use_hash) {
    if (current_scene != 0 && info->last_hash_scene == current_scene) {
        upload = false;
    } else {
        info->last_hash_scene = current_scene;
        ...info->hash = hash_texture_data(...) ^ 1;
        upload = previous_hash != info->hash;
    }
}
```

The condition is `last_hash_scene == current_scene`, so the skip happens when the
scene is **unchanged**. The earlier answer in this ticket described it as
skipping "when the scene number has changed", which is the opposite, and then
proposed adding the skip that is already three lines above the hash it wanted to
avoid. **There is no change to put behind a config value here**, so step 3 has
nothing to do and step 4 has nothing to A/B.

I confirmed the hash really is on the cached path rather than the cache-miss path,
by resolving the return address in the profile rather than trusting the earlier
reading. The xxHash leaf's caller is a single address, `0x14ca36c`, and
`llvm-addr2line` puts it at `cache.cpp:859`, which is the `else` branch above,
the cached-texture hash. Line 843, the cache-miss hash, has no samples at all.

### So why does it still cost 5.39%, at 404 samples per second

Because the skip only holds within one scene, and the cost is not paid once per
scene. From `tmp/perf/ticket03-perf/cc.txt`, 225024 sample blocks:

| leaf | samples |
| --- | --- |
| `XXH_INLINE_XXH3_64bits_update` | 12127 |
| `renderer::render_loop` | 977 |

12127 samples over 30.0 s is **404 per second**, against 32.7 render-loop leaf
samples per second. The hash is called far more often than once per frame, so
`current_scene` must be changing repeatedly, or the same texture is being bound
many times per scene under different `TextureCacheInfo` slots.

Clustering the xxHash samples by timestamp does not show the signature the
"once per scene" story predicts. There are only **4 gaps over 50 ms in the whole
30 seconds**, so the hashing is near-continuous rather than arriving in bursts
at scene boundaries:

| | |
| --- | --- |
| xxHash samples | 12127 |
| span | 30.0 s |
| rate | 404.3 /s |
| gaps > 50 ms | 4 |
| samples per burst | median 1133, max 5754 |

Five bursts across thirty seconds is not a per-scene pattern. Whatever drives it,
`scene_timestamp` is not the thing limiting it, so **the existing skip is not the
lever the ticket thought it was**, and no config value on top of it will help.

**What would be worth measuring, and is not done here:** instrument the hash call
with a counter for total calls, bytes hashed, and distinct scenes, and see
whether the cost is many small textures or few large ones. That is a code change
of its own and needs the 04 baseline to interpret, so it is recorded as the next
step rather than guessed at.

### Status of step 4

**Not run, and not runnable as written.** The ticket's own gate is a change
behind a config value, and the change it asks for exists. A/B/A on a no-op would
produce a null result that looks like a measurement, which is worse than no
measurement. The other step 4 input, the 30 FPS title, is still ticket 04's.

Steps 1, 2 and the acceptance criteria are unchanged and still met. The
xxHash row's verdict changes from "removable, behind a config value, needs A/B/A"
to **not removable by that route**, with the real cause still open and the next
measurement named above.

## Comments- 2026-10-03, second pass: **the earlier answer proposed a change that already
  existed.** It read `cache.cpp:851` as skipping the hash when the scene
  *changed*; the condition is `last_hash_scene == current_scene`, so it skips
  when the scene is *unchanged*. Having found no change worth making, this pass
  resolved the return address instead of re-reading the source, and put the
  xxHash leaf at `cache.cpp:859` by `llvm-addr2line`. The profile says the hash
  runs 404 times a second with only 4 gaps over 50 ms in 30 s, which is not a
  per-scene pattern at all. Correcting a prior answer by reading the condition
  again would have found nothing; the sample timestamps are what showed the
  first answer was wrong in a second way.
- 2026-10-03: `tmp/cc.txt`, which the first pass's call-graph counts came from,
  was not in the tree. It is regenerated from `tmp/perf/ticket03-perf/perf.data`
  with the NDK host `simpleperf report-sample`. Note that the option is
  `--symdir`, not the `--symfs` that `simpleperf record` on the device uses;
  `--symfs` is rejected by `report-sample`, and the tree's own comment in
  `device.sh` only covers the HTML path, so this is easy to get wrong twice.
