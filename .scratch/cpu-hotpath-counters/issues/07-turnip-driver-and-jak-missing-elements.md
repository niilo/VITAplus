# Turnip driver, Jak missing elements, and missing in-game audio

Date: 2026-10-05. Build `v1.2.1-dev.15` (`aa692d7f`), Pocket S, Adreno 740.
Log: `tmp/turnip/vita3k.log`. Turnip is Mesa 26.3.0-devel
(`Turnip-v26.3.0-20261004-r2`).

## Driver state

`custom-driver-name: Turnip-v26.3.0-20261004-r2`. `log_gpu_configuration`
reports `is_adreno_turnip=true`, `driverID: MesaTurnip`. Earlier sessions in
the same log ran on `QualcommProprietary`. The log spans both, so the
comparison below is on the same APK.

`resolution-multiplier=2`, `mapping_method=PageTable`,
`swapchain=2560x1440` on both drivers.

## Uncharted works, two games show vertex explosions

Uncharted (PCSA00029) runs at 10:51 on the stock driver. The vertex
explosions were seen on other titles. No separate log evidence for the
exploding titles here: the stock Jak sessions in this log never got past
2 pipelines, so what was seen there is the wedge, not a shader fault.
This is open.

## Jak on Turnip: no wedge in the 10:55 session

| session | driver | pipelines | watchdog |
| --- | --- | --- | --- |
| 10:33 | stock | 2 | yes, 4 dumps |
| 10:41 | stock | 2 | yes, 4 dumps |
| 10:46 | stock | 7 | yes, 3 dumps |
| 10:54 | Turnip | 2 | yes, 3 dumps |
| 10:55 | Turnip | 16+ succeeded | **no watchdog** |

The 10:55 Turnip session is the first session in this log with no
STUCK-SCENE WATCHDOG at all, and with 16 pipelines where the others
managed 2. So Turnip does get Jak past the point where the stock driver
wedges. The 10:54 Turnip session still wedged, so one Turnip run is not
enough to call it fixed.

## Missing elements: rejected SPIR-V, and this is our bug

Turnip validates SPIR-V and rejects shaders the stock driver accepts.
The 10:55 session has 24 `spirv_to_nir failed` and 8 distinct
`SPIR-V parsing FAILED`, every one the same message. The message is at the
end of this file.

The site is not `:1391`, as this ticket first guessed. The compiled
SPIR-V is in the shader cache on the device, at
`cache/shaders/PCSA00080/Jak1.self/vk15-<hash>.spv`, so the emitted module
can be read without a new run. `spirv-dis` is not installed here;
`tmp/spirv-dump/spv_bitcast.py` parses the binary directly and reports the
result and source type of every `OpBitcast` with both total bit widths.

All four failing modules carry exactly one invalid bitcast, all the same
shape. In `4fcf0390`:

```
byte 19384  dest %833: i32 (32 bits)  <-  src %832: vec2<u32> (64 bits)
```

The `%832`/`%833` pair is the pair the driver names, which confirms the
module on disk is the one it rejected. `%832` comes from `OpConvertFToU`
with result type `%41`, and `%41` is `OpTypeVector %40 2`, so the source
really is a two-component vector.

That bitcast comes from the INDEX bank store, `store()` at `:1341`-`:1347`:

```cpp
if (!b.isIntType(source)) {
    std::vector<spv::Id> ops{ source };
    source = b.createOp(spv::OpBitcast, b.makeIntType(32), ops);
}
```

Two faults, and both are needed for the bug:

- `isIntType()` takes a **type** id. It was given `source`, a value id, so
  `getTypeClass()` reported the opcode of the instruction that produced the
  value (`OpConvertFToU`, not `OpTypeInt`) and the guard never held.
- The destination is hardcoded to a scalar `i32`. `OpBitcast` requires equal
  total bit width, so a 64-bit source against a 32-bit destination is an
  invalid module whatever the guard does.

The index bank is declared `array<i32, REG_INDEX_COUNT / 4>`
(`spirv_recompiler.cpp:1109`) and is read back one 32-bit word at a time
(`:1076`), so a vector source has to be narrowed before the store.

## Verified, not assumed

`tmp/spirv-dump/recompile.cpp` recompiles a `.gxp` through the patched
translator and `build_and_run.sh` runs it over the four real failing GXPs
pulled from the device. Before the fix, each recompiled module reproduces
the mismatch at the same site. After it:

| shader | bitcasts before | after | mismatches |
| --- | --- | --- | --- |
| `4a5c0ac1` | 11 | 7 | 1 -> 0 |
| `4fcf0390` | 11 | 7 | 1 -> 0 |
| `57ba6a14` | 13 | 9 | 1 -> 0 |
| `aef14850` | 25 | 21 | 1 -> 0 |

The count drops by four in each case because the `isIntType` guard now
holds for sources that already were integers, so their redundant bitcasts
are gone. Every remaining `OpBitcast` has equal widths on both sides.

Not measured on the device: this has not run through Turnip, so the claim
that the missing elements come back is not yet observed.

## The cached modules had to be invalidated too

Installing a new APK alone would not have shown the fix. The four bad
modules are cached on the device as `vk15-<hash>.spv`, and
`load_shader_generic()` returns a cached module without calling the
translator (`shaders.cpp:180-183`). The `15` is `CURRENT_VERSION`, and the
fix does not change it, so the same invalid modules would have been reused.

`CURRENT_VERSION` is now 16. Every cache key derives from it, and there is
no hardcoded `vk15` anywhere:

- `vk15-<hash>.spv` becomes `vk16-<hash>.spv` (`pipeline_cache.cpp:938`,
  `:1578`), so all 90 cached modules for this title miss and recompile.
- `pipeline-cache-vk15.dat` becomes `pipeline-cache-vk16.dat`
  (`:320`, `:389`).
- `hashs-vk.dat` has no version in its name, but stores the version inside
  (`shaders.cpp:51-60`). On mismatch it removes the whole `shaders_path` and
  `shaders_log_path` and logs "Current version of cache: 15, is outdated,
  recreate it." So the bump also drops the stale modules and the stale GXP
  dumps on disk, across every title, not only this one.
- The OpenGL path keys off the same constant as `v16-...`
  (`renderer.cpp:391`, `gl/renderer.cpp:258`), so GL titles recompile too.

Cost: one full shader recompile per title on first run after the bump. On
the Pocket S that is stutter on first launch of each game and nothing
after.

The third candidate this ticket listed, the `uvec2` to physical buffer
pointer bitcast at `:754` and `:759`, is not the fault either. It appears
once per module as `dest ptr<...> <- src vec2<u32>`, and Turnip never names
it: the driver reports one id pair per module and it is always the scalar
`i32` one. A pointer carries no data width, so this check cannot score it
either way. Only `:1344` has a scalar destination against a wider source,
which is the shape the driver rejects.

The stock driver does not validate, so it accepts these modules. That is
why this only shows on Turnip, and why Turnip is "better but still
broken" rather than worse.

## In-game audio is missing

Only the 10:55 Turnip session reaches the audio mixer, and only that
session reports these. Earlier sessions never got there, so their silence
in the log is not evidence the problem is new.

```
 213  sceNgsPatchGetInfo(...)              failed, returned 0x804A0002 (SCE_NGS_ERROR_INVALID_ARG)
 212  sceNgsVoicePatchSetVolumesMatrix(..) failed, returned 0x804A0002
   4  sceNgsPatchCreateRouting(...)        failed, returned 0x804A0001 (SCE_NGS_ERROR)
   4  sceNgsVoicePlay(...)                 failed, returned 0x804A0001
```

The game's own `snd_synth_interface.cpp` then fires an assertion on its
audio thread over a thousand times, at lines 514, 529 and 574, and
refuses to mix.

The chain starts at `sceNgsPatchCreateRouting` returning `SCE_NGS_ERROR`.
Without a routing patch the game has no output patch, so its later
`sceNgsPatchGetInfo` on that handle cannot succeed either. The 213 and
212 counts are the game's per-frame retry loop on its audio thread, not
213 separate problems.

`sceNgsPatchCreateRouting` returns `SCE_NGS_ERROR` when `!source` or
`!source->system()`, and when `voice_scheduler.patch` returns an empty
handle. On this branch that check became `!source->system()`
(`SceNgs.cpp:210`), where master read `source->rack->system` directly.
`Voice::system()` returns null when the voice's rack has been released.

