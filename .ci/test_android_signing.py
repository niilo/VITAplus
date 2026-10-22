"""Exercise keystore validation with temporary keys and real JDK tools."""

import base64
import contextlib
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import unittest
from unittest.mock import patch

specification = importlib.util.spec_from_file_location("android_signing", Path(__file__).with_name("prepare-android-signing.py"))
signing = importlib.util.module_from_spec(specification)
specification.loader.exec_module(signing)


class AndroidSigningTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.keys = tempfile.TemporaryDirectory()
        cls.password = secrets.token_urlsafe(24)
        environment = dict(os.environ, TEST_STORE_PASSWORD=cls.password, TEST_KEY_PASSWORD=cls.password)
        cls.key = Path(cls.keys.name) / "temporary.jks"
        command = ["keytool", "-keystore", str(cls.key), "-storepass:env", "TEST_STORE_PASSWORD", "-alias", "androiddebugkey"]
        subprocess.run(command + ["-genkeypair", "-storetype", "JKS", "-keypass:env", "TEST_KEY_PASSWORD", "-keyalg", "RSA", "-dname", "CN=Temporary CI Test", "-validity", "1"], env=environment, capture_output=True, check=True)
        certificate = subprocess.check_output(command + ["-exportcert"], env=environment, stderr=subprocess.DEVNULL)
        cls.fingerprint = hashlib.sha256(certificate).hexdigest()
        cls.default_key = Path(cls.keys.name) / "standard-debug.jks"
        environment.update(TEST_STORE_PASSWORD="android", TEST_KEY_PASSWORD="android")
        command[2] = str(cls.default_key)
        subprocess.run(command + ["-genkeypair", "-storetype", "JKS", "-keypass:env", "TEST_KEY_PASSWORD", "-keyalg", "RSA", "-dname", "CN=Temporary Standard Debug Test", "-validity", "1"], env=environment, capture_output=True, check=True)
        certificate = subprocess.check_output(command + ["-exportcert"], env=environment, stderr=subprocess.DEVNULL)
        cls.default_fingerprint = hashlib.sha256(certificate).hexdigest()

    @classmethod
    def tearDownClass(cls):
        cls.keys.cleanup()

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.output = Path(self.directory.name) / "github-env"
        self.environment = dict(os.environ, RUNNER_TEMP=self.directory.name, GITHUB_ENV=str(self.output))
        for name in list(self.environment):
            if name.startswith(("DEV_SIGNING_", "SIGNING_")) or name in ("KEYSTORE", "DEV_KEYSTORE"):
                del self.environment[name]

    def use_key(self, channel="dev"):
        prefix = "DEV_" if channel == "dev" else ""
        self.environment.update({
            prefix + "KEYSTORE": base64.b64encode(self.key.read_bytes()).decode(),
            prefix + "SIGNING_STORE_PASSWORD": self.password,
            prefix + "SIGNING_KEY_ALIAS": "androiddebugkey",
            prefix + "SIGNING_KEY_PASSWORD": self.password,
        })

    def assert_no_export(self):
        self.assertFalse(self.output.exists())
        self.assertEqual(list(Path(self.directory.name).glob("*.keystore")), [])

    def test_dev_does_not_fall_back_to_production_key(self):
        self.use_key("release")
        with self.assertRaisesRegex(ValueError, "Missing DEV_KEYSTORE"):
            signing.prepare("dev", self.environment)
        self.assert_no_export()

    def test_release_does_not_fall_back_to_dev_key(self):
        self.use_key()
        with self.assertRaisesRegex(ValueError, "Missing KEYSTORE"):
            signing.prepare("release", self.environment)
        self.assert_no_export()

    def test_malformed_base64_is_rejected(self):
        self.environment["DEV_KEYSTORE"] = "invalid!"
        with self.assertRaisesRegex(ValueError, "base64-encoded"):
            signing.prepare("dev", self.environment)
        self.assert_no_export()

    def test_release_requires_explicit_credentials(self):
        self.use_key("release")
        del self.environment["SIGNING_STORE_PASSWORD"]
        with self.assertRaisesRegex(ValueError, "Missing SIGNING_STORE_PASSWORD"):
            signing.prepare("release", self.environment)
        self.assert_no_export()

    def test_new_certificate_is_rejected_and_removed(self):
        self.use_key()
        with self.assertRaisesRegex(ValueError, "does not match the existing VITAdev"):
            signing.prepare("dev", self.environment)
        self.assert_no_export()

    def test_password_failure_does_not_echo_credentials(self):
        self.use_key()
        self.environment["DEV_SIGNING_STORE_PASSWORD"] = secrets.token_urlsafe(24)
        with self.assertRaisesRegex(ValueError, "Cannot open") as failure:
            signing.prepare("dev", self.environment)
        self.assertNotIn(self.password, str(failure.exception))
        self.assertNotIn(self.environment["DEV_SIGNING_STORE_PASSWORD"], str(failure.exception))
        self.assert_no_export()

    def test_private_key_password_is_checked(self):
        self.use_key()
        self.environment["DEV_SIGNING_KEY_PASSWORD"] = secrets.token_urlsafe(24)
        with patch.object(signing, "CERTIFICATE_SHA256", self.fingerprint), self.assertRaisesRegex(ValueError, "Cannot sign"):
            signing.prepare("dev", self.environment)
        self.assert_no_export()

    def test_dev_key_can_use_standard_android_defaults(self):
        self.environment["DEV_KEYSTORE"] = base64.b64encode(self.default_key.read_bytes()).decode()
        with patch.object(signing, "CERTIFICATE_SHA256", self.default_fingerprint), contextlib.redirect_stdout(io.StringIO()):
            signing.prepare("dev", self.environment)
        values = self.output.read_text()
        self.assertIn("\nandroiddebugkey\n", values)
        self.assertEqual(values.count("\nandroid\n"), 2)

    def test_build_script_requires_the_selected_credentials(self):
        script = Path(__file__).with_name("build-android.sh")
        for channel, missing in (("dev", "DEV_SIGNING_STORE_PATH"), ("release", "SIGNING_STORE_PATH")):
            with self.subTest(channel=channel):
                result = subprocess.run(["bash", str(script), str(Path(self.directory.name) / "artifacts")], env=dict(self.environment, ANDROID_BUILD_CHANNEL=channel), capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Missing " + missing, result.stderr)
                self.assertFalse((Path(self.directory.name) / "artifacts").exists())

    def test_verified_key_exports_only_its_channel(self):
        for channel, constant in (("dev", "CERTIFICATE_SHA256"), ("release", "RELEASE_CERTIFICATE_SHA256")):
            with self.subTest(channel=channel):
                self.use_key(channel)
                log = io.StringIO()
                with patch.object(signing, constant, self.fingerprint), contextlib.redirect_stdout(log):
                    signing.prepare(channel, self.environment)
                self.assertIn(self.fingerprint, log.getvalue())
                self.assertNotIn(self.password, log.getvalue())
                values = self.output.read_text()
                prefix = "DEV_" if channel == "dev" else ""
                self.assertIn(prefix + "SIGNING_STORE_PATH<<", values)
                self.assertIn(prefix + "SIGNING_KEY_PASSWORD<<", values)
                if channel == "dev":
                    self.assertNotIn("\nSIGNING_STORE_PATH<<", values)
                else:
                    self.assertNotIn("DEV_SIGNING_", values)
                for key in Path(self.directory.name).glob("*.keystore"):
                    self.assertEqual(key.stat().st_mode & 0o777, 0o600)
                    key.unlink()
                self.output.unlink()


if __name__ == "__main__":
    unittest.main()
