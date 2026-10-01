#!/usr/bin/env bash
# Collect the build artifacts into release-assets/ with names that carry the tag.
# Usage: .ci/release-collect.sh <tag>      (run in the folder that holds the downloaded artifacts)
# Targets: Android arm64 (Ayaneo Pocket S) and Linux x86_64 (Steam Deck).
set -euo pipefail

tag="${1:-}"
if [[ -z "$tag" ]]; then
    echo "Usage: $0 <tag>" >&2
    exit 1
fi

assets_dir="$(pwd)/release-assets"
mkdir -p "$assets_dir"

mapfile -t artifact_dirs < <(find . -mindepth 1 -maxdepth 1 -type d -name 'vita3k-*' -print | sort)
if [[ ${#artifact_dirs[@]} -eq 0 ]]; then
    echo "No artifact directory found" >&2
    exit 1
fi

for dir in "${artifact_dirs[@]}"; do
    abs_dir="$(cd "$dir" && pwd)"
    artifact_name="$(basename "$abs_dir")"

    case "$artifact_name" in
        vita3k-*-android)
            cp "$abs_dir/app.apk" "$assets_dir/VITA-Plus-${tag}-android-arm64.apk"
            ;;
        vita3k-*-linux-x64)
            shopt -s nullglob
            appimages=("$abs_dir"/*.AppImage*)
            shopt -u nullglob
            if [[ ${#appimages[@]} -eq 0 ]]; then
                echo "No AppImage in $artifact_name" >&2
                exit 1
            fi
            for file in "${appimages[@]}"; do
                # keep the suffix after ".AppImage", for example ".zsync"
                suffix=".AppImage${file##*.AppImage}"
                cp "$file" "$assets_dir/VITA-Plus-${tag}-linux-x86_64${suffix}"
            done
            ;;
        *)
            echo "Unknown artifact directory: $artifact_name" >&2
            exit 1
            ;;
    esac
done

(cd "$assets_dir" && sha256sum * > SHA256SUMS.txt)

echo "=== release-assets ==="
ls -al "$assets_dir"
