#!/usr/bin/env python3
"""Actual offline replay campaigns; synthetic pixels/labels are not accuracy evidence."""
import csv
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import test_second_pass_exporter as t

AUDITOR = Path(__file__).resolve().parents[1] / "check-second-pass-runner.py"
a = t.t.load("campaign_auditor", AUDITOR)
r, c, e, v = a.r, a.c, a.e, a.v
REAL_RUN = subprocess.run
BINARY = None
SPLITS = ("development", "validation", "test")
CONFIGURATIONS = ("near", "exact")
POLICIES = ("inclusive", "clear")
MATCHES = (0, 1, None, 2, 0, None, 3, 1)
REFERENCES = ((20, 60, 100, 140), (80, 120, 160, 200), (200, 140, 80, 20), (10, 90, 170, 250))
FRAMES = 8
REVISION = "synthetic-campaign-declared-revision"


def read(root, name):
    return json.loads((root / name).read_text())


def snapshot(root):
    return {p.relative_to(root).as_posix(): (t.t.fixtures.digest(p.read_bytes()), p.stat().st_mtime_ns)
            for p in root.rglob("*") if p.is_file()}


def make_campaign(inputs):
    """Explicit 3 x 2 x 2 matrix with two non-equivalent scoring policies.

    Pixel permutations preserve distances but avoid exact cross-split copies.
    These are related synthetic fixtures, never independent physical captures.
    """
    inputs.mkdir(parents=True)
    request = dict(schema_version=1, collection_id="synthetic-three-split-campaign", disjoint_references=True,
                   datasets=[], runs=[])
    for name in CONFIGURATIONS:
        config = t.configuration()
        config["minimum_confidence"] = 0.99 if name == "near" else 1.0
        e.write_json(inputs / (name + ".json"), config)
    for name in POLICIES:
        policy = t.t.default_policy()
        policy.update(policy_id="synthetic-" + name, scorable_visibility=["clear", "degraded"] if name == "inclusive" else ["clear"])
        e.write_json(inputs / (name + ".json"), policy)
    for index, split in enumerate(SPLITS):
        root = inputs / split
        meta, frames, labels = t.make_oracle(root)
        meta.update(dataset_id=split, split=split, query_session_id="query-" + split, capture_group_id="group-" + split)
        meta["reference"]["recording_session_id"] = "reference-" + split
        meta["clock"]["clock_id"] = "synthetic-clock-" + split
        references = [row[index:] + row[:index] for row in REFERENCES]
        route = struct.pack("<4sHHBBHI", b"VHRS", 1, 16, 1, 0, 0, 4)
        for ref_index, pixels in enumerate(references):
            route += struct.pack("<QQhfHHBBHI", ref_index, 10 + ref_index, 0, 0.0, 2, 2, 1, 0, 0, 4) + bytes(pixels)
        (root / meta["reference"]["path"]).write_bytes(route)
        meta["reference"]["sha256"] = t.t.fixtures.digest(route)
        for frame_index, (frame, label, match) in enumerate(zip(frames, labels, MATCHES)):
            pixels = bytes(p + 1 for p in references[match]) if match is not None else bytes(
                [255 - index * 10 if frame_index == 2 else index * 10] * 4)
            data = b"P5\n2 2\n255\n" + pixels
            (root / frame["path"]).write_bytes(data)
            frame.update(frame_id=str(1000 * (index + 1) + frame_index),
                         timestamp_ns=str(100000 * (index + 1) + frame_index * 100), sha256=t.t.fixtures.digest(data))
            label["frame_id"] = frame["frame_id"]
        labels[6]["visibility"] = "degraded"
        meta["sequence"]["actual"] = dict(first_frame_id=int(frames[0]["frame_id"]), last_frame_id=int(frames[-1]["frame_id"]),
                                            first_timestamp_ns=int(frames[0]["timestamp_ns"]), last_timestamp_ns=int(frames[-1]["timestamp_ns"]))
        t.t.fixtures.save_dataset(root, meta, frames, labels)
        validated = v.validate_datasets([root])["datasets"][0]
        request["datasets"].append({key: validated[key] for key in ("dataset_id", "split", "input_sha256")} | {"path": split})
        for config in CONFIGURATIONS:
            for policy in POLICIES:
                request["runs"].append(dict(run_id=f"{split}-{config}-{policy}", dataset_id=split, timeout_seconds=2,
                                           configuration=e.binding(inputs, config + ".json"), policy=e.binding(inputs, policy + ".json")))
    e.write_json(inputs / "request.json", request)
    return request


