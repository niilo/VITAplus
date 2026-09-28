# 06: Record device facts and the baseline

Status: open
Type: experiment
Label: ready-for-human
Blocked by: 02, 04, 05, 19, 20, 21, 22, 23

## Goal

Know where the device is today, on the final code base, so every later
change is compared with this baseline.

## Human steps (session H1)

1. Pick 4 titles from your own library: one 3D title that targets 60 FPS,
   one 3D title that targets 30 FPS, one 2D title, and one title that runs
   badly today.
2. For each title: name one scene, the save slot to load, and the exact
   buttons from boot to the scene.
3. Pick the Ayaneo mode for all tests: Full Power, Game or Balanced.
4. Install the 4 games in the release package `org.vita3k.emulator`.
5. Install a Turnip driver from K11MCH1/AdrenoToolsDrivers that is built
   from Mesa 24.2 or newer, with the driver picker in the app. Older Turnip
   does not know this GPU. Tell the agent the file name.

## Agent steps

1. Build and install the release APK from `master` (or from the branch
   that ticket 02 created).
2. Run `device.sh info`. Put the output under `## Answer`. Update the
   "verify" rows in `../spec.md`.
3. Boot one game. Copy from `vita3k.log`: the Vulkan device name, driver
   version, chosen memory mapping mode, present mode, and the validation
   layer line. Copy the list of allowed modes from the Memory Mapping
   setting.
4. Run the protocol in `../spec.md` on all 4 titles:
   - A: defaults with the stock driver. B: defaults with Turnip.
   - With the better driver: A: `validation-layer: true`. B:
     `validation-layer: false`.
5. With the better driver and validation off, do one 20 minute run of the
   title with the lowest average FPS, with `device.sh thermal` running.
6. On that title, do one run in each of the three Ayaneo modes.
7. If `keys` does not reach the game (ticket 04), ask the user to do the
   button sequence for each run, and plan the runs in one sitting.

## Output

Under `## Answer`:

1. Device facts.
2. A table: title, scene, driver, validation, average FPS, percent at
   target, 99th percentile ms, maximum ms.
3. The 20 minute result: FPS per minute and thermal status per minute.
4. The Ayaneo mode result.
5. The settings used as "defaults" for all later tickets: driver and
   validation value. Later A runs use these.

## Answer

Progress 2026-09-28 (agent steps 1 to 3 in part, before session H1):

- The device had the official Vita3K 0.2.1 release (Vita3K team key).
  A build with another key cannot update it. With the user's consent, the
  agent uninstalled it and installed a debug-key release build. Before
  that, `files/` (373 MB) was copied to `tmp/device-backup/files/`, and
  the official APK to `tmp/device-backup/official-0.2.1.apk`. The files
  are back on the device. The games and saves are on the SD card
  (`pref-path: /storage/4CDE-C1FC/emu-app-data/psvita/`) and did not
  change. `MANAGE_EXTERNAL_STORAGE` was allowed again with `appops`.
- Custom drivers are kept in the app's internal storage, so the uninstall
  deleted them. The config had `custom-driver-name:
  Balemuni_Apex_v2_ULTIMATE_SD8Gen2`. It is now `""` (stock driver). The
  driver zips are in `/sdcard/Download/turnip-drivers/` (Turnip v26.x,
  mainline-turnip-V31, mrpurple T26 and T30, Balemuni Apex v2).
- The container build made a new debug key on each run. Fix: commit in
  `pocket-s/keep-debug-key` (the key is now in the cache volume).
- The user's config is not the default config. Values that differ include
  `resolution-multiplier: 3`, `anisotropic-filtering: 16`,
  `performance-overlay: true`. The baseline "defaults" must be chosen in
  session H1.
- Device facts are in `../spec.md` and in ticket 04. The GPU reports the
  name `Adreno (TM) 740`.
- 21 games are installed. Candidates: WipEout 2048 (PCSA00015), Uncharted
  (PCSA00029), Killzone Mercenary (PCSA00107), Ratchet & Clank (PCSF00484),
  Rayman Legends (PCSE00277), Sly Cooper: Thieves in Time (PCSA00068),
  LittleBigPlanet (PCSA00549), Sky Force Anniversary (PCSE00865).
