# Plus baseline experiment

Install the adjacent APK over VITAdev. It uses `org.vita3k.emulator.debug` and
the development signing key that worked on the device. Keep the installed app
and its data. This experiment's version code is 1030800, above the previous
build's 1030711. The baseline and first candidate deliberately share the same
version code so either can replace the other without uninstalling.

The baseline starts from Plus `all-enhancements` at
`a86b5f93db43e4c4640ad1527e2a4695aad0f35b`. Its renderer, Android frontend,
shaders, assets, dependencies and native build configuration are unchanged.
App identity, version metadata, signing and release tooling are adapted for
this project.

Both builds share one native build correction. Upstream calls
`Atrac9DecodeBounded`, which its pinned LibAtrac9 dependency does not provide.
The baseline restores the preceding `Atrac9Decode` call. The workflow checks
that this is the entire ATRAC9 diff. This uses the existing decoder API and
does not implement bounded decoding. Compare audio as well as graphics.
The NGS candidate adds only the pool allocation fix and its regression tests.

Plus requires an untracked `fork_build.h`; the build supplies its logging
identity constants, as our previous project setup did. No emulator source is
edited to provide that header. Native About-sheet
version information remains Plus's original version; identify each APK with
its Android version name, source commit and build manifest.

Use the same game revision, save, camera, resolution and memory mapping as in
Vita3K-Plus. Check the actual `driverID` in the newest log session. An imported
driver that cannot load falls back to the system loader. Keep asynchronous
shader compilation off during visual comparisons. Let shader compilation
finish, restart, and repeat the scene.

- Jak: report the collection entry and checkpoint. Compare the sky, terrain,
  water, HUD and animation with Plus in the same scene.
- Uncharted: repeat the camera view with the black rectangle. Compare
  lighting, shadows and stencil effects. Record Turnip and stock-driver
  results separately.
- Record startup, save/load, audio, controls, settings and driver retention.
  Keep paired screenshots and the latest log session for each APK.

Each release's manifest records the exact declared runtime changes. Compare a
candidate with the baseline before accepting it. Measure sustained gameplay
performance, power and thermals only after its visuals match.

Device rendering and performance have not been measured in this environment.