def variants(requested):
    return Path(requested["configuration"]["path"]).stem, Path(requested["policy"]["path"]).stem


def expected_metrics(requested, failed=False):
    config, policy = variants(requested)
    # GT: six on-route positives, one off-route negative, one unknown.
    # Clear-only excludes the degraded correct match at frame 6.
    positives = 6 if policy == "inclusive" else 5
    return (0, 0, positives) if failed or config == "exact" else (positives - 3, 2, 3)


class CampaignTests(unittest.TestCase):
    def setUp(self):
        e.platform.platform()  # Warm the platform probe before child-failure mocks.
        self.temp = tempfile.TemporaryDirectory(prefix="second pass campaign ")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.inputs = self.base / "inputs"
        self.request = make_campaign(self.inputs)
        self.output = self.base / "archive"

    def run_campaign(self, status="verified"):
        report = r.run(self.inputs / "request.json", self.output, BINARY, REVISION)
        self.assertEqual(report["status"], status, report)
        self.assertEqual(report, read(self.output, "runner-state.json"))
        return report

    def audit(self, status, states):
        before = snapshot(self.output)
        plan = e.binding(self.output, "runner-plan.json")["sha256"]
        state = e.binding(self.output, "runner-state.json")["sha256"]
        with patch.object(e.subprocess, "run", side_effect=AssertionError("audit must not launch a process")), \
             patch.object(r, "run", side_effect=AssertionError("audit must not resume")):
            report = a.check(self.output, plan, state)
            self.assertEqual(report, a.check(self.output, plan, state))
        self.assertEqual(report["status"], status, report)
        self.assertTrue(report["archive_integrity_verified"])
        self.assertTrue(report["saved_report_reproduced"])
        self.assertTrue(report["results_released"])
        for flag in ("binary_executed", "semantic_independence_verified", "chronology_authenticated"):
            self.assertFalse(report[flag])
        collection = report["collection"]
        self.assertEqual([row["status"] for row in collection["runs"]], states)
        self.assertNotIn("metrics", collection)  # No pooled accuracy across policies/configs.
        groups = [(collection["coverage"], states)]
        for index, split in enumerate(SPLITS):
            split_states = states[index * 4:(index + 1) * 4]
            groups.extend((collection[key][split], split_states) for key in ("by_dataset", "by_split"))
        for coverage, expected in groups:
            counts = {key: expected.count(key) for key in c.RUN_STATES}
            self.assertEqual(coverage, dict(expected_runs=len(expected), runs_by_status=counts,
                expected_frame_run_pairs=FRAMES * len(expected), frame_run_pairs_by_status={key: FRAMES * n for key, n in counts.items()},
                all_runs_verified=all(state == "verified" for state in expected)))
        for index, (requested, checked) in enumerate(zip(self.request["runs"], collection["runs"])):
            self.assertEqual((checked["run_id"], checked["dataset_id"], checked["path"]),
                             (requested["run_id"], requested["dataset_id"], f"bundles/{index:04d}"))
            if checked["status"] == "incomplete":
                self.assertNotIn("bundle", checked)
                continue
            bundle = checked["bundle"]
            self.assertEqual(bundle["peer_scope"]["peer_ids"], [split for split in SPLITS if split != requested["dataset_id"]])
            self.assertTrue(bundle["peer_scope"]["disjoint_references"])
            score = bundle["score"]
            metric = score["metrics"]
            failed = checked["status"] == "replay_failed"
            self.assertEqual(tuple(metric[k] for k in ("tp", "fp", "fn")), expected_metrics(requested, failed))
            config, policy = variants(requested)
            positives = 6 if policy == "inclusive" else 5
            self.assertEqual(metric["recall"]["denominator"], positives)
            self.assertEqual(metric["prediction_coverage"], dict(numerator=0 if failed else 8, denominator=8, value=0 if failed else 1))
            self.assertEqual(metric["unscorable_fraction"]["numerator"], 1 if policy == "inclusive" else 2)
            self.assertEqual(score["policy"]["policy_id"], "synthetic-" + policy)
            root = self.output / "collection" / checked["path"]
            with (root / "predictions.csv").open(newline="") as stream:
                predictions = list(csv.DictReader(stream))
            with (self.inputs / requested["dataset_id"] / "frames.csv").open(newline="") as stream:
                source = list(csv.DictReader(stream))
            self.assertEqual((len(predictions), len(source)), (FRAMES, FRAMES))
            for frame_index, (prediction, frame) in enumerate(zip(predictions, source)):
                for key in ("sequence", "frame_id", "timestamp_ns"):
                    self.assertEqual(prediction[key], frame[key])
                for key in c.b.UNSUPPORTED:
                    self.assertEqual(prediction[key], "")
                expected_index = MATCHES[frame_index] if config == "near" and not failed else None
                self.assertEqual(prediction["reference_index"], "" if expected_index is None else str(expected_index))
                self.assertEqual(prediction["observation"], "missing" if failed else "observed")
                self.assertEqual(prediction["match_valid"], "" if failed else "true" if expected_index is not None else "false")
                self.assertEqual(prediction["navigation_command_valid"], prediction["match_valid"])
        self.assertEqual(before, snapshot(self.output))
        return report

    def test_complete_cli_repeat_and_relocation_match_matrix_oracles(self):
        before = snapshot(self.inputs)
        validation = v.validate_datasets([self.inputs / split for split in SPLITS], True)
        self.assertEqual(validation["status"], "structurally_valid", validation)
        args = [sys.executable, str(r.SCRIPT), str(self.inputs / "request.json"), str(self.output),
                "--binary", str(BINARY), "--code-revision", REVISION]
        process = REAL_RUN(args, capture_output=True, text=True, check=False)
        self.assertEqual(process.returncode, 0, process.stderr + process.stdout)
        self.assertEqual(json.loads(process.stdout)["exporter_calls"], 12)
        first = self.audit("verified", ["verified"] * 12)
        initial_plan = (self.output / "runner-plan.json").read_bytes()
        predictions = [(self.output / f"collection/bundles/{i:04d}/predictions.csv").read_bytes() for i in range(12)]
        for index in range(0, 12, 4):
            self.assertEqual(predictions[index], predictions[index + 1])  # Policy changes scores, not replay.
            self.assertEqual(predictions[index + 2], predictions[index + 3])
            self.assertNotEqual(predictions[index], predictions[index + 2])  # Configuration really changes outcomes.
        moved = self.base / "relocated archive"
        shutil.copytree(self.output, moved)
        self.output = moved
        relocated = self.audit("verified", ["verified"] * 12)
        self.assertEqual(first["plan_sha256"], relocated["plan_sha256"])
        self.output = self.base / "repeat"
        calls = []
        def launch(args, **kwargs):
            index = len(calls)
            plan = read(self.output, "runner-plan.json")
            e.verify_frozen(self.output, plan["input_sha256"])
            self.assertEqual(plan["initial_manifest"], r.request_contract(self.request))
            self.assertEqual((self.output / "runner-plan.json").read_bytes(), initial_plan)
            state = read(self.output, "runner-state.json")
            self.assertEqual(state["exporter_calls"], index + 1)
            self.assertEqual([row["status"] for row in state["runs"]], ["verified"] * index + ["running"] + ["pending"] * (11 - index))
            calls.append(args)
            return REAL_RUN(args, **kwargs)
        with patch.object(e.subprocess, "run", side_effect=launch):
            self.run_campaign()
        self.assertEqual(len(calls), 12)
        repeated = self.audit("verified", ["verified"] * 12)
        self.assertEqual([row["bundle"]["score"]["metrics"] for row in first["collection"]["runs"]],
                         [row["bundle"]["score"]["metrics"] for row in repeated["collection"]["runs"]])
        self.assertEqual(predictions, [(self.output / f"collection/bundles/{i:04d}/predictions.csv").read_bytes() for i in range(12)])
        self.assertEqual(before, snapshot(self.inputs))

    def test_failed_children_keep_slots_and_policy_denominators(self):
        before = snapshot(self.inputs)
        calls = []
        def launch(args, **kwargs):
            index = len(calls)
            calls.append(args)
            code = {1: "raise SystemExit(7)", 6: "print('malformed replay CSV')"}.get(index)
            return REAL_RUN([sys.executable, "-c", code] if code else args, **kwargs)
        with patch.object(e.subprocess, "run", side_effect=launch):
            self.run_campaign("replay_failed")
        self.assertEqual(len(calls), 12)
        states = ["replay_failed" if i in (1, 6) else "verified" for i in range(12)]
        report = self.audit("replay_failed", states)
        for index, reason in ((1, "replay_failed"), (6, "replay_output_invalid")):
            metric = report["collection"]["runs"][index]["bundle"]["score"]["metrics"]
            self.assertEqual(metric["missing_reasons"], {reason: 8})
        self.assertEqual(before, snapshot(self.inputs))

    def test_partial_export_then_interrupt_preserves_attempts_and_future_slots(self):
        before = snapshot(self.inputs)
        export, calls = e.export, []
        def injected(*args, **kwargs):
            index = len(calls)
            calls.append(args[6])
            if index == 1:
                args[1].mkdir(parents=True)
                (args[1] / "partial.txt").write_text("controlled partial exporter output")
                raise OSError("controlled campaign export failure")
            if index == 5:
                raise KeyboardInterrupt
            return export(*args, **kwargs)
        with patch.object(e, "export", side_effect=injected):
            state = self.run_campaign("incomplete")
        self.assertEqual(calls, [row["run_id"] for row in self.request["runs"][:6]])
        expected = ["verified", "incomplete", "verified", "verified", "verified", "incomplete"] + ["pending"] * 6
        self.assertEqual([row["status"] for row in state["runs"]], expected)
        self.assertEqual(state["exporter_calls"], 6)
        self.audit("incomplete", ["incomplete" if status == "pending" else status for status in expected])
        archived = snapshot(self.output)
        with patch.object(e, "export", side_effect=AssertionError("no resume")):
            self.assertEqual(r.run(self.inputs / "request.json", self.output, BINARY, REVISION)["status"], "invalid")
        self.assertEqual(archived, snapshot(self.output))
        self.assertEqual(before, snapshot(self.inputs))

    def test_cross_split_leakage_and_review_stop_entire_campaign_before_output(self):
        root = self.inputs / "test"
        path = root / "dataset.json"
        original = path.read_bytes()
        meta = read(root, "dataset.json")
        meta["independence"]["review_status"] = "pending"
        e.write_json(path, meta)
        self.request["datasets"][2]["input_sha256"] = v.validate_datasets([root])["datasets"][0]["input_sha256"]
        e.write_json(self.inputs / "request.json", self.request)
        with patch.object(e, "export", side_effect=AssertionError("no partial campaign launch")):
            self.assertEqual(r.run(self.inputs / "request.json", self.output, BINARY, REVISION)["status"], "needs_review")
        self.assertFalse(self.output.exists())
        path.write_bytes(original)
        shutil.copyfile(self.inputs / "development/frames/0.pgm", root / "frames/0.pgm")
        with (root / "frames.csv").open(newline="") as stream:
            frames = list(csv.DictReader(stream))
        frames[0]["sha256"] = e.binding(root, "frames/0.pgm")["sha256"]
        t.t.fixtures.write_csv(root / "frames.csv", v.FRAME_FIELDS, frames)
        self.request["datasets"][2]["input_sha256"] = v.validate_datasets([root])["datasets"][0]["input_sha256"]
        e.write_json(self.inputs / "request.json", self.request)
        with patch.object(e, "export", side_effect=AssertionError("no partial campaign launch")):
            report = r.run(self.inputs / "request.json", self.output, BINARY, REVISION)
        self.assertEqual(report["status"], "invalid", report)
        self.assertIn("query_pixels", json.dumps(report))
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    BINARY = Path(sys.argv.pop(1)).resolve()
    unittest.main()
