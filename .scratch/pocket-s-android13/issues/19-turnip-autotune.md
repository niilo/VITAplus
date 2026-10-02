# 19: Try the Turnip autotuner settings

Status: open
Type: experiment
Label: ready-for-human
Blocked by: 00, 04

## Question

Turnip picks between GMEM (binning) and direct rendering per render pass, and
its default heuristics were tuned for desktop. A Vita3K frame is many small
passes with few vertices, which is the case Turnip sends to direct rendering.
Does telling it otherwise help?

## The knobs

From the Mesa source, not from an old wiki page:

- `TU_AUTOTUNE_FLAGS=tune_small` makes small render passes use GMEM. Mesa's own
  comment says small passes normally use direct mode "because they generally
  don't benefit from GMEM rendering due to the overhead of tiling". Vita3K is
  exactly that case, so this is the one to try first.
- `TU_AUTOTUNE_ALGO=profiled_imm` or `profiled` profiles the load instead of
  using bandwidth estimates.
- `TU_DEBUG=forcecb` turns on concurrent binning. It is off by default because
  it regressed desktop games. On Adreno 7xx it overlaps binning of one pass
  with rendering of the previous pass. It needs dependency-free render passes,
  so a game that reuses one depth buffer with clears, or whose vertex shader
  reads an attachment written by the previous pass, will not get it.
- `TU_DEBUG=nocb` turns it off again, for the reverse test.

## How to set them

Turnip reads options through `os_get_option()`, which on Android checks the
system property `debug.mesa.tu.debug` before the environment variable. So:

- `adb shell setprop debug.mesa.tu.debug <flags>` works without root, but does
  not survive a reboot.
- The Plus `tu-debug` config setting sets the environment variable only
  (`vita3k/android/jni/native_bootstrap.cpp:105-107`), so it cannot reach the
  property route. Both work. Record which route each run used.
- `TU_DEBUG_FILE` allows toggling flags at runtime by writing flag names into a
  watched file. That is the cheapest A/B, because it does not need an app
  restart.

## Steps

1. The driver is Turnip; ticket 00 settled it. Do not spend a run confirming
   it.

**Why this ticket moved up.** Turnip takes the fast framebuffer-fetch path and
the GPU is still at 93% busy. Whether the driver bins into GMEM or renders
direct is the largest remaining driver-side lever on the measured driver, and
this is the only ticket that can change it.
2. Baseline with no extra flags, then A/B/A for `tune_small`, then for
   `profiled_imm`, then for `forcecb`. Run each on the 60 FPS title and the 30
   FPS title.
3. Record FPS, the frame interval 99th percentile, GPU busy percentage, GPU
   clock and CPU clock. Also record whether any title shows new artifacts:
   concurrent binning needs dependency-free passes, and a violation shows as
   corruption, not as a slowdown.
4. Record whether GPU clock or GPU busy percentage moved. If the clock moved,
   the driver chose a different mode; if only busy percentage moved, it did
   more work for the same picture.
5. **Record the counter before running, so a null result has a cause.**
   Concurrent binning needs dependency-free render passes, and every pass here
   declares the colour attachment as an input attachment unconditionally
   (`pipeline_cache.cpp:622`). If `forcecb` does nothing, the likely reason is
   that dependency, and ticket 06's new step 2 is what would change it. Write
   down how many renders per scene `scenes.csv` reports for each configuration,
   so the record says whether the flag engaged at all.

## Acceptance

- A table per knob, with the artifact check.
- If a knob wins, record it in `../spec.md` with the exact `setprop` command
  and the exact `TU_DEBUG` value. Do not put it in the preset from ticket 22:
  an app cannot set a system property, so it would be a value the preset claims
  to set and does not.
- If none wins, set `Status: rejected` with the numbers.

## Answer

## Comments
