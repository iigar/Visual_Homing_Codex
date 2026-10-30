#!/usr/bin/env python3
"""Combined frame/catalog contracts with actual replay; no timing assertions."""
import copy
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

import benchmark_second_pass_campaign as p

f = p.f
BINARY = None


def inputs_at(root, frames):
    request = p.make_inputs(root, frames)
    p.resolution.apply_resolution(root, request, 32)
    return p.catalog.expand_catalog(root, request, 12)


class LargeArchiveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temporary = tempfile.TemporaryDirectory(prefix="large archive tests ")
        cls.addClassCleanup(temporary.cleanup)
        cls.base = Path(temporary.name)
        cls.inputs, cls.archive = cls.base / "inputs", cls.base / "archive"
        cls.request = inputs_at(cls.inputs, 64)
        state = f.r.run(cls.inputs / "request.json", cls.archive, BINARY, f.REVISION)
        p.require(state["status"] == "verified", str(state))
        cls.report = f.a.check(cls.archive)
        p.require(cls.report["status"] == "verified", str(cls.report))

    def test_composed_inputs_keep_references_settings_identities_and_block_content(self):
        small = self.base / "eight"
        request = inputs_at(small, 8)
        self.assertEqual(self.request["runs"], request["runs"])
        self.assertEqual(len(self.request["datasets"]), 12)
        for name in (*f.CONFIGURATIONS, *f.POLICIES):
            self.assertEqual((small / (name + ".json")).read_bytes(), (self.inputs / (name + ".json")).read_bytes())
        for item in self.request["datasets"]:
            root, original = self.inputs / item["path"], small / item["path"]
            meta, previous = f.read(root, "dataset.json"), f.read(original, "dataset.json")
            self.assertEqual({k: v for k, v in meta.items() if k != "sequence"},
                             {k: v for k, v in previous.items() if k != "sequence"})
            self.assertEqual(meta["sequence"]["stored_count"], 64)
            self.assertEqual(meta["capture"]["native_width"], 32)
            self.assertEqual(meta["capture"]["native_height"], 32)
            self.assertEqual(meta["reference"]["entry_count"], 4)
            self.assertEqual((root / meta["reference"]["path"]).read_bytes(),
                             (original / meta["reference"]["path"]).read_bytes())
            rows = p.resolution.rows(root / "frames.csv")
            labels = p.resolution.rows(root / "labels.csv")
            self.assertEqual(len(rows), 64)
            self.assertEqual(len(labels), 64)
            first_id, first_time = int(rows[0]["frame_id"]), int(rows[0]["timestamp_ns"])
            for index, (row, label) in enumerate(zip(rows, labels)):
                self.assertEqual((int(row["sequence"]), int(row["frame_id"]), int(row["timestamp_ns"])),
                                 (index, first_id + index, first_time + index * 100))
                self.assertEqual(label["frame_id"], row["frame_id"])
                self.assertEqual((root / row["path"]).read_bytes(), (original / f"frames/{index % 8}.pgm").read_bytes())
            same_split = f.read(self.inputs / item["split"], "dataset.json")
            self.assertEqual(meta, {**same_split, "dataset_id": item["dataset_id"]})

    def test_actual_confusion_counts_and_complete_catalog_split_coverage(self):
        oracle = p.verify_collection(self.report, self.request, self.archive, self.inputs, 64)
        self.assertEqual(oracle["coverage"]["expected_frame_run_pairs"], 768)
        self.assertEqual([(row["tp"], row["fp"], row["fn"]) for row in oracle["metrics"]],
                         [(24, 16, 24), (16, 16, 24), (0, 0, 48), (0, 0, 40)] * 3)
        collection = self.report["collection"]
        self.assertEqual([row["expected_frame_run_pairs"] for row in collection["by_dataset"].values()], [64] * 12)
        self.assertEqual([row["expected_frame_run_pairs"] for row in collection["by_split"].values()], [256] * 3)
        self.assertTrue(all(len(row["bundle"]["peer_scope"]["peer_ids"]) == 11 for row in collection["runs"]))

    def test_oracle_rejects_unscaled_copy_coverage_and_missing_peer(self):
        for kind in ("coverage", "peer"):
            report = copy.deepcopy(self.report)
            if kind == "coverage":
                report["collection"]["by_dataset"]["test-copy-03"]["expected_frame_run_pairs"] = 8
            else:
                report["collection"]["runs"][-1]["bundle"]["peer_scope"]["peer_ids"].pop()
            with self.subTest(kind=kind), self.assertRaises(RuntimeError):
                p.verify_collection(report, self.request, self.archive, self.inputs, 64)

    def test_relocation_reproduces_oracle_without_writes_or_execution(self):
        moved = self.base / "relocated"
        shutil.copytree(self.archive, moved)
        before = f.snapshot(moved)
        with patch.object(f.e.subprocess, "run", side_effect=AssertionError("audit executed child")), \
             patch.object(f.r, "run", side_effect=AssertionError("audit resumed runner")):
            report = f.a.check(moved, p.digest(moved / "runner-plan.json"), p.digest(moved / "runner-state.json"))
        self.assertEqual(p.verify_collection(report, self.request, moved, self.inputs, 64),
                         p.verify_collection(self.report, self.request, self.archive, self.inputs, 64))
        self.assertEqual(f.snapshot(moved), before)

    def test_large_archive_rejects_other_axes_before_output_creation(self):
        output = self.base / "invalid-output"
        for arguments in ([], ["--runs", "12"], ["--datasets", "12"], ["--resolution", "32"]):
            argv = [str(p.SCRIPT), "--binary", str(BINARY), "--output", str(output), "--large-archive", *arguments]
            with self.subTest(arguments=arguments), patch.object(sys, "argv", argv), self.assertRaisesRegex(RuntimeError, "requires --frames"):
                p.main()
            self.assertFalse(output.exists())


if __name__ == "__main__":
    BINARY = Path(sys.argv.pop(1)).resolve()
    unittest.main()
