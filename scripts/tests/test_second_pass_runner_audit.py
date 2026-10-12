#!/usr/bin/env python3
"""Outer archive contracts using inert synthetic bundles; no replay execution."""
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

SCRIPT = Path(__file__).resolve().parents[1] / "check-second-pass-runner.py"
a = t.t.t.load("runner_audit", SCRIPT)
r, c, e, v = a.r, a.c, a.e, a.v


class RunnerAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="runner audit ")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "archive"
        self.bundle = self.root / "collection/bundles/0000"
        self.bundle.parent.mkdir(parents=True)
        t.fixture(self.bundle)
        self.helper = t.BundleTests()
        self.helper.root = self.bundle
        shutil.copytree(self.bundle / "dataset", self.root / "collection/datasets/0000")
        (self.root / "frozen/toolchain").mkdir(parents=True)
        for name in r.TOOLS:
            shutil.copyfile(SCRIPT.with_name(name), self.root / "frozen/toolchain" / name)
        shutil.copyfile(self.bundle / "implementation/replay", self.root / "frozen/replay")
        validation = v.validate_datasets([self.root / "collection/datasets/0000"])["datasets"][0]
        bundle_plan = self.helper.read("plan.json")
        self.request = dict(schema_version=1, collection_id="synthetic-runner-audit", disjoint_references=False,
                            datasets=[{key: validation[key] for key in ("dataset_id", "split", "input_sha256")} | {"path": "original"}],
                            runs=[dict(run_id=bundle_plan["run_id"], dataset_id="one", timeout_seconds=bundle_plan["timeout_seconds"],
                                       configuration=e.binding(self.bundle, "configuration.json"), policy=e.binding(self.bundle, "policy.json"))])
        self.state = dict(report_version=1, status="verified", errors=[], exporter_calls=1, output=str(self.root),
                          semantic_independence_verified=False, chronology_authenticated=False, runs=[
                              dict(run_id=bundle_plan["run_id"], dataset_id="one", path="bundles/0000", status="verified",
                                   attempted=True, export_status="exported", errors=[])])
        self.seal_inputs()
        self.manifest = r.request_contract(self.request)
        self.seal_bundle()
        self.publish()

    def read(self, name):
        return json.loads((self.root / name).read_text())

    def write(self, name, value):
        e.write_json(self.root / name, value)

    def seal_inputs(self):
        self.write("request.json", self.request)
        initial = r.request_contract(self.request)
        self.write("initial-manifest.json", initial)
        paths = ["request.json", "initial-manifest.json", "frozen/replay", *("frozen/toolchain/" + name for name in r.TOOLS)]
        paths += ["collection/" + row["path"] + "/" + path for row in initial["datasets"] for path in row["input_sha256"]]
        for index in range(len(self.request["runs"])):
            for key in ("configuration", "policy"):
                name = f"frozen/{index:04d}-{key}.json"
                if not (self.root / name).exists():
                    shutil.copyfile(self.bundle / (key + ".json"), self.root / name)
                paths.append(name)
        self.write("runner-plan.json", dict(schema_version=1, code_revision_declared="synthetic-not-a-build",
                   execution_policy=a.EXECUTION_POLICY, initial_manifest=initial,
                   request_sha256=t.digest(self.root / "request.json"), input_sha256={p: t.digest(self.root / p) for p in paths}))
        self.state["plan_sha256"] = t.digest(self.root / "runner-plan.json")

    def seal_bundle(self):
        self.manifest["runs"][0].update(plan_sha256=t.digest(self.bundle / "plan.json"),
                                        execution_sha256=t.digest(self.bundle / "execution.json"))

    def publish(self):
        self.write("collection/collection.json", self.manifest)
        digest = t.digest(self.root / "collection/collection.json")
        audit = c.check(self.root / "collection", digest)
        self.write("collection-report.json", audit)
        self.state.update(collection_status=audit["status"], manifest_sha256=digest,
                          collection_report_sha256=t.digest(self.root / "collection-report.json"))
        self.write("runner-state.json", self.state)

    def pending(self):
        row = copy.deepcopy(self.request["runs"][0])
        row["run_id"] = "pending-" + str(len(self.request["runs"]))
        self.request["runs"].append(row)
        initial = r.request_contract(self.request)
        self.manifest["runs"].append(initial["runs"][-1])
        self.state["runs"].append({key: initial["runs"][-1][key] for key in ("run_id", "dataset_id", "path")} |
                                  dict(status="pending", attempted=False, export_status=None, errors=[]))
        self.state.update(status="incomplete", errors=["interrupted; no automatic retry or resume"])
        self.seal_inputs()
        self.publish()

    def result(self, status="invalid", message=None, **kwargs):
        result = a.check(self.root, **kwargs)
        self.assertEqual(result["status"], status, result)
        if message:
            self.assertIn(message, json.dumps(result))
        for key in ("binary_executed", "semantic_independence_verified", "chronology_authenticated"):
            self.assertFalse(result[key])
        if not result["results_released"]:
            self.assertNotIn("collection", result)
            self.assertFalse(result["archive_integrity_verified"])
        return result

    def snapshot(self):
        return {p.relative_to(self.root).as_posix(): (t.digest(p), p.stat().st_mtime_ns)
                for p in self.root.rglob("*") if p.is_file()}

    def test_complete_deterministic_read_only_never_executes(self):
        before = self.snapshot()
        with patch("subprocess.run", side_effect=AssertionError("execution forbidden")), \
             patch("subprocess.Popen", side_effect=AssertionError("execution forbidden")), \
             patch("os.system", side_effect=AssertionError("execution forbidden")), \
             patch.object(r, "run", side_effect=AssertionError("runner forbidden")), \
             patch.object(e, "export", side_effect=AssertionError("export forbidden")):
            first = self.result("verified", expected_plan_sha256=t.digest(self.root / "runner-plan.json"),
                                expected_state_sha256=t.digest(self.root / "runner-state.json"))
            self.assertEqual(first, self.result("verified", expected_plan_sha256=first["plan_sha256"],
                                               expected_state_sha256=first["state_sha256"]))
        self.assertEqual(before, self.snapshot())
        self.assertTrue(first["saved_report_reproduced"])
        metric = first["collection"]["runs"][0]["bundle"]["score"]["metrics"]
        self.assertEqual((metric["tp"], metric["fp"], metric["fn"]), (3, 2, 3))

    def test_relocation_normalizes_only_display_paths(self):
        relocated = self.base / "relocated archive"
        shutil.copytree(self.root, relocated)
        self.assertEqual(a.check(relocated)["status"], "verified")
        self.state["output"] = "/unavailable/historical/output"
        self.write("runner-state.json", self.state)
        self.result("verified")

    def test_all_failed_replay_reasons_stay_failed(self):
        for reason in ("replay_failed", "replay_timeout", "replay_launch_failed", "replay_output_invalid"):
            self.helper.failed(reason)
            self.seal_bundle()
            self.state["runs"][0].update(status="replay_failed", export_status="replay_failed")
            self.state["status"] = "replay_failed"
            self.publish()
            result = self.result("replay_failed")
            self.assertEqual(result["collection"]["coverage"]["frame_run_pairs_by_status"]["replay_failed"], 8)

    def test_interrupted_and_partial_slots_keep_denominators(self):
        self.pending()
        self.pending()
        self.state["runs"][1].update(status="incomplete", attempted=True, export_status=None,
                                      errors=["attempt ended without a sealed result"])
        self.state["exporter_calls"] = 2
        self.publish()
        result = self.result("incomplete")
        self.assertTrue(result["archive_integrity_verified"])
        self.assertEqual(result["collection"]["coverage"]["expected_frame_run_pairs"], 24)
        self.assertEqual([r["status"] for r in result["progress"]], ["verified", "incomplete", "pending"])
        self.state["runs"][1].update(export_status="exception", errors=["disk failure"])
        partial = self.root / "collection/bundles/0001"
        partial.mkdir()
        (partial / "failure.json").write_text("untrusted partial data")
        self.publish()
        self.result("incomplete")

    def test_entire_catalog_and_embedded_peers_survive_relocation(self):
        fixture = self.base / "peer fixture"
        t.fixture(fixture, peer=True)
        shutil.copytree(fixture, self.bundle, dirs_exist_ok=True)
        peer_root = self.root / "collection/datasets/0001"
        shutil.copytree(self.bundle / "peers/0001", peer_root)
        peer = v.validate_datasets([peer_root])["datasets"][0]
        self.pending()
        self.request["datasets"].append({key: peer[key] for key in ("dataset_id", "split", "input_sha256")} | {"path": "original-peer"})
        self.request["runs"][1]["dataset_id"] = self.state["runs"][1]["dataset_id"] = peer["dataset_id"]
        self.seal_inputs()
        self.manifest = r.request_contract(self.request)
        self.seal_bundle()
        self.publish()
        result = self.result("incomplete")
        self.assertEqual(result["collection"]["coverage"]["expected_frame_run_pairs"], 11)
        self.assertEqual(result["collection"]["by_split"]["test"]["expected_runs"], 1)
        moved = self.base / "moved peers"
        shutil.copytree(self.root, moved)
        self.assertEqual(a.check(moved)["status"], "incomplete")
        self.manifest["runs"][0]["peer_ids"] = []
        self.publish()
        self.result("invalid", "final inventory/request")

    def test_running_state_can_lag_atomic_manifest_but_releases_no_results(self):
        self.state["status"] = "running"
        self.state["runs"][0].update(status="running", export_status=None)
        for key in a.AUDIT_FIELDS:
            self.state.pop(key)
        self.write("runner-state.json", self.state)
        result = self.result("incomplete")
        self.assertFalse(result["results_released"])
        self.manifest["runs"][0].update(plan_sha256=None, execution_sha256=None)
        self.write("collection/collection.json", self.manifest)
        (self.root / "collection-report.json").unlink()
        self.result("incomplete")

    def test_missing_control_files_are_incomplete_without_adoption(self):
        for name in ("runner-plan.json", "runner-state.json", "collection-report.json", "collection/collection.json"):
            with self.subTest(name=name):
                path = self.root / name
                content = path.read_bytes()
                path.unlink()
                self.assertFalse(self.result("incomplete")["results_released"])
                path.write_bytes(content)

    def test_all_known_temporaries_are_uncommitted_and_preserved(self):
        for name in a.TEMPORARIES:
            path = self.root / name
            path.write_bytes(b"partial json")
            before = self.snapshot()
            self.assertFalse(self.result("incomplete")["results_released"])
            self.assertEqual(before, self.snapshot())
            path.unlink()

    def test_preparation_without_plan_has_no_integrity_claim(self):
        empty = self.base / "preparation"
        empty.mkdir()
        result = a.check(empty)
        self.assertEqual(result["status"], "incomplete")
        self.assertFalse(result["archive_integrity_verified"])
        (empty / "request.json").write_text("malformed")
        self.assertEqual(a.check(empty)["status"], "invalid")

    def test_exact_request_initial_and_final_inventory(self):
        self.pending()
        original = copy.deepcopy(self.manifest)
        for mutate in (lambda m: m["runs"].reverse(), lambda m: m["runs"].pop(),
                       lambda m: m["runs"][0].update(peer_ids=["one"]),
                       lambda m: m.update(disjoint_references=True), lambda m: m.update(collection_id="changed")):
            self.manifest = copy.deepcopy(original)
            mutate(self.manifest)
            self.publish()
            self.result()

    def test_initial_and_embedded_manifest_must_derive_from_request(self):
        for name in ("initial-manifest.json", "runner-plan.json"):
            original = (self.root / name).read_bytes()
            data = self.read(name)
            target = data if name.startswith("initial") else data["initial_manifest"]
            target["runs"][0]["run_id"] = "changed"
            self.write(name, data)
            self.result("invalid", "initial manifest/request")
            (self.root / name).write_bytes(original)

    def test_plan_exact_schema_hash_map_and_execution_policy(self):
        original = self.read("runner-plan.json")
        for mutate in (lambda p: p.update(schema_version=True), lambda p: p.update(extra=1),
                       lambda p: p.update(execution_policy="retry"), lambda p: p.update(request_sha256="0" * 64),
                       lambda p: p["input_sha256"].pop("frozen/replay"),
                       lambda p: p["input_sha256"].update({"../escape": "0" * 64}),
                       lambda p: p["input_sha256"].update({"frozen/toolchain/run-second-pass-collection.py": "0" * 64})):
            plan = copy.deepcopy(original)
            mutate(plan)
            self.write("runner-plan.json", plan)
            self.result()

    def test_frozen_files_missing_corrupt_and_tool_mismatch(self):
        for name in ("request.json", "initial-manifest.json", "frozen/replay", "frozen/0000-policy.json",
                     "frozen/0000-configuration.json", "collection/datasets/0000/frames.csv",
                     "frozen/toolchain/run-second-pass-collection.py"):
            path = self.root / name
            content = path.read_bytes()
            path.write_bytes(content + b" ")
            self.result()
            path.unlink()
            self.result()
            path.write_bytes(content)

    def test_bundle_timeout_revision_and_split_must_match_request(self):
        original = self.helper.read("plan.json")
        for key, value in (("timeout_seconds", 17), ("code_revision_declared", "other-build"), ("disjoint_references", True)):
            plan = copy.deepcopy(original)
            plan[key] = value
            self.helper.write("plan.json", plan)
            if key == "code_revision_declared":
                run = self.helper.read("run.json")
                run["producer"]["code_revision"] = value
                self.helper.write("run.json", run)
            self.helper.repin(outputs=["run.json"], rescore=True)
            self.seal_bundle()
            self.publish()
            self.assertEqual(c.check(self.root / "collection")["status"], "verified")
            self.result("invalid", "bundle")
            # Restore producer for the next independent variant.
            run = self.helper.read("run.json")
            run["producer"]["code_revision"] = original["code_revision_declared"]
            self.helper.write("run.json", run)

    def test_internally_valid_wrong_policy_cannot_be_admitted(self):
        policy = self.helper.read("policy.json")
        policy["policy_id"] = "other-policy"
        self.helper.write("policy.json", policy)
        run = self.helper.read("run.json")
        run["policy"]["sha256"] = t.digest(self.bundle / "policy.json")
        self.helper.write("run.json", run)
        self.helper.repin(inputs=["policy.json"], outputs=["run.json"], rescore=True)
        self.seal_bundle()
        self.publish()
        self.assertEqual(c.check(self.root / "collection")["status"], "verified")
        self.result("invalid", "bundle differs from request")

    def test_frozen_binary_and_configuration_cannot_diverge_from_bundle(self):
        for name in ("frozen/replay", "frozen/0000-configuration.json"):
            original = (self.root / name).read_bytes()
            if name.endswith(".json"):
                data = self.read(name)
                data["minimum_confidence"] = 0.98
                self.write(name, data)
                self.request["runs"][0]["configuration"]["sha256"] = t.digest(self.root / name)
            else:
                (self.root / name).write_bytes(b"different inert binary")
            self.seal_inputs()
            self.publish()
            self.result("invalid", "bundle differs from request")
            (self.root / name).write_bytes(original)

    def test_progress_schema_types_identity_order_and_counts(self):
        self.pending()
        original = copy.deepcopy(self.state)
        cases = [lambda d: d.update(report_version=True), lambda d: d.update(exporter_calls=True),
                 lambda d: d.update(exporter_calls=2), lambda d: d.update(extra=1),
                 lambda d: d.update(semantic_independence_verified=0), lambda d: d.update(errors="error"),
                 lambda d: d["runs"].reverse(), lambda d: d["runs"].pop(),
                 lambda d: d["runs"][0].update(attempted=1), lambda d: d["runs"][0].update(status="other"),
                 lambda d: d["runs"][0].update(export_status={}),
                 lambda d: d["runs"][1].update(export_status="exported"),
                 lambda d: d["runs"][0].update(export_status="replay_failed"),
                 lambda d: d.pop("manifest_sha256"), lambda d: d.update(plan_sha256="0" * 64)]
        for mutate in cases:
            state = copy.deepcopy(original)
            mutate(state)
            self.write("runner-state.json", state)
            self.result()

    def test_attempted_prefix_cannot_skip_or_follow_running_slot(self):
        self.pending()
        self.pending()
        self.state["runs"][2].update(status="incomplete", attempted=True, export_status="exception")
        self.state["exporter_calls"] = 2
        self.write("runner-state.json", self.state)
        self.result("invalid", "ordered prefix")
        self.state["status"] = "running"
        self.state["runs"][1].update(status="running", attempted=True)
        self.state["exporter_calls"] = 3
        for key in a.AUDIT_FIELDS:
            self.state.pop(key)
        self.write("runner-state.json", self.state)
        self.result("invalid", "ordered prefix")

    def test_progress_cannot_adopt_unadmitted_or_missing_sealed_results(self):
        original = copy.deepcopy(self.state)
        self.state["status"] = "incomplete"
        self.state["runs"][0].update(status="incomplete", export_status="exception")
        self.publish()
        self.result("invalid", "unadmitted slot")
        self.state = original
        (self.bundle / "execution.json").unlink()
        self.publish()
        self.result("invalid", "sealed result missing")

    def test_invalid_and_incomplete_outcomes_never_become_success(self):
        self.state["status"] = "invalid"
        self.state["errors"] = ["recorded runner error"]
        self.publish()
        self.result("invalid", "runner recorded")
        self.state["status"] = "incomplete"
        self.publish()
        self.result("incomplete")
        self.state["errors"] = []
        self.publish()
        self.result("invalid", "inconsistent incomplete")
        self.state.update(status="verified", errors=["error cannot disappear"])
        self.publish()
        self.result("invalid", "success contains")

    def test_saved_report_hash_and_full_typed_semantic_equality(self):
        original = self.read("collection-report.json")
        for mutate in (lambda d: d.update(status="replay_failed"),
                       lambda d: d["coverage"].update(expected_runs=True),
                       lambda d: d["runs"][0]["bundle"]["score"]["metrics"].update(tp=4),
                       lambda d: d["runs"][0]["bundle"]["score"].update(extra="hidden"),
                       lambda d: d["dataset_validation"]["datasets"][0].update(root=17)):
            data = copy.deepcopy(original)
            mutate(data)
            self.write("collection-report.json", data)
            self.result("invalid", "SHA-256 mismatch")
            self.state["collection_report_sha256"] = t.digest(self.root / "collection-report.json")
            self.write("runner-state.json", self.state)
            self.result()

    def test_external_plan_and_state_anchors_detect_consistent_rewrites(self):
        plan_hash = t.digest(self.root / "runner-plan.json")
        state_hash = t.digest(self.root / "runner-state.json")
        self.request["collection_id"] = self.manifest["collection_id"] = "consistently rewritten"
        self.seal_inputs()
        self.publish()
        self.result("verified")
        self.result("invalid", "SHA-256 mismatch", expected_plan_sha256=plan_hash)
        self.result("invalid", "SHA-256 mismatch", expected_state_sha256=state_hash)
        self.result("invalid", expected_plan_sha256="bad")
        self.result("invalid", expected_state_sha256=True)

    def test_plan_anchor_alone_does_not_authenticate_resealed_outputs(self):
        plan_hash = t.digest(self.root / "runner-plan.json")
        state_hash = t.digest(self.root / "runner-state.json")
        (self.bundle / "replay.log").write_text("internally repinned output")
        self.helper.repin(outputs=["replay.log"])
        self.seal_bundle()
        self.publish()
        self.result("verified", expected_plan_sha256=plan_hash)
        self.result("invalid", "SHA-256 mismatch", expected_state_sha256=state_hash)

    def test_unlisted_outer_files_directories_and_collection_entries(self):
        for name, directory in (("extra", False), ("frozen/extra", False), ("frozen/unused", True),
                                ("collection/bundles/hidden", True), ("collection/extra", False)):
            path = self.root / name
            path.mkdir() if directory else path.write_text("extra")
            self.result("invalid", "unlisted")
            path.rmdir() if directory else path.unlink()

    def test_symlinks_and_special_files_are_rejected(self):
        path = self.root / "link"
        try:
            path.symlink_to(self.bundle, target_is_directory=True)
        except OSError:
            self.skipTest("symlink unavailable")
        self.result("invalid", "symlinks")
        path.unlink()
        if hasattr(os, "mkfifo"):
            os.mkfifo(path)
            self.result("invalid", "regular file")

    def test_json_duplicates_nonfinite_depth_and_size_limits(self):
        original_state = (self.root / "runner-state.json").read_bytes()
        for name in ("request.json", "runner-plan.json", "runner-state.json", "collection-report.json"):
            original = (self.root / name).read_bytes()
            for content in ('{"x":1,"x":1}', '{"x":NaN}', "[" * 1100):
                (self.root / name).write_text(content)
                if name == "collection-report.json":
                    self.state["collection_report_sha256"] = t.digest(self.root / name)
                    self.write("runner-state.json", self.state)
                self.result()
            (self.root / name).write_bytes(original)
            (self.root / "runner-state.json").write_bytes(original_state)
        (self.root / "runner-plan.json").write_text(" " * (4 * 1024 * 1024 + 1))
        self.result()

    def test_present_null_or_scalar_controls_are_invalid_not_missing(self):
        for name in ("request.json", "initial-manifest.json", "runner-plan.json", "runner-state.json", "collection/collection.json"):
            original = (self.root / name).read_bytes()
            for value in (None, [], True, 1, "invalid"):
                self.write(name, value)
                self.result()
            (self.root / name).write_bytes(original)

    def test_changes_during_audit_withhold_all_results(self):
        original = c.check
        for name in ("frozen/0000-policy.json", "runner-state.json", "collection-report.json", "extra",
                     "collection/bundles/0000/replay.log"):
            path = self.root / name
            content = path.read_bytes() if path.exists() else None
            def mutate(*args, **kwargs):
                result = original(*args, **kwargs)
                path.write_text("changed after audit")
                return result
            with patch.object(c, "check", side_effect=mutate):
                self.result()
            path.write_bytes(content) if content is not None else path.unlink()

    def test_trusted_source_change_during_check(self):
        original = e.binding
        count = 0
        def mutate(root, name):
            nonlocal count
            result = original(root, name)
            if name == SCRIPT.name:
                count += 1
                if count > 1:
                    result["sha256"] = "0" * 64
            return result
        with patch.object(e, "binding", side_effect=mutate):
            self.result("invalid", "trusted source changed")

    def test_cli_deterministic_status_codes(self):
        def cli(status):
            args = [sys.executable, str(SCRIPT), str(self.root)]
            first = subprocess.run(args, capture_output=True, check=False)
            second = subprocess.run(args, capture_output=True, check=False)
            self.assertEqual((first.returncode, first.stdout), (second.returncode, second.stdout))
            self.assertEqual(first.returncode, c.EXIT_CODES[status], first.stderr + first.stdout)
            self.assertEqual(json.loads(first.stdout)["status"], status)
        cli("verified")
        self.helper.failed("replay_failed")
        self.seal_bundle()
        self.state["status"] = "replay_failed"
        self.state["runs"][0].update(status="replay_failed", export_status="replay_failed")
        self.publish()
        cli("replay_failed")
        self.pending()
        cli("incomplete")
        (self.root / "frozen/replay").write_text("changed")
        cli("invalid")


if __name__ == "__main__":
    unittest.main()
