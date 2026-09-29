#!/usr/bin/env bash
# Builds the APK inside the Android container. Start it with
# `container/vita3k.sh android [reldebug|release] [output dir]`.
#
#   reldebug  both ABIs (arm64-v8a, x86_64), debuggable, no R8. Same as CI.
#   release   arm64-v8a only, R8 shrinks the code. Signed with the Android
#             debug key unless the SIGNING_* variables are set.
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

# The root CMakeLists.txt uses ccache when it finds it. The ccache folder
# is in the cache volume, so a clean build reuses earlier compiles.
export CCACHE_BASEDIR=/src

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
