# 22: Write the Pocket S preset

Status: open
Type: task
Label: ready-for-human
Blocked by: 05, 09, 17, 18, 19, 20, 21, 24, 26, 27, 28

## Goal

On first launch on a Pocket S, the app uses the settings that the measured
tickets chose. The user does not have to find them, and can still change each
one.

This replaces `.scratch/pocket-s-optimization/issues/16-device-preset.md`.

## Steps

1. Take the recommended values from tickets 05, 09, 12, 13, 17, 18, 19, 20 and
  21. Only a value with a measurement goes in. A value from a ticket that was
  rejected does not go in.
2. `config.yml` is written on the first `NativeLib.init`
   (`vita3k/config/src/config.cpp`). `AppStorage.isInitialSetupCompleted` is a
   separate first-launch signal. Use one of them and do not use both.
3. Match on `Build.MANUFACTURER` = `AYANEO` and `Build.MODEL`, or on the Vulkan
   device ID that ticket 01 recorded. Do not match GPU names that contain "740"
   or "7xx": the stock driver reports `Adreno (TM) 740` on more than one device.
4. Put every value in one place, with a comment naming the ticket that measured
   it and the number it produced. Name the ticket and the number on every value,
   so a later reader can check the value instead of trusting it.
5. Settings that need a device property, such as the Turnip flags from ticket
   19, cannot be set from the app. Leave them out of the preset. Put the
   `setprop` command in the `CLAUDE.md` section that ticket 23 writes, and say
   in the answer that they are not in the preset and why.
6. **Eight** settings have no field in `EmulatorConfig.kt`: `guest-cores`,
  `cpu-pool-size`, `hashless-texture-cache`, `disable-programmable-blending`,
  `surface-sync-clamp-rt`, `preempt-on-wake`, `preempt-on-wake-us` and
  `disable-raster-order`. `perf-log` is **not** among them: ticket 02 added the
  field and a switch. Several of the eight are in this plan's measurement set.
  If the preset sets one, record that it reaches the app through `config.yml`
  only, which is a different path from every other preset value, and check that
  the preset code writes it there.
7. Keep the existing `config.yml` on an installed app untouched. Only a first
  launch gets the preset.
8. **The driver needs a decision, and it is the one this preset cannot make.**
  The measured driver is Turnip, and the driver pack lives in the app's internal
  data directory, so it is lost on every uninstall (ticket 01). An app cannot
  ship it, and it cannot check for the pack before the first launch, because
  `custom-driver-name` is read before the renderer exists. Decide and record:
  does the preset write a `custom-driver-name` at all, and if so, what happens on
  a device where that pack is not installed? Or does it leave it empty and rely
  on the stock driver, which ticket 00 measured at 5.76 FPS? Whichever is
  chosen, say in `CLAUDE.md` that the Turnip pack must be installed once through
  the app's own GPU settings, and that it does not survive an uninstall.


## Answer

## Comments
