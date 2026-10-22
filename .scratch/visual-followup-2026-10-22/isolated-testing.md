# VITAdev Visual Test

The user requested this separate test app after the original VITAdev key could
not be found. It installs as `VITAdev Visual Test`, package
`org.vita3k.emulator.debug.visualtest`. Its certificate is the existing CI
release certificate listed in docs/release.md. It does not update VITAdev or
the two earlier test apps. Future visual-test builds must keep this package and
certificate to update it in place.

Import the intended Turnip driver into this app and configure its game folder.
Settings and driver storage are separate from the other apps. Match the Plus
app's settings before comparing scenes. Keep the existing VITAdev app installed.

The application code matches the visual ports validated at a59d2796. This build
changes the package identity and records a fresh native build ID. Use the
adjacent manifest, checksums and matching profiling symbols.

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
