# 09: Turnip NGS reads a stale host view and drops all in-game audio

Status: claimed
Type: task
Label: ready-for-agent
Claimed: 2026-10-10 agent

## Goal

Turnip in-game creates zero `NGSRATE` resamplers. Stock in-game creates
12,424. Both create the same audio threads. Fix Turnip so it mixes.

## Evidence

Device log `/tmp/vitalogs/vita3k.log`, build `v1.2.1-dev.18` (`00514fb7`),
PCSA00080, Pocket S, Turnip `26.2.99`, `mapping_method=PageTable`.

In-game Turnip boots 3 and 5, in order:

1. `release_external_shadow_pages` releases 293-898 MiB of arena pages
   shadowed by external mappings.
2. `STALE HOST POINTER` at `sceNgsSystemUpdate`, guests `0x89F3B010` and
   `0x89F14600`.
3. `STALE HOST POINTER` at `sceNgsVoiceKill`, guests `0x89F19140` and
   `0x89F2AFC0`.
4. 65x `sceNgsPatchCreateRouting: source missing or rack released
   (system=0x0)`.

Stock in-game (boot 1, `mapping_method=DoubleBuffer`) logs none of these.

## Root cause hypothesis

`vita3k/mem/src/mem.cpp:546-553` defines the stale case: a host-side access
into the arena whose live backing is a mapped buffer. The data diverges from
what the guest reads. The NGS structs live in that arena. A host read through
a stale pointer sees a null `rack->system`. The current
`vita3k/modules/SceNgsUser/SceNgs.cpp:214-224` treats null system as a
released rack and returns a null patch with success. All later routing is
then a no-op. So null system has two causes: a truly released rack, and a
stale host view of a live rack. The code treats both the same. Turnip hits
the second case.

## Steps

1. Reproduce the stale read without a device if possible. The trigger is
   `PageTable` plus external GPU mappings over the NGS arena. Check if a
   desktop Vulkan run with page-table mapping hits the same
   `STALE HOST POINTER` at `sceNgsSystemUpdate` / `sceNgsVoiceKill`.
2. In `sceNgsPatchCreateRouting`, `sceNgsVoiceKill`, and the scheduler
   queue filter (`vita3k/ngs/src/scheduler.cpp:242-250`), distinguish a
   stale view from a released rack before acting. Candidates: resolve the
   guest address through the page table or `host_to_guest`
   (`vita3k/mem/src/mem.cpp:589-610`) and compare, or re-read through the
   guest mapping instead of the cached host pointer.
3. Keep the take-2 behaviour for a truly released rack: success with a null
   handle, stereo info from `sceNgsPatchGetInfo`, no-ops on volume and
   transport calls. Only the stale case changes: it must reach the live
   rack.
4. Do not change the stock `DoubleBuffer` path. It works today.

## Acceptance

- Same Jak sequence on Turnip: in-game boot creates `NGSRATE` resamplers at
  the same order as stock (thousands, not zero).
- `STALE HOST POINTER` at `sceNgsSystemUpdate` / `sceNgsVoiceKill` no longer
  precedes the 65 `rack released` warnings, or the warnings are shown to be
  true releases with a guest-side check.
- `sceNgs.*failed` and `Assertion failed` stay at 0.
- Stock in-game still mixes (no regression).
- `./format.sh` is clean. New behaviour has a googletest in the suite the
  code belongs to (`vita3k/mem/tests`, `vita3k/module/tests`,
  `vita3k/ngs/tests`) where one can run without a device.

## Answer

### The cause, confirmed against the code

`vita3k/mem/src/mem.cpp:689-722`: `add_external_mapping()` copies the live arena
pages into a GPU buffer, repoints `mem.page_table[]` at that buffer, then
`mprotect`s the arena pages `PROT_NONE` and `madvise(MADV_DONTNEED)`s them.
`release_external_shadow_pages` is the log line the device run showed.

The NGS structs live in that arena, and the pointers *between* them were raw C++
host pointers, resolved exactly once by `Ptr<T>::get(mem)` at construction:

| Member | Was | Now |
|---|---|---|
| `Voice::rack` | `Rack *` | `Ptr<Rack>` |
| `Rack::system` | `System *` | `Ptr<System>` |
| `Rack::vdef` | `VoiceDefinition *` | `Ptr<VoiceDefinition>` |
| `ModuleData::parent` | `Voice *` | `Ptr<Voice>` |
| `System::racks` | `std::vector<Rack *>` | `std::vector<Ptr<Rack>>` |
| `State::systems` | `std::vector<System *>` | `std::vector<Ptr<System>>` |
| `VoiceScheduler::queue` | `std::vector<Voice *>` | `std::vector<Ptr<Voice>>` |
| `OperationPending::system`, `::release_data.rack` | `System *`, `Rack *` | `Ptr<System>`, `Ptr<Rack>` |