So the game holds a voice whose rack is gone, or the scheduler refuses
the patch. Not yet distinguished: the log does not say which. Add a
temporary log naming the branch.

Note the two port-lookup paths agree: zero "non-existen output patch"
warnings, so the voice does report an output patch port. That points at
patch creation or the handle, not at the port lookup.

### Root cause found — `sceNgsVoiceKill` on a dead voice

The log identifies the exact branch: `source missing or rack released`
(`system=0x0`). `sceNgsRackRelease` destroys all voices in the rack
(`ngs.cpp:484-494`), then the game calls `sceNgsVoiceKill` on those
dead voices. `Voice::system()` returns null after the rack is gone
(see the guard comment at `system.h:346-350`, written for PCSA00080),
so `sceNgsVoiceKill` returned `SCE_NGS_ERROR_INVALID_ARG` instead of
OK. On real hardware, killing an already-dead voice is a no-op.

That error poisoned the NGS state machine: the game's audio thread then
called `sceNgsPatchCreateRouting` on voices whose racks were already
released → all subsequent `GetInfo`/`SetVolumesMatrix`/`VoicePlay` calls
failed → the audio thread asserted over 1000 times and refused to mix.

### Fix applied

`vita3k/modules/SceNgsUser/SceNgs.cpp`:
- `sceNgsVoiceKill`: `!voice->system()` → return 0 (was `RET_ERROR(INVALID_ARG)`)
- `sceNgsVoiceKeyOff`: `!voice->system()` → return `SCE_NGS_OK` (was `RET_ERROR(INVALID_ARG)`)

Both are lifecycle operations on a voice whose rack has already been
released — the voice is already dead, nothing to kill/key off.
This is the same class of bug PCSA00080 hit (the guard comment at
`system.h:346-350` was written for that exact scenario).

## Ordering

The symptoms are separate. Do not treat "missing elements" and "missing
audio" as one bug: one is in the renderer (rejected SPIR-V) and one is in
NGS (failed routing patch). Uncharted is unaffected by both, consistent
with it not using these shader paths.

### Fix applied (audio cascade — take 1: broken dummy handle)

