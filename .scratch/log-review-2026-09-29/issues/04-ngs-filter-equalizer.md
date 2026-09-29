# 04: Implement the NGS filter and equalizer modules

Status: open
Type: task
Label: ready-for-agent

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

## Comments
