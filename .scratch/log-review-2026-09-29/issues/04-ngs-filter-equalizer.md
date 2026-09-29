# 04: Implement the NGS filter and equalizer modules

Status: open
Claimed: 2026-09-29 Claude Code session (Opus 5.5)
Type: task
Label: needs-info

## Context

Uncharted logged the filter and equalizer warnings at 00:16:29. Both
modules are biquad filters. The filter module has one filter. The equalizer
has up to 4 in series. Each takes either filter parameters
(`SceNgsParamFilter`: mode, frequency, resonance, gain) or raw
coefficients (`SceNgsParamCoEff`, formula in `filter.h`).

## Plan

1. Write one shared biquad (direct form 1 or transposed direct form 2) with
   per-channel history in a logical state.
2. Compute coefficients from `SceNgsParamFilter` with the standard Audio
   EQ Cookbook formulas (Robert Bristow-Johnson) for each
   `SceNgsParamFilterMode`. `SCE_NGS_FILTER_MODE_OFF` passes the sound
   through. Resonance is Q. Gain is linear, not dB (check the SDK sample
   values if they are available; record the choice).
3. For the coefficient struct IDs, use the coefficients as given.
4. Keep the product copy code that exists today (the filter copies product
   0 to product 1; the equalizer copies to products 1 to 3), after the
   filter runs.

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

Not implemented. No public source gives the units of `fResonance` and `fGain`, or the formula for each mode. vitaAL hints that the SDK param layout may have one entry per channel. The routing is only inferred: each send filter acts on its own output (SEND_1 on output 0, SEND_2 on output 1). A wrong filter on the main output makes the sound muffled, which is worse than no filter.

What was done instead: the module logs the parameters that the game
sends, up to 16 distinct sets, at info level. The lines start with
`NGS filter`. The sound still passes through with no change.
The helper is `is_new_param_set` in `vita3k/ngs/src/param_log.cpp`.

Next step: play Uncharted (and WipEout 2048) with `log-level: 2`, pull
`vita3k.log`, and read the values. For example, a `gain` near 1.0 means a
linear gain, and a `gain` from -12 to 12 means dB. Then decide the units
and implement the module. The research notes are in the session scratchpad (`ngs-research.md`). Their summary is in `spec.md`.

## Comments
