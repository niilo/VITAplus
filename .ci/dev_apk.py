"""Check the identity needed to update the existing VITAdev app."""

import re

PACKAGE = "org.vita3k.emulator.debug"
CERTIFICATE_SHA256 = "85028da2c7e4321de4bd4215a45773bddcc8f90629bafd6e19ea07cc6d91cac0"


def apk_identity(badging, certificates):
    package = re.search(r"^package: name='([^']+)' versionCode='(\d+)'", badging, re.M)
    fingerprints = re.findall(r"^Signer #\d+ certificate SHA-256 digest: ([0-9a-fA-F:]+)$", certificates, re.M)
    if package is None or len(fingerprints) != 1:
        raise ValueError("APK package, version code or signing certificate is missing or ambiguous")
    return {
        "package_id": package[1],
        "version_code": int(package[2]),
        "certificate_sha256": fingerprints[0].replace(":", "").lower(),
    }


def verify_dev_identity(identity, previous=None):
    if identity["package_id"] != PACKAGE:
        raise ValueError("VITAdev must use org.vita3k.emulator.debug. A different ID installs a separate app")
    if identity["certificate_sha256"] != CERTIFICATE_SHA256:
        raise ValueError("APK does not use the existing VITAdev signing key. Restore that key before building")
    if identity["version_code"] < 1:
        raise ValueError("APK version code must be positive")
    if previous is not None:
        if any(identity[field] != previous[field] for field in ("package_id", "certificate_sha256")):
            raise ValueError("APK cannot update the previous APK: package or certificate differs")
        if identity["version_code"] < previous["version_code"]:
            raise ValueError("APK version code is older than the previous APK")


ISOLATED_PACKAGE = "org.vita3k.emulator.debug.visualtest"
ISOLATED_CERTIFICATE_SHA256 = "04c7cfc63ab68d91d23a34108516066b737c87e5528f7bff621afa3fea38522a"


def verify_isolated_identity(identity, previous=None):
    if identity["package_id"] != ISOLATED_PACKAGE:
        raise ValueError("The separate visual test must use its dedicated package ID")
    if identity["certificate_sha256"] != ISOLATED_CERTIFICATE_SHA256:
        raise ValueError("The separate visual test must use the existing CI signing key")
    if identity["version_code"] < 1:
        raise ValueError("APK version code must be positive")
    if previous is not None:
        if any(identity[field] != previous[field] for field in ("package_id", "certificate_sha256")):
            raise ValueError("The previous APK belongs to a different test app or signing key")
        if identity["version_code"] < previous["version_code"]:
            raise ValueError("APK version code is older than the previous test APK")
