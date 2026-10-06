#!/usr/bin/env python3
"""Local collection orchestration tests with the actual offline C++ adapter."""
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import test_second_pass_exporter as t
import test_second_pass_bundle as bundle_tests

SCRIPT = Path(__file__).resolve().parents[1] / "run-second-pass-collection.py"
r = t.t.load("collection_runner", SCRIPT)
e, c, v = r.e, r.c, r.v
REAL_RUN = subprocess.run
BINARY = None


class RunnerTests(unittest.TestCase):
    def setUp(self):
        e.platform.platform()
        self.temp = tempfile.TemporaryDirectory(prefix="collection runner tests ")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.inputs = self.base / "inputs"
        self.inputs.mkdir()
        t.make_oracle(self.inputs / "one")
        e.write_json(self.inputs / "config.json", t.configuration())
        e.write_json(self.inputs / "policy.json", t.t.default_policy())
        self.request_path = self.inputs / "request.json"
        self.output = self.base / "result with spaces"
        self.request = dict(schema_version=1, collection_id="synthetic-runner", disjoint_references=False,
                            datasets=[self.dataset("one")], runs=[])
        for index in range(2):
            self.request["runs"].append(dict(run_id=f"oracle-{index}", dataset_id="one", timeout_seconds=1,
                                             configuration=e.binding(self.inputs, "config.json"), policy=e.binding(self.inputs, "policy.json")))

    def dataset(self, name):
        validated = v.validate_datasets([self.inputs / name])["datasets"][0]
        return {key: validated[key] for key in ("dataset_id", "split", "input_sha256")} | {"path": name}

    def save(self):
        e.write_json(self.request_path, self.request)

    def run_collection(self, status="verified", save=True):
        if save:
            self.save()
        result = r.run(self.request_path, self.output, BINARY, "synthetic-test-revision")
        self.assertEqual(result["status"], status, result)
        self.assertFalse(result["chronology_authenticated"])
        self.assertFalse(result["semantic_independence_verified"])
        return result

    def read(self, name):
        return json.loads((self.output / name).read_text())

    def snapshot(self, root):
        return {p.relative_to(root).as_posix(): (bundle_tests.digest(p), p.stat().st_mtime_ns)
                for p in root.rglob("*") if p.is_file()}

    def test_real_replay_complete_plan_before_first_child_and_source_readonly(self):
        self.save()
        before = self.snapshot(self.inputs)
        calls, plan_hashes = [], []
        def launch(args, **kwargs):
            index = len(calls)
            initial = self.read("initial-manifest.json")
            self.assertTrue(all(row["plan_sha256"] is None and row["execution_sha256"] is None for row in initial["runs"]))
            plan = self.read("runner-plan.json")
            e.verify_frozen(self.output, plan["input_sha256"])
            self.assertEqual(plan["initial_manifest"], initial)
            state = self.read("runner-state.json")
            self.assertEqual(state["runs"][index]["status"], "running")
            self.assertEqual(state["exporter_calls"], index + 1)
            manifest = self.read("collection/collection.json")
            self.assertEqual(sum(row["execution_sha256"] is not None for row in manifest["runs"]), index)
            plan_hashes.append(bundle_tests.digest(self.output / "runner-plan.json"))
            calls.append(args)
            return REAL_RUN(args, **kwargs)
        with patch.object(e.subprocess, "run", side_effect=launch):
            report = self.run_collection(save=False)
        self.assertEqual(len(calls), 2)
        self.assertEqual(plan_hashes, [report["plan_sha256"]] * 2)
        self.assertEqual(before, self.snapshot(self.inputs))
        self.assertEqual(report, self.read("runner-state.json"))
        audit = self.read("collection-report.json")
        self.assertEqual(audit["coverage"]["expected_frame_run_pairs"], 16)
        self.assertTrue(audit["coverage"]["all_runs_verified"])
        for row in audit["runs"]:
            metrics = row["bundle"]["score"]["metrics"]
            self.assertEqual((metrics["tp"], metrics["fp"], metrics["fn"]), (3, 2, 3))
        self.assertEqual(report["collection_report_sha256"], bundle_tests.digest(self.output / "collection-report.json"))

    def test_repeat_has_identical_predictions_and_checker_accepts_relocation(self):
        self.run_collection()
        first = [(self.output / f"collection/bundles/{i:04d}/predictions.csv").read_bytes() for i in range(2)]
        initial = (self.output / "runner-plan.json").read_bytes()
        moved = self.base / "moved collection"
        shutil.copytree(self.output / "collection", moved)
        self.assertEqual(c.check(moved)["status"], "verified")
        self.output = self.base / "repeat"
        self.run_collection()
        self.assertEqual(first, [(self.output / f"collection/bundles/{i:04d}/predictions.csv").read_bytes() for i in range(2)])
        self.assertEqual(initial, (self.output / "runner-plan.json").read_bytes())

    def test_original_inputs_can_change_after_freeze_without_changing_later_runs(self):
        calls = []
        def launch(args, **kwargs):
            if not calls:
                for path in (self.request_path, self.inputs / "config.json", self.inputs / "policy.json", self.inputs / "one/frames/0.pgm"):
                    path.write_text("original changed after freezing")
            calls.append(args)
            return REAL_RUN(args, **kwargs)
        with patch.object(e.subprocess, "run", side_effect=launch):
            self.run_collection()
        self.assertEqual(len(calls), 2)
        self.assertEqual(self.read("collection-report.json")["runs"][1]["bundle"]["score"]["metrics"]["tp"], 3)

    def add_peer(self):
        t.t.fixtures.make_dataset(self.inputs / "two", name="two", split="test", pixel_offset=40)
        self.request["datasets"].append(self.dataset("two"))
        self.request["runs"][1]["dataset_id"] = "two"

    def test_all_catalog_datasets_become_explicit_peers(self):
        self.add_peer()
        self.run_collection()
        audit = self.read("collection-report.json")
        self.assertEqual(audit["coverage"]["expected_frame_run_pairs"], 11)
        self.assertEqual([row["bundle"]["peer_scope"]["peer_ids"] for row in audit["runs"]], [["two"], ["one"]])

    def test_replay_failures_are_sealed_and_next_run_executes_once(self):
        for reason in ("replay_failed", "replay_timeout", "replay_launch_failed", "replay_output_invalid"):
            with self.subTest(reason=reason):
                self.output = self.base / reason
                calls = []
                def launch(args, **kwargs):
                    calls.append(args)
                    if len(calls) == 1:
                        if reason == "replay_launch_failed":
                            raise OSError("synthetic launch failure")
                        code = {"replay_failed": "raise SystemExit(7)", "replay_timeout": "import time; time.sleep(3)",
                                "replay_output_invalid": "print('malformed CSV')"}[reason]
                        return REAL_RUN([sys.executable, "-c", code], **kwargs)
                    return REAL_RUN(args, **kwargs)
                with patch.object(e.subprocess, "run", side_effect=launch):
                    report = self.run_collection("replay_failed")
                self.assertEqual(len(calls), 2)
                self.assertEqual([row["status"] for row in report["runs"]], ["replay_failed", "verified"])
                audit = self.read("collection-report.json")
                self.assertEqual(audit["coverage"]["expected_frame_run_pairs"], 16)
                self.assertEqual(audit["runs"][0]["bundle"]["score"]["metrics"]["missing_reasons"], {reason: 8})

    def test_exporter_exception_retains_partial_files_and_remaining_slot(self):
        export = e.export
        calls = []
        def injected(*args, **kwargs):
            calls.append(args)
            if len(calls) == 1:
                args[1].mkdir(parents=True)
                (args[1] / "partial.txt").write_text("retained")
                raise OSError("synthetic incomplete write")
            return export(*args, **kwargs)
        with patch.object(e, "export", side_effect=injected):
            report = self.run_collection("incomplete")
        self.assertEqual(len(calls), 2)
        self.assertEqual([row["status"] for row in report["runs"]], ["incomplete", "verified"])
        self.assertEqual((self.output / "collection/bundles/0000/partial.txt").read_text(), "retained")
        self.assertIsNone(self.read("collection/collection.json")["runs"][0]["execution_sha256"])

    def test_interrupt_preserves_completed_current_and_pending_slots(self):
        third = copy.deepcopy(self.request["runs"][0])
        third["run_id"] = "never-started"
        self.request["runs"].append(third)
        export, calls = e.export, []
        def interrupt(*args, **kwargs):
            calls.append(args)
            if len(calls) == 2:
                raise KeyboardInterrupt
            return export(*args, **kwargs)
        with patch.object(e, "export", side_effect=interrupt):
            report = self.run_collection("incomplete")
        self.assertEqual([row["status"] for row in report["runs"]], ["verified", "incomplete", "pending"])
        self.assertEqual(report["exporter_calls"], 2)
        self.assertEqual(self.read("collection-report.json")["coverage"]["expected_frame_run_pairs"], 24)
        before = self.snapshot(self.output)
        with patch.object(e, "export") as launch:
            self.run_collection("invalid")
            launch.assert_not_called()
        self.assertEqual(before, self.snapshot(self.output))

    def test_existing_output_and_input_overlap_are_rejected(self):
        self.output.mkdir()
        (self.output / "keep.txt").write_text("keep")
        with patch.object(e, "export") as launch:
            self.run_collection("invalid")
            self.assertEqual(list(self.output.iterdir()), [self.output / "keep.txt"])
            self.output = self.inputs / "nested"
            self.run_collection("invalid")
            self.assertFalse(self.output.exists())
            launch.assert_not_called()

    def test_symlink_output_and_source_escape_rejected(self):
        target = self.base / "target"
        target.mkdir()
        try:
            self.output.symlink_to(target, target_is_directory=True)
        except OSError:
            self.skipTest("symlink unavailable")
        self.run_collection("invalid")
        self.assertEqual(list(target.iterdir()), [])
        self.output = self.base / "different"
        self.request["runs"][0]["configuration"]["path"] = "../outside.json"
        self.run_collection("invalid")
        self.assertFalse(self.output.exists())

    def test_request_schema_ids_hashes_and_limits_fail_before_export(self):
        original = copy.deepcopy(self.request)
        cases = [lambda q: q.update(schema_version=True), lambda q: q.update(disjoint_references=0),
                 lambda q: q.update(extra=1), lambda q: q.update(datasets=[]), lambda q: q.update(runs=[]),
                 lambda q: q["runs"][0].update(timeout_seconds=True), lambda q: q["runs"][0].update(timeout_seconds=3601),
                 lambda q: q["runs"][1].update(run_id=q["runs"][0]["run_id"]),
                 lambda q: q["runs"][0].update(dataset_id="unknown"),
                 lambda q: q["runs"][0]["policy"].update(sha256=None),
                 lambda q: q["runs"][0]["configuration"].update(sha256="0" * 64),
                 lambda q: q["datasets"][0].update(split="test"),
                 lambda q: q["datasets"][0]["input_sha256"].pop("frames.csv"),
                 lambda q: q["datasets"][0].update(path="../one")]
        with patch.object(e, "export") as launch:
            for mutate in cases:
                self.request = copy.deepcopy(original)
                mutate(self.request)
                self.run_collection("invalid")
                self.assertFalse(self.output.exists())
            launch.assert_not_called()

    def test_malformed_request_json_and_size_are_rejected(self):
        for content in ('{"schema_version":1,"schema_version":1}', '{"x":NaN}', "[" * 1100, " " * (4 * 1024 * 1024 + 1)):
            self.request_path.write_text(content)
            self.run_collection("invalid", save=False)
            self.assertFalse(self.output.exists())

    def test_all_configurations_are_validated_before_first_export(self):
        bad = t.configuration()
        bad["minimum_confidence"] = 2
        e.write_json(self.inputs / "bad.json", bad)
        self.request["runs"][1]["configuration"] = e.binding(self.inputs, "bad.json")
        with patch.object(e, "export") as launch:
            self.run_collection("invalid")
            launch.assert_not_called()
        self.assertFalse(self.output.exists())

    def test_review_and_collection_split_policy_block_all_exports(self):
        path = self.inputs / "one/dataset.json"
        meta = json.loads(path.read_text())
        meta["annotation"]["review_status"] = "pending"
        e.write_json(path, meta)
        self.request["datasets"][0] = self.dataset("one")
        with patch.object(e, "export") as launch:
            self.run_collection("needs_review")
            self.assertFalse(self.output.exists())
            self.request["datasets"][0]["input_sha256"]["frames.csv"] = "0" * 64
            self.run_collection("invalid")
            meta["annotation"]["review_status"] = "reviewed"
            e.write_json(path, meta)
            self.request["datasets"][0] = self.dataset("one")
            self.add_peer()
            self.request["disjoint_references"] = True
            self.run_collection("invalid")
            launch.assert_not_called()
        self.assertFalse(self.output.exists())

    def test_changed_frozen_copy_is_not_repinned_before_launch(self):
        copy_checked = e.copy_checked
        def mutate(store, path, destination, expected):
            copy_checked(store, path, destination, expected)
            if destination.name == "0000-policy.json":
                destination.write_text("changed during freezing")
        with patch.object(e, "copy_checked", side_effect=mutate), patch.object(e, "export") as launch:
            self.run_collection("invalid")
            launch.assert_not_called()

    def test_changed_source_during_copy_is_rejected_before_launch(self):
        copy_checked = e.copy_checked
        def mutate(store, path, destination, expected):
            if destination.name == "0000-policy.json":
                (store.root / path).write_text("changed original")
            copy_checked(store, path, destination, expected)
        with patch.object(e, "copy_checked", side_effect=mutate), patch.object(e, "export") as launch:
            self.run_collection("invalid")
            launch.assert_not_called()

    def test_shared_frozen_or_control_drift_stops_later_launches(self):
        for target in ("frozen/0001-policy.json", "collection/datasets/0000/dataset.json", "collection/collection.json", "runner-state.json"):
            with self.subTest(target=target):
                self.output = self.base / ("drift-" + str(len(list(self.base.iterdir()))))
                calls = []
                def launch(args, **kwargs):
                    calls.append(args)
                    result = REAL_RUN(args, **kwargs)
                    (self.output / target).write_text("changed shared data")
                    return result
                with patch.object(e.subprocess, "run", side_effect=launch):
                    report = self.run_collection("invalid")
                self.assertEqual(len(calls), 1)
                self.assertEqual(report["runs"][1]["status"], "pending")
                if target == "runner-state.json":
                    self.assertEqual((self.output / target).read_text(), "changed shared data")

    def test_previously_sealed_bundle_drift_is_not_adopted(self):
        calls = []
        def launch(args, **kwargs):
            calls.append(args)
            result = REAL_RUN(args, **kwargs)
            if len(calls) == 2:
                (self.output / "collection/bundles/0000/replay.log").write_text("changed previous result")
            return result
        with patch.object(e.subprocess, "run", side_effect=launch):
            self.run_collection("invalid")
        self.assertEqual(c.check(self.output / "collection")["status"], "invalid")

    def test_internally_valid_bundle_with_other_policy_cannot_be_sealed(self):
        policy = t.t.default_policy()
        policy["policy_id"] = "not-predeclared"
        other = self.base / "other-policy.json"
        e.write_json(other, policy)
        export, calls = e.export, []
        def substitute(*args, **kwargs):
            calls.append(args)
            if len(calls) == 1:
                args = (*args[:4], other, *args[5:])
            return export(*args, **kwargs)
        with patch.object(e, "export", side_effect=substitute):
            report = self.run_collection("invalid")
        self.assertEqual(report["collection_status"], "incomplete")
        self.assertEqual([row["status"] for row in report["runs"]], ["invalid", "verified"])
        self.assertEqual(c.b.check(self.output / "collection/bundles/0000")["status"], "verified")
        self.assertIsNone(self.read("collection/collection.json")["runs"][0]["execution_sha256"])

    def test_trusted_source_drift_is_detected_before_first_export(self):
        binding, reads = e.binding, []
        def changed(root, name):
            result = binding(root, name)
            if name == SCRIPT.name:
                reads.append(name)
                if len(reads) > 1:
                    result["sha256"] = "0" * 64
            return result
        with patch.object(e, "binding", side_effect=changed), patch.object(e, "export") as launch:
            self.run_collection("invalid")
            launch.assert_not_called()

    def test_manifest_commit_failure_stops_later_exports(self):
        atomic = r.atomic_json
        def fail(root, name, value, previous=None):
            if name == "collection/collection.json" and previous is not None:
                raise OSError("synthetic disk full")
            return atomic(root, name, value, previous)
        with patch.object(r, "atomic_json", side_effect=fail):
            report = self.run_collection("invalid")
        self.assertEqual(report["exporter_calls"], 1)
        self.assertEqual([row["status"] for row in report["runs"]], ["incomplete", "pending"])
        self.assertTrue(all(row["execution_sha256"] is None for row in self.read("collection/collection.json")["runs"]))
        self.assertEqual(self.read("collection-report.json")["status"], "incomplete")

    def test_final_audit_cannot_hide_outer_frozen_input_mutation(self):
        checker, calls = c.check, []
        def mutate(*args, **kwargs):
            result = checker(*args, **kwargs)
            calls.append(result["status"])
            if len(calls) == 2:
                (self.output / "frozen/0000-policy.json").write_text("changed during final audit")
            return result
        with patch.object(c, "check", side_effect=mutate):
            report = self.run_collection("invalid")
        self.assertEqual(calls, ["incomplete", "verified"])
        self.assertNotIn("collection_report_sha256", report)
        self.assertEqual(self.read("runner-state.json")["status"], "invalid")

    def test_cli_success_existing_output_and_review_exit_codes(self):
        self.save()
        args = [sys.executable, str(SCRIPT), str(self.request_path), str(self.output), "--binary", str(BINARY),
                "--code-revision", "synthetic-cli-test"]
        result = REAL_RUN(args, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertEqual(json.loads(result.stdout)["status"], "verified")
        before = self.snapshot(self.output)
        result = REAL_RUN(args, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(before, self.snapshot(self.output))
        args[3] = str(self.base / "review-output")
        path = self.inputs / "one/dataset.json"
        meta = json.loads(path.read_text())
        meta["annotation"]["review_status"] = "pending"
        e.write_json(path, meta)
        self.request["datasets"][0] = self.dataset("one")
        self.save()
        result = REAL_RUN(args, capture_output=True, text=True)
        self.assertEqual(result.returncode, 5, result.stderr)

    def test_cli_failed_and_incomplete_statuses_preserve_all_runs(self):
        self.save()
        invalid_binary = self.base / "trusted-test-invalid-executable"
        invalid_binary.write_bytes(b"inert fixture: expected OS launch failure\n")
        args = [sys.executable, str(SCRIPT), str(self.request_path), str(self.output), "--binary", str(invalid_binary),
                "--code-revision", "synthetic-failure-test"]
        result = REAL_RUN(args, capture_output=True, text=True)
        self.assertEqual(result.returncode, 3, result.stderr + result.stdout)
        self.assertEqual([row["status"] for row in json.loads(result.stdout)["runs"]], ["replay_failed"] * 2)
        config = t.configuration()
        config.update(target_width=3, target_height=3)  # Valid config, incompatible reference geometry.
        e.write_json(self.inputs / "config.json", config)
        for row in self.request["runs"]:
            row["configuration"] = e.binding(self.inputs, "config.json")
        self.save()
        self.output = self.base / "incomplete-output"
        args[3], args[5] = str(self.output), str(BINARY)
        result = REAL_RUN(args, capture_output=True, text=True)
        self.assertEqual(result.returncode, 4, result.stderr + result.stdout)
        self.assertEqual([row["status"] for row in json.loads(result.stdout)["runs"]], ["incomplete"] * 2)
        self.assertEqual(self.read("collection-report.json")["coverage"]["expected_frame_run_pairs"], 16)


if __name__ == "__main__":
    BINARY = Path(sys.argv.pop(1)).resolve()
    unittest.main()
