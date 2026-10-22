Download **@APK@**. It installs as **@LABEL@**, package `@PACKAGE@`.

This is the separate test app requested for testing now. It does not update
VITAdev or the two earlier pre-release apps. Configure the game folder and
import your intended Vulkan driver into this app. Check the actual `driverID`
in its session log and match the Plus app's settings.

This candidate includes Plus's Turnip framebuffer-read depth/stencil workaround
and Vulkan vertex alignment with matching buffer repacking. It retains the
previous visual ports. Shader cache version 21 rebuilds older pipelines.

The visual application code was validated at `@APPLICATION_SOURCE@` with Linux
and Android ARM64 native builds, 119 C++ tests and 34 Python tests. This build
adds an explicit separate-app option and its packaging checks. CI checks the
APK signature, package, label, 16 KiB alignment, assets and matching symbols.
The separate app uses the existing CI signing certificate. Future builds of
this app must keep its ID and certificate.

Source: `@SOURCE@`.
Native build ID: `@NATIVE_ID@`.
APK SHA-256: `@APK_SHA256@`.

Follow TESTING.md for the matched Jak and Uncharted scenes. The missing sky and
black rectangle still need device confirmation. Remaining GXM/JPEG coordination
and photo read-back ports are absent. Device performance is not measured.
