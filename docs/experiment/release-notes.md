Download **@APK@** and install it over **VITAdev** (`@PACKAGE@`). It uses the
confirmed development key and retains that app's data and imported drivers.

Read `experiment.purpose`, `experiment.device_checks` and
`experiment.expected_runtime_paths` in the adjacent build manifest. A
runtime list containing only `vita3k/codec/src/atrac9.cpp` identifies the Plus
baseline. Both builds restore `Atrac9Decode` because the upstream bounded
decoder call has no implementation in its pinned dependency. The NGS candidate
adds the pool allocation fix and its tests. The renderer and shaders remain
unchanged from Plus. Follow TESTING.md for matched Jak and Uncharted comparisons.

Upstream: `a86b5f93db43e4c4640ad1527e2a4695aad0f35b`.
Source: `@SOURCE@`. Native build ID: `@NATIVE_ID@`.
APK SHA-256: `@APK_SHA256@`.

The first experiment releases use Android version code 1030800 so the
baseline and candidate can replace each other without uninstalling. Their
version names, source commits, manifests and release tags identify the build.
Rendering and performance still need confirmation on the device.
