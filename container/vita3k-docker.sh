#!/usr/bin/env bash
# Build, test and format Vita3K in a Linux container with Docker or Podman.
#
# This is the x86_64 Linux path. For the Apple `container` CLI on macOS, use
# container/vita3k.sh instead. Both read the same Containerfiles and run the
# same commands, so the commands below mean the same thing in either one.
#
# Usage: container/vita3k-docker.sh <command> [args]
#
#   image [linux|android]   build the image (done on demand by the commands)
#   shell [linux|android]   open a shell in the container, repo mounted at /src
#   configure               cmake --preset container-linux
#   build [config]          configure if needed, then build (default RelWithDebInfo)
#   test [ctest args]       run the googletest suites with ctest
#   format                  clang-format the sources in place (same as format.sh)
#   format-check            fail if any source is not formatted (same check as CI)
#   android [type] [dir]    build the APK: reldebug (default, both ABIs) or
#                           release (arm64 only, shrunk); output in build/android-apk
#   run <cmd...>            run any command in the Linux container
#   clean-cache             delete the ccache and Gradle cache volumes
#
# Environment:
#   VITA3K_CONTAINER_CPUS    CPUs for the container (default: all host CPUs)
#   VITA3K_CONTAINER_MEMORY  memory for the container, in bytes or with a
#                            suffix (default: 16G; the container default of
#                            1 GiB is too small to link)
#
# Everything runs without a device and without a person. That is the point of
# this script: the tickets under .scratch/pocket-s-android13/ that are code
# work rather than device measurement can be built and tested from it.
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
container_dir="$repo_root/container"
cpus="${VITA3K_CONTAINER_CPUS:-$(nproc)}"
memory="${VITA3K_CONTAINER_MEMORY:-16G}"
preset="container-linux"
linux_cache_volume="vita3k-linux-ccache"
android_cache_volume="vita3k-android-cache"

die() {
    echo "vita3k-docker.sh: $*" >&2
    exit 1
}

# Docker and Podman are not interchangeable in one respect: Podman needs
# :Z on a bind mount when SELinux is enforcing. Detect once instead of
# guessing per run.
detect_engine() {
    if [[ -n "${VITA3K_ENGINE:-}" ]]; then
        command -v "$VITA3K_ENGINE" > /dev/null || die "VITA3K_ENGINE=$VITA3K_ENGINE not found"
        engine="$VITA3K_ENGINE"
    elif command -v docker > /dev/null; then
        engine=docker
    elif command -v podman > /dev/null; then
        engine=podman
    else
        die "neither docker nor podman found; install one of them"
    fi

    selinux_opts=""
    if [[ "$engine" == podman ]] && [[ -e /sys/fs/selinux/enforce ]] \
        && [[ "$(cat /sys/fs/selinux/enforce)" == "1" ]]; then
        selinux_opts=",Z"
    fi
}

# The image tag is a hash of the Containerfile, so an edit to the file builds
# a new image on the next command. The arch is part of the tag because the
# same Containerfile gives a different image per architecture.
image_tag() {
    local flavor="$1" hash
    hash="$(sha256sum "$container_dir/$flavor.Containerfile" | cut -c1-12)"
    echo "vita3k-$flavor:docker-$hash"
}

build_image() {
    local flavor="$1"
    [[ "$flavor" == "linux" || "$flavor" == "android" ]] || die "unknown image: $flavor"
    echo "vita3k-docker.sh: building $(image_tag "$flavor")" >&2
    "$engine" build --platform linux/amd64 -t "$(image_tag "$flavor")" \
        -f "$container_dir/$flavor.Containerfile" "$container_dir"
}

ensure_image() {
    local flavor="$1" tag
    tag="$(image_tag "$flavor")"
    if ! "$engine" image inspect "$tag" > /dev/null 2>&1; then
        build_image "$flavor"
    fi
}

ensure_volume() {
    "$engine" volume inspect "$1" > /dev/null 2>&1 || "$engine" volume create "$1" > /dev/null
}

# run_in <linux|android> <cmd...>
run_in() {
    local flavor="$1"
    shift
    ensure_image "$flavor"
    local args=(--rm -m "$memory" -v "$repo_root:/src$selinux_opts" -w /src)
    # --cpus only where the engine supports it. Podman's rootless mode maps it
    # differently but accepts the flag, so pass it for both.
    args=(--cpus "$cpus" "${args[@]}")
    if [[ "$flavor" == "android" ]]; then
        # The Android image installs the SDK, the NDK and vcpkg under /opt as
        # root, and vcpkg needs to write to its own buildtrees. So this flavor
        # runs as root and chowns what it produced afterwards.
        ensure_volume "$android_cache_volume"
        # ANDROID_USER_HOME keeps debug.keystore in the cache volume. Without
        # it, each run makes a new debug key, and Android refuses to update an
        # installed APK that has another key.
        args+=(-v "$android_cache_volume:/cache" -e ANDROID_USER_HOME=/cache/android-user)
        # VITA_SIGN_WITH_RELEASE_KEY=1 signs the release APK with the key in
        # .signing/release/ (see docs/release.md).
        args+=(-e "VITA_SIGN_WITH_RELEASE_KEY=${VITA_SIGN_WITH_RELEASE_KEY:-0}")
    else
        # The Linux image has nothing that needs root, so build as the host uid.
        # The files in build/ then belong to the host user and an incremental
        # build can reuse them.
        ensure_volume "$linux_cache_volume"
        args+=(--user "$(id -u):$(id -g)" --env "HOME=/tmp/vita3k-home")
        args+=(-v "$linux_cache_volume:/ccache")
    fi
    [[ -t 0 && -t 1 ]] && args+=(-it)
    local status=0
    "$engine" run "${args[@]}" "$(image_tag "$flavor")" "$@" || status=$?
    if [[ "$flavor" == "android" ]]; then
        chown_paths
    fi
    return $status
}

# An Android build runs as root and writes into build/ and android/app/build/.
# Hand those folders back to the host user so a later git or cmake command can
# read and remove them.
chown_paths() {
    "$engine" run --rm --user 0:0 -v "$repo_root:/src" --entrypoint /bin/sh \
        "$(image_tag linux)" -c "chown -R $(id -u):$(id -g) /src/build /src/android 2>/dev/null || true" \
        > /dev/null 2>&1 || true
}

configure_cmd="cmake --preset $preset"
ensure_configured="[ -f build/$preset/build.ninja ] || $configure_cmd"
format_files="find vita3k tools/gen-modules tools/native-tool \\( -name '*.cpp' -o -name '*.h' \\) -print0"

detect_engine

command="${1:-}"
[[ $# -gt 0 ]] && shift

case "$command" in
    image)
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
        "$engine" volume rm "$linux_cache_volume" "$android_cache_volume" 2> /dev/null || true
        ;;
    *)
        sed -n '2,28p' "$0" | sed 's/^# \{0,1\}//'
        [[ -z "$command" ]] || exit 1
        ;;
esac