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
secret. Without the CLI: add `~/.vita-plus-signing/vita-plus-release.p12` to a
1Password item as a file, copy the password with
`pbcopy < ~/.vita-plus-signing/store-password.txt`, paste it into the item, and
clear the clipboard.

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

The APKs that you build in the container (`container/vita3k.sh android release`)
are signed with the debug key of the container. A CI APK cannot update them.
Uninstall once when you change from one to the other. The games and saves stay,
because they are in the `pref-path` folder.

## Targets and the other platforms

- Android: arm64 only (`abiFilters "arm64-v8a"`). Target device: Ayaneo Pocket S.
- Linux: x86_64 AppImage. Target device: Steam Deck.
- Windows, macOS and Linux arm64: the code stays, but CI does not build them. The
  old jobs are in the git history of `.github/workflows/c-cpp.yml` (before the
  commit that made the builds tag-only). Add a job back to build one of them.
