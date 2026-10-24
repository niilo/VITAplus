"""Reject incomplete or mismatched build artifacts before publication."""

import hashlib
import importlib.util
from pathlib import Path
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


if __name__ == "__main__":
    unittest.main()
