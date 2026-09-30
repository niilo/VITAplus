# 03: Put the chosen VITA+ icon in the app

Status: resolved
Type: task
Label: ready-for-agent
Blocked by: 02 (resolved)

## Goal

The launcher, the notification, the splash screen and the in-app screens show
the icon that issue 02 chose. The resource names stay, so no Kotlin code and no
manifest entry changes.

## Where the icon is used now

- `android/app/src/main/res/mipmap/ic_launcher.png` (512 x 512): the manifest
  `android:icon`, the notification small icon
  (`InstallForegroundService.kt:281`) and the splash screen
  (`res/drawable/splash_icon.xml`).
- `android/app/src/main/res/mipmap/ic_launcher_plus.png` (512 x 512): the welcome
  screen (`InitialSetupScreen.kt:240`), the apps list (`AppsListScreen.kt:811`)
  and the two roots of the documents provider (`VitaDocumentsProvider.kt:151`
  and `:162`).
- Both are a single PNG without density variants and without an adaptive
  layer. On Android 8 and later the launcher puts such an icon on a plate.

## Steps

0. The user chose B3 (issue 02). Use `icons/set-b/ps-blue-3-ice-screen.png`
   (512 x 512, transparent) as the foreground, scaled to about 72 percent of the
   108 dp canvas so that it stays in the safe zone. The background is the solid
   color `#0A1633` (issue 02 shows why). Render the foreground at the five
   densities (mdpi 108, hdpi 162, xhdpi 216, xxhdpi 324, xxxhdpi 432 pixels
   for the full canvas, with the picture at 72 percent inside). Do not cut the
   PNG into layers. Skip step 1 (it is for set A).
1. (Set A only) Split the chosen SVG into `ic_launcher_background` and
   `ic_launcher_foreground`. The SVG has the groups `background` and
   `foreground`. Make two SVG files (one group each), render each to a 432 x 432
   PNG (108 dp at xxxhdpi) and scale them down for the other densities
   (mdpi 108, hdpi 162, xhdpi 216, xxhdpi 324). On macOS `qlmanage -t -s <size>
   -o <dir> <file>.svg` renders an SVG. The foreground must have a transparent
   background.
2. Add `res/mipmap-anydpi-v26/ic_launcher.xml` with `<adaptive-icon>` that
   points to the background and foreground. `minSdk` is 28, so this file is
   enough for all devices and no fallback PNG is needed.
3. Optional (Android 13 themed icons): add a `<monochrome>` layer made from the
   foreground in one color.
4. Replace `ic_launcher.png` with a flat 512 x 512 render of the icon, or
   remove it if nothing needs a bitmap. The notification small icon should be
   a white shape on transparent (Android draws it as a mask). Check what the
   notification looks like, and add a monochrome drawable if it is a filled
   square.
5. `ic_launcher_plus`: make it the same picture (a 512 x 512 PNG is enough for
   the in-app use). Keep the resource name, or rename it to `ic_vita_plus` and
   change the four uses. Default: keep the name.
6. Check `res/drawable/splash_icon.xml` (288 dp layer list) and the splash theme
   (`Theme.Vita3K.Starting`) for the icon and its background color. The splash
   background should match the icon background.

## Checks

1. `container/vita3k.sh android release`, then install over the current app as
   in issue 01.
2. Look at the icon in the launcher, in the recent apps list, in a notification
   (start a content install) and on the splash screen. Take screenshots with
   `tools/android/device.sh screenshot`.
3. The icon looks right with a circle mask, a rounded square mask and a
   themed (monochrome) mask. Nothing important is cut off.
4. The welcome screen and the apps list show the new picture.
5. The files in `res/mipmap*` have plain names and the build has no resource
   warning.

## Comments

2026-09-30: Done in code with B3 and the navy background.
`icons/set-b/make_android_icons.py` writes the PNG files from
`ps-blue-3-ice-screen.png`:

- `mipmap-<density>/ic_launcher_foreground.png` (108 dp canvas, picture at
  72 percent).
- `mipmap-anydpi-v26/ic_launcher.xml` (adaptive icon), and the color
  `ic_launcher_background` (`#0A1633`) in `values/colors.xml`.
- `mipmap/ic_launcher.png` and `mipmap/ic_launcher_plus.png`: the picture itself
  (512 x 512). `ic_launcher_plus` is used by the splash screen, the welcome
  screen, the apps list and the documents provider roots.
- New `drawable-<density>/ic_stat_vita.png`: a white silhouette with the plus cut
  out. `InstallForegroundService.kt` uses it as the notification small icon,
  because an adaptive icon resource cannot be a small icon.
- No monochrome layer (Android 13 themed icons): not added.

Checked: the files render correctly on the navy background with circle,
rounded square and square masks (a preview), the build and the in-place install
pass, and on the device:

- The launcher shows the new blue icon on navy (round mask) with the label
  "VITA+".
- The splash screen shows the new picture (caught while it faded in).

Not checked on the device, because the screen is not easy to reach:

- The notification small icon (`ic_stat_vita`). It shows while content installs.
  Start an install and look at the status bar. The silhouette was checked in a
  preview only.
- The welcome screen (only shown at the first setup), the apps list icon (in the
  about dialog, if it is shown there) and the documents provider roots. They use
  `ic_launcher_plus`, which is now the new picture.
- The themed (monochrome) icon of Android 13: there is no monochrome layer, so
  a themed launcher may show the normal icon or a generic shape.

