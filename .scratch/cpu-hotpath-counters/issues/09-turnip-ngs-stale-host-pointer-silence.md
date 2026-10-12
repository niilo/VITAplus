# 09: Turnip NGS reads a stale host view and drops all in-game audio

Status: open
Type: task
Label: ready-for-agent

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

## Comments
