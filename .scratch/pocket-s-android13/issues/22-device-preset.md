# 22: Write the Pocket S preset

Status: open
Type: task
Label: ready-for-human
Blocked by: 05, 09, 12, 13, 17, 18, 19, 20, 21

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
6. Nine settings have no field in `EmulatorConfig.kt`: `guest-cores`,
   `cpu-pool-size`, `hashless-texture-cache`, `disable-programmable-blending`,
   `surface-sync-clamp-rt`, `preempt-on-wake`, `preempt-on-wake-us`,
   `disable-raster-order` and, after ticket 02, `perf-log`. Several of them are
   in this plan's measurement set. If the preset sets one, record that it
   reaches the app through `config.yml` only, which is a different path from
   every other preset value, and check that the preset code writes it there.
7. Keep the existing `config.yml` on an installed app untouched. Only a first
   launch gets the preset.

## Acceptance

- A Pocket S with no `config.yml` gets the preset on first launch.
- An existing install keeps every value it has.
- Each value in the preset names its ticket and its measured number.

## Answer

## Comments
