#!/usr/bin/env python3
"""Restore and verify the signing key before an Actions Android build."""

import argparse
import base64
import binascii
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import uuid

from dev_apk import CERTIFICATE_SHA256, RELEASE_CERTIFICATE_SHA256


def prepare(channel, environment):
    prefix = "DEV_" if channel == "dev" else ""
    keystore_name = prefix + "KEYSTORE"
    encoded = environment.get(keystore_name, "")
    if not encoded:
        raise ValueError(f"Missing {keystore_name} repository secret")

    # These are Android's public debug-key defaults. Custom dev keys can supply
    # the same three password and alias secrets as a release key, with DEV_.
    defaults = ("android", "androiddebugkey", "android") if channel == "dev" else ("", "", "")
    names = [prefix + "SIGNING_" + name for name in ("STORE_PASSWORD", "KEY_ALIAS", "KEY_PASSWORD")]
    credentials = {name: environment.get(name) or default for name, default in zip(names, defaults)}
    for name, value in credentials.items():
        if not value:
            raise ValueError(f"Missing {name} repository secret")
    try:
        decoded = base64.b64decode("".join(encoded.split()), validate=True)
    except (ValueError, binascii.Error):
        raise ValueError(f"{keystore_name} must contain a base64-encoded keystore") from None
    if not decoded:
        raise ValueError(f"{keystore_name} contains an empty keystore")

    descriptor, filename = tempfile.mkstemp(prefix=f"vita-{channel}-", suffix=".keystore", dir=environment["RUNNER_TEMP"])
    path = Path(filename)
    try:
        with os.fdopen(descriptor, "wb") as store:
            store.write(decoded)
        child_environment = dict(environment, **credentials)
        command = ["keytool", "-keystore", filename, "-alias", credentials[names[1]], "-storepass:env", names[0]]
        certificate = subprocess.run(
            command + ["-exportcert"], env=child_environment, capture_output=True, check=False
        )
        if certificate.returncode:
            raise ValueError(f"Cannot open {keystore_name}; check the store password and key alias")
        fingerprint = hashlib.sha256(certificate.stdout).hexdigest()
        expected = CERTIFICATE_SHA256 if channel == "dev" else RELEASE_CERTIFICATE_SHA256
        if fingerprint != expected:
            raise ValueError(f"{keystore_name} certificate does not match the existing {'VITAdev' if channel == 'dev' else 'VITA+'} app (SHA-256: {fingerprint})")
        # A certificate alone does not prove that the private key is present or
        # that its password works. Sign a certificate request without saving it.
        request = subprocess.run(
            command + ["-certreq", "-keypass:env", names[2]], env=child_environment, capture_output=True, check=False
        )
        if request.returncode:
            raise ValueError(f"Cannot sign with {keystore_name}; check the private key and key password")

        credentials[prefix + "SIGNING_STORE_PATH"] = filename
        # Use multiline values so passwords never become workflow commands.
        with open(environment["GITHUB_ENV"], "a") as github_environment:
            for name, value in credentials.items():
                delimiter = "vita_" + uuid.uuid4().hex
                github_environment.write(f"{name}<<{delimiter}\n{value}\n{delimiter}\n")
        print(f"Validated {keystore_name}; certificate SHA-256: {fingerprint}")
    except BaseException:
        path.unlink(missing_ok=True)
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("channel", choices=("dev", "release"))
    arguments = parser.parse_args()
    try:
        prepare(arguments.channel, os.environ)
    except (ValueError, OSError) as error:
        # keytool output can include user-provided values. Never forward it.
        print(f"::error::{error}" if isinstance(error, ValueError) else "::error::Cannot restore or verify the Android signing key")
        raise SystemExit(1)
