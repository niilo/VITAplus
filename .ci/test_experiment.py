"""Checks for provenance and pre-release identity constraints."""

import copy
import json
from pathlib import Path
import unittest

from experiment import AVAILABLE_DECODER_CALL, DECODER_PATH, UNAVAILABLE_DECODER_CALL, check_decoder_source, check_paths, validate_config


class ExperimentTests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads(Path(__file__).with_name("experiment.json").read_text())

    def test_baseline_contains_only_the_shared_build_correction(self):
        baseline = copy.deepcopy(self.config)
        baseline.update(id="baseline", expected_runtime_paths=[DECODER_PATH])
        validate_config(baseline)
        check_paths(baseline, [DECODER_PATH])
        with self.assertRaisesRegex(ValueError, "Runtime diff"):
            check_paths(baseline, [DECODER_PATH, "vita3k/renderer/src/renderer.cpp"])
        for paths in ([], [DECODER_PATH, "vita3k/io/src/io.cpp"]):
            baseline["expected_runtime_paths"] = paths
            with self.assertRaisesRegex(ValueError, "decoder correction"):
                validate_config(baseline)

    def test_decoder_correction_cannot_include_other_audio_changes(self):
        upstream = f"before\n{UNAVAILABLE_DECODER_CALL}\nafter\n"
        current = upstream.replace(UNAVAILABLE_DECODER_CALL, AVAILABLE_DECODER_CALL)
        check_decoder_source(upstream, current)
        for source, changed in ((upstream, upstream), (upstream, current + "unrelated change\n"), ("different upstream", current)):
            with self.assertRaisesRegex(ValueError, "only the shared decoder"):
                check_decoder_source(source, changed)

    def test_candidate_must_match_its_exact_declared_changes(self):
        self.config.update(id="candidate", expected_runtime_paths=[DECODER_PATH, "vita3k/io/src/io.cpp"])
        validate_config(self.config)
        check_paths(self.config, self.config["expected_runtime_paths"])
        for paths in ([], [DECODER_PATH, "vita3k/io/src/io.cpp", "vita3k/display/src/display.cpp"]):
            with self.assertRaisesRegex(ValueError, "Runtime diff"):
                check_paths(self.config, paths)

    def test_unrelated_base_is_rejected(self):
        self.config["upstream_base"] = "0" * 40
        with self.assertRaisesRegex(ValueError, "recorded"):
            validate_config(self.config)

    def test_release_tag_is_limited_to_experiments(self):
        for tag in ("v1.3.8", "continuous", "exp-plus-$(id)", "exp-plus-good\nBAD=1"):
            self.config["tag"] = tag
            with self.assertRaisesRegex(ValueError, "tag"):
                validate_config(self.config)

    def test_older_version_code_is_rejected(self):
        for code in (1030711, 0, True, "1030800"):
            self.config["version_code"] = code
            with self.assertRaisesRegex(ValueError, "version code"):
                validate_config(self.config)

    def test_paths_cannot_escape_the_checkout(self):
        self.config["id"] = "candidate"
        for paths in (["../other"], ["/tmp/other"], ["same", "same"]):
            self.config["expected_runtime_paths"] = paths
            with self.assertRaisesRegex(ValueError, "paths"):
                validate_config(self.config)

    def test_workflow_environment_values_cannot_inject_lines(self):
        self.config["title"] = "title\nBAD=1"
        with self.assertRaisesRegex(ValueError, "title"):
            validate_config(self.config)


if __name__ == "__main__":
    unittest.main()
