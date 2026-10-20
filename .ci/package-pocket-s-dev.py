#!/usr/bin/env python3
"""Verify an isolated development APK and stage public release files."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile


def run(*args):
    return subprocess.check_output([str(arg) for arg in args], text=True)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--package", default="org.vita3k.emulator.debug.pockets3xci")
    parser.add_argument("--label", default="VITAdev 3x CI")
    parser.add_argument("--name", default="VITAdev-3x-p1")
    parser.add_argument("--application-source", default="aad783f02ad94780f1165d8da8af4c3b1a904a69")
    parser.add_argument("--testing", type=Path, default=Path(".scratch/pocket-s-60fps-3x/testing.md"))
    parser.add_argument("--notes", type=Path)
    parser.add_argument("--build-label")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.name):
        parser.error("Artifact name must contain only letters, digits, underscores and hyphens")
    root = Path.cwd()
    sdk = Path(os.environ["ANDROID_HOME"]) / "build-tools/36.0.0"
    ndk = Path(os.environ["ANDROID_NDK_HOME"])
    readelf = ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf"
    apk = root / "android/app/build/outputs/apk/reldebug/app-reldebug.apk"
    natives = list((root / "android/app/build/intermediates/cxx/RelWithDebInfo").glob("*/obj/arm64-v8a/libVita3K.so"))
    run(sdk / "apksigner", "verify", "--verbose", apk)
    run(sdk / "zipalign", "-c", "-P", "16", "4", apk)
    badging = run(sdk / "aapt2", "dump", "badging", apk)
    if f"name='{args.package}'" not in badging or f"application-label:'{args.label}'" not in badging:
        raise RuntimeError("APK package or launcher label does not match the isolated test build")
    if "native-code: 'arm64-v8a'" not in badging:
        raise RuntimeError("APK does not advertise the ARM64 ABI")

    def build_id(path):
        match = re.search(r"Build ID: ([0-9a-f]+)", run(readelf, "--notes", path))
        if not match:
            raise RuntimeError("Native library has no build ID")
        return match.group(1)

    assets_checked = 0
    with zipfile.ZipFile(apk) as archive:
        names = set(archive.namelist())
        if any(name.startswith("lib/") and name.endswith(".so") and not name.startswith("lib/arm64-v8a/") for name in names):
            raise RuntimeError("Unexpected native ABI in APK")
        for source, destination in [("data", "data"), ("vita3k/shaders-builtin", "shaders-builtin"), ("vita3k/icons", "icons")]:
            for path in (root / source).rglob("*"):
                if path.is_file():
                    name = "assets/" + destination + "/" + path.relative_to(root / source).as_posix()
                    if name not in names or archive.read(name) != path.read_bytes():
                        raise RuntimeError(f"Required APK asset is missing or changed: {name}")
                    assets_checked += 1
        with tempfile.TemporaryDirectory(prefix="vita-ci-verify-") as tmp:
            packaged = Path(tmp) / "libVita3K.so"
            packaged.write_bytes(archive.read("lib/arm64-v8a/libVita3K.so"))
            native_id = build_id(packaged)
            matching = [path for path in natives if build_id(path) == native_id]
            if not matching:
                raise RuntimeError("No profiling library matches the APK build ID")
            native = matching[0]
            if args.build_label and args.build_label.encode() not in packaged.read_bytes():
                raise RuntimeError("Packaged library does not contain the expected build label")

    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    shutil.copyfile(apk, out / f"{args.name}-arm64.apk")
    with zipfile.ZipFile(out / f"{args.name}-symbols.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        archive.write(native, "lib/arm64-v8a/libVita3K.so")
    revision = run("git", "rev-parse", "HEAD").strip()
    patch = run("git", "diff", "--binary", "09f207d31196c20c238183a01420207ea1c640a2", "HEAD")
    (out / f"{args.name}-source.patch").write_text(patch)
    testing = (root / args.testing).read_text()
    testing = testing.replace("org.vita3k.emulator.debug.pockets3x", args.package).replace("VITAdev 3x", args.label)
    testing = testing.replace("cloud-generated", "CI-generated")
    testing += "\nThis APK was rebuilt in GitHub Actions. Use its adjacent build manifest and matching CI symbols. The original cloud APK has a different signing key and native build ID.\n"
    (out / "TESTING.md").write_text(testing)
    manifest = {
        "source_commit": revision,
        "application_code_reference_commit": args.application_source,
        "package_id": args.package,
        "launcher_label": args.label,
        "variant": "reldebug",
        "native_configuration": "RelWithDebInfo",
        "abi": "arm64-v8a",
        "native_build_id": native_id,
        "signing": "runner-generated development key",
        "workflow_run": f"https://github.com/niilo/VITAplus/actions/runs/{os.environ.get('GITHUB_RUN_ID', 'local-check')}",
        "verification": {"apk_signature": "passed", "zipalign": "passed", "required_assets_checked": assets_checked, "symbols_build_id": "matches APK", "physical_device_performance": "not measured"},
        "artifacts": {path.name: {"size_bytes": path.stat().st_size, "sha256": digest(path)} for path in sorted(out.iterdir())},
    }
    (out / "build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (out / "SHA256SUMS").write_text("".join(digest(path) + "  " + path.name + "\n" for path in sorted(out.iterdir()) if path.name != "SHA256SUMS"))
    notes = f"""GitHub Actions development build for Pocket S phase 1 testing.

Download **VITAdev-3x-p1-arm64.apk**. It installs as **{args.label}**, package `{args.package}`, alongside VITA+ and the earlier VITAdev test apps. It uses a runner-generated development signing key and has separate settings and driver storage. Import the intended Turnip driver into this app and check the actual driver ID in the session log.

This build contains optional Vulkan event waiting, GPU timestamp measurements, pipeline creation durations and renderer queue reports. Enable performance logging and compare **Renderer event wait (experimental)** off/on in the same gameplay scene after warming the caches. The experimental setting is off by default. See `TESTING.md`.

The application code is unchanged from tested source `aad783f02ad94780f1165d8da8af4c3b1a904a69`. This CI build uses source `{revision}` and records its own native build ID and checksums. The workflow runs the 27 Python tests and verifies the APK signature, alignment, required packaged assets and matching profiling symbols. The same application code previously passed the Linux build, GUI startup and all 87 C++ tests in the cloud environment.

**60 FPS at 3x has not been established.** Physical Pocket S and Steam Deck gameplay, power and thermals remain unmeasured. Full pipeline recipe preparation and game-specific FPS unlocks remain pending. This is an optimized RelWithDebInfo development build; confirm final performance with a Release build.

Files: the APK, matching profiling symbols, source patch, build manifest, testing guide and SHA256SUMS. This freshly built CI APK replaces the unavailable cloud download; use its own checksums and symbols.

APK SHA-256: `{manifest['artifacts'][f'{args.name}-arm64.apk']['sha256']}`.
"""
    if args.notes:
        notes = (root / args.notes).read_text()
        for placeholder, value in {
            "@LABEL@": args.label, "@PACKAGE@": args.package,
            "@SOURCE@": revision, "@APPLICATION_SOURCE@": args.application_source,
            "@APK@": f"{args.name}-arm64.apk", "@NATIVE_ID@": native_id,
            "@APK_SHA256@": manifest["artifacts"][f"{args.name}-arm64.apk"]["sha256"],
        }.items():
            notes = notes.replace(placeholder, value)
    (out / "RELEASE_NOTES.md").write_text(notes)
    print(f"Verified APK and matching symbols; checked {assets_checked} assets. Source: {revision}")


if __name__ == "__main__":
    main()
