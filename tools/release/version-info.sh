#!/usr/bin/env bash
# Print the build version of this checkout as key=value lines.
#
# One implementation, two callers: vita3k/CMakeLists.txt reads it with
# execute_process, and android/app/build.gradle runs it to fill in versionName
# and versionCode. Change a rule here and both sides follow.
#
# Keys:
#   release_tag    newest release tag merged into HEAD, for example v1.1.
#                  Empty when the checkout has no git history.
#   is_release     1 when HEAD is exactly the release tag, 0 otherwise.
#   commits_since  commits between the release tag and HEAD. 0 on a release.
#   base_version   the release tag without the leading v, for example 1.1.
#   version        the version id to show, for example:
#                    v1.1        a release build
#                    v1.1-dev.63 a build 63 commits after v1.1
#   ver_major      numeric parts of base_version, for the file version resource
#   ver_minor
#   ver_patch      0 when the tag has no third part
#   ver_build      0 on a release build, commits_since on a development build,
#                  so the four-part Windows file version rises with every build
#   build_date     UTC build time, for example 2026-10-03T11:52Z. Empty on a
#                  release build, where the tag already names the build.
#   version_code   monotone integer for the Android package. It rises within a
#                  base version and across base versions, so an APK always
#                  installs over the build that came before it.
#
# A checkout without git history (a source tarball) falls back to base version
# 0.0 with is_release 0. It is a development build, and says so.
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

# Release tags only: v1.1 and v1.2.3 are versions, v0.0.1-test and continuous
# are not, and neither is a branch.
tag_pattern='^v[0-9]+\.[0-9]+(\.[0-9]+)?$'

# sort -V is GNU and not on every BSD. Fall back to a numeric sort on the
# dotted parts, which is what sort -V does for these tags.
sort_versions() {
    if printf '1.0\n' | sort -V > /dev/null 2>&1; then
        sort -V
    else
        sort -t. -k1,1n -k2,2n -k3,3n
    fi
}

release_tag=""
commits_since=0

if git rev-parse --git-dir > /dev/null 2>&1; then
    release_tag="$(
        git tag --merged HEAD --list 'v*' 2> /dev/null |
            grep -E "$tag_pattern" |
            sort_versions |
            tail -n 1
    )" || release_tag=""

    if [[ -n "$release_tag" ]]; then
        commits_since="$(git rev-list --count "$release_tag"..HEAD 2> /dev/null || echo 0)"
    fi
fi

# A release build is a build made on the tag itself. Anything else is a
# development build of the tag it is based on.
is_release=0
if [[ -n "$release_tag" ]] && [[ "$commits_since" -eq 0 ]]; then
    is_release=1
fi

base_version="${release_tag#v}"
if [[ -z "$base_version" ]]; then
    base_version="0.0"
fi

IFS=. read -r ver_major ver_minor ver_patch <<< "$base_version"
ver_major="${ver_major:-0}"
ver_minor="${ver_minor:-0}"
ver_patch="${ver_patch:-0}"

if [[ "$is_release" -eq 1 ]]; then
    version="${release_tag}"
    ver_build=0
else
    version="${release_tag:-v0.0}-dev.${commits_since}"
    ver_build="${commits_since}"
fi

# Respect SOURCE_DATE_EPOCH so a reproducible build keeps one version id.
if [[ -n "${SOURCE_DATE_EPOCH:-}" ]]; then
    build_date="$(date -u -d "@${SOURCE_DATE_EPOCH}" +%Y-%m-%dT%H:%MZ 2> /dev/null || true)"
    if [[ -z "$build_date" ]]; then
        build_date="$(date -u -r "$SOURCE_DATE_EPOCH" +%Y-%m-%dT%H:%MZ 2> /dev/null || true)"
    fi
    if [[ -z "$build_date" ]]; then
        build_date="$(date -u +%Y-%m-%dT%H:%MZ)"
    fi
else
    build_date="$(date -u +%Y-%m-%dT%H:%MZ)"
fi

# A release is named by its tag, so it carries no build date.
if [[ "$is_release" -eq 1 ]]; then
    build_date=""
fi

# Android versionCode. Each part has a fixed width so the number rises in the
# same order as the version: every build of 1.2 is above every build of 1.1,
# and inside one base version a development build is above the release it is
# based on and above the development build before it. commits_since runs past
# its own field and lifts the next one when a base version goes over 99 commits
# of development, which keeps the order correct.
version_code=$((ver_major * 1000000 + ver_minor * 10000 + ver_patch * 100 + commits_since))

# Android rejects versionCode below 1, which the no-git fallback lands on.
if [[ "$version_code" -lt 1 ]]; then
    version_code=1
fi

cat <<EOF
release_tag=${release_tag}
is_release=${is_release}
commits_since=${commits_since}
base_version=${base_version}
version=${version}
ver_major=${ver_major}
ver_minor=${ver_minor}
ver_patch=${ver_patch}
ver_build=${ver_build}
build_date=${build_date}
version_code=${version_code}
EOF