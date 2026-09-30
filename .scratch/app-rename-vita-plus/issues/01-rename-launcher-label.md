# 01: Show the name VITA+ in the launcher and in the app, keep every path

Status: open
Type: task
Label: ready-for-agent

## Goal

The app shows "VITA+" instead of "Vita3K" (and "Vita3K+" in the app
screens). Nothing that decides where data is stored changes. After
`adb install -r` over the current app, the same settings, games and save files
are in use.

## What changes (user-visible text only)

`android/app/src/main/res/values/strings.xml`:

| Line | Key | Now | New |
| --- | --- | --- | --- |
| 3 | `app_name` | `Vita3K` | `VITA+` |
| 41 | `apps_list_app_title` | `Vita3K+` | `VITA+` |
| 176 | `initial_setup_welcome_title` | `Welcome to Vita3K+` | `Welcome to VITA+` |

Text that talks about the emulator project (for example "Vita3K is an
open-source PlayStation Vita emulator", the piracy notice, the update texts
"A newer Vita3K build", "while Vita3K restarts the game", the notification
channel text) names the project, not the app. Decide with the user. Default:
leave them, and change them only if the user wants every text to say VITA+.

Also check these for text the user sees:

- `vita3k/android/jni/main_android.cpp:380`: the SDL window title `"Vita3K"`.
  It is not visible on a phone. Change it to `"VITA+"` only if the window
  title is shown (for example on a desktop mode or on a DeX display).
- The Kotlin screens: `AppsListScreen.kt` (about text, lines 481 and 487) and
  `InitialSetupScreen.kt` hold text about the project. Leave them unless the
  user decides otherwise.

## What must not change

See `../spec.md`, section Decisions. In short: `applicationId`, `namespace`,
the Java package, the library name, the data layout, `pref-path`, the Qt names,
the Vulkan application name, and the default user name.

## Checks

1. Build the release APK: `container/vita3k.sh android release`.
2. Before the install, note the current values on the device:
   `tools/android/device.sh config-get org.vita3k.emulator pref-path` and a
   list of the games (`adb shell ls <pref-path>/ux0/app`).
3. Install over the current app: `tools/android/device.sh install
   build/android-apk/app-release.apk`. It must say `Success` without an
   uninstall. If it asks for an uninstall, the `applicationId` or the signature
   changed, and the issue failed.
4. The launcher shows "VITA+". `adb shell pm list packages | grep vita` still
   shows `org.vita3k.emulator` and `org.vita3kplus.emulator`.
5. Start the app. The game list shows the same games. `config-get pref-path`
   gives the same value as in step 2.
6. Start one game that has a save file (for example Uncharted: Golden Abyss,
   `PCSA00029`) and check that the save is found (no "new game" prompt).
7. Start Vita3K+ (`org.vita3kplus.emulator`) and check that it still works
   beside the renamed app. Both names must be easy to tell apart in the
   launcher: "VITA+" and "Vita3K+" look alike, so ask the user if the Plus app
   gets another label (this checkout does not build that app).

## Comments
