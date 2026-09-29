# 06: Implement the NGS distortion module

Status: open
Type: task
Label: ready-for-agent

## Context

Uncharted logged `Game is using unimplemented distortion audio module` at
00:16:29. The params are `fA`, `fB`, `fClip`, `fGate`, `fWetGain` and
`fDryGain`. No public document gives the exact curve.

## Plan

1. Find what `fA` and `fB` mean. Look for SDK docs, homebrew, or other
   emulators first. If nothing is found, use a waveshaper
   `y = clamp(A * x + B * x^3, -clip, clip)` style curve, a gate that sets
   samples under `fGate` to 0, and `out = dry * x + wet * y`.
2. Record the source of the curve, or record that it is a guess.
3. If a guess is too likely to sound wrong, set Status to `rejected` and
   keep the passthrough. A passthrough sounds clean. A wrong distortion
   sounds broken.

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
