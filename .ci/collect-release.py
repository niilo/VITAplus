#!/usr/bin/env python3
"""Collect verified Android and Linux artifacts for one immutable release."""

import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

from dev_apk import CERTIFICATE_SHA256, PACKAGE, RELEASE_CERTIFICATE_SHA256, RELEASE_PACKAGE


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_checksums(directory):
    entries = (directory / "SHA256SUMS").read_text().splitlines()
    if not entries:
        raise ValueError("Artifact checksums are empty")
    for entry in entries:
        expected, name = entry.split("  ", 1)
        if Path(name).name != name or not re.fullmatch(r"[0-9a-f]{64}", expected):
            raise ValueError("Invalid artifact checksum entry")
        if digest(directory / name) != expected:
            raise ValueError(f"Artifact checksum mismatch: {name}")


def validate_android_manifest(manifest, channel, tag, source, version_code):
    expected_package = PACKAGE if channel == "dev" else RELEASE_PACKAGE
    expected_certificate = CERTIFICATE_SHA256 if channel == "dev" else RELEASE_CERTIFICATE_SHA256
    expected_label = "VITAdev" if channel == "dev" else "VITA+"
    expected_variant = "reldebug" if channel == "dev" else "release"
    expected_configuration = "RelWithDebInfo" if channel == "dev" else "Release"
    expected = {
        "source_commit": source,
        "package_id": expected_package,
        "signing_certificate_sha256": expected_certificate,
        "launcher_label": expected_label,
        "variant": expected_variant,
        "native_configuration": expected_configuration,
        "version_code": version_code,
        "version_name": tag.removeprefix("v"),
        "native_version": tag,
        "updates_vitadev": channel == "dev",
        "abi": "arm64-v8a",
    }
    if any(manifest.get(key) != value for key, value in expected.items()):
        raise ValueError("Android artifact identity or source differs from the release")
    checks = manifest.get("verification", {})
    if checks.get("apk_signature") != "passed" or checks.get("zipalign") != "passed" or checks.get("symbols_build_id") != "matches APK" or checks.get("required_assets_checked", 0) < 1:
        raise ValueError("Android artifact verification is incomplete")
    if not re.fullmatch(r"[0-9a-f]+", manifest.get("native_build_id", "")):
        raise ValueError("Android native build identity is missing")


def collect(tag, downloaded, output):
    if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9][A-Za-z0-9.-]*)?", tag):
        raise ValueError("Invalid release tag")
    info = dict(line.split("=", 1) for line in subprocess.check_output(["bash", "tools/release/version-info.sh"], text=True).splitlines())
    if info["version"] != tag:
        raise ValueError("The release tag and compiled version differ")
    source = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    short_source = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], text=True).strip()
    provenance = json.loads(Path(".ci/provenance.json").read_text())
    expected_patch = subprocess.check_output(["git", "diff", "--binary", provenance["upstream_commit"], "HEAD"])
    output.mkdir(parents=True, exist_ok=False)
    channels = ("dev", "release") if "-" not in tag else ("dev",)
    manifests = {}
    for channel in channels:
        directory = downloaded / f"android-{channel}"
        verify_checksums(directory)
        manifest = json.loads((directory / "build-manifest.json").read_text())
        validate_android_manifest(manifest, channel, tag, source, int(info["version_code"]))
        prefix = f"{'VITAdev' if channel == 'dev' else 'VITA-Plus'}-{tag}-android"
        for suffix in ("-arm64.apk", "-symbols.zip"):
            asset = directory / (prefix + suffix)
            if digest(asset) != manifest["artifacts"][asset.name]["sha256"]:
                raise ValueError("Android manifest does not match its binary")
            shutil.copyfile(asset, output / asset.name)
        if (directory / f"{prefix}-source.patch").read_bytes() != expected_patch:
            raise ValueError("Android source patch differs from the release checkout")
        manifest_name = f"{prefix}-build-manifest.json"
        shutil.copyfile(directory / "build-manifest.json", output / manifest_name)
        manifests[channel] = manifest_name
    linux = downloaded / f"vita3k-{short_source}-linux-x64"
    appimages = list(linux.glob("*.AppImage"))
    if len(appimages) != 1:
        raise ValueError("Expected one Linux AppImage for this source commit")
    image = appimages[0]
    for asset in (image, image.with_name(image.name + ".zsync")):
        if asset.exists():
            suffix = asset.name[asset.name.index(".AppImage"):]
            shutil.copyfile(asset, output / f"VITA-Plus-{tag}-linux-x86_64{suffix}")
    (output / f"VITA-Plus-{tag}-source.patch").write_bytes(expected_patch)
    shutil.copyfile("docs/TESTING.md", output / "TESTING.md")
    notes_path = Path("docs/releases") / f"{tag}.md"
    notes = notes_path.read_text() if notes_path.exists() else "See the build manifests and TESTING.md for this release.\n"
    (output / "RELEASE_NOTES.md").write_text(f"Vita3K Build: {info['version_code']}\n\nVersion: {tag}\n\n{notes}")
    release_manifest = {
        "tag": tag,
        "source_commit": source,
        "version_code": int(info["version_code"]),
        "provenance": provenance,
        "android_manifests": manifests,
        "artifacts": {path.name: {"sha256": digest(path), "size_bytes": path.stat().st_size} for path in sorted(output.iterdir())},
    }
    (output / "release-manifest.json").write_text(json.dumps(release_manifest, indent=2) + "\n")
    (output / "SHA256SUMS.txt").write_text("".join(f"{digest(path)}  {path.name}\n" for path in sorted(output.iterdir())))
    print(f"Collected {len(list(output.iterdir()))} release files for {tag}, source {source}")


if __name__ == "__main__":
    collect(sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3]))
