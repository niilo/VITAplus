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
