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
with `v` and must be a version, so `v1.2` and `v1.2.0` are both tags and
`v0.0.1-test` and `continuous` are not.

## How a build names itself

`tools/release/version-info.sh` decides the version of every build. CMake runs it
for the native library and `android/app/build.gradle` runs it for `versionName`
and `versionCode`, so the package version and the version the app shows always
match. Read it with:

```sh
bash tools/release/version-info.sh
```

It finds the newest version tag that is merged into HEAD and counts the commits
between that tag and HEAD.

| Build | `version` | `versionCode` |
| --- | --- | --- |
| On the tag `v1.1` | `v1.1` | `1010000` |
| 63 commits after `v1.1` | `v1.1-dev.63` | `1010063` |
| On the tag `v1.2` | `v1.2` | `1020000` |

A development build names the release it is based on and how many commits past
that release it is, so a tester can say which build they have without reading
the commit hash. It also carries its UTC build time, which the about sheet shows
as `Built`. A release build has no build time, because the tag already names it.

`versionCode` is the Android version. It is
`major*1000000 + minor*10000 + patch*100 + commits_since`, so it rises with every
release and with every development build inside a release. That is what lets an
APK install over the build that came before it. The number is also written into
the release body as `Vita3K Build: <n>`, and that is the number the in-app
updater compares against.

A checkout with no git history, such as a source tarball, reports base version
`0.0` as a development build.

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

## The Android signing key

A release APK must always use the same key. If the key changes, Android refuses
to install the new APK over the old app, and the user has to uninstall first. If
the key is lost, no installed app can be updated.

The key was made on 2026-10-01: PKCS12 key store, RSA 4096, valid for 100 years,
alias `vita-plus`, one random password (192 bits) for the store and the key.
Certificate SHA-256 fingerprint (public, use it to check a restored key):

```
04:C7:CF:C6:3A:B6:8D:91:D2:3A:34:10:85:16:06:6B:73:7C:87:E5:52:8F:7B:FF:62:1A:FA:3F:EA:38:52:2A
```

### Where the key is

| Place | What | Why |
| --- | --- | --- |
| GitHub repository secrets | `KEYSTORE` (base64), `SIGNING_STORE_PASSWORD`, `SIGNING_KEY_PASSWORD`, `SIGNING_KEY_ALIAS` | The release job signs with them. GitHub never shows a secret again, so this is not a backup. |
| 1Password item "VITA+ Android release signing key" | the same four values | The backup. |
| `~/.vita-plus-signing/` on the Mac that made the key | the key store and the password files | A temporary copy. Delete it after the 1Password step. |

The Android job stops at once if one of the four secrets is missing.

### Store the key in 1Password (run once)

```sh
brew install --cask 1password-cli      # once; then in the 1Password app turn on
                                       # Settings > Developer > Integrate with 1Password CLI
op whoami                              # must show your account
tools/release/store-signing-key-in-1password.sh        # or: ... "Vault name"
```

The script makes one Secure Note item, reads the values back, compares them with
the local files, and then asks if it may delete the local copy. It prints no
secret.

### Store the key in 1Password by hand (no CLI)

Do this in the 1Password app. Nothing here prints a secret.

1. `open ~/.vita-plus-signing` opens the folder in Finder (it is a hidden folder,
   so open it this way).
2. In 1Password: **New Item > Secure Note**. Title: `VITA+ Android release signing key`.
3. Drag `vita-plus-release.p12` from Finder into the item (or **Add More > Attach
   a file**). This is the key store.
4. Add a **Password** field named `store and key password`. Copy the value without
   showing it: `pbcopy < ~/.vita-plus-signing/store-password.txt`, paste it into
   the field, then clear the clipboard at once: `pbcopy < /dev/null`.
5. Add a text field named `key alias` with the value `vita-plus`.
6. In the notes: `Release signing key of the VITA+ Android app. PKCS12, RSA 4096.
   Certificate SHA-256 04:C7:CF:C6:...:38:52:2A. If it is lost, installed apps
   cannot be updated. See docs/release.md.` Save the item.
