#!/usr/bin/env python3
"""Read-only audit of a saved collection runner archive; never resumes replay.

See docs/SECOND_PASS_RUNNER_AUDIT_UA.md for completion and trust boundaries.
"""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import sys


SCRIPT = Path(__file__).resolve()
spec = importlib.util.spec_from_file_location("trusted_collection_runner", SCRIPT.with_name("run-second-pass-collection.py"))
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)
c, e, s, v, b = r.c, r.e, r.s, r.v, r.c.b
TOOLS = (SCRIPT.name, *r.TOOLS)
CONTROLS = {"runner-state.json", "collection/collection.json", "collection-report.json"}
TEMPORARIES = {name + ".tmp" for name in CONTROLS | {"runner-plan.json", "initial-manifest.json"}}
AUDIT_FIELDS = {"collection_status", "manifest_sha256", "collection_report_sha256"}
EXECUTION_POLICY = "manifest order; one exporter call per run; no implicit retry or resume"


def diagnostics(value, where):
    v.require(type(value) is list and all(type(item) is str for item in value), where, "expected diagnostic strings")


def state_contract(state, initial):
    v.require(type(state) is dict, "state", "expected object")
    present = AUDIT_FIELDS & state.keys()
    v.require(not present or present == AUDIT_FIELDS, "state", "partial audit bindings")
    fields = "report_version status errors runs exporter_calls semantic_independence_verified chronology_authenticated output plan_sha256"
    v.obj(state, fields + (" " + " ".join(sorted(AUDIT_FIELDS)) if present else ""), "state")
    v.integer(state["report_version"], "report_version", 1, 1)
    v.choice(state["status"], "running verified replay_failed incomplete invalid", "state.status")
    for key in ("semantic_independence_verified", "chronology_authenticated"):
        b.same(state[key], False, key)
    v.string(state["output"], "state.output")  # Historical display path, never followed.
    v.sha256(state["plan_sha256"], "state.plan_sha256")
    diagnostics(state["errors"], "state.errors")
    v.require(type(state["runs"]) is list and len(state["runs"]) == len(initial["runs"]), "state", "run count differs from request")
    attempted, pending, running = 0, False, False
    for row, expected in zip(state["runs"], initial["runs"]):
        v.obj(row, "run_id dataset_id path status attempted export_status errors", "state.run")
        for key in ("run_id", "dataset_id", "path"):
            b.same(row[key], expected[key], "state.run." + key)
        v.choice(row["status"], "pending running verified replay_failed incomplete invalid", "state.run.status")
        v.boolean(row["attempted"], "attempted")
        diagnostics(row["errors"], "state.run.errors")
        v.require(row["export_status"] is None or row["export_status"] in
                  ("exported", "replay_failed", "exception", "invalid", "needs_review"), "export_status", "unknown status")
        v.require(row["attempted"] == (row["status"] != "pending"), "state", "attempt/status mismatch")
        if row["attempted"]:
            v.require(not pending and not running, "state", "attempts must form an ordered prefix with running last")
            attempted += 1
        else:
            pending = True
        if row["status"] in ("pending", "running"):
            v.require(row["export_status"] is None and row["errors"] == [], "state", "uncommitted attempt has results")
        if row["status"] == "running":
            running = True
            v.require(state["status"] == "running", "state", "final state contains running attempt")
        if row["status"] in ("verified", "replay_failed", "invalid"):
            allowed = {"verified": ("exported",), "replay_failed": ("replay_failed",), "invalid": ("exported", "replay_failed")}
            v.require(row["export_status"] in allowed[row["status"]], "state", "admission/export status mismatch")
    v.integer(state["exporter_calls"], "exporter_calls", 0, len(initial["runs"]))
    v.require(state["exporter_calls"] == attempted, "state", "exporter call count mismatch")
    if present:
        v.require(state["status"] != "running", "state", "running state has final audit bindings")
        v.choice(state["collection_status"], "verified replay_failed incomplete invalid needs_review", "collection_status")
        for key in ("manifest_sha256", "collection_report_sha256"):
            v.sha256(state[key], "state." + key)
    return bool(present)


def portable_report(report, initial):
    """Normalize only documented dataset root display fields, never diagnostics."""
    normalized = copy.deepcopy(b.portable_score(report, [d["path"] for d in initial["datasets"]]))
    rows = normalized.get("runs")
    v.require(type(rows) is list and len(rows) == len(initial["runs"]), "saved report", "run scope mismatch")
    for row, expected in zip(rows, initial["runs"]):
        v.require(type(row) is dict, "saved report.run", "expected object")
        if "bundle" in row:
            bundle = row["bundle"]
            v.require(type(bundle) is dict and "score" in bundle, "saved report.bundle", "missing score")
            prefixes = ["dataset", *(f"peers/{i:04d}" for i in range(1, len(expected["peer_ids"]) + 1))]
            bundle["score"] = b.portable_score(bundle["score"], prefixes)
    return normalized


