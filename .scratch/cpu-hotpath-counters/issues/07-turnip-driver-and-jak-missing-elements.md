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

## Ordering

The symptoms are separate. Do not treat "missing elements" and "missing
audio" as one bug: one is in the renderer (rejected SPIR-V) and one is in
NGS (failed routing patch). Uncharted is unaffected by both, consistent
with it not using these shader paths.

## Next

1. Done: the site is `:1344`, not `:1391`, and the fix is in. See above.
2. Build an APK and run the same Jak sequence on Turnip. Watch for
   `SPIR-V parsing FAILED` and `spirv_to_nir failed`: both should be gone,
   and the compiled-shader count should rise above the 16 the 10:55
   session reached. The pipeline count is the number to watch.
3. For audio, log which of the three `sceNgsPatchCreateRouting` early
   returns fires, then fix that one. Re-test that `sceNgsPatchGetInfo`
   stops returning `INVALID_ARG` on the game's audio thread; the
   per-frame assert count is the signal to watch.
4. Re-run the same Jak sequence on both drivers after each fix, so the
   driver comparison stays clean.

The message Turnip prints, for reference:

```
Source (%832) and destination (%833) of OpBitcast must have the same
total number of bits
in SPIR-V source file 4fcf03906cddf09c... (fragment enabled)
```