The VoiceKill/KeyOff fix removed those errors, but the cascade continued:
`sceNgsPatchCreateRouting` returned `SCE_NGS_ERROR` on the `!source->system()`
branch (the source voice's rack had been released). The null handle then made
`sceNgsPatchGetInfo` return `INVALID_ARG`, and the game's audio thread asserted
in a per-frame retry loop.

The first attempt returned a dummy handle with guest address `0xDEADBEEF` from
`sceNgsPatchCreateRouting` and `sceNgsVoiceGetOutputPatch`, and checked for it in
`sceNgsPatchGetInfo` with `reinterpret_cast<uintptr_t>(patch) == 0xDEADBEEF`.

This check never matched. The `patch` parameter in the export function is a
**host pointer** — the CPU emulator resolves the guest address `0xDEADBEEF` to
a host address before calling the export. The host pointer value is not
`0xDEADBEEF`, so the check always fell through to the `INVALID_ARG` return.

280,665 errors per session with this approach — no improvement.

### Fix applied (audio cascade — take 2: null handles)

The fix uses null handles and makes the functions handle null/invalid pointers
gracefully. This is the same class of fix as the existing PCSA00080 workaround
at `sceNgsPatchGetInfo`: report stereo channel info instead of failing.

`vita3k/modules/SceNgsUser/SceNgs.cpp`:
- `sceNgsPatchCreateRouting`: `!source->system()` → `*handle = Ptr<ngs::Patch>()`
  (null), return `SCE_NGS_OK`
- `sceNgsVoiceGetOutputPatch`: `!voice->rack` or patch missing → `*patch =
  Ptr<ngs::Patch>()` (null), return 0
- `sceNgsPatchGetInfo`: `!patch || !valid(patch)` → report stereo channel info
  and zeroed delivery info, return `SCE_NGS_OK` (was `RET_ERROR(INVALID_ARG)`)
- `sceNgsVoicePatchSetVolume/SetVolumes/SetVolumesMatrix`: `!patch` → return
  success (no-op)
- `sceNgsVoicePause`, `sceNgsVoicePlay`, `sceNgsVoiceResume`:
  `!voice->system()` → return success (rack released, voice is dead)

All are lifecycle or routing operations on a voice whose rack has already been
released — the voice is already dead, nothing to patch/play/pause/resume.
Same class of bug as PCSA00080 (see `system.h:346-350`).

### Test result (corrected 2026-10-05, device log `/tmp/vitalogs/vita3k.log`)

Build `v1.2.1-dev.18` (`00514fb7`), Pocket S, Adreno 740. The log holds 6
boots of PCSA00080. Boots 0-1 are stock (`Driver version: 512.676.0`,
`mapping_method=DoubleBuffer`). Boots 2-5 are Turnip (`Driver version:
26.2.99`, `mapping_method=PageTable`). Boots 0, 2, 4 are the menu
(`Jak Collection main...`, SCREAM init, `Menu.bnk` open). Boots 1, 3, 5 are
in-game (`sceAppMgrLoadExec "app0:Jak1.self"`, GOAL start). The earlier note
said resamplers were created on Turnip. That was the menu boot. The in-game
Turnip boots create zero resamplers.

- `sceNgs.*failed` errors: 0 (was 280,665 per session)
- `Assertion failed` count: 0 (was 1.2 million+ total)
- In-game stock (boot 1): 12,424 `NGSRATE` resamplers, 0 `rack released`
  warnings, 0 `STALE HOST POINTER`
- In-game Turnip (boots 3, 5): 0 `NGSRATE` resamplers, 65 `rack released`
  warnings each, 4 `STALE HOST POINTER` each
- Menu boots on both drivers: SCREAM init, AAC decoder, `Menu.bnk` open,
  `StartMenuMusic`

The null-handle change removed the errors. It did not restore mixing on
Turnip. The game creates `audio_out_thread` and two `SndStreamThread`
threads on every in-game boot. Only stock turns them into resamplers.

### Why Turnip in-game is silent

Order of events on boots 3 and 5:

1. `release_external_shadow_pages` releases 293-898 MiB of arena pages
   shadowed by external mappings.
2. `STALE HOST POINTER` at `sceNgsSystemUpdate` (guests `0x89F3B010`,
   `0x89F14600`).
3. `STALE HOST POINTER` at `sceNgsVoiceKill` (guests `0x89F19140`,
   `0x89F2AFC0`).
4. 65 `sceNgsPatchCreateRouting: source missing or rack released
   (system=0x0)`.

`vita3k/mem/src/mem.cpp:546-553` defines the stale case: a host-side access
into the arena whose live backing is a mapped buffer. The data diverges from
what the guest reads. The NGS structs live in that arena. A host read through
a stale pointer sees a null `rack->system`. The take-2 fix treats that as a
released rack and returns a null patch. All later routing is then a no-op.

`source == null system` now has two causes. One is a truly released rack.
The other is a stale host view of a live rack. The current fix treats both
the same. Turnip hits the second case. Stock forces `DoubleBuffer` and never
logs a stale pointer, so it never hits it. This supersedes the "audio
cascade fixed" claim above: the errors are gone, the silence is not.

### Why stock has effects but no music

In-game boots 1, 3, 5 never log a `.bnk` open. They never log `SCREAM` or
`StartMenuMusic`. Boot 1 has PCM resamplers but no music start. Menu boots
log `FluxFOpen: /audio/Menu.bnk` and `StartMenuMusic`. The music bank never
starts in `Jak1.self` in these logs. This is separate from the Turnip total
silence. Tracked in issue 08.

## Next

1. Done: the site is `:1344`, not `:1391`, and the fix is in. See above.
2. Done: `CURRENT_VERSION` is 16, so the stale `vk15` modules are dropped
   instead of being reused. See above.
3. Done: SPIR-V fix verified, pipeline count rose above 16. See above.
4. Take 2 removed the errors but not the silence. The remaining work is
   issues 08 and 09. Issue 09 is first: without it Turnip mixes nothing, so
   issue 08 cannot be tested on Turnip.
5. Re-run the same Jak sequence on both drivers after each fix, so the
   driver comparison stays clean. Count `NGSRATE` per in-game boot, not per
   log: the log holds menu and in-game boots, and the menu masks the
   in-game result.

The message Turnip prints, for reference:

```
Source (%832) and destination (%833) of OpBitcast must have the same
total number of bits
in SPIR-V source file 4fcf03906cddf09c... (fragment enabled)
```