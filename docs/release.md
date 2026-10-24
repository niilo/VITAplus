# Releases

Develop from `master`. It adopts the device-tested Plus NGS experiment at
`c48f8b20ce61958fa091489e68836c782a85bdaa`. The previous master remains on
`archive/master-before-plus-promotion-20261024`. `.ci/provenance.json` records
both source revisions.

## Build and publish

Set `.ci/version-base.txt` to the next `major.minor.patch` version and add its
notes under `docs/releases/v<version>.md`. Merge the change, then tag master:

```sh
git tag v1.3.8
git push origin v1.3.8
```

The `Release build` workflow builds the Linux x86_64 AppImage and two ARM64
Android APKs. `VITA-Plus-<tag>-android-arm64.apk` updates VITA+.
`VITAdev-<tag>-android-arm64.apk` updates VITAdev. A dashed tag such as
`v1.3.9-rc1` publishes a pre-release containing VITAdev and Linux. Declare its
base version before creating the pre-release tag.

Android builds share `.github/workflows/android-apk.yml`. Stable builds use
the production key and the `Release` configuration. Development builds use
`DEV_KEYSTORE` and `RelWithDebInfo`. Master and development branch pushes also
produce verified VITAdev Actions artifacts. They can be built manually.

Each release includes matching Android native libraries, per-APK manifests,
a source patch, testing instructions, a release manifest and SHA-256 checksums.
Both Android packages use the same version code. They keep their own app data
because their package IDs differ.

Publication starts only after all required builds succeed. Artifacts are
verified again after download. The workflow uploads to a draft and publishes
only after the complete asset set is present. A published release cannot be
overwritten by rerunning the workflow. Fixes need a new tag.

## Version identity

`tools/release/version-info.sh` supplies the native and Android versions.
An exact stable tag reports that version. Commits after it report
`v<version>-dev.<count>` and increase the Android version code. The code is
`major*1000000 + minor*10000 + patch*100 + commits_since`.

The declared version in `.ci/version-base.txt` provides a floor before its tag
exists. This prevents the promoted history from lowering the version below
the installed experiment. The v1.3.8 release and the first experiment both use
1030800, so either can be installed over the other within the same package.
Future development builds increase from that code.

The in-app update links point to `niilo/VITAplus`. The release body includes
`Vita3K Build: <version_code>` for the existing updater contract.

## Signing identities

| App | Package | Existing key |
| --- | --- | --- |
| VITA+ | `org.vita3k.emulator` | `KEYSTORE`, `SIGNING_STORE_PASSWORD`, `SIGNING_KEY_ALIAS`, `SIGNING_KEY_PASSWORD` |
| VITAdev | `org.vita3k.emulator.debug` | `DEV_KEYSTORE`, optional `DEV_SIGNING_*` overrides |

The VITA+ certificate SHA-256 is
`04c7cfc63ab68d91d23a34108516066b737c87e5528f7bff621afa3fea38522a`.
The confirmed VITAdev certificate is
`c6520e3f27f5d1c54469c86111e693d938f04cb1209bb30e8c51c420117155b8`.
These fingerprints are public. Never commit, print or publish the key stores
or their passwords.

`.ci/prepare-android-signing.py` restores the appropriate key in the runner's
temporary directory and verifies its certificate and private-key access.
Development credentials default to Android's standard debug-key values;
production credentials have no defaults. CI deletes the restored files even
if the build fails. Missing or different keys stop the build.

Android updates require the same package ID and signing certificate and a
version code at least as high as the installed APK. Keep the installed app;
do not uninstall it to work around a signing failure.

## Local checks

```sh
bash .ci/check-tools.sh
bash .ci/test-native-mempool.sh
bash tools/release/check-no-signing-keys.sh --staged
```

A manual native build also needs `python3 .ci/prepare-plus-build.py` and the
pinned submodules. See `building.md` and the CI workflows for platform tools.
