"""Regression checks for development APK update compatibility."""

import unittest

from dev_apk import CERTIFICATE_SHA256, ISOLATED_CERTIFICATE_SHA256, PACKAGE, ISOLATED_PACKAGE, apk_identity, verify_dev_identity, verify_isolated_identity


class DevApkTests(unittest.TestCase):
    def identity(self, package=PACKAGE, code=103708, certificate=CERTIFICATE_SHA256):
        return apk_identity(
            f"package: name='{package}' versionCode='{code}' versionName='v1.3.7-dev.8'\n",
            f"Signer #1 certificate SHA-256 digest: {certificate}\n",
        )

    def test_update_keeps_package_and_signer(self):
        verify_dev_identity(self.identity(), self.identity(code=103707))

    def test_both_previous_isolated_ids_are_rejected(self):
        for suffix in (".visualfixes", ".pockets3xci"):
            with self.subTest(suffix=suffix), self.assertRaisesRegex(ValueError, "separate app"):
                verify_dev_identity(self.identity(package=PACKAGE + suffix))

    def test_other_dev_keys_are_rejected(self):
        for certificate in (
            "ab" * 32,
            "85028da2c7e4321de4bd4215a45773bddcc8f90629bafd6e19ea07cc6d91cac0",
            ISOLATED_CERTIFICATE_SHA256,
        ):
            with self.subTest(certificate=certificate), self.assertRaisesRegex(ValueError, "signing key"):
                verify_dev_identity(self.identity(certificate=certificate))

    def test_version_downgrade_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "older"):
            verify_dev_identity(self.identity(), self.identity(code=103709))

    def test_rebuild_at_same_version_is_allowed(self):
        verify_dev_identity(self.identity(), self.identity())

    def test_previous_package_and_signer_must_match(self):
        for previous in (self.identity(package=PACKAGE + ".visualfixes"), self.identity(certificate="ab" * 32)):
            with self.assertRaisesRegex(ValueError, "differs"):
                verify_dev_identity(self.identity(), previous)

    def test_explicit_separate_test_uses_the_existing_ci_key(self):
        verify_isolated_identity(self.identity(package=ISOLATED_PACKAGE, certificate=ISOLATED_CERTIFICATE_SHA256))

    def test_separate_test_never_passes_as_a_vitadev_update(self):
        with self.assertRaisesRegex(ValueError, "separate app"):
            verify_dev_identity(self.identity(package=ISOLATED_PACKAGE, certificate=ISOLATED_CERTIFICATE_SHA256))
        with self.assertRaisesRegex(ValueError, "dedicated package"):
            verify_isolated_identity(self.identity())

    def test_separate_test_rejects_a_new_random_key(self):
        with self.assertRaisesRegex(ValueError, "existing CI signing key"):
            verify_isolated_identity(self.identity(package=ISOLATED_PACKAGE, certificate="ab" * 32))

    def test_unknown_signature_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "missing or ambiguous"):
            apk_identity("", "")


if __name__ == "__main__":
    unittest.main()
