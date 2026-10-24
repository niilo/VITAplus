"""Reject incomplete or mismatched build artifacts before publication."""

import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from dev_apk import CERTIFICATE_SHA256, PACKAGE

spec = importlib.util.spec_from_file_location("collect_release", Path(__file__).with_name("collect-release.py"))
collect_release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(collect_release)


class ReleaseTests(unittest.TestCase):
    def manifest(self):
        return {
            "source_commit": "a" * 40, "package_id": PACKAGE,
            "signing_certificate_sha256": CERTIFICATE_SHA256,
            "launcher_label": "VITAdev", "variant": "reldebug",
            "native_configuration": "RelWithDebInfo", "version_code": 1030800,
            "version_name": "1.3.8", "updates_vitadev": True,
            "abi": "arm64-v8a", "native_build_id": "b" * 40,
            "verification": {"apk_signature": "passed", "zipalign": "passed", "symbols_build_id": "matches APK", "required_assets_checked": 59},
        }

    def validate(self, manifest, channel="dev"):
        collect_release.validate_android_manifest(manifest, channel, "v1.3.8", "a" * 40, 1030800)

    def test_verified_dev_artifact_is_accepted(self):
        self.validate(self.manifest())

    def test_wrong_source_package_version_and_signer_are_rejected(self):
        for key, value in (("source_commit", "c" * 40), ("package_id", PACKAGE + ".test"), ("version_code", 1030700), ("signing_certificate_sha256", "d" * 64)):
            manifest = self.manifest()
            manifest[key] = value
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "identity or source"):
                self.validate(manifest)

    def test_dev_artifact_cannot_be_published_as_production(self):
        with self.assertRaisesRegex(ValueError, "identity or source"):
            self.validate(self.manifest(), "release")

    def test_failed_asset_or_symbol_verification_is_rejected(self):
        for key, value in (("required_assets_checked", 0), ("symbols_build_id", "different"), ("apk_signature", "failed"), ("zipalign", "failed")):
            manifest = self.manifest()
            manifest["verification"][key] = value
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "incomplete"):
                self.validate(manifest)

    def test_modified_download_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="vita-release-test-") as temporary:
            root = Path(temporary)
            asset = root / "app.apk"
            asset.write_bytes(b"original")
            (root / "SHA256SUMS").write_text(hashlib.sha256(asset.read_bytes()).hexdigest() + "  app.apk\n")
            collect_release.verify_checksums(root)
            asset.write_bytes(b"modified")
            with self.assertRaisesRegex(ValueError, "checksum mismatch"):
                collect_release.verify_checksums(root)

    def test_complete_release_collects_both_apks_and_linux_with_valid_checksums(self):
        project = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory(prefix="vita-collect-test-") as temporary:
            root = Path(temporary)
            for directory in (".ci", "tools/release", "docs", "downloaded"):
                (root / directory).mkdir(parents=True)
            shutil.copyfile(project / "tools/release/version-info.sh", root / "tools/release/version-info.sh")
            (root / ".ci/version-base.txt").write_text("1.3.8\n")
            (root / "docs/TESTING.md").write_text("Device test instructions\n")

            def git(*args):
                return subprocess.check_output(["git", *args], cwd=root, text=True).strip()

            git("init", "-q")
            git("config", "user.name", "Release test")
            git("config", "user.email", "release-test@example.invalid")
            git("add", ".")
            git("commit", "-qm", "Base")
            base = git("rev-parse", "HEAD")
            (root / ".ci/provenance.json").write_text(json.dumps({"upstream_commit": base}))
            git("add", ".")
            git("commit", "-qm", "Release")
            git("tag", "v1.3.8")
            source = git("rev-parse", "HEAD")
            patch = subprocess.check_output(["git", "diff", "--binary", base, "HEAD"], cwd=root)
            for channel in ("dev", "release"):
                directory = root / "downloaded" / f"android-{channel}"
                directory.mkdir()
                prefix = f"{'VITAdev' if channel == 'dev' else 'VITA-Plus'}-v1.3.8-android"
                manifest = self.manifest()
                manifest["source_commit"] = source
                if channel == "release":
                    manifest.update(package_id=collect_release.RELEASE_PACKAGE, signing_certificate_sha256=collect_release.RELEASE_CERTIFICATE_SHA256, launcher_label="VITA+", variant="release", native_configuration="Release", updates_vitadev=False)
                for suffix in ("-arm64.apk", "-symbols.zip"):
                    (directory / (prefix + suffix)).write_bytes(b"test binary")
                manifest["artifacts"] = {path.name: {"sha256": collect_release.digest(path)} for path in directory.iterdir()}
                (directory / f"{prefix}-source.patch").write_bytes(patch)
                (directory / "build-manifest.json").write_text(json.dumps(manifest))
                checksums = "".join(f"{collect_release.digest(path)}  {path.name}\n" for path in sorted(directory.iterdir()))
                (directory / "SHA256SUMS").write_text(checksums)
            linux = root / "downloaded" / f"vita3k-{git('rev-parse', '--short', 'HEAD')}-linux-x64"
            linux.mkdir()
            (linux / "Vita3K-x86_64.AppImage").write_bytes(b"test AppImage")
            subprocess.run([sys.executable, str(project / ".ci/collect-release.py"), "v1.3.8", "downloaded", "release-assets"], cwd=root, check=True, capture_output=True)
            output = root / "release-assets"
            self.assertTrue((output / "VITAdev-v1.3.8-android-arm64.apk").exists())
            self.assertTrue((output / "VITA-Plus-v1.3.8-android-arm64.apk").exists())
            self.assertTrue((output / "VITA-Plus-v1.3.8-linux-x86_64.AppImage").exists())
            self.assertEqual((output / "VITA-Plus-v1.3.8-source.patch").read_bytes(), patch)
            self.assertIn("Vita3K Build: 1030800", (output / "RELEASE_NOTES.md").read_text())
            subprocess.run(["sha256sum", "-c", "SHA256SUMS.txt"], cwd=output, check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()
