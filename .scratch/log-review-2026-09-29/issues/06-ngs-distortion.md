# 06: Implement the NGS distortion module

Status: open
Claimed: 2026-09-29 Claude Code session (Opus 5.5)
Type: task
Label: needs-info

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

## Answer

Not implemented. No public source describes `fA`, `fB`, `fClip`, `fGate` or the curve. A wrong curve sounds broken.

What was done instead: the module logs the parameters that the game
sends, up to 16 distinct sets, at info level. The lines start with
`NGS distortion`. The sound still passes through with no change.
The helper is `is_new_param_set` in `vita3k/ngs/src/param_log.cpp`.

Next step: play Uncharted (and WipEout 2048) with `log-level: 2`, pull
`vita3k.log`, and read the values. For example, a `gain` near 1.0 means a
linear gain, and a `gain` from -12 to 12 means dB. Then decide the units
and implement the module. The research notes are in the session scratchpad (`ngs-research.md`). Their summary is in `spec.md`.

### Update, 2026-09-29 (second play session)

Uncharted sent one distortion set: A 0, B 0, clip 0, gate 0, wet gain 0,
dry gain 1. With wet 0 and dry 1, the output is the input. So the
pass-through is correct for Uncharted. The logging stays in place for
other games.

## Comments
