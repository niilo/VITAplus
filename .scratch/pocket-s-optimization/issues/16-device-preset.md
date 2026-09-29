# 16: Apply the measured defaults on the Pocket S

Status: open
Type: task
Label: ready-for-agent
Blocked by: 03, 08, 09, 10, 11, 12, 13, 14, 15

## Goal

On first launch on a Pocket S, the app uses the settings that tickets 03
to 15 kept. The user does not have to find them.

## Context

- `config.yml` is written on the first `NativeLib.init`
  (`config/src/config.cpp:321-327`, `:473-476`), started from
  `MainActivity.kt:107-108`. `AppStorage.isInitialSetupCompleted` is
  another first-launch signal.
- Device strings: `Build.MANUFACTURER` = `AYANEO`, `Build.MODEL` =
  `Pocket S` (`../spec.md`). Do not match GPU names.

## Steps

1. Add `data/DevicePresets.kt` with a matcher that takes manufacturer and
   model strings, and a list of presets. Each preset holds only the values
   that differ from the global defaults. Use the values under `## Answer`
   of tickets 03 to 15.
2. Apply the preset only when `config.yml` does not exist before
   `NativeLib.init`. Never overwrite a value the user set.
3. Add a "Reset to device defaults" action in the settings, with a
   confirmation.
4. Show the line "Settings tuned for Ayaneo Pocket S" in the settings.
   Put the text in `strings.xml`.
5. In the driver picker, mark the Turnip file from ticket 06 as
   recommended for this device. Do not download without a user action.
6. Add a unit test of the matcher with fake manufacturer and model
   strings: the Pocket S matches; other strings do not.

## Acceptance

- Both targets build. The unit test passes.
- On the device: stop the app, move `config.yml` to `tmp/` on the device,
  launch, pull the new `config.yml`, and paste the preset lines under
  `## Answer`. Then put the old `config.yml` back. Do not use
  `pm clear`: it deletes the installed games and saves.
- After the user changes a value and restarts, the value stays.

## Answer
