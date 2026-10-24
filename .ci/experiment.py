"""Validate experiment provenance and declared runtime changes."""

import json
import os
from pathlib import Path
import re
import subprocess

BASE = "a86b5f93db43e4c4640ad1527e2a4695aad0f35b"
DECODER_PATH = "vita3k/codec/src/atrac9.cpp"
UNAVAILABLE_DECODER_CALL = "Atrac9DecodeBounded(decoder_handle, data, superframe_data_left, reinterpret_cast<short *>(result.data()), &decode_used)"
AVAILABLE_DECODER_CALL = "Atrac9Decode(decoder_handle, data, reinterpret_cast<short *>(result.data()), &decode_used)"
RUNTIME_PATHS = ("vita3k", "external", "cmake", "CMakeLists.txt", "vcpkg.json", "vcpkg-configuration.json", ".gitmodules", "android/app/src/main", "android/prebuilt", "data")


def validate_config(config):
    if config["upstream_base"] != BASE:
        raise ValueError("Experiment must start from the recorded all-enhancements revision")
    if not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", config["id"]):
        raise ValueError("Invalid experiment ID")
    if not re.fullmatch(r"exp-plus-[a-z0-9]+(?:-[a-z0-9]+)*", config["tag"]):
        raise ValueError("Invalid experimental pre-release tag")
    if type(config["version_code"]) is not int or not 1030800 <= config["version_code"] <= 2100000000:
        raise ValueError("Experiment version code must update the previous VITAdev build")
    for field in ("title", "version_name", "purpose"):
        if not config[field] or "\n" in config[field] or "\r" in config[field]:
            raise ValueError(f"Invalid experiment {field}")
    paths = config["expected_runtime_paths"]
    if len(set(paths)) != len(paths) or any(path.startswith("/") or ".." in Path(path).parts for path in paths):
        raise ValueError("Expected runtime paths must be unique relative file paths")
    if DECODER_PATH not in paths:
        raise ValueError("Every build needs the shared ATRAC9 decoder correction")
    if config["id"] == "baseline" and paths != [DECODER_PATH]:
        raise ValueError("The baseline can contain only the shared ATRAC9 decoder correction")
    if not config["device_checks"]:
        raise ValueError("An experiment needs a device test question")
    return config


def check_paths(config, changed):
    if set(changed) != set(config["expected_runtime_paths"]):
        raise ValueError(f"Runtime diff differs from the experiment declaration: {sorted(changed)}")


def check_decoder_source(upstream, current):
    if upstream.count(UNAVAILABLE_DECODER_CALL) != 1 or current != upstream.replace(UNAVAILABLE_DECODER_CALL, AVAILABLE_DECODER_CALL):
        raise ValueError("ATRAC9 must contain only the shared decoder build correction")


def load(root=Path.cwd()):
    config = validate_config(json.loads((root / ".ci/experiment.json").read_text()))
    changed = subprocess.check_output(["git", "diff", "--name-only", BASE, "HEAD", "--", *RUNTIME_PATHS], cwd=root, text=True).splitlines()
    check_paths(config, changed)
    upstream_decoder = subprocess.check_output(["git", "show", f"{BASE}:{DECODER_PATH}"], cwd=root, text=True)
    current_decoder = subprocess.check_output(["git", "show", f"HEAD:{DECODER_PATH}"], cwd=root, text=True)
    check_decoder_source(upstream_decoder, current_decoder)
    return config


if __name__ == "__main__":
    config = load()
    if os.environ.get("GITHUB_ENV"):
        with open(os.environ["GITHUB_ENV"], "a") as output:
            for field in ("id", "tag", "title"):
                output.write(f"EXPERIMENT_{field.upper()}={config[field]}\n")
    print(f"Verified {config['id']}: {len(config['expected_runtime_paths'])} declared runtime files; upstream {BASE}")