A host pointer resolved before the mapping keeps its value and names released
memory, so `rack->system` read zero, `sceNgsPatchCreateRouting` returned a null
patch, and every later routing call was a no-op. That is the 65 warnings and
then the silence. The ordering in the device log matches: the shadow pages are
released first, then the stale reads, then the warnings.

`ModuleData::parent` and `VoiceScheduler::queue` were found by review, not by
the first reading. Both are one hop earlier than the members the ticket named,
and both are on the mixing path, so fixing only the named sites would have left
the silence in place.

### Why storing the guest address, and not repairing the pointer

An earlier attempt added a `relocate()` helper in `mem` and called it at the
named sites. It was dropped for two reasons. The sites that matter are the mix
loop and the module `process()` implementations, and relocating at a chosen set
of call sites is a list that can be incomplete; a review of that attempt found a
`Voice *` cast to `Rack *`, which would have dropped live voices. Storing
`Ptr<>` makes the compiler name every site instead.

`Ptr<T>::get(mem)` returns `page_table[addr / 4096] + addr`, so it is the same
lookup `relocate()` computed. Nothing was added to `mem` except
`std::hash<Ptr<T>>`, so a `Ptr` can key the sets the implicit-master routing
uses.

`OperationPending` had an anonymous `union` around its release fields. A `Ptr`
member gave the union no usable default constructor, so it is a plain struct
now. It holds one variant, so the union bought nothing.

### Verified here

- `container/vita3k-docker.sh build`, `test` (3 of 3) and `format-check` are
  clean.
- `vita3k/mem/tests/external_mapping_tests.cpp`, 4 tests. It runs a real
  `add_external_mapping` / `remove_external_mapping` round trip and shows both
  halves: the guest address resolves to the mapped buffer and sees the data,
  and a host pointer cached before the mapping does not address it any more. It
  does not read through that stale pointer, because the target is `PROT_NONE`.
- Struct sizes, measured before and after with the same compiler:

  | | before | after |
  |---|---|---|
  | `sizeof(ngs::Rack)` | 112 | 104 |
  | `sizeof(ngs::Voice)` | 264 | 264 |
  | `sizeof(ngs::System)` | 288 | 288 |
  | `sizeof(ngs::ModuleData)` | 128 | 128 |

  `offsetof(Voice, datas)` stays 8, so the padding absorbs the narrower member.
  `sizeof(Rack)` drops by 8 because two 8-byte pointers became two 4-byte
  addresses. `Rack::get_required_memspace_size` reports that number to the guest
  and the emulator uses the same `sizeof`, so the two stay consistent inside a
  run, but the size the guest is told is 8 bytes smaller than before.

### Not verified, and what to watch

- **No device run.** No APK was built or installed from this change, so the
  acceptance criteria above are untested. The claim that this restores mixing on
  Turnip rests on the mechanism above, not on a measurement.
- **Unlocked page-table reads.** `Ptr<T>::get` reads `mem.page_table[]` and
  `mem.use_page_table` without a lock, as it already did everywhere. The change
  turns a handful of cached reads per frame into one per access in the mix loop
  and in every module `process()`, on x86-64 and on the Pocket S. That is a data
  race on a non-atomic object, not a demonstrated fault. Making the read take
  `external_transition_mutex` belongs in `mem`, not here.
- **No NGS-level test.** `vita3k/ngs/tests` deliberately builds only `dsp.cpp`
  and links googletest alone (`vita3k/ngs/CMakeLists.txt`), so a test there
  cannot reach NGS or the emulator memory. Linking the real library into a test
  target is a separate change. The test that exists proves the property the
  conversion relies on; it does not exercise `Voice`, `Rack` or `update`.

### Still to do

1. Run the Jak in-game walk on Turnip and count `NGSRATE` per in-game boot. The
   log holds menu and in-game boots together and the menu masks the result, so
   count per boot as issue 07 says.
2. Confirm `STALE HOST POINTER` at `sceNgsSystemUpdate` / `sceNgsVoiceKill` is
   gone, or no longer precedes the warnings.
3. Confirm stock in-game still mixes.

### Two things left alone, both pre-existing

- `release_rack` does not check that its erase from `system->racks` matched, so
  a second `sceNgsRackRelease` on the same handle destroys the rack twice.
- The null check added in `sceNgsRackRelease` is defensive. `init_rack` requires
  a non-null system and `Rack::system` is never cleared, so for any rack the
  emulator created it does not fire.

## Comments
