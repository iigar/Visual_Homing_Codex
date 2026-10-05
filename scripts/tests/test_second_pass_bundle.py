#!/usr/bin/env python3
"""Synthetic saved-bundle contracts. No matcher executable is needed or run."""
import copy
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import test_second_pass_exporter as t

SCRIPT = Path(__file__).resolve().parents[1] / "check-second-pass-bundle.py"
c = t.t.load("bundle_checker", SCRIPT)
e, s, v = c.e, c.s, c.v


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(root, peer=False):
    """Encode exporter v1 independently; binary contents are inert test data."""
    root.mkdir()
    meta, frames, labels = t.make_oracle(root / "dataset")
    peers = []
    if peer:
        peers = [root / "peers/0001"]
        t.t.fixtures.make_dataset(peers[0], name="peer", split="test", pixel_offset=40)
    e.write_json(root / "configuration.json", t.configuration())
    e.write_json(root / "policy.json", t.t.default_policy())
    (root / "implementation").mkdir()
    (root / "implementation/replay").write_bytes(b"INERT SYNTHETIC BINARY; must never be executed\n")
    (root / "toolchain").mkdir()
    for name in c.TOOLS:
        shutil.copyfile(SCRIPT.with_name(name), root / "toolchain" / name)
    (root / "manifest.csv").write_text("".join(f'{f["frame_id"]},{f["timestamp_ns"]},dataset/{f["path"]}\n' for f in frames))
    (root / "clock.txt").write_text("".join(f'{f["timestamp_ns"]}\n' for f in frames))
    validation = v.validate_datasets([root / "dataset", *peers])
    assert validation["status"] == "structurally_valid", validation
    inputs = {p.relative_to(root).as_posix(): digest(p) for p in root.rglob("*") if p.is_file()}
    config = t.configuration()
    plan = dict(schema_version=1, run_id="synthetic-saved-bundle", code_revision_declared="synthetic-not-a-build",
                input_sha256=inputs, timeout_seconds=300, disjoint_references=False, split_peer_count=len(peers),
                arguments=["implementation/replay", "dataset/" + meta["reference"]["path"], "manifest.csv", "clock.txt",
                           *[str(config[key]) for key in e.PARAMETERS]], environment=dict(platform="synthetic", python="3.12"),
                **c.PLAN_CONSTANTS)
    e.write_json(root / "plan.json", plan)
    rows = []
    for frame, index in zip(frames, [0, 1, None, 2, 0, None, 3, 1]):
        rows.append({**{k: frame[k] for k in ("sequence", "frame_id", "timestamp_ns")}, "observation": "observed",
                     "match_valid": "false" if index is None else "true", "reference_index": "" if index is None else str(index),
                     "navigation_command_valid": "false" if index is None else "true"})
    t.t.fixtures.write_csv(root / "predictions.csv", s.PREDICTION_FIELDS, rows)
    shutil.copyfile(root / "predictions.csv", root / "raw-predictions.csv")
    (root / "replay.log").write_text("Synthetic saved log; no process executed\n")
    pin = lambda name: {"path": name, "sha256": digest(root / name)}
    run = dict(schema_version=1, run_id=plan["run_id"], dataset_id=meta["dataset_id"],
               input_sha256=validation["datasets"][0]["input_sha256"], inputs_frozen_before_run=True,
               producer=dict(kind="external_export", code_revision=plan["code_revision_declared"],
                             implementation=pin("implementation/replay"), configuration=pin("configuration.json"),
                             state_reset_policy=c.STATE, preprocessing=c.PREPROCESSING),
               policy=pin("policy.json"), predictions=pin("predictions.csv"))
    e.write_json(root / "run.json", run)
    score = s.score(root / "dataset", root, peers)
    assert score["status"] == "scored", score
    e.write_json(root / "score.json", score)
    e.write_json(root / "execution.json", dict(status="complete", returncode=0, missing_reason=None,
                 plan_sha256=digest(root / "plan.json"), output_sha256={p: digest(root / p) for p in c.OUTPUTS}))


class BundleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="saved bundle tests ")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "bundle"
        fixture(self.root)

    def read(self, name):
        return json.loads((self.root / name).read_text())

    def write(self, name, value):
        e.write_json(self.root / name, value)

    def repin(self, inputs=(), outputs=(), rescore=False):
        plan = self.read("plan.json")
        for path in inputs:
            plan["input_sha256"][path] = digest(self.root / path)
        self.write("plan.json", plan)
        execution = self.read("execution.json")
        execution["plan_sha256"] = digest(self.root / "plan.json")
        if rescore:
            peers = [self.root / f"peers/{i:04d}" for i in range(1, plan["split_peer_count"] + 1)]
            self.write("score.json", s.score(self.root / "dataset", self.root, peers, plan["disjoint_references"]))
            outputs = [*outputs, "score.json"]
        for path in outputs:
            execution["output_sha256"][path] = digest(self.root / path)
        self.write("execution.json", execution)

    def predictions(self):
        with (self.root / "predictions.csv").open(newline="") as stream:
            return list(csv.DictReader(stream))

    def change_predictions(self, rows):
        t.t.fixtures.write_csv(self.root / "predictions.csv", s.PREDICTION_FIELDS, rows)
        shutil.copyfile(self.root / "predictions.csv", self.root / "raw-predictions.csv")
        run = self.read("run.json")
        run["predictions"]["sha256"] = digest(self.root / "predictions.csv")
        self.write("run.json", run)
        self.repin(outputs=["run.json", "predictions.csv", "raw-predictions.csv"], rescore=True)

    def failed(self, reason):
        execution = self.read("execution.json")
        execution.update(status="failed", missing_reason=reason,
                         returncode=7 if reason == "replay_failed" else 0 if reason == "replay_output_invalid" else None)
        if reason in ("replay_launch_failed", "replay_output_invalid"):
            execution["error"] = "Synthetic error"
        self.write("execution.json", execution)
        rows = self.predictions()
        for row in rows:
            row.update(observation="missing", missing_reason=reason)
            row.update({key: "" for key in s.PREDICTION_FIELDS[5:]})
        self.change_predictions(rows)
        (self.root / "raw-predictions.csv").write_bytes(b"Synthetic incomplete raw output\xff")
        self.repin(outputs=["raw-predictions.csv"])

    def result(self, status="invalid", message=None, **kwargs):
        result = c.check(self.root, **kwargs)
        self.assertEqual(result["status"], status, result)
        if message:
            self.assertIn(message, str(result["errors"]))
        self.assertFalse(result["binary_executed"])
        if status in ("invalid", "incomplete"):
            self.assertFalse(result["bundle_integrity_verified"])
            self.assertFalse(result["score_reproduced"])
            self.assertNotIn("score", result)
        return result

    def test_complete_oracle_read_only_deterministic_and_no_processes(self):
        before = {p.relative_to(self.root).as_posix(): (digest(p), p.stat().st_mtime_ns)
                  for p in self.root.rglob("*") if p.is_file()}
        with patch("subprocess.run", side_effect=AssertionError("must not execute")), \
             patch("subprocess.Popen", side_effect=AssertionError("must not execute")), \
             patch("os.system", side_effect=AssertionError("must not execute")):
            first = self.result("verified", expected_plan_sha256=digest(self.root / "plan.json"), expected_peer_ids=[])
            self.assertEqual(first, self.result("verified", expected_plan_sha256=digest(self.root / "plan.json"), expected_peer_ids=[]))
        metric = first["score"]["metrics"]
        self.assertEqual((metric["tp"], metric["fp"], metric["fn"]), (3, 2, 3))
        self.assertTrue(first["external_plan_anchor_checked"])
        self.assertTrue(first["peer_scope"]["external_inventory_checked"])
        self.assertFalse(first["chronology_authenticated"])
        self.assertFalse(first["semantic_independence_verified"])
        self.assertEqual(before, {p.relative_to(self.root).as_posix(): (digest(p), p.stat().st_mtime_ns)
                                for p in self.root.rglob("*") if p.is_file()})

    def test_relocated_bundle_ignores_only_dataset_root_location(self):
        old_root = self.read("score.json")["dataset_validation"]["datasets"][0]["root"]
        new_root = self.base / "relocated with spaces"
        shutil.copytree(self.root, new_root)
        self.root = new_root
        report = self.result("verified")
        self.assertNotEqual(report["score"]["dataset_validation"]["datasets"][0]["root"], old_root)
        saved = self.read("score.json")
        saved["dataset_validation"]["datasets"][0]["dataset_id"] = "different"
        self.write("score.json", saved)
        self.repin(outputs=["score.json"])
        self.result(message="saved/recomputed score")

    def test_embedded_peer_scope_and_external_inventory(self):
        self.root = self.base / "peer-bundle"
        fixture(self.root, peer=True)
        report = self.result("verified", expected_peer_ids=["peer"])
        self.assertEqual(report["peer_scope"]["peer_ids"], ["peer"])
        self.result(message="peer scope", expected_peer_ids=[])
        self.result(message="peer scope", expected_peer_ids=["other"])
        self.result(expected_peer_ids=["peer", "peer"])
        plan = self.read("plan.json")
        plan["disjoint_references"] = True
        self.write("plan.json", plan)
        self.repin()
        self.result(message="snapshot review/structure/split")

    def test_failed_runs_keep_zero_coverage_and_non_success_status(self):
        original = self.root
        for reason in ("replay_failed", "replay_timeout", "replay_launch_failed", "replay_output_invalid"):
            with self.subTest(reason=reason):
                self.root = self.base / reason
                shutil.copytree(original, self.root)
                self.failed(reason)
                result = self.result("replay_failed")
                self.assertTrue(result["bundle_integrity_verified"])
                self.assertTrue(result["score_reproduced"])
                self.assertEqual(result["score"]["metrics"]["fn"], 6)
                self.assertEqual(result["score"]["metrics"]["prediction_coverage"]["value"], 0)

    def test_incomplete_absent_or_partial_completion(self):
        (self.root / "execution.json").unlink()
        self.result("incomplete")
        (self.root / "failure.json").write_text("partial failure diagnostic")
        self.result("incomplete")
        (self.root / "failure.json").unlink()
        for content in ("", "{", "{}", "null"):
            (self.root / "execution.json").write_text(content)
            self.result()
        self.root = self.base / "absent"
        self.result()

    def test_conflicting_failure_marker_and_extra_files_or_directories(self):
        for name in ("failure.json", "notes.txt", "peers/0001/hidden.json"):
            with self.subTest(name=name):
                path = self.root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("unbound")
                self.result()
                path.unlink()
                if name.startswith("peers/"):
                    path.parent.rmdir()
                    path.parent.parent.rmdir()
        (self.root / "empty-peer").mkdir()
        self.result(message="unbound directory")

    def test_every_bound_artifact_detects_byte_tampering(self):
        names = ["plan.json", *self.read("plan.json")["input_sha256"], *c.OUTPUTS]
        for name in names:
            with self.subTest(name=name):
                path = self.root / name
                original = path.read_bytes()
                path.write_bytes(original + b"\n")
                self.result()
                path.write_bytes(original)

    def test_missing_or_extra_hash_bindings_and_forbidden_paths(self):
        original = self.read("plan.json")
        for name in ("configuration.json", "dataset/labels.csv", "toolchain/score-second-pass.py"):
            plan = copy.deepcopy(original)
            del plan["input_sha256"][name]
            self.write("plan.json", plan)
            self.repin()
            self.result()
        for name in ("../outside", "/absolute", "dataset//dataset.json", "dataset\\dataset.json", "execution.json"):
            plan = copy.deepcopy(original)
            plan["input_sha256"][name] = "0" * 64
            self.write("plan.json", plan)
            self.repin()
            self.result()

    def test_hash_types_and_execution_output_set_are_strict(self):
        original = self.read("execution.json")
        for bad in (None, True, "", "A" * 64, "0" * 63):
            value = copy.deepcopy(original)
            value["output_sha256"]["run.json"] = bad
            self.write("execution.json", value)
            self.result()
        for path in ("score.json", "unexpected"):
            value = copy.deepcopy(original)
            if path in value["output_sha256"]:
                del value["output_sha256"][path]
            else:
                value["output_sha256"][path] = "0" * 64
            self.write("execution.json", value)
            self.result()

    def test_symlinks_and_non_regular_files_are_rejected_without_opening(self):
        outside = self.base / "outside"
        outside.mkdir()
        (outside / "payload").write_text("outside")
        for directory in (False, True):
            link = self.root / "link"
            link.symlink_to(outside if directory else outside / "payload", target_is_directory=directory)
            self.result(message="symlinks")
            link.unlink()
        if hasattr(os, "mkfifo"):
            os.mkfifo(self.root / "pipe")
            self.result(message="regular file")

    def test_bundled_sources_are_never_imported_even_when_repinned(self):
        for name in c.TOOLS:
            with self.subTest(name=name):
                path = self.root / "toolchain" / name
                original = path.read_bytes()
                path.write_text("raise RuntimeError('BUNDLED CODE EXECUTED')\n")
                self.repin(inputs=["toolchain/" + name])
                self.result(message="pinned/trusted source mismatch")
                path.write_bytes(original)
                self.repin(inputs=["toolchain/" + name])

    def test_plan_schema_types_and_simulation_contract(self):
        original = self.read("plan.json")
        for key, value in (("schema_version", True), ("split_peer_count", -1), ("split_peer_count", 1001),
                           ("timeout_seconds", 0), ("disjoint_references", 0), ("clock", "wall_time"),
                           ("telemetry", "real"), ("state_reset_policy", "reset each frame"),
                           ("unsupported_outcomes", []), ("environment", {"python": "3"}), ("extra", 1)):
            with self.subTest(key=key, value=value):
                self.write("plan.json", {**original, key: value})
                self.repin()
                self.result()

    def test_arguments_manifest_and_clock_cannot_be_rebased_or_reordered(self):
        plan = self.read("plan.json")
        original = copy.deepcopy(plan)
        for index, value in ((0, "other-binary"), (1, "outside/route"), (4, "3"), (7, "0.01")):
            plan = copy.deepcopy(original)
            plan["arguments"][index] = value
            self.write("plan.json", plan)
            self.repin()
            self.result(message="plan.arguments")
        self.write("plan.json", original)
        for name in ("manifest.csv", "clock.txt"):
            path = self.root / name
            content = path.read_bytes()
            path.write_bytes(b"1" + content)
            self.repin(inputs=[name])
            self.result(message="manifest" if name == "manifest.csv" else "clock")
            path.write_bytes(content)
            self.repin(inputs=[name])
        (self.root / "clock.txt").write_bytes((self.root / "clock.txt").read_bytes().replace(b"\n", b"\r\n"))
        self.repin(inputs=["clock.txt"])
        self.result("verified")

    def test_config_run_and_plan_must_agree(self):
        config = self.read("configuration.json")
        config["minimum_confidence"] = 0.5
        self.write("configuration.json", config)
        self.repin(inputs=["configuration.json"])
        self.result(message="plan.arguments")
        self.write("configuration.json", t.configuration())
        self.repin(inputs=["configuration.json"])
        original = self.read("run.json")
        for key, value in (("run_id", "other"), ("inputs_frozen_before_run", 1), ("dataset_id", "other"), ("extra", 1)):
            self.write("run.json", {**original, key: value})
            self.repin(outputs=["run.json"])
            self.result(message="run/plan bindings")

    def test_execution_status_returncode_and_reason_agree(self):
        original = self.read("execution.json")
        for changes in ({"returncode": True}, {"returncode": 7}, {"missing_reason": "replay_failed"},
                        {"status": "failed"}, {"status": "pending"}, {"error": "unexpected"}):
            self.write("execution.json", {**original, **changes})
            self.result()
        self.write("execution.json", original)
        self.failed("replay_failed")
        execution = self.read("execution.json")
        execution["returncode"] = 0
        self.write("execution.json", execution)
        self.result(message="returncode/reason mismatch")

    def test_complete_cannot_have_unknown_navigation_or_supported_fake_gates(self):
        original = self.predictions()
        for key, value in (("navigation_command_valid", ""), ("endpoint_stop", "false"), ("published", "true")):
            rows = copy.deepcopy(original)
            rows[0][key] = value
            self.change_predictions(rows)
            self.result(message="unsupported or missing")

    def test_failed_cannot_retain_partial_observed_credit_or_wrong_missing_reason(self):
        observed = self.predictions()[0]
        self.failed("replay_failed")
        original = self.predictions()
        rows = copy.deepcopy(original)
        rows[0] = observed
        self.change_predictions(rows)
        self.result(message="every source frame")
        rows = copy.deepcopy(original)
        rows[0]["missing_reason"] = "different"
        self.change_predictions(rows)
        self.result(message="same explicit missing reason")

    def test_complete_raw_and_exported_predictions_must_be_identical(self):
        with (self.root / "raw-predictions.csv").open("ab") as stream:
            stream.write(b"\n")
        self.repin(outputs=["raw-predictions.csv"])
        self.result(message="differs from raw output")

    def test_saved_score_is_recomputed_with_type_sensitive_comparison(self):
        original = self.read("score.json")
        for kind in ("tp", "bool", "float", "extra", "root_type", "peer_scope"):
            saved = copy.deepcopy(original)
            if kind == "tp":
                saved["metrics"]["tp"] += 1
            elif kind == "bool":
                saved["semantic_independence_verified"] = 0
            elif kind == "float":
                saved["metrics"]["tp"] = float(saved["metrics"]["tp"])
            elif kind == "extra":
                saved["hidden"] = "unrecognized"
            elif kind == "root_type":
                saved["dataset_validation"]["datasets"][0]["root"] = None
            else:
                saved["dataset_validation"]["datasets"].append(saved["dataset_validation"]["datasets"][0])
            self.write("score.json", saved)
            self.repin(outputs=["score.json"])
            self.result()

    def test_review_and_split_validation_are_repeated(self):
        meta = self.read("dataset/dataset.json")
        meta["independence"]["review_status"] = "pending"
        self.write("dataset/dataset.json", meta)
        self.repin(inputs=["dataset/dataset.json"])
        self.result(message="snapshot review/structure/split")
        self.root = self.base / "split-leak"
        fixture(self.root, peer=True)
        peer = self.read("peers/0001/dataset.json")
        peer["query_session_id"] = "query-one"
        self.write("peers/0001/dataset.json", peer)
        self.repin(inputs=["peers/0001/dataset.json"])
        self.result(message="snapshot review/structure/split")

    def test_external_anchor_detects_consistent_rewriting(self):
        anchor = digest(self.root / "plan.json")
        plan = self.read("plan.json")
        run = self.read("run.json")
        plan["code_revision_declared"] = run["producer"]["code_revision"] = "rewritten-declaration"
        self.write("plan.json", plan)
        self.write("run.json", run)
        self.repin(outputs=["run.json"], rescore=True)
        self.result("verified")  # Hash consistency alone cannot authenticate history.
        self.result(message="external SHA-256 anchor", expected_plan_sha256=anchor)
        self.result(expected_plan_sha256="wrong-format")

    def test_changes_during_validation_and_rescore_are_not_repinned(self):
        original_score = c.s.score
        original_policy = (self.root / "policy.json").read_bytes()
        for kind in ("policy", "new_file"):
            def change(*args, **kwargs):
                result = original_score(*args, **kwargs)
                path = self.root / ("policy.json" if kind == "policy" else "new-file")
                with path.open("ab") as stream:
                    stream.write(b"\n")
                return result
            with patch.object(c.s, "score", side_effect=change):
                self.result()
            (self.root / "policy.json").write_bytes(original_policy)
        (self.root / "new-file").unlink()
        original_validate = c.v.validate_datasets
        def change_metadata(*args, **kwargs):
            validation = original_validate(*args, **kwargs)
            # Remove required shape after validation, before the checker rereads it.
            self.write("dataset/dataset.json", {})
            return validation
        with patch.object(c.v, "validate_datasets", side_effect=change_metadata):
            self.result(message="dataset reread")

    def test_json_and_size_limits(self):
        original = (self.root / "plan.json").read_bytes()
        for raw in (b'{"schema_version":1,"schema_version":1}', b'{"schema_version":NaN}', b'[]', b'\xff'):
            (self.root / "plan.json").write_bytes(raw)
            self.result()
        (self.root / "plan.json").write_bytes(original)
        with patch.object(c, "SCORE_BYTES", 10):
            self.result(message="size limit")
        with patch.object(c, "INVENTORY_LIMIT", 1):
            self.result(message="inventory limit")

    def test_cli_exit_codes_and_deterministic_stdout(self):
        args = [sys.executable, str(SCRIPT), str(self.root), "--expected-peer-ids"]
        first = subprocess.run(args, check=True, capture_output=True)
        second = subprocess.run(args, check=True, capture_output=True)
        self.assertEqual(first.stdout, second.stdout)
        self.assertEqual(json.loads(first.stdout)["status"], "verified")
        self.failed("replay_failed")
        result = subprocess.run(args, capture_output=True)
        self.assertEqual(result.returncode, 3)
        self.assertEqual(json.loads(result.stdout)["status"], "replay_failed")
        (self.root / "execution.json").unlink()
        self.assertEqual(subprocess.run(args, capture_output=True).returncode, 4)
        (self.root / "execution.json").write_text("{")
        self.assertEqual(subprocess.run(args, capture_output=True).returncode, 2)


if __name__ == "__main__":
    unittest.main()
