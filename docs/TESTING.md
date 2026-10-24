# Release checks on the device

Install the APK for the app already in use. `VITAdev-<tag>-android-arm64.apk`
updates VITAdev, including the Plus experiment. `VITA-Plus-<tag>-android-arm64.apk`
updates the regular VITA+ app. Keep both apps installed if both are in use.
Each retains its own settings, saves, imported games and Vulkan drivers.

The development build uses the tested Plus native configuration. The VITA+
release uses the existing optimized Release configuration and Android resource
shrinking. Verify startup, installation, controls, save/load, settings, driver
retention, dialogue, music and sound effects after updating either app.

Use the same game revision, save, camera, resolution, memory mapping and actual
Vulkan driver as the tested experiment. Read the latest session's `driverID`:
a requested custom driver can fall back to the system loader. Keep async
shader compilation off during visual comparisons. Let shaders compile,
restart, and repeat the same scene.

- Jak: compare the sky, terrain, water, HUD and animation at the same checkpoint.
- Uncharted: repeat the camera view that previously showed a black rectangle.
  Compare lighting, shadows and stencil effects.
- Audio and loading: repeat save/load and level transitions. Listen for missing
  or stuck voices and verify that music and dialogue continue.
- Linux: launch the AppImage on the Steam Deck, open the existing data folder,
  check controller input and repeat a known gameplay scene.

The manifests record the source commit, package, certificate, version and
native build ID. Use only the matching native symbols when investigating a
crash. APK asset, signing, alignment and symbol checks run before publication.
Sustained performance, power and thermals require device measurements.
