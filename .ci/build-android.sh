#!/usr/bin/env bash
set -euo pipefail

OUTPUT_DIR="${1:-}"
if [[ -z "$OUTPUT_DIR" ]]; then
    echo "Usage: $0 <output-dir>" >&2
    exit 1
fi

mkdir -p android/app/assets
rm -rf android/app/assets/data android/app/assets/shaders-builtin
cp -r data android/app/assets/data
cp -r vita3k/shaders-builtin android/app/assets/shaders-builtin

chmod +x android/gradlew

# A release build is signed with the release key, which only CI has. Without the
# secrets the script builds reldebug instead, which carries the .debug
# application ID and so is a separate app from the release one.
#
# The build type is decided here and named in the log. It used to be taken from
# whether SIGNING_STORE_PATH was set, with no message, so a missing secret or a
# changed variable name silently produced a development APK and published it as
# the release. Fail instead, because a release APK that cannot replace the
# release app is worse than no APK at all.
if [[ -n "${SIGNING_STORE_PATH:-}" && -e "${SIGNING_STORE_PATH}" ]]; then
    BUILD_TYPE="Release"
elif [[ "${ALLOW_DEV_RELEASE_BUILD:-0}" == "1" ]]; then
    BUILD_TYPE="Reldebug"
    echo "build-android.sh: no signing key, so this is a reldebug build."
    echo "build-android.sh: it has the .debug application ID and cannot replace the release app."
else
    echo "::error::SIGNING_STORE_PATH is not set, so no release key is available" >&2
    echo "build-android.sh: a release APK must use the release key, or Android" >&2
    echo "refuses to install it over the release app already on a device." >&2
    echo "build-android.sh: set the signing secrets, or set ALLOW_DEV_RELEASE_BUILD=1" >&2
    echo "to build reldebug on purpose. See docs/release.md." >&2
    exit 1
fi
echo "build-android.sh: building ${BUILD_TYPE}"

pushd android > /dev/null
./gradlew --stacktrace ":app:assemble${BUILD_TYPE}"
popd > /dev/null

APK_PATH="android/app/build/outputs/apk/${BUILD_TYPE,,}/app-${BUILD_TYPE,,}.apk"
mkdir -p "$OUTPUT_DIR"
cp "$APK_PATH" "$OUTPUT_DIR/app.apk"
