#!/usr/bin/env python3
"""Synthetic collection contracts; archived executables are inert fixture data."""
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import test_second_pass_bundle as t

SCRIPT = Path(__file__).resolve().parents[1] / "check-second-pass-collection.py"
c = t.t.t.load("collection_checker", SCRIPT)
e, v = c.e, c.v


class CollectionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="collection tests ")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "collection"
        (self.root / "bundles").mkdir(parents=True)
        self.bundle = self.root / "bundles/first"
        t.fixture(self.bundle)
        self.helper = t.BundleTests()
        self.helper.root = self.bundle
        shutil.copytree(self.bundle / "dataset", self.root / "datasets/one")
        self.manifest = dict(schema_version=1, collection_id="synthetic-collection", disjoint_references=False,
                             datasets=[self.catalog_row("datasets/one")], runs=[self.run_row(self.bundle)])

    def catalog_row(self, path):
        validation = v.validate_datasets([self.root / path])["datasets"][0]
        return {"dataset_id": validation["dataset_id"], "split": validation["split"], "path": path,
                "input_sha256": validation["input_sha256"]}

    def run_row(self, root):
        plan = json.loads((root / "plan.json").read_text())
        run = json.loads((root / "run.json").read_text())
        peers = [json.loads((root / f"peers/{i:04d}/dataset.json").read_text())["dataset_id"]
                 for i in range(1, plan["split_peer_count"] + 1)]
        return dict(run_id=plan["run_id"], dataset_id=run["dataset_id"], path=root.relative_to(self.root).as_posix(),
                    plan_sha256=t.digest(root / "plan.json"), execution_sha256=t.digest(root / "execution.json"), peer_ids=peers)

    def save(self):
        e.write_json(self.root / "collection.json", self.manifest)

    def result(self, status="invalid", message=None, save=True, **kwargs):
        if save:
            self.save()
        result = c.check(self.root, **kwargs)
        self.assertEqual(result["status"], status, result)
        if message:
            self.assertIn(message, json.dumps(result))
        self.assertFalse(result["binary_executed"])
        self.assertFalse(result["semantic_independence_verified"])
        self.assertFalse(result["chronology_authenticated"])
        if not result["results_released"]:
            self.assertTrue(all("bundle" not in row for row in result["runs"]))
        return result

    def pending(self, did="one", name="pending"):
        row = dict(run_id=name, dataset_id=did, path="bundles/" + name,
                   plan_sha256=None, execution_sha256=None, peer_ids=[])
        self.manifest["runs"].append(row)
        return row

    def second_dataset(self, split="test", shared_queries=False):
        path = self.root / "datasets/two"
        if shared_queries:
            shutil.copytree(self.root / "datasets/one", path)
            meta = json.loads((path / "dataset.json").read_text())
            meta.update(dataset_id="two", query_session_id="query-two", capture_group_id="group-two", split=split)
            e.write_json(path / "dataset.json", meta)
        else:
            t.t.t.fixtures.make_dataset(path, name="two", split=split, pixel_offset=40)
        self.manifest["datasets"].append(self.catalog_row("datasets/two"))
        self.pending("two", "second")

    def test_complete_read_only_deterministic_no_execution(self):
        self.save()
        snapshot = lambda: {p.relative_to(self.root).as_posix(): (t.digest(p), p.stat().st_mtime_ns)
                            for p in self.root.rglob("*") if p.is_file()}
        before = snapshot()
        with patch("subprocess.run", side_effect=AssertionError("no execution")), \
             patch("subprocess.Popen", side_effect=AssertionError("no execution")), \
             patch("os.system", side_effect=AssertionError("no execution")):
            first = self.result("verified", save=False, expected_manifest_sha256=t.digest(self.root / "collection.json"))
            self.assertEqual(first, self.result("verified", save=False, expected_manifest_sha256=t.digest(self.root / "collection.json")))
        self.assertEqual(before, snapshot())
        self.assertTrue(first["external_manifest_anchor_checked"])
        self.assertTrue(first["coverage"]["all_runs_verified"])
        metrics = first["runs"][0]["bundle"]["score"]["metrics"]
        self.assertEqual((metrics["tp"], metrics["fp"], metrics["fn"]), (3, 2, 3))
        self.assertEqual(first["coverage"]["expected_frame_run_pairs"], 8)

    def test_missing_and_unsealed_runs_stay_in_coverage(self):
        self.pending()
        self.second_dataset()
        result = self.result("incomplete")
        self.assertEqual(result["coverage"]["runs_by_status"], dict(verified=1, replay_failed=0, incomplete=2, invalid=0))
        self.assertEqual(result["coverage"]["expected_frame_run_pairs"], 19)
        self.assertEqual(result["by_split"]["test"]["frame_run_pairs_by_status"]["incomplete"], 3)
        self.assertEqual(result["by_dataset"]["one"]["expected_runs"], 2)
        self.assertNotIn("bundle", result["runs"][1])

    def test_null_anchors_never_adopt_existing_results(self):
        original = copy.deepcopy(self.manifest)
        for field in ("execution_sha256", "both"):
            with self.subTest(field=field):
                self.manifest = copy.deepcopy(original)
                self.manifest["runs"][0]["execution_sha256"] = None
                if field == "both":
                    self.manifest["runs"][0]["plan_sha256"] = None
                result = self.result("incomplete")
                self.assertNotIn("bundle", result["runs"][0])

    def test_missing_pinned_completion_and_entire_bundle(self):
        (self.bundle / "execution.json").unlink()
        self.result("incomplete", "execution.json is absent")
        shutil.rmtree(self.bundle)
        self.result("incomplete", "plan.json is absent")

    def test_empty_partial_bundle_is_unassessed(self):
        row = self.pending()
        path = self.root / row["path"]
        path.mkdir()
        self.result("incomplete")
        (path / "failure.json").write_text("arbitrary untrusted partial data")
        result = self.result("incomplete")
        self.assertNotIn(row["path"] + "/failure.json", result["observed_sha256"])

    def test_all_failed_reasons_retain_missing_frames_and_nonzero_status(self):
        for reason in ("replay_failed", "replay_timeout", "replay_launch_failed", "replay_output_invalid"):
            with self.subTest(reason=reason):
                self.helper.failed(reason)
                self.manifest["runs"][0] = self.run_row(self.bundle)
                result = self.result("replay_failed")
                self.assertEqual(result["coverage"]["frame_run_pairs_by_status"]["replay_failed"], 8)
                metric = result["runs"][0]["bundle"]["score"]["metrics"]
                self.assertEqual(metric["prediction_coverage"]["numerator"], 0)
                self.assertEqual(metric["fn"], 6)

    def test_invalid_precedes_incomplete_and_other_rows_are_reported(self):
        self.pending()
        (self.bundle / "replay.log").write_text("tampered")
        result = self.result("invalid")
        self.assertTrue(result["catalog_verified"])
        self.assertEqual([r["status"] for r in result["runs"]], ["invalid", "incomplete"])
        self.assertEqual(result["coverage"]["expected_runs"], 2)

    def test_completion_anchor_rejects_internally_consistent_output_rewrite(self):
        (self.bundle / "replay.log").write_text("changed but internally repinned")
        self.helper.repin(outputs=["replay.log"])
        self.assertEqual(t.c.check(self.bundle)["status"], "verified")
        self.result("invalid", "SHA-256 mismatch")

    def test_external_manifest_anchor_rejects_resealing(self):
        self.save()
        anchor = t.digest(self.root / "collection.json")
        self.manifest["collection_id"] = "rewritten"
        self.result("invalid", "external manifest anchor", expected_manifest_sha256=anchor)

    def test_manifest_schema_types_ids_paths_and_hashes(self):
        original = copy.deepcopy(self.manifest)
        cases = [lambda m: m.update(schema_version=True), lambda m: m.update(disjoint_references=0),
                 lambda m: m.update(extra=1), lambda m: m.update(datasets=[]), lambda m: m.update(runs=[]),
                 lambda m: m["datasets"].append(copy.deepcopy(m["datasets"][0])),
                 lambda m: m["runs"].append(copy.deepcopy(m["runs"][0])),
                 lambda m: m["runs"][0].update(dataset_id="absent"),
                 lambda m: m["runs"][0].update(plan_sha256=None),
                 lambda m: m["runs"][0].update(execution_sha256=True),
                 lambda m: m["datasets"][0].update(input_sha256={"dataset.json": None}),
                 lambda m: m["runs"][0].update(peer_ids=["one"]),
                 lambda m: m["runs"][0].update(peer_ids=["absent"]),
                 lambda m: m["runs"][0].update(peer_ids=["absent", "absent"])]
        for mutate in cases:
            self.manifest = copy.deepcopy(original)
            mutate(self.manifest)
            self.result()
        for path in ("/absolute", "../escape", "a//b", "a/./b", "a\\b", "a:b", "a,b", "a\nb", "a/ b", ".",
                     "datasets/one", "DATASETS/ONE", "datasets/one/nested", "datasets", "collection.json/sub"):
            with self.subTest(path=path):
                self.manifest = copy.deepcopy(original)
                self.manifest["runs"][0]["path"] = path
                self.result()

    def test_catalog_binding_id_split_and_full_map(self):
        original = copy.deepcopy(self.manifest)
        for mutation in (lambda row: row.update(split="test"),
                         lambda row: row["input_sha256"].pop("frames.csv"),
                         lambda row: row["input_sha256"].update({"dataset.json": "0" * 64})):
            self.manifest = copy.deepcopy(original)
            mutation(self.manifest["datasets"][0])
            self.result()
        self.manifest = original
        self.manifest["datasets"][0]["dataset_id"] = "renamed"
        self.manifest["runs"][0]["dataset_id"] = "renamed"
        self.result("invalid", "ID/split")

    def test_every_catalog_dataset_requires_expected_run(self):
        self.second_dataset()
        self.manifest["runs"].pop()
        self.result("invalid", "every catalog dataset")

    def test_unlisted_files_directories_and_bundles(self):
        for path, directory in (("report.json", False), ("datasets/one/extra", False),
                                ("datasets/hidden", True), ("bundles/hidden", True)):
            with self.subTest(path=path):
                target = self.root / path
                target.mkdir() if directory else target.write_text("extra")
                self.result("invalid", "unlisted")
                target.rmdir() if directory else target.unlink()
        row = self.pending()
        (self.root / row["path"]).write_text("not a directory")
        self.result("invalid", "bundle path is a file")

    def test_symlinks_and_nonregular_files_even_in_partial_run(self):
        row = self.pending()
        path = self.root / row["path"]
        path.mkdir()
        link = path / "link"
        try:
            link.symlink_to(self.bundle, target_is_directory=True)
        except OSError:
            self.skipTest("symlink unavailable")
        self.result("invalid", "symlinks")
        link.unlink()
        if hasattr(os, "mkfifo"):
            os.mkfifo(link)
            self.result("invalid", "regular file")

    def test_cross_split_leakage_is_checked_beyond_embedded_peers(self):
        self.second_dataset(shared_queries=True)
        self.assertEqual(t.c.check(self.bundle)["status"], "verified")
        report = self.result("invalid", "query_pixels")
        self.assertFalse(report["results_released"])
        self.assertEqual(report["runs"], [])

    def test_shared_reference_policy_applies_to_whole_catalog(self):
        self.second_dataset()
        self.result("incomplete")
        self.manifest["disjoint_references"] = True
        self.result("invalid", "shared_reference")

    def test_catalog_review_blocks_all_run_assessment(self):
        path = self.root / "datasets/one/dataset.json"
        meta = json.loads(path.read_text())
        meta["annotation"]["review_status"] = "pending"
        e.write_json(path, meta)
        self.manifest["datasets"][0] = self.catalog_row("datasets/one")
        result = self.result("needs_review")
        self.assertEqual(result["runs"], [])
        self.assertNotIn("coverage", result)

    def test_embedded_dataset_must_equal_catalog_snapshot(self):
        path = self.root / "datasets/one/dataset.json"
        meta = json.loads(path.read_text())
        meta["annotation"]["revision"] = "different-valid-revision"
        e.write_json(path, meta)
        self.manifest["datasets"][0] = self.catalog_row("datasets/one")
        self.result("invalid", "embedded/catalog")

    def test_embedded_peers_are_named_and_bound_to_catalog(self):
        shutil.rmtree(self.bundle)
        t.fixture(self.bundle, peer=True)
        shutil.copytree(self.bundle / "peers/0001", self.root / "datasets/peer")
        self.manifest["datasets"].append(self.catalog_row("datasets/peer"))
        self.manifest["runs"][0] = self.run_row(self.bundle)
        self.pending("peer", "peer-run")
        self.result("incomplete")
        self.manifest["runs"][0]["peer_ids"] = []
        self.result("invalid", "peer scope")
        self.manifest["runs"][0]["peer_ids"] = ["peer"]
        path = self.root / "datasets/peer/dataset.json"
        meta = json.loads(path.read_text())
        meta["annotation"]["revision"] = "changed-peer"
        e.write_json(path, meta)
        self.manifest["datasets"][1] = self.catalog_row("datasets/peer")
        self.result("invalid", "embedded/catalog")

    def test_run_identity_is_bound(self):
        self.manifest["runs"][0]["run_id"] = "not-this-run"
        self.result("invalid", "run_id")

    def test_repeated_dataset_runs_keep_distinct_policies_and_denominators(self):
        repeated = self.root / "bundles/repeat"
        shutil.copytree(self.bundle, repeated)
        helper = t.BundleTests()
        helper.root = repeated
        plan, run, policy = (helper.read(name) for name in ("plan.json", "run.json", "policy.json"))
        plan["run_id"] = run["run_id"] = "repeated-with-other-policy"
        policy["policy_id"] = "distinct-policy"
        helper.write("policy.json", policy)
        run["policy"]["sha256"] = t.digest(repeated / "policy.json")
        helper.write("plan.json", plan)
        helper.write("run.json", run)
        helper.repin(inputs=["policy.json"], outputs=["run.json"], rescore=True)
        self.manifest["runs"].append(self.run_row(repeated))
        result = self.result("verified")
        self.assertEqual(result["coverage"]["expected_frame_run_pairs"], 16)
        self.assertNotIn("metrics", result)
        policies = [row["bundle"]["score"]["policy"]["policy_id"] for row in result["runs"]]
        self.assertNotEqual(*policies)
        helper.failed("replay_timeout")
        self.manifest["runs"][1] = self.run_row(repeated)
        result = self.result("replay_failed")
        self.assertEqual(result["coverage"]["frame_run_pairs_by_status"]["verified"], 8)
        self.assertEqual(result["coverage"]["frame_run_pairs_by_status"]["replay_failed"], 8)

    def test_trusted_tool_mutation_withholds_results(self):
        original = c.e.binding
        counts = {}
        def mutate(root, name):
            binding = original(root, name)
            if name == SCRIPT.name:
                counts[name] = counts.get(name, 0) + 1
                if counts[name] > 1:
                    binding["sha256"] = "0" * 64
            return binding
        with patch.object(c.e, "binding", side_effect=mutate):
            self.result("invalid", "trusted source changed")

    def test_mutation_after_bundle_check_removes_metrics(self):
        original = c.b.check
        def mutate(*args, **kwargs):
            result = original(*args, **kwargs)
            (self.root / "datasets/one/evidence/review.txt").write_text("changed after catalog validation")
            return result
        with patch.object(c.b, "check", side_effect=mutate):
            result = self.result("invalid", "SHA-256 mismatch")
        self.assertFalse(result["results_released"])
        self.assertNotIn("coverage", result)

    def test_manifest_and_membership_mutations_during_check(self):
        original = c.b.check
        for name in ("collection.json", "extra.json"):
            with self.subTest(name=name):
                def mutate(*args, **kwargs):
                    result = original(*args, **kwargs)
                    (self.root / name).write_text("changed")
                    return result
                with patch.object(c.b, "check", side_effect=mutate):
                    self.result("invalid")
                if name == "extra.json":
                    (self.root / name).unlink()

    def test_bundle_completion_mutation_during_check(self):
        original = c.b.check
        def mutate(*args, **kwargs):
            (self.bundle / "replay.log").write_text("changed")
            self.helper.repin(outputs=["replay.log"])
            return original(*args, **kwargs)
        with patch.object(c.b, "check", side_effect=mutate):
            self.result("invalid", "completion changed")

    def test_json_duplicates_limits_and_manifest_hash_format(self):
        path = self.root / "collection.json"
        for content in ('{"schema_version":1,"schema_version":1}', '{"x":NaN}', "[" * 1100,
                        " " * (4 * 1024 * 1024 + 1)):
            path.write_text(content)
            self.result("invalid", save=False)
        self.result("invalid", expected_manifest_sha256="bad")
        self.manifest["runs"] *= 4097
        self.result("invalid", "list size")

    def test_relocated_collection_and_cli_exit_codes(self):
        self.save()
        relocated = self.base / "relocated collection"
        shutil.copytree(self.root, relocated)
        self.assertEqual(c.check(relocated)["status"], "verified")
        def cli(status):
            self.save()
            args = [sys.executable, str(SCRIPT), str(self.root)]
            first = subprocess.run(args, capture_output=True, text=True, check=False)
            second = subprocess.run(args, capture_output=True, text=True, check=False)
            self.assertEqual((first.returncode, first.stdout), (second.returncode, second.stdout))
            self.assertEqual(first.returncode, c.EXIT_CODES[status], first.stderr + first.stdout)
            self.assertEqual(json.loads(first.stdout)["status"], status)
        cli("verified")
        self.helper.failed("replay_failed")
        self.manifest["runs"][0] = self.run_row(self.bundle)
        cli("replay_failed")
        self.pending()
        cli("incomplete")
        self.manifest["runs"][0]["execution_sha256"] = "0" * 64
        cli("invalid")
        path = self.root / "datasets/one/dataset.json"
        meta = json.loads(path.read_text())
        meta["annotation"]["review_status"] = "pending"
        e.write_json(path, meta)
        self.manifest["datasets"][0] = self.catalog_row("datasets/one")
        cli("needs_review")


if __name__ == "__main__":
    unittest.main()