7. Check the copy. Click the copy button of the password field in 1Password, then
   run both lines and compare the two short hashes (they must be equal):

   ```sh
   pbpaste | tr -d '\n' | shasum -a 256 | cut -c1-12
   tr -d '\n' < ~/.vita-plus-signing/store-password.txt | shasum -a 256 | cut -c1-12
   pbcopy < /dev/null
   ```

   Then check the key store. In 1Password, save the attachment to `~/Downloads`
   (the item's attachment menu > **Save as...**) and run
   `shasum -a 256 ~/.vita-plus-signing/vita-plus-release.p12 ~/Downloads/vita-plus-release.p12`.
   The two hashes must be equal. Delete the file in `~/Downloads` with `rm -P`.
8. Only when both checks pass, delete the staging folder:
   `rm -rP ~/.vita-plus-signing`. (The copy in the project folder `.signing/`
   stays.)

Restore by hand: open the item, save the attachment, and copy the password. Set the
GitHub secrets from the saved file:

```sh
base64 -i vita-plus-release.p12 | tr -d '\n' | gh secret set KEYSTORE -R owner/repo
pbpaste | tr -d '\n' | gh secret set SIGNING_STORE_PASSWORD -R owner/repo   # password copied from 1Password
pbpaste | tr -d '\n' | gh secret set SIGNING_KEY_PASSWORD -R owner/repo
printf '%s' vita-plus | gh secret set SIGNING_KEY_ALIAS -R owner/repo
pbcopy < /dev/null
```

### Restore the key from 1Password

Set the GitHub secrets again (for example in a new repository):

```sh
R=owner/repo
item="VITA+ Android release signing key"
op item get "$item" --fields "label=keystore (base64)" --reveal | gh secret set KEYSTORE -R $R
op item get "$item" --fields "label=store password" --reveal | gh secret set SIGNING_STORE_PASSWORD -R $R
op item get "$item" --fields "label=key password" --reveal | gh secret set SIGNING_KEY_PASSWORD -R $R
op item get "$item" --fields "label=key alias" --reveal | gh secret set SIGNING_KEY_ALIAS -R $R
```

Get the key store as a file (for example to sign by hand). Use a private folder
and delete the file after:

```sh
umask 077; mkdir -p ~/.vita-plus-signing
op item get "$item" --fields "label=keystore (base64)" --reveal | base64 --decode > ~/.vita-plus-signing/vita-plus-release.p12
```

### Keys in the project folder (`.signing/`)

So that local work needs no input, the project folder holds a copy of the keys in
`.signing/` (git ignores it; mode 700, files mode 600):

| File | What |
| --- | --- |
| `.signing/dev/debug.keystore` | the dev key. It is the debug key of the Android container cache volume (`vita3k-android-cache`), which signs the app that is installed on the Pocket S. A copy here survives `container/vita3k.sh clean-cache`. |
| `.signing/release/vita-plus-release.p12` and `signing.env` | the release key and its passwords. The same key as in the GitHub secrets. |

`container/vita3k.sh android release` signs with the dev key. It copies
`.signing/dev/debug.keystore` into the container cache at the start of every
build, so the key never changes, also after `clean-cache`. On the very first run
without a project copy, it copies the key of the cache volume into `.signing/dev/`.

To sign a local APK with the release key (the key of the CI releases), use:

```sh
VITA_SIGN_WITH_RELEASE_KEY=1 container/vita3k.sh android release build/android-apk-releasekey
```

Do not install that APK over a dev-signed app (see below). Check which key signed an
APK with `apksigner verify --print-certs <apk>` (it is in the Android container,
`/opt/android-sdk/build-tools/*/apksigner`). The two certificate SHA-256 digests are:

| Key | SHA-256 of the certificate |
| --- | --- |
| dev | `85028da2c7e4321de4bd4215a45773bddcc8f90629bafd6e19ea07cc6d91cac0` |
| release | `04c7cfc63ab68d91d23a34108516066b737c87e5528f7bff621afa3fea38522a` |

The repository is public, so these files must never be committed.
`.gitignore` blocks them, and `tools/release/check-no-signing-keys.sh` fails if a
key store or a private key is tracked. The workflow `No signing keys` runs it on
every push and pull request.

The dev key is not the release key. An APK that is signed with one cannot update
an app that is signed with the other. A device that has a dev-signed app needs one
uninstall before it takes a release-signed APK (the games and saves stay in the
`pref-path` folder; the app settings and an installed custom driver are lost).

### How the secrets are protected

- The key, the passwords and the base64 text were never printed, never put in a
  command line (`ps` shows command lines) and never written inside a git
  repository. `gh secret set` read them from standard input.
- The local folder is outside every repository, mode 700, and the files are mode
  600. `.gitignore` also blocks `*.jks`, `*.p12`, `*.keystore` and
  `.vita-plus-signing/`.
- The workflow runs only for a pushed tag `v*`, so a pull request or a branch
  push can never reach the secrets. The default token is read-only; only the
  release job can write. Every action is pinned to an exact commit, so a moved
  tag cannot change the code that runs next to the secrets.
- The secrets are given only to three steps of the Android job (the check, the
  decode and the build). The key store is mode 600 and is deleted after the
  build, also when the build fails. The job uploads only the artifact folder.
- GitHub hides the secret values in the logs. A value that the build derives from
  a secret is not hidden, so no script may print them.

Remaining risks:

- The Gradle build and its libraries run with the secrets in their environment. A
  harmful library could read them. The remedy is to build unsigned and sign in a
  last step that has the secrets, which is a larger change of `build.gradle`.
- Whoever can push a tag to this repository can start a signed build. Limit it
  with a tag rule: GitHub > Settings > Rules > New tag ruleset > target `v*` >
  restrict creations.
- 1Password and the GitHub account are now the weak points. Use two-factor
  authentication on both.

### Make a key (the first time, or a new one)

The Mac has no Java, so use the JDK container. The password files are made with
`umask 077` in a private folder; nothing is printed.

```sh
umask 077; D=~/.vita-plus-signing; mkdir -p "$D"
openssl rand -hex 24 | tr -d '\n' > "$D/store-password.txt"
printf '%s' vita-plus > "$D/key-alias.txt"
container run --rm -v "$D:/work" -w /work eclipse-temurin:17-jdk keytool -genkeypair \
  -storetype PKCS12 -keystore vita-plus-release.p12 -alias vita-plus -keyalg RSA \
  -keysize 4096 -validity 36500 -dname "CN=VITA Plus release, O=niilo" \
  -storepass:file store-password.txt -keypass:file store-password.txt
base64 -i "$D/vita-plus-release.p12" | tr -d '\n' | gh secret set KEYSTORE -R owner/repo
gh secret set SIGNING_STORE_PASSWORD -R owner/repo < "$D/store-password.txt"
gh secret set SIGNING_KEY_PASSWORD   -R owner/repo < "$D/store-password.txt"
gh secret set SIGNING_KEY_ALIAS      -R owner/repo < "$D/key-alias.txt"
```

The name must not contain a `+` (it is a special character in certificate names).
Then run the 1Password script above.

### If the key may have leaked

```sh
for n in KEYSTORE SIGNING_STORE_PASSWORD SIGNING_KEY_PASSWORD SIGNING_KEY_ALIAS; do gh secret delete $n -R owner/repo; done
```

Then make a new key and set new secrets (below), and store the new key in
1Password. Every
user has to uninstall the old app once, because the signature is different. A
self-signed key cannot be revoked, so delete the old releases that are signed
with it.

## Two Android apps: release and development

A device can hold two apps from this project at the same time. They are separate
apps, each with its own application ID, its own signing key and its own name.

| | release | development |
| --- | --- | --- |
| Built by | CI, on a pushed tag | anyone, at any time |
| Build type | `release` | `reldebug` or `debug` |
| Application ID | `org.vita3k.emulator` | `org.vita3k.emulator.debug` |
| Launcher name | `VITA+` | `VITAdev` |
| Signing key | the release key | the debug key |
| Command | `container/vita3k.sh android release` | `container/vita3k.sh android` |

Within one row the builds replace each other. Android matches an install on the
application ID, so a new release APK replaces the release app already on the
device, and a new development APK replaces the development app. Each row also
keeps its own data: the `pref-path` folder, the games and the saves of the other
app are untouched by an update.

Across rows nothing is shared. The IDs differ, so the two install side by side,
and the keys differ, so neither can replace the other even if the IDs matched.

A release build fails when no release key is passed in, rather than falling back
to the debug key. A debug-key release APK cannot be installed over a CI release
app, so it appears as a second app and can never become the release app on that
device. Failing at build time is clearer than that.

To build a release APK locally, put the release key in `.signing/release/` (see
[Where the key is](#where-the-key-is)) and run:

```sh
VITA_SIGN_WITH_RELEASE_KEY=1 container/vita3k.sh android release
```

The result is signed with the same key CI uses, so it replaces the release app
on a device in the same way a CI APK does.

The development app name is in `android/app/src/reldebug/res/values/strings.xml`,
and `android/app/src/debug/res/values/strings.xml` for the `debug` build type.
Both override `app_name` from `src/main`, which a release build keeps as `VITA+`.

### One app ID changed once

The release application ID was `org.vita3kplus.emulator` up to the commit
`e07fd837`, and is `org.vita3k.emulator` after it. A device that still has a
release app from before that commit sees the first new release APK as a second
app, because Android does not know the two are related. Uninstall the old app
once:

```sh
adb uninstall org.vita3kplus.emulator
```

The games and saves stay, because they are in the `pref-path` folder and not in
the app.

## Targets and the other platforms

- Android: arm64 only (`abiFilters "arm64-v8a"`). Target device: Ayaneo Pocket S.
- Linux: x86_64 AppImage. Target device: Steam Deck.
- Windows, macOS and Linux arm64: the code stays, but CI does not build them. The
  old jobs are in the git history of `.github/workflows/c-cpp.yml` (before the
  commit that made the builds tag-only). Add a job back to build one of them.
