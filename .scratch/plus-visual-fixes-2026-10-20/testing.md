# Plus visual fixes: device comparison

This build contains the selected Plus ports from c2f81a34 and the framebuffer
and surface fixes recorded in review.md. It is newer than the phase 1 APK at
dev-pockets3x-p1-20261019. Check build-manifest.json for its exact source and
native build ID. The launcher label is VITAdev Visual 20. Its package is
org.vita3k.emulator.debug.visualfixes.

It installs alongside the earlier apps. Settings and imported Vulkan drivers
are separate. Import the driver used by the Plus app into this app too. Check
the actual driverID line in each session log. A missing imported driver can
silently fall back to the system driver.

## Compare visuals first

Use the same game ID, update, save, scene, camera and driver in both apps.
Match memory mapping, accuracy, precision, resolution and asynchronous shader
compilation settings. Start at 1x, then repeat at 3x. Leave asynchronous shader
compilation off for the visual comparison so skipped compiling draws do not
look like rendering errors. Keep programmable blending enabled.

Shader cache version 20 rebuilds the older shader products. Compare after
compilation has settled, then restart the app and repeat to check cached shaders.
A cold-cache frame rate is not a steady gameplay measurement.

- Uncharted: compare jungle colours, lighting, silhouettes, close camera views,
  and effects that read the framebuffer. Check the same scene on Turnip and
  the stock driver when available. First and subsequent camera photos remain
  a separate check: the GPU-to-JPEG CPU read-back port is still pending.
- Jak and Daxter Collection: identify each entry separately. Compare collection
  menus, animated backgrounds and transitions, then terrain, sky, water,
  characters and HUD during gameplay. Report stale or black frames separately
  from geometry and colour errors. Test Page Table and Double Buffer separately.
- Steam Deck: use the Linux build of the same application source. Repeat the
  same visual checks with AMD Vulkan. The cloud Linux build and tests do not
  establish that device rendering is correct.

Record the launcher/build label, actual driverID, mapping mode, resolution,
scene and visible difference with every result. Paired screenshots from the
same checkpoint help distinguish geometry, colour and stale-buffer problems.

## Performance after correctness

The phase 1 counters remain available. Leave Renderer event wait off for the
first comparison. With performance logging enabled, run the same gameplay
checkpoint after warming caches at 1x and 3x. Record CPU and GPU frame times,
pipeline compilation time, sustained FPS, power and thermals. Menus do not
establish gameplay performance. This build does not include game-specific
30-to-60 FPS unlocks or full pipeline recipe preparation.

The APK and profiling symbols must have the same native build ID. Use this
release's SHA256SUMS and build-manifest.json. Its runner-generated development
signing key differs from the earlier cloud and phase 1 apps.
