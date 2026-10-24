#!/usr/bin/env bash
set -euo pipefail

OUTPUT_DIR="${1:-}"
if [[ -z "$OUTPUT_DIR" ]]; then
    echo "Usage: $0 <output-dir>" >&2
    exit 1
fi

# Select the app explicitly. Missing production credentials must never select
# a development app. Pre-release workflows pass ANDROID_BUILD_CHANNEL=dev.
channel="${ANDROID_BUILD_CHANNEL:-release}"
case "$channel" in
    release) BUILD_TYPE=Release; signing_prefix=SIGNING ;;
    dev) BUILD_TYPE=Reldebug; signing_prefix=DEV_SIGNING ;;
    *) echo "::error::ANDROID_BUILD_CHANNEL must be dev or release" >&2; exit 1 ;;
esac
for setting in STORE_PATH STORE_PASSWORD KEY_ALIAS KEY_PASSWORD; do
    name="${signing_prefix}_${setting}"
    if [[ -z "${!name:-}" ]]; then
        echo "::error::Missing $name for the $channel Android build" >&2
        exit 1
    fi
done
store_path="${signing_prefix}_STORE_PATH"
[[ -s "${!store_path}" ]] || { echo "::error::Android signing keystore is missing" >&2; exit 1; }

python3 .ci/prepare-plus-build.py
mkdir -p android/app/assets
for asset in data shaders-builtin icons; do
    if [[ "$asset" == data ]]; then source=data; else source="vita3k/$asset"; fi
    rm -rf "android/app/assets/$asset"
    cp -r "$source" "android/app/assets/$asset"
done
chmod +x android/gradlew

echo "build-android.sh: building ${BUILD_TYPE}"

pushd android > /dev/null
./gradlew --stacktrace --build-cache --max-workers=4 ":app:assemble${BUILD_TYPE}"
popd > /dev/null

APK_PATH="android/app/build/outputs/apk/${BUILD_TYPE,,}/app-${BUILD_TYPE,,}.apk"
python3 .ci/dev_apk.py "$APK_PATH" "$channel"
mkdir -p "$OUTPUT_DIR"
cp "$APK_PATH" "$OUTPUT_DIR/app.apk"