def check(archive_root, expected_plan_sha256=None, expected_state_sha256=None):
    report = dict(report_version=1, status="invalid", errors=[], archive_integrity_verified=False,
                  saved_report_reproduced=False, results_released=False, binary_executed=False,
                  semantic_independence_verified=False, chronology_authenticated=False)
    try:
        anchors = {"runner-plan.json": expected_plan_sha256, "runner-state.json": expected_state_sha256}
        for name, digest in anchors.items():
            if digest is not None:
                v.sha256(digest, "expected " + name)
        trusted = {name: e.binding(SCRIPT.parent, name)["sha256"] for name in TOOLS}
        store = v.Dataset(archive_root)
        inventory = b.inventory(store.root)
        files, directories = inventory

        def finish():
            e.verify_frozen(store.root, dict(store.inputs))
            v.require(b.inventory(store.root) == inventory, "archive", "membership changed during check")
            for name, digest in trusted.items():
                v.require(e.binding(SCRIPT.parent, name)["sha256"] == digest, "toolchain", "trusted source changed during check")
            report.update(observed_sha256=dict(store.inputs), toolchain_sha256=trusted,
                          external_plan_anchor_checked=expected_plan_sha256 is not None and "runner-plan.json" in files,
                          external_state_anchor_checked=expected_state_sha256 is not None and "runner-state.json" in files)
            return report

        for name, digest in anchors.items():
            if digest is not None and name in files:
                store.artifact({"path": name, "sha256": digest}, "external anchor")
        # A preparation interrupted before the plan is not an auditable snapshot.
        # Parse present controls even here so malformed JSON is never completion.
        controls = {name: s.read_json(store, name) for name in
                    ("request.json", "initial-manifest.json", "runner-plan.json", "runner-state.json", "collection/collection.json") if name in files}
        initial = r.request_contract(controls["request.json"]) if "request.json" in controls else None
        if "runner-plan.json" not in controls:
            report.update(status="incomplete", errors=["runner plan is absent; preparation is not verified"])
            return finish()
        v.require(initial is not None, "request", "missing request snapshot")
        plan = controls["runner-plan.json"]
        v.obj(plan, "schema_version code_revision_declared initial_manifest input_sha256 request_sha256 execution_policy", "runner plan")
        v.integer(plan["schema_version"], "plan.schema_version", 1, 1)
        v.string(plan["code_revision_declared"], "code_revision_declared")
        b.same(plan["execution_policy"], EXECUTION_POLICY, "execution policy")
        b.same(plan["initial_manifest"], initial, "plan initial manifest/request")
        v.require("initial-manifest.json" in controls, "initial manifest", "missing snapshot")
        b.same(controls["initial-manifest.json"], initial, "initial manifest/request")
        hashes = b.hash_map(plan["input_sha256"], "plan inputs")
        expected = {"request.json": store.inputs["request.json"], "initial-manifest.json": store.inputs["initial-manifest.json"]}
        v.sha256(plan["request_sha256"], "request_sha256")
        v.require(plan["request_sha256"] == expected["request.json"], "plan", "request hash mismatch")
        v.require("frozen/replay" in hashes, "plan", "binary binding absent")
        expected["frozen/replay"] = hashes["frozen/replay"]
        expected.update({"frozen/toolchain/" + name: trusted[name] for name in r.TOOLS})
        for dataset in initial["datasets"]:
            expected.update({"collection/" + dataset["path"] + "/" + path: digest for path, digest in dataset["input_sha256"].items()})
        request = controls["request.json"]
        for index, row in enumerate(request["runs"]):
            for key in ("configuration", "policy"):
                expected[f"frozen/{index:04d}-{key}.json"] = row[key]["sha256"]
        s.check_hashes(hashes, expected, "runner frozen inputs")
        for path, digest in hashes.items():
            store.artifact({"path": path, "sha256": digest}, "frozen input")
        for index in range(len(request["runs"])):
            name = f"frozen/{index:04d}-configuration.json"
            _, digest = e.read_config(store.path(name))
            v.require(digest == hashes[name], name, "configuration changed")
            name = f"frozen/{index:04d}-policy.json"
            s.read_policy(store, {"path": name, "sha256": hashes[name]})
        allowed = set(hashes) | CONTROLS | TEMPORARIES | {"runner-plan.json"}
        parents = {p.as_posix() for name in allowed for p in Path(name).parents if p != Path(".")}
        v.require(all(path in allowed or path.startswith("collection/") for path in files), "inventory", "unlisted archive file")
        v.require(all(path in parents or path.startswith("collection/") for path in directories), "inventory", "unlisted archive directory")
        v.require(not directories.intersection(allowed), "inventory", "file path is a directory")
        # A temp file is uncommitted data. Preserve/hash it without parsing or adopting it.
        for name in files & TEMPORARIES:
            store.artifact(e.binding(store.root, name), "uncommitted temporary")
        state = controls.get("runner-state.json")
        final = state_contract(state, initial) if "runner-state.json" in controls else False
        if state is not None:
            v.require(state["plan_sha256"] == store.inputs["runner-plan.json"], "state", "runner plan hash mismatch")
        manifest = controls.get("collection/collection.json")
        if "collection/collection.json" in controls:
            c.manifest_contract(manifest)
            unsealed = copy.deepcopy(manifest)
            for row in unsealed["runs"]:
                v.require((row["plan_sha256"] is None) == (row["execution_sha256"] is None), "manifest", "runner seals both anchors together")
                row.update(plan_sha256=None, execution_sha256=None)
            b.same(unsealed, initial, "final inventory/request")
            c.closed_inventory({p.removeprefix("collection/") for p in files if p.startswith("collection/") and p not in TEMPORARIES},
                               {p.removeprefix("collection/") for p in directories if p.startswith("collection/")}, manifest)
        for name, key in (("collection/collection.json", "manifest_sha256"), ("collection-report.json", "collection_report_sha256")):
            if final and name in files:
                store.artifact({"path": name, "sha256": state[key]}, "state audit anchor")
        if "collection-report.json" in files:
            saved = json.loads(store.small("collection-report.json", b.SCORE_BYTES), object_pairs_hook=v.unique_object,
                               parse_constant=v.invalid_constant, parse_float=v.finite_float)
            v.require(type(saved) is dict, "saved report", "expected object")
        if manifest is None:
            report.update(status="incomplete", errors=["collection manifest is absent; results withheld"])
            return finish()
        if files & TEMPORARIES:
            report.update(status="invalid" if state is not None and state["status"] == "invalid" else "incomplete",
                          errors=["uncommitted control temporary exists; results withheld"])
            return finish()
        audit = c.check(store.root / "collection", store.inputs["collection/collection.json"])
        v.require(audit["catalog_verified"] and audit["status"] != "invalid", "collection", str(audit["errors"]) or "invalid catalog or run")
        for path, digest in audit["observed_sha256"].items():
            store.artifact({"path": "collection/" + path, "sha256": digest}, "audited collection")
        for index, (row, checked) in enumerate(zip(manifest["runs"], audit["runs"])):
            if "bundle" in checked:
                bundle = checked["bundle"]
                b.same(bundle["peer_scope"]["disjoint_references"], request["disjoint_references"], "bundle split policy")
                bundle_plan = s.read_json(store, "collection/" + row["path"] + "/plan.json")
                for key, value in (("code_revision_declared", plan["code_revision_declared"]),
                                   ("timeout_seconds", request["runs"][index]["timeout_seconds"])):
                    b.same(bundle_plan[key], value, "bundle/request " + key)
                for name, digest in (("implementation/replay", hashes["frozen/replay"]),
                                     ("configuration.json", request["runs"][index]["configuration"]["sha256"]),
                                     ("policy.json", request["runs"][index]["policy"]["sha256"])):
                    v.require(bundle_plan["input_sha256"][name] == digest, name, "bundle differs from request")
            if state is not None:
                progress = state["runs"][index]["status"]
                if progress in ("verified", "replay_failed"):
                    v.require(checked["status"] == progress, "progress", "sealed result missing or differs from state")
                elif progress != "running":
                    v.require(row["plan_sha256"] is None, "progress", "unadmitted slot has sealed anchors")
        if state is None or not final or "collection-report.json" not in files:
            report.update(status="invalid" if state is not None and state["status"] == "invalid" else "incomplete",
                          errors=["final state/report transaction is incomplete; results withheld"])
            return finish()
        b.same(state["collection_status"], audit["status"], "state/collection status")
        b.same(portable_report(saved, initial), portable_report(audit, initial), "saved/recomputed collection report")
        statuses = {row["status"] for row in state["runs"]}
        if state["status"] in ("verified", "replay_failed"):
            v.require(statuses <= {"verified", "replay_failed"} and not state["errors"], "state", "success contains unfinished attempts or errors")
            b.same(state["status"], audit["status"], "final status")
        elif state["status"] == "incomplete":
            v.require("invalid" not in statuses and (state["errors"] or statuses & {"pending", "incomplete"}), "state", "inconsistent incomplete status")
        report.update(status=state["status"])
        finish()  # No result is released until all outer and inner bytes are rechecked.
        if state["status"] != "invalid":
            report.update(archive_integrity_verified=True, saved_report_reproduced=True, results_released=True,
                          collection=audit, progress=state["runs"], exporter_calls=state["exporter_calls"],
                          plan_sha256=store.inputs["runner-plan.json"], state_sha256=store.inputs["runner-state.json"])
        else:
            report["errors"] = ["runner recorded an invalid outcome; results withheld"]
    except c.ERRORS as error:
        report.update(status="invalid", errors=[str(error)], archive_integrity_verified=False,
                      saved_report_reproduced=False, results_released=False)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--expected-plan-sha256", help="independently retained runner plan hash")
    parser.add_argument("--expected-state-sha256", help="independently retained final state hash (anchors results)")
    args = parser.parse_args()
    report = check(args.archive, args.expected_plan_sha256, args.expected_state_sha256)
    print(json.dumps(report, indent=2, sort_keys=True, allow_nan=False))
    return c.EXIT_CODES[report["status"]]


if __name__ == "__main__":
    sys.exit(main())
