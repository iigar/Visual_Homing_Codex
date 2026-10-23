#!/usr/bin/env python3
"""Linux scaling-probe oracles with a real local replay; no timing thresholds."""
import copy
import csv
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

import benchmark_second_pass_campaign as p

f = p.f
BINARY = None


class ScalingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temporary = tempfile.TemporaryDirectory(prefix="frame scaling tests ")
        cls.addClassCleanup(temporary.cleanup)
        cls.base = Path(temporary.name)
        cls.inputs, cls.archive = cls.base / "inputs", cls.base / "archive"
        cls.request = p.make_inputs(cls.inputs, 16)
        state = f.r.run(cls.inputs / "request.json", cls.archive, BINARY, f.REVISION)
        p.require(state["status"] == "verified", str(state))
        cls.report = f.a.check(cls.archive)
        p.require(cls.report["status"] == "verified", str(cls.report))

    def test_eight_frame_inputs_are_byte_identical_to_existing_fixture(self):
        old, new = self.base / "original", self.base / "eight"
        self.assertEqual(f.make_campaign(old), p.make_inputs(new, 8))
        self.assertEqual({k: v[0] for k, v in f.snapshot(old).items()},
                         {k: v[0] for k, v in f.snapshot(new).items()})

    def test_expanded_inputs_preserve_fixed_dimensions_and_explicit_identities(self):
        larger = self.base / "sixty-four"
        request = p.make_inputs(larger, 64)
        self.assertEqual(len(request["runs"]), 12)
        self.assertEqual(len(request["datasets"]), 3)
        validation = f.v.validate_datasets([larger / split for split in f.SPLITS], True)
        self.assertEqual(validation["status"], "structurally_valid", validation)
        for index, split in enumerate(f.SPLITS):
            root = larger / split
            with (root / "frames.csv").open(newline="") as stream:
                frames = list(csv.DictReader(stream))
            with (root / "labels.csv").open(newline="") as stream:
                labels = list(csv.DictReader(stream))
            self.assertEqual(len(frames), 64)
            self.assertEqual(len(labels), 64)
            self.assertEqual([int(row["sequence"]) for row in frames], list(range(64)))
            self.assertEqual([int(row["frame_id"]) for row in frames], list(range(1000 * (index + 1), 1000 * (index + 1) + 64)))
            self.assertEqual([int(row["timestamp_ns"]) for row in frames], [100000 * (index + 1) + 100 * n for n in range(64)])
            self.assertEqual([row["frame_id"] for row in frames], [row["frame_id"] for row in labels])
            for n, row in enumerate(frames):
                self.assertEqual(row["path"], f"frames/{n}.pgm")
                self.assertEqual((row["width"], row["height"]), ("2", "2"))
                self.assertEqual(p.digest(root / row["path"]), row["sha256"])
                self.assertEqual((root / row["path"]).read_bytes(), (self.inputs / split / f"frames/{n % 8}.pgm").read_bytes())
            meta = f.read(root, "dataset.json")
            self.assertEqual(meta["reference"], f.read(self.inputs / split, "dataset.json")["reference"])
            self.assertEqual(meta["sequence"]["stored_count"], 64)
            self.assertEqual(meta["sequence"]["actual"]["last_frame_id"], 1000 * (index + 1) + 63)
        for name in ("near", "exact", "inclusive", "clear"):
            self.assertEqual((larger / (name + ".json")).read_bytes(), (self.inputs / (name + ".json")).read_bytes())

    def test_frame_count_validation_precedes_fixture_creation(self):
        with patch.object(f, "make_campaign", side_effect=AssertionError("invalid count created inputs")):
            for value in (True, None, "16", 8.0, 0, 7, 9, 513, 520):
                with self.subTest(value=value), self.assertRaises(RuntimeError):
                    p.make_inputs(self.base / "invalid", value)

    def test_scaled_report_and_relocation_are_read_only_without_execution(self):
        expected = p.verify_collection(self.report, self.request, self.archive, self.inputs, 16)
        self.assertEqual(expected["coverage"]["expected_frame_run_pairs"], 192)
        # Hand-calculated counts per two blocks, independent of helper arithmetic.
        self.assertEqual([(row["tp"], row["fp"], row["fn"]) for row in expected["metrics"]],
                         [(6, 4, 6), (4, 4, 6), (0, 0, 12), (0, 0, 10)] * 3)
        moved = self.base / "relocated"
        shutil.copytree(self.archive, moved)
        before = f.snapshot(moved)
        with patch.object(f.e.subprocess, "run", side_effect=AssertionError("audit executed child")), \
             patch.object(f.r, "run", side_effect=AssertionError("audit resumed runner")):
            report = f.a.check(moved, p.digest(moved / "runner-plan.json"), p.digest(moved / "runner-state.json"))
        self.assertEqual(p.verify_collection(report, self.request, moved, self.inputs, 16), expected)
        self.assertEqual(f.snapshot(moved), before)

    def test_report_oracle_rejects_dropped_slots_wrong_counts_and_peers(self):
        for kind in ("slot", "tp", "frames", "positive", "negative", "coverage", "peers"):
            report = copy.deepcopy(self.report)
            collection = report["collection"]
            metric = collection["runs"][0]["bundle"]["score"]["metrics"]
            if kind == "slot":
                collection["runs"].pop()
            elif kind in ("tp", "frames"):
                metric[kind] += 1
            elif kind == "positive":
                metric["recall"]["denominator"] -= 1
            elif kind == "negative":
                metric["negative_prediction_coverage"]["denominator"] -= 1
            elif kind == "coverage":
                collection["coverage"]["expected_frame_run_pairs"] -= 16
            else:
                collection["runs"][0]["bundle"]["peer_scope"]["peer_ids"].pop()
            with self.subTest(kind=kind), self.assertRaises(RuntimeError):
                p.verify_collection(report, self.request, self.archive, self.inputs, 16)

    def test_frame_oracle_rejects_identity_prediction_and_gate_changes(self):
        path = self.archive / "collection/bundles/0000/predictions.csv"
        original = path.read_bytes()
        for field, value in (("frame_id", "99999"), ("timestamp_ns", "1"), ("reference_index", "3"),
                             ("observation", "missing"), ("match_valid", "false"), ("published", "true")):
            try:
                with path.open(newline="") as stream:
                    rows = list(csv.DictReader(stream))
                rows[8][field] = value  # First frame of the added block.
                f.t.t.fixtures.write_csv(path, f.e.s.PREDICTION_FIELDS, rows)
                with self.subTest(field=field), self.assertRaises(RuntimeError):
                    p.verify_collection(self.report, self.request, self.archive, self.inputs, 16)
            finally:
                path.write_bytes(original)


if __name__ == "__main__":
    BINARY = Path(sys.argv.pop(1)).resolve()
    unittest.main()
