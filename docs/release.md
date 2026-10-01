# Release

CI builds only when a release tag is pushed. A normal push or a pull request does
not start a build. (The format check in `.github/workflows/format.yml` still runs
on every push, because it is not a build.)

## Make a release

```sh
git tag v1.2.0
git push origin v1.2.0
```

A tag with a dash, such as `v1.2.0-rc1`, makes a pre-release. The tag must start
with `v`.

The workflow `Release build` runs three jobs:

1. `linux-build`: the x86_64 AppImage for the Steam Deck.
2. `android-build`: the signed arm64 release APK for the Ayaneo Pocket S.
3. `create-release`: a GitHub release for the tag in this repository, with these
   files:
   - `VITA-Plus-<tag>-android-arm64.apk`
   - `VITA-Plus-<tag>-linux-x86_64.AppImage` (and `.zsync` if the build makes it)
   - `SHA256SUMS.txt`

Run the workflow again for the same tag to replace the files of the release.

CodeQL (`.github/workflows/codeql-analysis.yml`) runs on the same tags.

## Repository secrets for the Android signing key

The Android job fails at once if one of these secrets is missing. A release APK
must always use the same key. If the key changes, Android refuses to install the
new APK over the old app, and the user has to uninstall first.

| Secret | Value |
| --- | --- |
| `KEYSTORE` | the key store file, as one base64 line |
| `SIGNING_STORE_PASSWORD` | the key store password |
| `SIGNING_KEY_ALIAS` | the key alias |
| `SIGNING_KEY_PASSWORD` | the key password |

Make a key store once and keep it in a safe place (a password manager). If you
lose it, you cannot update installed apps.

```sh
keytool -genkeypair -v -keystore vita-plus.jks -alias vita-plus \
  -keyalg RSA -keysize 4096 -validity 36500
base64 -i vita-plus.jks | tr -d '\n' | gh secret set KEYSTORE
gh secret set SIGNING_STORE_PASSWORD
gh secret set SIGNING_KEY_ALIAS --body vita-plus
gh secret set SIGNING_KEY_PASSWORD
```

The APKs that you build in the container (`container/vita3k.sh android release`)
are signed with the debug key of the container. They cannot be updated by a CI
APK. Uninstall once when you change from one to the other. The games and saves
stay, because they are in the `pref-path` folder.

## Targets and the other platforms

- Android: arm64 only (`abiFilters "arm64-v8a"`). Target device: Ayaneo Pocket S.
- Linux: x86_64 AppImage. Target device: Steam Deck.
- Windows, macOS and Linux arm64: the code stays, but CI does not build them. The
  old jobs are in the git history of `.github/workflows/c-cpp.yml` (before the
  commit that made the builds tag-only). Add a job back to build one of them.
