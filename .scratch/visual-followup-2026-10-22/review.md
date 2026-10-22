# Visual and development update follow-up

User report: Jak improved but still has graphical defects and a missing sky.
Uncharted still has a black rectangle that Vita3K-Plus no longer shows. Both
recent GitHub pre-releases installed as separate apps instead of updating
VITAdev. Exact device, driver and tested tags were requested.

## Evidence

The release metadata identifies the likely two builds:

- `dev-pockets3x-p1-20261019`: `org.vita3k.emulator.debug.pockets3xci`.
- `dev-plus-visuals-20261020`: `org.vita3k.emulator.debug.visualfixes`.

Both workflows used a runner-generated debug key. Package IDs differ from
VITAdev and from each other. Changing only the package ID would still leave
the signing mismatch. Separate apps also have separate driver installations,
so an identical driver setting does not establish the same active driver.

The work starts from the second build, 94aad706. The Plus reference is
`all-enhancements` at a86b5f93. Plus 24702197 contains the Turnip LRZ workaround
that the October 20 build explicitly omitted. Plus 928e5917 contains the newer
vertex layout and repacking path that the build also omitted.

## Changes

Port the Turnip eligibility predicate, static stencil equality test and dynamic
zero compare mask together. Detect Turnip through Vulkan driver properties.
Restore the ordinary mask on subsequent draws and command-buffer starts. Keep
real stencil comparisons and other drivers unchanged. When two-sided rendering
is disabled, apply the front stencil state to both Vulkan faces.

Adapt the complete vertex alignment calculation and per-attribute binding path.
Use one layout for pipeline descriptions, copied bytes and buffer offsets.
Repack mapped streams when required. Derive their size for the current draw.
Preserve zero-stride attributes. Copy the last partial record without reading
past its declared source bytes. Clear padding in copied records. Retain the
current shader translator fixes and bump its cache version from 20 to 21.

Both development build entry points use the VITAdev package and name. CI requires
the existing development key through DEV_SIGNING_* and never creates a key as a
fallback. Packaging checks the actual APK certificate against the public
VITAdev fingerprint in docs/release.md. It records the fingerprint and version
code. An optional previous APK also checks for a version downgrade. Workflows
upload verified build artifacts and no longer target the two old draft release
IDs. No release or tag was created or modified during this work.

## Limits

The environment has an earlier cloud test key, not the documented VITAdev key.
Its public certificate differs. No compatible update APK can be signed here
without restoring the original key. The GitHub integration cannot list
repository secret names (HTTP 403), so CI key readiness is unknown.

There is no attached physical device or supplied game capture. The missing sky
has not been reproduced. The vertex correction is a missing Plus port, not a
proven diagnosis of that sky defect. The GXM wait, JPEG writer coordination,
memory placement and Uncharted photo read-back changes remain pending.

## Verification

The Linux x86-64 RelWithDebInfo application build passed. Discord Rich Presence
was disabled for this environment's optional SDK download. The final incremental
build rebuilt the changed renderer and linked the application and test binaries.
`ctest --test-dir build/container-linux --build-config RelWithDebInfo -V`
passed all 119 C++ tests across 10 CTest entries in 4.68 seconds. This includes
five vertex layout/copy tests and four Turnip eligibility tests. These tests do
not execute the game's GPU draws.

The Android ARM64 `:app:externalNativeBuildReldebug` build passed. A final
incremental native build also passed. This builds the native library, not a
signed update APK.

All 34 Python tests passed: seven APK identity checks and 27 existing performance
tool checks. The existing isolated cloud APK was rejected by the new verifier.
Gradle rejected missing CI signing settings on both a fresh configuration and a
reused configuration cache. The local verification task passed with CI disabled.
A package-task dry run confirmed the signing dependency. The old isolated suffix
was rejected during Gradle configuration.

Both workflow YAML files parsed. Their shell steps passed `bash -n`.
clang-format 22.1.8 checks, `git diff --check` and the repository signing-secret
check passed. The exported patch passed `git apply --check --reverse` against
the final working tree. No physical rendering or performance result is claimed.
