"""Check version identity and update ordering across release promotion."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class VersionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="vita-version-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "tools/release").mkdir(parents=True)
        shutil.copyfile(Path(__file__).resolve().parents[1] / "tools/release/version-info.sh", self.root / "tools/release/version-info.sh")
        (self.root / ".ci").mkdir()
        (self.root / ".ci/version-base.txt").write_text("1.3.8\n")
        self.git("init", "-q")
        self.git("config", "user.name", "Version test")
        self.git("config", "user.email", "version-test@example.invalid")
        self.commit()

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root, text=True)

    def commit(self):
        self.git("add", ".")
        self.git("commit", "--allow-empty", "-qm", "Version test")

    def version(self):
        output = subprocess.check_output(["bash", "tools/release/version-info.sh"], cwd=self.root, text=True)
        return dict(line.split("=", 1) for line in output.splitlines())

    def test_declared_next_version_updates_the_tested_apk_before_tagging(self):
        self.git("tag", "v1.3.7")
        self.commit()
        info = self.version()
        self.assertEqual(info["version"], "v1.3.8-dev.0")
        self.assertEqual(info["version_code"], "1030800")
        self.assertEqual(info["is_release"], "0")

    def test_stable_tag_matches_android_and_native_identity(self):
        self.git("tag", "v1.3.8")
        info = self.version()
        self.assertEqual(info["version"], "v1.3.8")
        self.assertEqual(info["version_code"], "1030800")
        self.assertEqual(info["is_release"], "1")
        self.assertEqual(info["build_date"], "")

    def test_development_after_release_increases_the_version_code(self):
        self.git("tag", "v1.3.8")
        self.commit()
        info = self.version()
        self.assertEqual(info["version"], "v1.3.8-dev.1")
        self.assertEqual(info["version_code"], "1030801")
        self.assertTrue(info["build_date"])

    def test_prerelease_retains_its_tag_without_becoming_stable(self):
        self.git("tag", "v1.3.8-rc1")
        info = self.version()
        self.assertEqual(info["version"], "v1.3.8-rc1")
        self.assertEqual(info["is_release"], "0")
        self.assertEqual(info["version_code"], "1030800")

    def test_newer_release_tag_is_never_lowered_by_the_declared_floor(self):
        self.git("tag", "v1.3.9")
        self.assertEqual(self.version()["version_code"], "1030900")

    def test_invalid_declared_version_stops_the_build(self):
        (self.root / ".ci/version-base.txt").write_text("invalid\n")
        result = subprocess.run(["bash", "tools/release/version-info.sh"], cwd=self.root, capture_output=True)
        self.assertNotEqual(result.returncode, 0)

    def test_source_archive_preserves_the_declared_version(self):
        shutil.rmtree(self.root / ".git")
        info = self.version()
        self.assertEqual(info["version"], "v1.3.8-dev.0")
        self.assertEqual(info["version_code"], "1030800")


if __name__ == "__main__":
    unittest.main()
