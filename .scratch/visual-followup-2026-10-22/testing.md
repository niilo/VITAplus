# VITAdev visual follow-up

This build updates `org.vita3k.emulator.debug`, with launcher name `VITAdev`.
The package verifier requires the existing development signing certificate
recorded in docs/release.md. The two October 19 and 20 pre-releases have separate
package IDs and do not share settings or imported drivers with VITAdev.

Keep the installed VITAdev app. Do not uninstall it to work around a signing
error. Restore its original signing key in the build environment instead.
When the previous VITAdev APK is available, pass it as `--previous-apk` to
`.ci/package-pocket-s-dev.py`. This checks the version code as well as package
and certificate equality. Check `build-manifest.json` for this APK's identity.

## Device comparison

Use the same game revision, save, camera, resolution, mapping and actual Vulkan
driver in VITAdev and Vita3K-Plus. Check the `driverID` line after the latest
session-start banner. An unavailable imported driver falls back to the system
loader. Disable asynchronous shader compilation during visual checks.

Shader cache version 21 rebuilds earlier pipelines. Let compilation finish,
then repeat after restarting the app.

- Uncharted: repeat the close camera view that showed the rectangle over Drake.
  Check Turnip and the stock driver. Check normal lighting, shadows and a scene
  with stencil effects. The new workaround applies only to Turnip framebuffer
  reads with depth writes and unconditional stencil tests.
- Jak: identify the collection entry and checkpoint. Check the missing sky,
  terrain, water, HUD and animation. Repeat with Page Table and Double Buffer.
  Vertex attributes now use the same aligned layout in pipeline descriptions
  and uploaded buffers. Remaining GXM/JPEG coordination changes from Plus are
  still absent. The missing sky has not been reproduced in this environment.
- Record the actual driver, checkpoint, mapping, resolution and visible defect.
  Keep paired screenshots from the same camera position.

Only measure performance after visuals match. Use sustained gameplay with warm
caches. Record frame times, power and thermals. Automated tests do not establish
rendering parity, gameplay performance or a fix for the reported missing sky.
