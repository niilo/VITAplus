# Spec: the Android app is called VITA+

## Goal

The launcher name of the Android app changes from "Vita3K" to "VITA+". The app
gets a new icon. Everything that decides where data lives stays as it is, so
the renamed app uses the same settings, installed games and save files as the
current app (`org.vita3k.emulator`) and as Vita3K+ (`org.vita3kplus.emulator`).

## Decisions

- Change only the text that the user sees. Do not change identifiers.
- Keep these values exactly:
  - `applicationId` and `namespace`: `org.vita3k.emulator`
    (`android/app/build.gradle`). A new ID would be a new app with a new data
    folder `/sdcard/Android/data/<id>/files`, and `adb install -r` would not
    replace the current app. The documents provider authority
    (`${applicationId}.documents`) also depends on it.
  - The native library name `Vita3K` (`System.loadLibrary("Vita3K")`) and the
    Java package `org.vita3k.emulator` (JNI symbols are bound to it).
  - The data layout: `config.yml`, `vita3k.log`, `stdio.log`, the folders
    `vita`, `shaderlog`, `texturelog` in the app folder, and the `pref-path`
    setting (for example `/storage/4CDE-C1FC/emu-app-data/psvita/`, which holds
    `ux0` with the games and the save files).
  - The Qt names `setOrganizationName("Vita3K")` and
    `setApplicationName("Vita3K")` (`vita3k/main.cpp`). On the desktop they
    decide the config folder.
  - The Vulkan application name `"Vita3K"` and engine name
    (`vita3k/util/src/android_driver.cpp`). Some drivers choose a profile by
    application name.
  - The user name `"Vita3K"` that new users get (`vita3k/app/src/app.cpp`).
  - Log tags `"Vita3K"` and Gradle names (`rootProject.name`, `Theme.Vita3K`).
- The user sets the name as "VITA+" (capital letters, with the plus).
- Sharing with Vita3K+: both apps read the same games and saves only when both
  have the same `pref-path`. A user with the default `pref-path` has a separate
  folder for each app. The rename does not change that. Note it in the issue
  that changes the name.

## Out of scope

- A new `applicationId`, a migration of data, or a change of the `pref-path`
  default.
- The desktop apps (Qt). They keep the name "Vita3K".
- Translations (`values/strings.xml` is the only language file).

## Issues

1. `01-rename-launcher-label.md`: the name.
2. `02-choose-icon.md`: pick one of five candidates (`icons/`).
3. `03-apply-icon.md`: put the chosen icon in the app. Blocked by 02.
