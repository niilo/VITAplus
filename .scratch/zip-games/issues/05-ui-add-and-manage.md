# 05: Add, show and remove zip games in the Android and Qt apps

Status: open
Type: task
Label: ready-for-agent
Blocked by: 03

## Goal

A user can add a zip from the apps screen, sees that the game runs from a zip,
and can remove the entry. Text follows the writing standard in `CLAUDE.md`.

## Android (Kotlin, `android/app/src/main/java/org/vita3k/emulator/`)

1. The "+" button of the apps list (`AppsListScreen.kt`) opens the install
   picker today. Add a second action "Add zip game (no install)" (new string
   resources in `res/values/strings.xml`). It picks one or more `.zip` files and
   passes their **paths** to a new JNI function
   `NativeLib.addZipApps(paths, callback)` in `vita3k/android/jni/` (new
   `native_zip_apps.cpp`, next to `native_install.cpp`). The app has
   `MANAGE_EXTERNAL_STORAGE`, so a path on the SD card works. A file that only
   has a `content://` URI is refused with a message that says to copy it to a
   folder (Storage Access Framework is out of scope).
2. Result dialog per file: added, "not an app zip", "encrypted dump: install it
   instead", "already installed", "already added".
3. The list row shows a small "ZIP" mark. A missing zip shows "File missing" and
   cannot start.
4. Long press / context menu: "Remove from list" (unregister, the zip stays) and
   "Show zip path". Do not offer to delete the zip file in phase 1.
5. App details (size, location) use the zip size and path.
6. Settings: nothing new in phase 1. Phase 2 of issue 03 adds "Games folders".

## Qt (`vita3k/gui-qt/`)

1. File menu "Add zip game...", and drag and drop of `.zip` files onto the main
   window (`main_window.cpp`). Both call the same registry functions as Android.
2. `apps_list_context_menu.cpp`: "Remove from list" for zip games, and hide the
   entries that need an installed folder (for example "Open app folder" becomes
   "Show zip").
3. A "ZIP" column or icon in the list, like Android.

## Tests

- JNI and Kotlin: a unit test of the result mapping, if the project has Kotlin
  unit tests (check `android/app/src/test`). If not, list manual steps.
- Manual on the device: add a stored zip of Uncharted from
  `/storage/<sd card>/...`, see the row, start it, remove it, add it again, move
  the zip and see "File missing".

## Done when

The manual steps pass on the Ayaneo Pocket S and on a desktop Qt build, and
`container/vita3k.sh android release` and the Linux build pass.

## Comments
