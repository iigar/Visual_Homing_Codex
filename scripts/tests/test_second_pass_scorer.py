#!/usr/bin/env python3
"""Hand-calculated scoring oracles; all capture/results are synthetic."""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


SCRIPT = Path(__file__).resolve().parents[1] / "score-second-pass.py"
scorer = load("scorer", SCRIPT)
fixtures = load("dataset_fixtures", Path(__file__).with_name("test_second_pass_dataset.py"))
FIELDS = ("sequence frame_id timestamp_ns observation missing_reason match_valid reference_index "
          "endpoint_stop navigation_command_valid route_readiness verification_accepted published").split()


def make_inputs(root, count=8):
    meta, _, _ = fixtures.make_dataset(root)
    frames, labels, predictions = [], [], []
    for i in range(count):
        data = b"P5\n2 2\n255\n" + bytes([10 + i, 32, 35, 10])
        path = f"frames/{i}.pgm"
        (root / path).write_bytes(data)
        identity = dict(sequence=str(i), frame_id=str(10 + i), timestamp_ns=str(100 + 100 * i))
        frames.append(dict(identity, path=path, sha256=fixtures.digest(data), width="2", height="2"))
        labels.append(dict(frame_id=str(10 + i), location="on_route", reference_index_min="1", reference_index_max="1",
                           endpoint="neither", direction="forward", visibility="clear", evidence_id="manual"))
        predictions.append(dict(identity, observation="observed", missing_reason="", match_valid="true", reference_index="1",
                                endpoint_stop="false", navigation_command_valid="", route_readiness="",
                                verification_accepted="", published=""))
    meta["sequence"].update(acquired_count=count, stored_count=count, dropped_count=0)
    meta["sequence"]["actual"].update(last_frame_id=9 + count, last_timestamp_ns=100 * count)
    fixtures.save_dataset(root, meta, frames, labels)
    return meta, frames, labels, predictions


def default_policy():
    return dict(schema_version=1, policy_id="synthetic-policy-v1", scorable_visibility=["clear", "degraded"],
                target_endpoint="end", endpoint_early_tolerance_ns=0, endpoint_late_tolerance_ns=0,
                reacquisition_min_correct_frames=2, reacquisition_min_duration_ns=100, max_interframe_gap_ns=100)


class ScorerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="scorer tests ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "dataset"
        self.run_root = Path(self.temp.name) / "run with spaces"
        self.run_root.mkdir()
        self.meta, self.frames, self.labels, self.predictions = make_inputs(self.root)
        self.policy = default_policy()
        (self.run_root / "implementation.txt").write_bytes(b"SYNTHETIC oracle; not executed code")
        (self.run_root / "configuration.txt").write_bytes(b"synthetic test configuration")
        self.run = dict(schema_version=1, run_id="synthetic-run", dataset_id="one", input_sha256={},
                        inputs_frozen_before_run=True, producer=dict(kind="synthetic", code_revision="synthetic-only",
                        implementation=self.binding("implementation.txt"), configuration=self.binding("configuration.txt"),
                        state_reset_policy="reset at session start", preprocessing="identity"), policy={}, predictions={})
        self.save()

    def binding(self, path):
        return {"path": path, "sha256": fixtures.digest((self.run_root / path).read_bytes())}

    def save(self):
        fixtures.save_dataset(self.root, self.meta, self.frames, self.labels)
        report = scorer.v.validate_datasets([self.root])
        self.run["input_sha256"] = report["datasets"][0]["input_sha256"]
        (self.run_root / "policy.json").write_text(json.dumps(self.policy) + "\n")
        fixtures.write_csv(self.run_root / "predictions.csv", FIELDS, self.predictions)
        self.run["policy"] = self.binding("policy.json")
        self.run["predictions"] = self.binding("predictions.csv")
        self.write_run()

    def write_run(self):
        (self.run_root / "run.json").write_text(json.dumps(self.run, indent=2) + "\n")

    def result(self, expected="scored", message=None, save=True):
        if save:
            self.save()
        result = scorer.score(self.root, self.run_root)
        self.assertEqual(result["status"], expected, result)
        if message:
            self.assertIn(message, json.dumps(result))
        if expected != "scored":
            self.assertNotIn("metrics", result)
        return result

    def label(self, i, location):
        self.labels[i]["location"] = location
        if location != "on_route":
            self.labels[i].update(reference_index_min="", reference_index_max="")

    def missing(self, i):
        self.predictions[i].update(observation="missing", missing_reason="not logged")
        for key in FIELDS[5:]:
            self.predictions[i][key] = ""

    def invalid(self, i):
        self.predictions[i].update(match_valid="false", reference_index="")

    def test_hand_calculated_confusion_and_coverage(self):
        self.labels[1]["reference_index_max"] = "2"
        self.predictions[1]["reference_index"] = "3"
        self.invalid(2)
        self.missing(3)
        self.label(4, "off_route")
        self.label(5, "off_route")
        self.invalid(5)
        self.label(6, "unknown")
        self.labels[7].update(reference_index_min="", reference_index_max="")
        result = self.result()
        metric = result["metrics"]
        self.assertEqual((metric["tp"], metric["fp"], metric["fn"]), (1, 2, 3))
        self.assertEqual(metric["precision"], {"numerator": 1, "denominator": 3, "value": 1 / 3})
        self.assertEqual(metric["recall"]["value"], 1 / 4)
        self.assertEqual(metric["negative_fp_fraction"]["value"], 1 / 2)
        self.assertEqual(metric["prediction_coverage"]["value"], 7 / 8)
        self.assertEqual(metric["abstention_fraction"]["value"], 2 / 7)
        self.assertEqual(metric["unscorable_fraction"]["value"], 2 / 8)
        self.assertEqual(metric["index_error"], {"count": 2, "min": 0, "max": 1, "p50": 0, "p95": 1})
        self.assertEqual(metric["normalized_index_error"]["max"], 1 / 3)
        self.assertEqual(metric["represented_duration_ns"]["missing"], 100)
        self.assertEqual(metric["represented_duration_ns"]["unscorable"], 100)
        self.assertEqual([r["outcome"] for r in result["decisions"]], ["tp", "fp_fn", "fn", "fn", "fp", "tn", "unscorable", "unscorable"])

    def test_small_exhaustive_interval_oracle(self):
        # Independent integer-set membership oracle, including all inclusive
        # intervals and query indices in a four-entry reference.
        cases = 0
        for lower in range(4):
            for upper in range(lower, 4):
                truth = set(range(lower, upper + 1))
                for index in range(4):
                    label = dict(self.labels[0], reference_index_min=str(lower), reference_index_max=str(upper))
                    pred = dict(observation="observed", missing_reason="", match_valid=True, reference_index=index,
                                endpoint_stop=False, **{key: None for key in scorer.GATES})
                    decisions, _ = scorer.frame_decisions([self.frames[0]], {10: label}, [pred], self.meta, self.policy)
                    row = decisions[0]
                    correct = index in truth
                    self.assertEqual((row["tp"], row["fp"], row["fn"]), (int(correct), int(not correct), int(not correct)))
                    self.assertEqual(row["index_error"], min(abs(index - actual) for actual in truth))
                    cases += 1
        self.assertEqual(cases, 40)

    def test_missing_positive_is_fn_missing_negative_is_not_tn(self):
        for i in range(8):
            self.missing(i)
        self.label(6, "off_route")
        self.label(7, "off_route")
        metric = self.result()["metrics"]
        self.assertEqual((metric["tp"], metric["fp"], metric["fn"]), (0, 0, 6))
        self.assertIsNone(metric["precision"]["value"])
        self.assertEqual(metric["recall"]["value"], 0)
        self.assertIsNone(metric["negative_fp_fraction"]["value"])
        self.assertEqual(metric["negative_prediction_coverage"]["value"], 0)
        self.assertEqual(metric["counts"]["missing_negative"], 2)
        self.assertNotIn("tn", metric["counts"])
        self.assertEqual(metric["missing_reasons"], {"not logged": 8})

    def test_zero_denominators_and_one_entry_normalization(self):
        for i in range(8):
            self.label(i, "unknown")
        metric = self.result()["metrics"]
        self.assertIsNone(metric["precision"]["value"])
        self.assertIsNone(metric["recall"]["value"])
        self.assertIsNone(metric["negative_fp_fraction"]["value"])
        self.assertEqual(metric["index_error"]["count"], 0)
        label = dict(self.labels[0], location="on_route", reference_index_min="0", reference_index_max="0")
        meta = copy.deepcopy(self.meta)
        meta["reference"]["entry_count"] = 1
        pred = dict(observation="observed", match_valid=True, reference_index=0)
        rows, _ = scorer.frame_decisions([self.frames[0]], {10: label}, [pred], meta, self.policy)
        self.assertEqual(rows[0]["tp"], 1)
        self.assertIsNone(rows[0]["normalized_index_error"])

    def test_visibility_policy_and_direction_strata(self):
        self.policy["scorable_visibility"] = ["clear"]
        self.labels[0]["visibility"] = "degraded"
        self.labels[1]["visibility"] = "unobservable"
        self.labels[2]["visibility"] = "unknown"
        self.labels[3]["direction"] = "reverse"
        self.labels[4]["direction"] = "stationary"
        result = self.result()
        self.assertEqual(result["metrics"]["tp"], 5)
        self.assertEqual(result["metrics"]["unscorable_reasons"], {"excluded_visibility": 3})
        self.assertEqual(result["by_direction"]["reverse"]["tp"], 1)
        self.assertEqual(sum(m["frames"] for m in result["by_direction"].values()), 8)
        self.assertEqual(sum(m["tp"] for m in result["by_visibility"].values()), 5)
        self.assertIsNone(result["capture_conditions"]["altitude"])

    def test_gate_outcomes_are_distinct_from_match_success(self):
        self.predictions[0].update(navigation_command_valid="false", route_readiness="true", verification_accepted="false", published="true")
        result = self.result()
        self.assertEqual(result["metrics"]["tp"], 8)
        gates = result["metrics"]["gate_outcomes"]
        self.assertEqual(gates["navigation_command_valid"], {"true": 0, "false": 1, "unknown": 7})
        self.assertEqual(gates["published"], {"true": 1, "false": 0, "unknown": 7})
        self.assertFalse(result["semantic_independence_verified"])

    def test_deterministic_read_only_report_and_source_hashes(self):
        before = {str(p): fixtures.digest(p.read_bytes()) for p in Path(self.temp.name).rglob("*") if p.is_file()}
        one = self.result(save=False)
        two = self.result(save=False)
        self.assertEqual(one, two)
        after = {str(p): fixtures.digest(p.read_bytes()) for p in Path(self.temp.name).rglob("*") if p.is_file()}
        self.assertEqual(before, after)
        self.assertEqual(one["scorer_sha256"][SCRIPT.name], fixtures.digest(SCRIPT.read_bytes()))
        self.assertEqual(one["input_kind"], "synthetic")
        self.assertIn("not independently verified", one["pinning_provenance"])

    def test_reordered_missing_extra_and_duplicate_predictions(self):
        good = copy.deepcopy(self.predictions)
        cases = [good[:-1], good + [good[0]], [good[1], good[0]] + good[2:], [good[0], good[0]] + good[2:]]
        for rows in cases:
            with self.subTest(rows=len(rows)):
                self.predictions = rows
                self.result("invalid")

    def test_prediction_types_ranges_and_no_timestamp_rebasing(self):
        good = copy.deepcopy(self.predictions[0])
        for patch in ({"timestamp_ns": "101"}, {"frame_id": "11"}, {"sequence": "-1"}, {"timestamp_ns": "1e2"},
                      {"match_valid": "1"}, {"reference_index": "4"}, {"reference_index": ""},
                      {"match_valid": "false"}, {"observation": "missing"}, {"missing_reason": "nope"},
                      {"published": "yes"}, {"endpoint_stop": "null"}):
            with self.subTest(patch=patch):
                self.predictions[0] = dict(good, **patch)
                self.result("invalid")

    def test_pins_reject_modified_dataset_policy_predictions_and_producer(self):
        for path in (self.root / "dataset.json", self.run_root / "policy.json", self.run_root / "predictions.csv",
                     self.run_root / "implementation.txt", self.run_root / "configuration.txt"):
            with self.subTest(path=path.name):
                good = path.read_bytes()
                path.write_bytes(good + b" ")
                self.result("invalid", save=False)
                path.write_bytes(good)
        self.run["input_sha256"].pop("labels.csv")
        self.write_run()
        self.result("invalid", "input hashes differ", save=False)

    def test_null_hashes_and_unpinned_runs_are_invalid(self):
        good = copy.deepcopy(self.run)
        for section in ("policy", "predictions"):
            self.run = copy.deepcopy(good)
            self.run[section]["sha256"] = None
            self.write_run()
            self.result("invalid", "lowercase SHA-256", save=False)
        for section in ("implementation", "configuration"):
            self.run = copy.deepcopy(good)
            self.run["producer"][section]["sha256"] = None
            self.write_run()
            self.result("invalid", "lowercase SHA-256", save=False)
        for patch in ({"inputs_frozen_before_run": False}, {"schema_version": True}, {"dataset_id": "other"}, {"input_sha256": None}):
            self.run = dict(good, **patch)
            self.write_run()
            self.result("invalid", save=False)

    def test_invalid_policy_types_unknown_keys_and_nonfinite(self):
        good = copy.deepcopy(self.policy)
        for patch in ({"max_interframe_gap_ns": 0}, {"reacquisition_min_correct_frames": True},
                      {"reacquisition_min_correct_frames": 0}, {"reacquisition_min_duration_ns": -1},
                      {"endpoint_early_tolerance_ns": 1.5}, {"scorable_visibility": []},
                      {"scorable_visibility": ["clear", "clear"]}, {"scorable_visibility": ["unknown"]},
                      {"target_endpoint": "both"}, {"extra": True}):
            self.policy = dict(good, **patch)
            self.result("invalid")
        self.policy = good
        self.save()
        text = (self.run_root / "policy.json").read_text()
        for value in ("NaN", "1e999"):
            (self.run_root / "policy.json").write_text(text.replace('"max_interframe_gap_ns": 100', '"max_interframe_gap_ns": ' + value))
            self.run["policy"] = self.binding("policy.json")
            self.write_run()
            self.result("invalid", save=False)

    def test_run_json_and_csv_schema_errors(self):
        original = (self.run_root / "run.json").read_text()
        for text in ("[]", "{", original.replace('"schema_version": 1', '"schema_version": 1, "schema_version": 1')):
            (self.run_root / "run.json").write_text(text)
            self.result("invalid", save=False)
        (self.run_root / "run.json").write_text(original)
        for data in ("match_frame id=10 route_index=1\n", "sequence,frame_id\n0,10\n", ','.join(FIELDS) + '\n"unfinished'):
            (self.run_root / "predictions.csv").write_text(data)
            self.run["predictions"] = self.binding("predictions.csv")
            self.write_run()
            self.result("invalid", save=False)

    def test_run_paths_cannot_escape_or_use_symlink_escape(self):
        good = copy.deepcopy(self.run["predictions"])
        for path in ("../predictions.csv", "/tmp/outside.csv", "C:/outside.csv"):
            self.run["predictions"] = dict(good, path=path)
            self.write_run()
            self.result("invalid", save=False)
        link = self.run_root / "escape.csv"
        try:
            link.symlink_to(self.root / "frames.csv")
        except OSError as exc:
            self.skipTest(f"symlinks unavailable: {exc}")
        self.run["predictions"] = dict(good, path="escape.csv")
        self.write_run()
        self.result("invalid", "escapes dataset root", save=False)

    def test_review_gate_and_split_peer_gate_block_metrics(self):
        self.meta["independence"]["review_status"] = "pending"
        self.result("needs_review")
        self.meta["independence"]["review_status"] = "reviewed"
        self.save()
        other = Path(self.temp.name) / "peer"
        meta, frames, labels = fixtures.make_dataset(other, "peer", "test", 40)
        meta["capture_group_id"] = self.meta["capture_group_id"]
        fixtures.save_dataset(other, meta, frames, labels)
        result = scorer.score(self.root, self.run_root, [other])
        self.assertEqual(result["status"], "invalid")
        self.assertNotIn("metrics", result)
        meta["capture_group_id"] = "another-group"
        fixtures.save_dataset(other, meta, frames, labels)
        self.assertEqual(scorer.score(self.root, self.run_root, [other])["status"], "scored")
        self.assertEqual(scorer.score(self.root, self.run_root, [other], True)["status"], "invalid")

    def endpoint_region(self):
        self.labels[3]["endpoint"] = self.labels[4]["endpoint"] = "end"
        self.policy.update(endpoint_early_tolerance_ns=100, endpoint_late_tolerance_ns=100)

    def test_endpoint_inclusive_tolerances_duplicates_early_and_false(self):
        self.endpoint_region()
        for i in (0, 2, 4, 7):
            self.predictions[i]["endpoint_stop"] = "true"
        endpoint = self.result()["endpoints"]
        self.assertEqual(endpoint["detected"], 1)
        self.assertEqual(endpoint["events"][0]["entry_delay_ns"], -100)
        self.assertEqual(endpoint["stop_counts"], {"early_stop": 1, "accepted": 1, "duplicate_stop": 1, "false_stop": 1})
        for i in (0, 2, 4, 7):
            self.predictions[i]["endpoint_stop"] = "false"
        self.predictions[5]["endpoint_stop"] = "true"
        endpoint = self.result()["endpoints"]
        self.assertEqual(endpoint["events"][0]["entry_delay_ns"], 200)
        self.predictions[5]["endpoint_stop"] = "false"
        self.predictions[6]["endpoint_stop"] = "true"
        endpoint = self.result()["endpoints"]
        self.assertEqual(endpoint["not_detected"], 1)
        self.assertEqual(endpoint["stop_counts"], {"false_stop": 1})

    def test_reverse_target_endpoint_and_unknown_stop(self):
        self.endpoint_region()
        self.labels[3]["endpoint"] = self.labels[4]["endpoint"] = "start"
        self.labels[3]["direction"] = self.labels[4]["direction"] = "reverse"
        self.policy["target_endpoint"] = "start"
        self.predictions[3]["endpoint_stop"] = "true"
        endpoint = self.result()["endpoints"]
        self.assertEqual(endpoint["detected"], 1)
        self.predictions[3]["endpoint_stop"] = "false"
        self.labels[2]["endpoint"] = "unknown"
        self.predictions[2]["endpoint_stop"] = "true"
        endpoint = self.result()["endpoints"]
        self.assertEqual(endpoint["stop_counts"], {"unscorable_stop": 1})
        self.assertTrue(endpoint["events"][0]["entry_left_censored"])

    def test_endpoint_one_ns_outside_late_window_is_not_accepted(self):
        self.endpoint_region()
        self.policy["max_interframe_gap_ns"] = 200
        self.predictions[5]["endpoint_stop"] = "true"
        self.assertEqual(self.result()["endpoints"]["detected"], 1)
        self.frames[5]["timestamp_ns"] = self.predictions[5]["timestamp_ns"] = "601"
        result = self.result()
        self.assertEqual(result["endpoints"]["detected"], 0)
        self.assertEqual(result["endpoints"]["stop_counts"], {"false_stop": 1})
        self.assertEqual(result["temporal_coverage"]["boundaries"], [])

    def test_endpoint_missing_observations_and_censoring(self):
        self.labels[0]["endpoint"] = "end"
        self.labels[-1]["endpoint"] = "end"
        self.predictions[0]["endpoint_stop"] = "true"
        self.predictions[-1]["endpoint_stop"] = ""
        endpoint = self.result()["endpoints"]
        self.assertTrue(endpoint["events"][0]["entry_left_censored"])
        self.assertIsNone(endpoint["events"][0]["entry_delay_ns"])
        self.assertEqual(endpoint["events"][0]["sample_delay_ns"], 0)
        self.assertEqual(endpoint["not_detected_right_censored"], 1)
        self.assertEqual(endpoint["events"][1]["unobserved_stop_frames"], 1)

    def test_overlapping_endpoint_windows_are_rejected(self):
        self.labels[2]["endpoint"] = self.labels[4]["endpoint"] = "end"
        self.policy.update(endpoint_early_tolerance_ns=100, endpoint_late_tolerance_ns=100)
        self.result("invalid", "overlapping endpoint tolerance windows")
        self.policy["endpoint_late_tolerance_ns"] = 0
        self.assertEqual(len(self.result()["endpoints"]["events"]), 2)

    def test_reacquisition_requires_count_and_duration_and_resets_on_wrong(self):
        self.label(0, "off_route")
        self.label(5, "off_route")
        self.predictions[2]["reference_index"] = "3"
        self.policy["reacquisition_min_duration_ns"] = 150
        reacq = self.result()["reacquisition"]
        self.assertEqual((reacq["returns"], reacq["confirmed_returns"], reacq["unconfirmed_returns"], reacq["right_censored_returns"]), (2, 0, 2, 1))
        self.assertEqual([e["termination"] for e in reacq["episodes"]], ["region_exit", "clip_end"])
        self.policy["reacquisition_min_duration_ns"] = 100
        reacq = self.result()["reacquisition"]
        self.assertEqual(reacq["confirmed_returns"], 2)
        self.assertEqual([e["observed_delay_ns"] for e in reacq["episodes"]], [300, 100])

    def test_missing_or_invalid_predictions_reset_confirmation(self):
        self.label(0, "off_route")
        for missing in (True, False):
            if missing:
                self.missing(2)
            else:
                self.predictions[2].update(observation="observed", missing_reason="", match_valid="false", endpoint_stop="false")
            reacq = self.result()["reacquisition"]
            self.assertEqual(reacq["episodes"][0]["confirmation_frame_id"], 14)
            self.assertEqual(reacq["return_delay_ns"]["min"], 300)

    def test_initial_acquisition_is_not_reported_as_reacquisition(self):
        reacq = self.result()["reacquisition"]
        self.assertEqual(reacq["returns"], 0)
        self.assertIsNone(reacq["return_delay_ns"]["p50"])
        self.assertEqual(reacq["episodes"][0]["kind"], "initial_acquisition")
        self.assertEqual(reacq["episodes"][0]["observed_delay_ns"], 100)

    def add_gap(self, after=2):
        for i in range(after + 1, len(self.frames)):
            fid = str(int(self.frames[i]["frame_id"]) + 1)
            self.frames[i]["frame_id"] = self.labels[i]["frame_id"] = self.predictions[i]["frame_id"] = fid
        self.meta["sequence"]["actual"]["last_frame_id"] += 1
        self.meta["sequence"].update(acquired_count=9, dropped_count=1)
        self.meta["sequence"]["gaps"] = [dict(after_frame_id=int(self.frames[after]["frame_id"]),
             before_frame_id=int(self.frames[after + 1]["frame_id"]), start_timestamp_ns=350, end_timestamp_ns=350,
             missing_count=1, reason="synthetic loss", evidence_id="manual")]

    def test_declared_gap_breaks_both_temporal_metrics_and_duration(self):
        self.add_gap()
        self.policy["reacquisition_min_correct_frames"] = 6
        for i in (2, 3, 4):
            self.labels[i]["endpoint"] = "end"
        self.policy.update(endpoint_early_tolerance_ns=1000, endpoint_late_tolerance_ns=1000)
        self.predictions[3]["endpoint_stop"] = "true"
        result = self.result()
        self.assertEqual([e["status"] for e in result["reacquisition"]["episodes"]], ["not_confirmed", "not_confirmed"])
        self.assertEqual(result["reacquisition"]["episodes"][1]["kind"], "after_gap")
        self.assertEqual((result["endpoints"]["detected"], result["endpoints"]["not_detected"]), (1, 1))
        self.assertEqual(result["temporal_coverage"]["represented_span_ns"], 600)
        self.assertEqual(result["temporal_coverage"]["unrepresented_span_ns"], 100)
        self.assertEqual(result["temporal_coverage"]["capture_span_ns"], 700)
        self.assertEqual(result["decisions"][2]["represented_interval_ns"], 0)
        self.assertEqual(result["capture_counts"], {"acquired_count": 9, "stored_count": 8, "dropped_count": 1})

    def test_interframe_threshold_is_inclusive_and_one_ns_breaks(self):
        result = self.result()
        self.assertEqual(result["temporal_coverage"]["boundaries"], [])
        for i in range(3, 8):
            stamp = str(int(self.frames[i]["timestamp_ns"]) + 1)
            self.frames[i]["timestamp_ns"] = self.predictions[i]["timestamp_ns"] = stamp
        self.meta["sequence"]["actual"]["last_timestamp_ns"] += 1
        result = self.result()
        self.assertEqual(result["temporal_coverage"]["unrepresented_span_ns"], 101)
        self.assertEqual(result["temporal_coverage"]["boundaries"][0]["reasons"], ["interframe_limit"])

    def test_large_epoch_retains_exact_ns_delays_and_no_latency_claim(self):
        base = scorer.v.I64 - 1000
        for frame, pred in zip(self.frames, self.predictions):
            stamp = str(int(frame["timestamp_ns"]) + base)
            frame["timestamp_ns"] = pred["timestamp_ns"] = stamp
        self.meta["sequence"]["actual"]["first_timestamp_ns"] += base
        self.meta["sequence"]["actual"]["last_timestamp_ns"] += base
        self.endpoint_region()
        self.predictions[5]["endpoint_stop"] = "true"
        result = self.result()
        self.assertEqual(result["endpoints"]["events"][0]["entry_delay_ns"], 200)
        self.assertEqual(result["temporal_coverage"]["capture_span_ns"], 700)
        self.assertEqual(result["decisions"][0]["timestamp_ns"], base + 100)

    def test_cli_determinism_and_exit_codes(self):
        def run():
            return subprocess.run([sys.executable, str(SCRIPT), str(self.root), str(self.run_root)], capture_output=True, text=True)
        one, two = run(), run()
        self.assertEqual(one.returncode, 0, one.stderr)
        self.assertEqual(one.stdout, two.stdout)
        self.assertEqual(one.stderr, "")
        self.meta["annotation"]["review_status"] = "pending"
        self.save()
        result = run()
        self.assertEqual(result.returncode, 3)
        self.assertNotIn("metrics", json.loads(result.stdout))
        self.meta["annotation"]["review_status"] = "reviewed"
        self.predictions.pop()
        self.save()
        result = run()
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stderr, "")


if __name__ == "__main__":
    unittest.main()
