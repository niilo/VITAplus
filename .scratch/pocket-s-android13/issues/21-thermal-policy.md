# 21: React to the thermal status

Status: open
Type: research
Label: ready-for-human
Blocked by: 04

## Goal

The part is rated at 15 W sustained (a vendor claim, in `../spec.md`). Under a
sustained Vulkan load the clocks will fall. Today the emulator does nothing
about that, so a session that starts fast ends slow and the user has no idea
why.

## What Android 13 offers

- `PowerManager.getCurrentThermalStatus()` is API 29. The NDK `AThermal_*` API
  is API 30. Both work.
- `AThermal_getThermalHeadroom` may return `NaN` on a device whose thermal HAL
  does not implement it. Check for `NaN`.
- Android's guidance warns that some devices always report
  `THERMAL_STATUS_NONE`, so the status alone is unreliable and headroom is the
  better signal.
- Do not poll headroom faster than about once a second. Use the listener.
- `cmd thermalservice override-status` and `reset` work on Android 13. There is
  no `get-current-status` and no `headroom` subcommand on this version, so the
  test harness uses `dumpsys thermalservice`.

Status values: 0 none, 1 light, 2 moderate, 3 severe, 4 critical, 5 emergency,
6 shutdown.

## Steps

1. Read the thermal status with the NDK API, behind a temporary setting
   `thermal-log`, default on, and log every change with the value and the
   current GPU and CPU clocks. Ticket 03's `clocks` command already samples
   those.
2. Run the 20 minute protocol from `../spec.md` on each benchmark title, at the
   configuration ticket 05 chose, and record the FPS curve against minutes.
   Criterion 3 of `../spec.md` is a number this produces. The preset from ticket
   22 does not exist yet, which is why this runs at the ticket 05 values. Ticket
   23 repeats the run once with the preset in place.
3. Produce the numbers for the three options, not the decision. Measure each
   one separately on the title that drops frames the most:
   - the resolution multiplier one step down;
   - the cheapest screen filter from ticket 18;
   - nothing, as the control.
   For each, record the FPS before the change, the FPS after it, the time
   between the status change and the change taking effect, and a screenshot
   from before and after.
4. Write the three results into `../spec.md` as a table and stop there. The
   policy of what the app does at a given status belongs to a person, because
   it decides what the user sees. Two constraints on that decision: do not lower
   the frame rate target, because a game that drops to 24 FPS instead of 30 is a
   worse result than a game that misses frames without changing the target; and do not ship a behaviour the user
   cannot see.
5. If the device does not drop below its target in 20 minutes on this title,
   set `Status: rejected` and stop here. A thermal policy for a device that does
   not throttle is untested code.

## Acceptance

- The FPS against minutes curve for a 20 minute run per title, with the
  thermal status logged alongside.
- The measured effect of each of the three options, as a table in `../spec.md`.
  This ticket does not choose a policy; it produces the numbers a person needs
  to choose one.
- `Status: rejected` if the device does not drop below its target in 20
  minutes.

## Answer

## Comments
