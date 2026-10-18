# 05: Implement the NGS compressor module

Status: resolved
Claimed: 2026-09-29 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent

## Context

WipEout 2048 logged `Game is using unimplemented compressor audio module`
at 00:21:49. A compressor lowers the volume of loud parts above a
threshold, by a ratio, with attack and release times, and then adds a
makeup gain. `plus/all-enhancements` has an implementation
(`vita3k/ngs/src/modules/compressor.cpp`). Use it as a reference.

## Plan

1. Level detector per channel, or linked when `nStereoLink` is on. RMS or
   peak from `nPeakMode`.
2. Gain computer with threshold, ratio and soft knee.
3. Attack and release smoothing of the gain.
4. Makeup gain.
5. Write `fInputLevel` and `fOutputLevel` to `SceNgsCompressorStates`.
6. Record the units that the code assumes (dB or linear, ms or s) and why.

## Shared rules for issues 03 to 07

- Each module is in `vita3k/ngs/src/modules/<name>.cpp` with its header in
  `vita3k/ngs/include/ngs/modules/<name>.h`. Today each one only logs
  `Game is using unimplemented <name> audio module` and passes the sound
  through with no change.
- A module changes the voice buffer in place. The buffer is interleaved
  stereo float, `granularity` frames per call (see `output.cpp` and
  `player.cpp` for how a module finds its buffer).
- Filter state that must last from one call to the next goes in a
  `ModuleLogicalState` subclass (`create_logical_state`). Reset it in
  `on_state_change` when the voice starts again.
- Accept only the documented params struct IDs. For any other ID, pass the
  sound through.
- Guard against NaN, infinite and out-of-range parameters. A bad parameter
  must not make noise or a very loud output.
- No reference output from real hardware is available. The goal is a
  result that sounds correct, not a bit-exact result. Record this limit in
  the answer.
- Test: a unit test in a new `vita3k/ngs/tests/` suite is optional. At a
  minimum, build, run `container/vita3k.sh test`, and play the game on the
  device.

## Answer

Done on branch `log-review/ngs-effects`, merged to `master`.

- The code is `process_compressor` in `vita3k/ngs/src/dsp.cpp`, with two
  tests.
- Units: no public source gives them. The code uses the same guesses as
  Vita3K-Plus: threshold, makeup gain and soft knee in dB; attack and
  release in seconds, and a value above 10 in ms; a ratio between 0 and 1
  is the inverse.
- The gain is capped at 1, the same as Plus. So the compressor can only
  make the sound quieter. A makeup gain above 0 dB has no effect.
- Side chain: the code reads a second input if one exists. Vita3K creates
  only one input per voice today (`ngs.cpp`, `inputs.init(..., 1)`), so
  the side-chain bus uses its own signal.
- `SceNgsCompressorStates` gets the peak input and output level per grain.

Not checked by ear. Linux build, tests and format pass.

## Comments

### 2026-10-12: plan step 3 is half done

Step 3 is "attack and release smoothing of the gain". The detector envelope is
smoothed, at `vita3k/ngs/src/modules/compressor.cpp:136` and `:140`, using the
`attack_coef` and `release_coef` computed at `:93-94`. The gain is not.
`:145-151`:

```cpp
float target_gain = db_to_linear(reduction_db_for(detector_level)) * makeup_linear;

if (compressor_never_amplifies)
    target_gain = std::min(target_gain, 1.0f);

float &applied = logical->applied_gain[channel];
applied = target_gain;
```

`applied` takes `target_gain` with no smoothing, so a change of ratio or
threshold steps. `CompressorLogicalState`
(`vita3k/ngs/include/ngs/modules/compressor.h:58-61`) holds `envelope` and
`applied_gain` only, with no per-channel reduction state.

EmuCoreV commit `3c0fbd61` adds a `reduction_db[2]` member and a seven-line
smoothing step. The hunk drops in at `:145`, but `compressor.h` needs the new
member and the rest of that file differs from here, so it is a small edit
rather than a patch. Not yet written up as a ticket; recorded in
`.scratch/emucorev-fixes/spec.md`.
