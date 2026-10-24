#!/usr/bin/env bash
set -euo pipefail
python3 .ci/test_dev_apk.py
python3 .ci/test_android_signing.py
python3 .ci/test_experiment.py
python3 .ci/test_version_info.py
python3 .ci/test_release_collect.py
python3 tools/android/test_fps_sample.py
bash tools/release/check-no-signing-keys.sh
