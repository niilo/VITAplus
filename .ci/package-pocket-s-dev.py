#!/usr/bin/env python3
"""Verify a VITAdev update APK and stage its build artifacts."""

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

from dev_apk import PACKAGE, ISOLATED_PACKAGE, apk_identity, verify_dev_identity, verify_isolated_identity


def run(*args):
    return subprocess.check_output([str(arg) for arg in args], text=True)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--package")
    parser.add_argument("--label")
    parser.add_argument("--name")
    parser.add_argument("--application-source", default=None)
    parser.add_argument("--testing", type=Path, default=Path(".scratch/visual-followup-2026-10-22/testing.md"))
    parser.add_argument("--notes", type=Path)
    parser.add_argument("--build-label")
    parser.add_argument("--isolated-test", action="store_true", help="Package the explicitly requested separate visual test app")
    parser.add_argument("--previous-apk", type=Path, help="Check package, certificate and version against the installed build APK")
    args = parser.parse_args()
    expected_package = ISOLATED_PACKAGE if args.isolated_test else PACKAGE
    expected_label = "VITAdev Visual Test" if args.isolated_test else "VITAdev"
    args.package = args.package or expected_package
    args.label = args.label or expected_label
    args.name = args.name or ("VITAdev-visual-test-20261022" if args.isolated_test else "VITAdev")
    if args.package != expected_package or args.label != expected_label:
        parser.error("APK identity must match the selected update or separate-test mode")
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
    identity = apk_identity(badging, run(sdk / "apksigner", "verify", "--print-certs", apk))
    previous = None
    if args.previous_apk:
        previous = apk_identity(run(sdk / "aapt2", "dump", "badging", args.previous_apk),
            run(sdk / "apksigner", "verify", "--print-certs", args.previous_apk))
    (verify_isolated_identity if args.isolated_test else verify_dev_identity)(identity, previous)
    if f"name='{args.package}'" not in badging or f"application-label:'{args.label}'" not in badging:
        raise RuntimeError("APK package or launcher label does not match VITAdev")
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
    testing += "\nUse the adjacent build manifest and matching symbols for this APK.\n"
    (out / "TESTING.md").write_text(testing)
    manifest = {
        "source_commit": revision,
        "application_code_reference_commit": args.application_source or revision,
        "package_id": args.package,
        "launcher_label": args.label,
        "variant": "reldebug",
        "native_configuration": "RelWithDebInfo",
        "abi": "arm64-v8a",
        "native_build_id": native_id,
        "signing": "existing CI signing key" if args.isolated_test else "existing VITAdev key",
        "updates_vitadev": not args.isolated_test,
        "signing_certificate_sha256": identity["certificate_sha256"],
        "version_code": identity["version_code"],
        "workflow_run": f"https://github.com/niilo/VITAplus/actions/runs/{os.environ.get('GITHUB_RUN_ID', 'local-check')}",
        "verification": {"apk_signature": "passed", "zipalign": "passed", "required_assets_checked": assets_checked, "symbols_build_id": "matches APK", "physical_device_performance": "not measured"},
        "artifacts": {path.name: {"size_bytes": path.stat().st_size, "sha256": digest(path)} for path in sorted(out.iterdir())},
    }
    (out / "build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (out / "SHA256SUMS").write_text("".join(digest(path) + "  " + path.name + "\n" for path in sorted(out.iterdir()) if path.name != "SHA256SUMS"))
    notes = f"""Download **{args.name}-arm64.apk**. It updates **VITAdev**, package `{args.package}`.

Source: `{revision}`. Native build ID: `{native_id}`.
The package and certificate match the existing VITAdev app. See TESTING.md for device checks.
Device rendering, performance, power and thermals remain unmeasured.
"""
    if args.isolated_test:
        notes = f"""Download **{args.name}-arm64.apk**. It installs as **{args.label}**, package `{args.package}`.

This is a separate test app. Import the intended Vulkan driver and match the Plus app's settings.
Source: `{revision}`. Native build ID: `{native_id}`. See TESTING.md for device checks.
"""
    if args.notes:
        notes = (root / args.notes).read_text()
        for placeholder, value in {
            "@LABEL@": args.label, "@PACKAGE@": args.package,
            "@SOURCE@": revision, "@APPLICATION_SOURCE@": args.application_source or revision,
            "@APK@": f"{args.name}-arm64.apk", "@NATIVE_ID@": native_id,
            "@APK_SHA256@": manifest["artifacts"][f"{args.name}-arm64.apk"]["sha256"],
        }.items():
            notes = notes.replace(placeholder, value)
    (out / "RELEASE_NOTES.md").write_text(notes)
    print(f"Verified APK and matching symbols; checked {assets_checked} assets. Source: {revision}")


if __name__ == "__main__":
    main()
