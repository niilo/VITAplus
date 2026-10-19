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
    args = parser.parse_args()
    root = Path.cwd()
    sdk = Path(os.environ["ANDROID_HOME"]) / "build-tools/36.0.0"
    ndk = Path(os.environ["ANDROID_NDK_HOME"])
    readelf = ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf"
    apk = root / "android/app/build/outputs/apk/reldebug/app-reldebug.apk"
    natives = list((root / "android/app/build/intermediates/cxx/RelWithDebInfo").glob("*/obj/arm64-v8a/libVita3K.so"))
    if len(natives) != 1:
        raise RuntimeError(f"Expected one unstripped ARM64 library; found {len(natives)}")
    native = natives[0]
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
                    if name not in names:
                        raise RuntimeError(f"Required APK asset is missing: {name}")
                    assets_checked += 1
        with tempfile.TemporaryDirectory(prefix="vita-ci-verify-") as tmp:
            packaged = Path(tmp) / "libVita3K.so"
            packaged.write_bytes(archive.read("lib/arm64-v8a/libVita3K.so"))
            native_id = build_id(native)
            if build_id(packaged) != native_id:
                raise RuntimeError("Packaged and profiling libraries have different build IDs")

    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    shutil.copyfile(apk, out / "VITAdev-3x-p1-arm64.apk")
    with zipfile.ZipFile(out / "VITAdev-3x-p1-symbols.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        archive.write(native, "lib/arm64-v8a/libVita3K.so")
    revision = run("git", "rev-parse", "HEAD").strip()
    patch = run("git", "diff", "--binary", "09f207d31196c20c238183a01420207ea1c640a2", "HEAD")
    (out / "VITAdev-3x-p1-source.patch").write_text(patch)
    testing = (root / ".scratch/pocket-s-60fps-3x/testing.md").read_text()
    testing = testing.replace("org.vita3k.emulator.debug.pockets3x", args.package).replace("VITAdev 3x", args.label)
    testing = testing.replace("cloud-generated", "CI-generated")
    testing += "\nThis APK was rebuilt in GitHub Actions. Use its adjacent build manifest and matching CI symbols. The original cloud APK has a different signing key and native build ID.\n"
    (out / "TESTING.md").write_text(testing)
    manifest = {
        "source_commit": revision,
        "application_code_reference_commit": "aad783f02ad94780f1165d8da8af4c3b1a904a69",
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

APK SHA-256: `{manifest['artifacts']['VITAdev-3x-p1-arm64.apk']['sha256']}`.
"""
    (out / "RELEASE_NOTES.md").write_text(notes)
    print(f"Verified APK and matching symbols; checked {assets_checked} assets. Source: {revision}")


if __name__ == "__main__":
    main()
