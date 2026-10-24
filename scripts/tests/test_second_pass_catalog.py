#!/usr/bin/env python3
"""Fixed-workload catalog scaling contracts with actual offline replay."""
import copy
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

import benchmark_second_pass_campaign as p

f, catalog = p.f, p.catalog
BINARY = None


def inputs_at(root, count):
    request = p.make_inputs(root, 8)
    p.resolution.apply_resolution(root, request, 32)
    return catalog.expand_catalog(root, request, count)


class CatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temporary = tempfile.TemporaryDirectory(prefix="catalog scaling tests ")
        cls.addClassCleanup(temporary.cleanup)
        cls.base, cls.cases = Path(temporary.name), {}
        for count in (3, 6, 12):
            inputs, archive = cls.base / f"inputs-{count}", cls.base / f"archive-{count}"
            request = inputs_at(inputs, count)
            state = f.r.run(inputs / "request.json", archive, BINARY, f.REVISION)
            p.require(state["status"] == "verified", str(state))
            report = f.a.check(archive)
            p.require(report["status"] == "verified", str(report))
            cls.cases[count] = inputs, archive, request, report

    def test_invalid_counts_precede_writes(self):
        with patch.object(catalog.shutil, "copytree", side_effect=AssertionError("invalid count copied inputs")):
            for count in (None, True, "6", 6.0, 0, 1, 2, 4, 9, 13):
                with self.subTest(count=count), self.assertRaises(RuntimeError):
                    catalog.expand_catalog(self.base / "invalid", {}, count)

    def test_three_dataset_inputs_are_unchanged(self):
        inputs = self.base / "original"
        request = p.make_inputs(inputs, 8)
        p.resolution.apply_resolution(inputs, request, 32)
        before = f.snapshot(inputs)
        self.assertEqual(catalog.expand_catalog(inputs, copy.deepcopy(request), 3), request)
        self.assertEqual(f.snapshot(inputs), before)

    def test_catalog_content_bindings_assignment_and_all_peers(self):
        original_inputs, _, original_request, _ = self.cases[3]
        for count, (inputs, _, request, _) in self.cases.items():
            self.assertEqual(len(request["datasets"]), count)
            self.assertEqual(len(request["runs"]), 12)
            validation = f.v.validate_datasets([inputs / row["path"] for row in request["datasets"]], True)
            self.assertEqual(validation["status"], "structurally_valid", validation)
            self.assertEqual(validation["cross_split_overlaps"], [])
            manifest = f.r.request_contract(request)
            for row, validated in zip(request["datasets"], validation["datasets"]):
                did, split = row["dataset_id"], row["split"]
                self.assertEqual(row["input_sha256"], validated["input_sha256"])
                actual = f.snapshot(inputs / row["path"])
                original = f.snapshot(original_inputs / split)
                self.assertEqual({k: v[0] for k, v in actual.items() if k != "dataset.json"},
                                 {k: v[0] for k, v in original.items() if k != "dataset.json"})
                meta = f.read(inputs / row["path"], "dataset.json")
                self.assertEqual(meta, {**f.read(original_inputs / split, "dataset.json"), "dataset_id": did})
                self.assertEqual(sum(run["dataset_id"] == did for run in request["runs"]), 12 // count)
            for run, original, frozen in zip(request["runs"], original_request["runs"], manifest["runs"]):
                self.assertEqual({k: v for k, v in run.items() if k != "dataset_id"},
                                 {k: v for k, v in original.items() if k != "dataset_id"})
                self.assertEqual(frozen["peer_ids"], [d["dataset_id"] for d in request["datasets"] if d["dataset_id"] != run["dataset_id"]])
                owner = next(d for d in request["datasets"] if d["dataset_id"] == run["dataset_id"])
                self.assertEqual(owner["split"], original["dataset_id"])

    def test_actual_results_equal_with_independent_confusion_counts(self):
        oracles = [p.verify_collection(report, request, archive, inputs, 8)
                   for inputs, archive, request, report in self.cases.values()]
        self.assertTrue(all(row == oracles[0] for row in oracles))
        self.assertEqual(oracles[0]["coverage"]["expected_frame_run_pairs"], 96)
        self.assertEqual([(row["tp"], row["fp"], row["fn"]) for row in oracles[0]["metrics"]],
                         [(3, 2, 3), (2, 2, 3), (0, 0, 6), (0, 0, 5)] * 3)
        for _, _, _, report in self.cases.values():
            self.assertTrue(all(row["expected_runs"] == 4 for row in report["collection"]["by_split"].values()))

    def test_unused_catalog_and_cross_split_leakage_prevent_launch(self):
        for kind in ("unused", "leakage"):
            inputs, output = self.base / kind, self.base / (kind + "-output")
            request = inputs_at(inputs, 6)
            if kind == "unused":
                request["runs"] = [r for r in request["runs"] if r["dataset_id"] != "development-copy-01"]
            else:
                root = inputs / "validation-copy-01"
                meta = f.read(root, "dataset.json")
                meta["capture_group_id"] = "group-development"
                f.e.write_json(root / "dataset.json", meta)
                binding = f.v.validate_datasets([root])["datasets"][0]["input_sha256"]
                next(d for d in request["datasets"] if d["dataset_id"] == "validation-copy-01")["input_sha256"] = binding
            f.e.write_json(inputs / "request.json", request)
            with patch.object(f.e, "export", side_effect=AssertionError("invalid request launched replay")):
                report = f.r.run(inputs / "request.json", output, BINARY, f.REVISION)
            self.assertEqual(report["status"], "invalid", report)
            self.assertEqual(report["exporter_calls"], 0)
            self.assertFalse(output.exists())

    def test_oracle_rejects_copy_coverage_split_groups_and_missing_peers(self):
        inputs, archive, request, original = self.cases[12]
        for kind in ("copy", "split", "extra", "peers"):
            report = copy.deepcopy(original)
            collection = report["collection"]
            if kind == "copy":
                collection["by_dataset"]["test-copy-03"]["expected_frame_run_pairs"] -= 8
            elif kind == "split":
                collection["by_split"]["test"]["expected_runs"] -= 1
            elif kind == "extra":
                collection["by_dataset"]["extra"] = {}
            else:
                collection["runs"][-1]["bundle"]["peer_scope"]["peer_ids"].pop()
            with self.subTest(kind=kind), self.assertRaises(RuntimeError):
                p.verify_collection(report, request, archive, inputs, 8)

    def test_largest_catalog_relocation_is_read_only(self):
        inputs, archive, request, report = self.cases[12]
        moved = self.base / "relocated"
        shutil.copytree(archive, moved)
        before = f.snapshot(moved)
        with patch.object(f.e.subprocess, "run", side_effect=AssertionError("audit executed child")), \
             patch.object(f.r, "run", side_effect=AssertionError("audit resumed runner")):
            checked = f.a.check(moved, p.digest(moved / "runner-plan.json"), p.digest(moved / "runner-state.json"))
        self.assertEqual(p.verify_collection(checked, request, moved, inputs, 8),
                         p.verify_collection(report, request, archive, inputs, 8))
        self.assertEqual(f.snapshot(moved), before)


if __name__ == "__main__":
    BINARY = Path(sys.argv.pop(1)).resolve()
    unittest.main()
