#!/usr/bin/env bash
# Develop, test and build Vita3K in Linux containers with Apple's `container`
# CLI (https://github.com/apple/container). Needs macOS 26 on Apple silicon.
#
# Usage: container/vita3k.sh <command> [args]
#
#   image [linux|android]   build the image (done on demand by the other commands)
#   shell [linux|android]   open a shell in the container, repo mounted at /src
#   configure               cmake --preset container-linux
#   build [config]          configure if needed, then build (default RelWithDebInfo)
#   test [ctest args]       run the googletest suites with ctest
#   format                  clang-format the sources in place (same as format.sh)
#   format-check            fail if any source is not formatted (same check as CI)
#   android [type] [dir]    build the APK: reldebug (default, both ABIs) or
#                           release (arm64 only, shrunk); output in build/android-apk
#   run <cmd...>            run any command in the Linux container
#   clean-cache             delete the ccache, vcpkg and Gradle cache volumes
#
# Environment:
#   VITA3K_CONTAINER_CPUS    CPUs for the container (default: all host CPUs)
#   VITA3K_CONTAINER_MEMORY  memory for the container (default: 16G)
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
container_dir="$repo_root/container"
cpus="${VITA3K_CONTAINER_CPUS:-$(sysctl -n hw.ncpu)}"
memory="${VITA3K_CONTAINER_MEMORY:-16G}"
preset="container-linux"
linux_cache_volume="vita3k-linux-ccache"
android_cache_volume="vita3k-android-cache"

die() {
    echo "vita3k.sh: $*" >&2
    exit 1
}

ensure_system() {
    command -v container > /dev/null || die "install Apple container first: brew install container"
    if ! container system status > /dev/null 2>&1; then
        container system start
    fi
}

# The image tag is a hash of its Containerfile, so an edit to the file
# builds a new image on the next run.
image_tag() {
    local flavor="$1"
    local hash
    hash="$(shasum -a 256 "$container_dir/$flavor.Containerfile" | cut -c1-12)"
    echo "vita3k-$flavor:$hash"
}

image_arch() {
    [[ "$1" == "android" ]] && echo amd64 || echo arm64
}

ensure_image() {
    local flavor="$1" tag
    tag="$(image_tag "$flavor")"
    if ! container image inspect "$tag" > /dev/null 2>&1; then
        build_image "$flavor"
    fi
}

build_image() {
    local flavor="$1"
    [[ "$flavor" == "linux" || "$flavor" == "android" ]] || die "unknown image: $flavor"
    container build --arch "$(image_arch "$flavor")" -t "$(image_tag "$flavor")" \
        -f "$container_dir/$flavor.Containerfile" "$container_dir"
}

ensure_volume() {
    container volume inspect "$1" > /dev/null 2>&1 || container volume create "$1" > /dev/null
}

# run_in <linux|android> <cmd...>
run_in() {
    local flavor="$1"
    shift
    ensure_system
    ensure_image "$flavor"
    local args=(--rm -c "$cpus" -m "$memory" -v "$repo_root:/src" -w /src)
    if [[ "$flavor" == "android" ]]; then
        ensure_volume "$android_cache_volume"
        # ANDROID_USER_HOME keeps debug.keystore in the cache volume. Without
        # it, each run makes a new debug key, and Android refuses to update
        # an installed APK that has another key.
        args+=(--arch amd64 --rosetta -v "$android_cache_volume:/cache" -e ANDROID_USER_HOME=/cache/android-user)
    else
        ensure_volume "$linux_cache_volume"
        args+=(-v "$linux_cache_volume:/ccache")
    fi
    [[ -t 0 && -t 1 ]] && args+=(-it)
    container run "${args[@]}" "$(image_tag "$flavor")" "$@"
}

configure_cmd="cmake --preset $preset"
ensure_configured="[ -f build/$preset/build.ninja ] || $configure_cmd"
format_files="find vita3k tools/gen-modules tools/native-tool \\( -name '*.cpp' -o -name '*.h' \\) -print0"

command="${1:-}"
[[ $# -gt 0 ]] && shift

case "$command" in
    image)
        ensure_system
        build_image "${1:-linux}"
        ;;
    shell)
        run_in "${1:-linux}" bash
        ;;
    configure)
        run_in linux bash -c "$configure_cmd"
        ;;
    build)
        run_in linux bash -c "$ensure_configured && cmake --build build/$preset --config ${1:-RelWithDebInfo}"
        ;;
    test)
        run_in linux bash -c "$ensure_configured && cmake --build build/$preset --config RelWithDebInfo --target mem-tests module-tests ngs-tests \
            && ctest --test-dir build/$preset --build-config RelWithDebInfo --output-on-failure $*"
        ;;
    format)
        run_in linux bash -c "$format_files | xargs -0 clang-format -i"
        ;;
    format-check)
        run_in linux bash -c "$format_files | xargs -0 clang-format --dry-run --Werror"
        ;;
    android)
        run_in android bash container/build-android.sh "${1:-reldebug}" "${2:-build/android-apk}"
        ;;
    run)
        [[ $# -gt 0 ]] || die "run needs a command"
        run_in linux "$@"
        ;;
    clean-cache)
        ensure_system
        container volume delete "$linux_cache_volume" "$android_cache_volume" 2> /dev/null || true
        ;;
    *)
        sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'
        [[ -z "$command" ]] || exit 1
        ;;
esac
