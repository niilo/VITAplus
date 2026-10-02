# 24: Find the lowest GPU clock that holds a steady frame rate

Status: open
Type: experiment
Label: ready-for-human
Blocked by: 00

## Question, after the retarget to energy

**What is the lowest GPU clock that still holds a steady frame rate, and what
does it save?**

This question was written the other way round. It asked whether the 680 MHz cap
could be raised, because the goal was frame rate. The goal is now energy per
played frame, so a clock the game does not need is cost with no benefit.

A GPU running at 680 MHz when 400 MHz would hold the same frame rate is drawing
power for nothing. Clock is the main term in a GPU's energy, so the lowest clock
that holds the target is one of the largest single wins on this device, and it
costs nothing in playability because the frame rate does not move.

## What is already known, from ticket 00

- The GPU never exceeds 680 MHz on either driver, in any run.
- `max_pwrlevel` reads `0`, the fastest of the 15 levels, and `freq_table_mhz`
  lists 1000 MHz as the top bin. So the fast bin is selectable and something
  above the driver is choosing not to use it.
- Turnip reached 29.96 FPS in gameplay at 93% GPU busy, with a mean device draw
  not yet measured on a valid run. The first energy attempt was rejected by
  `tools/android/run_is_valid.sh`: the emulator reported no flip for 8453
  vblanks and the device was at thermal status 3.
- A later partial sample, taken while the game was not in the level, read
  220 MHz and 2% GPU busy. That is not a gameplay figure and must not be used as
  one. It is recorded only to show the idle clock is low, so there is room to
  look below 680 MHz.

## Three hypotheses

1. `KGSL_PROP_PWR_CONSTRAINT`, which WinNative's Windows build sets to
   `PWR_MAX` at queue creation and re-asserts every 1000 submissions. Under
   the new target this is the wrong direction, and the interesting experiment
   is the opposite: can the app ask for a lower constraint?
2. **A userspace governor or a vendor power mode.** `max_pwrlevel` is 0 and the
   top bin is selectable, so the driver is not holding it back. This is what
   `turbo-mode` and `adrenotools_set_turbo` touch, and what the Ayaneo power
   modes change. Test all three modes and record the GPU clock each one holds.
   This is the most likely of the three and the cheapest to test: no code at
   all, just a setting change between runs.
3. Thermal. Ticket 01 measured status 0 and 39.8 degrees C cold, but the GPU
   reached 66.7 degrees C during the ticket 00 runs, so a hot device may
   already be holding a lower clock than it could.

`/dev/kgsl/kgsl-3d0` pwrlevel control needs root, so the app cannot set it
directly. The vendor power mode and `turbo-mode` are the routes reachable from
the app and from a user.

## Steps

1. Read the current state. Log `/sys/class/kgsl/kgsl-3d0/max_gpuclk` and the
   mean of **`gpuclk`**, which is in Hz, under load, from
   `tools/android/device.sh clocks`. `gpuclk_khz` does not exist on this kernel;
   ticket 01 measured that and the map records it. Use
   `tools/android/device_power_sample.sh` for the power figure beside it, since
   that is the quantity this ticket now optimises.
2. Test the three Ayaneo power modes first, before writing any code. Run the
   30 FPS title in each with the power sampler, and record the GPU clock mean
   and the device power mean for each. If a mode holds the frame rate at a lower
   clock and draws less power, the answer is already found and the code steps
   below are not needed.
3. Test `turbo-mode: true` the same way, then `turbo-mode: false`, since it
   calls `adrenotools_set_turbo` (`renderer.cpp:2388`) and may set driver state
   that changes the clock.
4. Only then consider code. Add a temporary setting `gpu-power-constraint` with
   three values: 0 leaves the driver default, 1 asks for `PWR_MAX`, 2 asks for
   the lowest constraint. Apply it at queue creation. If the driver resets the
   constraint while running, re-assert it from the frame loop every 1000
   submissions, behind the same setting.
   Under this target, value 2 is the interesting one and value 1 is the control
   that shows whether the cap is real at all.
5. Run A/B/A at 0, 1 and 2 on the 30 FPS title and on the 60 FPS title. Record
   FPS, the frame interval p99, the fraction of late frames, **mean device power
   in watts**, GPU clock mean and maximum, and the `throttling` file. The power
   figure is the outcome. The frame rate and criterion 2 steadiness are the gate
   that decides whether a lower clock is admissible at all.
6. Read the GPU clock over 20 minutes as well as over 60 seconds. A clock that
   holds for one minute and climbs back after ten is not a result for a play
   session.
7. Every run goes through `tools/android/run_is_valid.sh`. A run at thermal
   status 3 is invalid, and the earlier energy attempt failed exactly there.

## Acceptance

- Mean device power in watts per GPU clock, for each Ayaneo power mode and each
  `gpu-power-constraint` value, on the 30 FPS title.
- The lowest GPU clock that still meets criterion 1 and criterion 2 of
  `../spec.md`.
- The power saved at that clock, against the spread of the A runs.
- The measured constraint the driver uses, in a number or in the vendor enum
  name.
- If nothing lowers the clock, `Status: rejected` with the numbers.

## Answer

## Comments