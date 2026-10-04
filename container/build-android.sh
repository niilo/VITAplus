#!/usr/bin/env bash
# Builds the APK inside the Android container. Start it with
# `container/vita3k.sh android [reldebug|release] [output dir]`.
#
#   reldebug  both ABIs (arm64-v8a, x86_64), debuggable, no R8. Same as CI.
#   release   arm64-v8a only, R8 shrinks the code. Signed with the dev key,
#             or with the release key when VITA_SIGN_WITH_RELEASE_KEY=1.
#
# Signing keys live in the folder .signing/ of the project. Git ignores it.
# See docs/release.md.
#   .signing/dev/debug.keystore      the dev key. It is the source of truth, so
#                                    `clean-cache` does not change the key and an
#                                    installed APK can always be updated.
#   .signing/release/                the release key (vita-plus-release.p12 and
#                                    signing.env). The same key as in CI.
#
# vcpkg dependencies are installed by the CMake configure step (manifest
# mode) and reused from the binary cache in the cache volume.
set -euo pipefail

build_type="${1:-reldebug}"
output_dir="${2:-build/android-apk}"

case "$build_type" in
    reldebug) task=assembleReldebug; gradle_args=() ;;
    release) task=assembleRelease; gradle_args=(-Pandroid.injected.build.abi=arm64-v8a) ;;
    *) echo "build-android.sh: unknown build type: $build_type" >&2; exit 1 ;;
esac

mkdir -p "$VCPKG_DEFAULT_BINARY_CACHE" "$VCPKG_DOWNLOADS" "$CCACHE_DIR"

# Dev key: the project copy wins. On the first run the key of the cache volume
# is copied into the project.
signing_dir=/src/.signing
dev_key="$signing_dir/dev/debug.keystore"
volume_key="${ANDROID_USER_HOME:?ANDROID_USER_HOME is not set}/debug.keystore"
mkdir -p "$ANDROID_USER_HOME" "$signing_dir/dev"
chmod 700 "$signing_dir" "$signing_dir/dev"
if [[ -s "$dev_key" ]]; then
    cp -f "$dev_key" "$volume_key"
elif [[ -s "$volume_key" ]]; then
    cp "$volume_key" "$dev_key"
    chmod 600 "$dev_key"
    echo "build-android.sh: copied the dev key of the cache volume to .signing/dev/"
fi

# Release key: only when asked. Otherwise the SIGNING_* variables stay unset, so
# the build uses the dev key.
unset SIGNING_STORE_PATH SIGNING_STORE_PASSWORD SIGNING_KEY_ALIAS SIGNING_KEY_PASSWORD
if [[ "${VITA_SIGN_WITH_RELEASE_KEY:-0}" == 1 ]]; then
    [[ "$build_type" == release ]] || { echo "build-android.sh: the release key is for the release build type only" >&2; exit 1; }
    release_key="$signing_dir/release/vita-plus-release.p12"
    [[ -s "$release_key" && -s "$signing_dir/release/signing.env" ]] || {
        echo "build-android.sh: no release key in .signing/release/. See docs/release.md." >&2
        exit 1
    }
    set -a
    # shellcheck disable=SC1091
    . "$signing_dir/release/signing.env"
    set +a
    export SIGNING_STORE_PATH="$release_key"
    echo "build-android.sh: signing with the release key"
fi

# The root CMakeLists.txt uses ccache when it finds it. The ccache folder
# is in the cache volume, so a clean build reuses earlier compiles.
export CCACHE_BASEDIR=/src

# The build version is computed by tools/release/version-info.sh at CMake
# configure time and written into a generated vita3k/config/version.cpp.
# Gradle reuses the configure it already did in android/app/.cxx, so a build
# after a new commit keeps the version of the first build and its version.cpp
# is never regenerated. The APK then reports the wrong commit, which makes two
# different builds indistinguishable on a device. Deleting the CMake cache
# forces the configure step to run again. It costs one configure and no
# recompile, because ccache still serves every object.
find android/app/.cxx -name CMakeCache.txt -delete

# Same asset staging as .ci/build-android.sh.
mkdir -p android/app/assets
rm -rf android/app/assets/data android/app/assets/shaders-builtin
cp -r data android/app/assets/data
cp -r vita3k/shaders-builtin android/app/assets/shaders-builtin

cd android
./gradlew --stacktrace --build-cache --parallel ":app:$task" "${gradle_args[@]}"
cd ..
ccache -s | grep -E "Hits|Misses|Cache size" || true

# With android.injected.build.abi, Gradle writes the APK only under
# intermediates/, not outputs/. Take the newest, so an old APK from an
# earlier run in the other folder is not copied.
apk="$(ls -t android/app/build/{outputs,intermediates}/apk/"$build_type"/app-"$build_type".apk 2> /dev/null | head -n 1 || true)"
[[ -n "$apk" ]] || { echo "build-android.sh: no APK found for $build_type" >&2; exit 1; }

mkdir -p "$output_dir"
cp "$apk" "$output_dir/app-$build_type.apk"
ls -l "$output_dir/app-$build_type.apk"
