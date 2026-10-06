#!/usr/bin/env python3
"""Freeze an explicit evaluation request and run each local replay once.

Use only a trusted locally built second_pass_replay binary. No resume or retry.
See docs/SECOND_PASS_COLLECTION_RUNNER_UA.md for the request/archive contract.
"""
import argparse
import copy
import importlib.util
import json
import os
from pathlib import Path
import sys


SCRIPT = Path(__file__).resolve()
spec = importlib.util.spec_from_file_location("trusted_collection_checker", SCRIPT.with_name("check-second-pass-collection.py"))
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
e, s, v = c.e, c.s, c.v
TOOLS = (SCRIPT.name, *c.TOOLS)


def atomic_json(root, name, value, previous=None):
    """Same-directory replace after flush; no multi-file durability claim."""
    target = root / name
    if previous is None:
        v.require(not target.exists() and not target.is_symlink(), name, "unexpected existing control file")
    else:
        e.verify_frozen(root, {name: previous})
    temporary = target.with_name(target.name + ".tmp")
    with temporary.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2, sort_keys=True, allow_nan=False)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, target)
    return e.binding(root, name)["sha256"]


def request_contract(request):
    v.obj(request, "schema_version collection_id disjoint_references datasets runs", "request")
    v.require(type(request["datasets"]) is list and 1 <= len(request["datasets"]) <= 256, "datasets", "invalid list size")
    v.require(type(request["runs"]) is list and 1 <= len(request["runs"]) <= 4096, "runs", "invalid list size")
    manifest = {key: request[key] for key in ("schema_version", "collection_id", "disjoint_references")}
    manifest.update(datasets=[], runs=[])
    for index, row in enumerate(request["datasets"]):
        v.obj(row, "dataset_id split path input_sha256", "dataset")
        c.relative_path(row["path"])
        manifest["datasets"].append({**row, "path": f"datasets/{index:04d}"})
    for index, row in enumerate(request["runs"]):
        v.obj(row, "run_id dataset_id configuration policy timeout_seconds", "run")
        v.integer(row["timeout_seconds"], "timeout_seconds", 1, 3600)
        for key in ("configuration", "policy"):
            v.obj(row[key], "path sha256", key)
            c.relative_path(row[key]["path"])
            v.sha256(row[key]["sha256"], key + ".sha256")
        manifest["runs"].append({key: row[key] for key in ("run_id", "dataset_id")} |
                                dict(path=f"bundles/{index:04d}", plan_sha256=None, execution_sha256=None,
                                     peer_ids=[d["dataset_id"] for d in request["datasets"] if d["dataset_id"] != row["dataset_id"]]))
    c.manifest_contract(manifest)
    return manifest


def admit_bundle(root, row, requested, catalog, binary_hash, revision):
    checked = c.b.check(root, expected_peer_ids=row["peer_ids"])
    v.require(checked["status"] in ("verified", "replay_failed"), "bundle", str(checked["errors"]))
    v.require(checked["run_id"] == row["run_id"] and checked["peer_scope"]["dataset_id"] == row["dataset_id"],
              "bundle", "run/dataset differs from frozen request")
    for dataset in checked["score"]["dataset_validation"]["datasets"]:
        expected = catalog[dataset["dataset_id"]]
        v.require(dataset["split"] == expected["split"], "bundle", "split differs from frozen request")
        s.check_hashes(dataset["input_sha256"], expected["input_sha256"], "frozen dataset")
    store = v.Dataset(root)
    plan = s.read_json(store, "plan.json")
    v.require(store.inputs["plan.json"] == checked["plan_sha256"], "plan", "changed after bundle check")
    for key, expected in (("code_revision_declared", revision), ("timeout_seconds", requested["timeout_seconds"])):
        c.b.same(plan[key], expected, "frozen " + key)
    for name, digest in (("implementation/replay", binary_hash), ("configuration.json", requested["configuration"]["sha256"]),
                         ("policy.json", requested["policy"]["sha256"])):
        v.require(plan["input_sha256"][name] == digest, name, "differs from frozen request")
    return checked


def run(request_path, output_root, binary, code_revision):
    output = None
    report = dict(report_version=1, status="invalid", errors=[], runs=[], exporter_calls=0,
                  semantic_independence_verified=False, chronology_authenticated=False)
    immutable, controls, trusted = {}, {}, {}
    manifest = None

    def control(name, value):
        controls[name] = atomic_json(output, name, value, controls.get(name))

    def guard():
        e.verify_frozen(output, {**immutable, **controls})
        for name, digest in trusted.items():
            v.require(e.binding(SCRIPT.parent, name)["sha256"] == digest, "toolchain", "trusted source changed")

    def freeze(store, path, destination, digest):
        e.copy_checked(store, path, output / destination, digest)
        immutable[destination] = digest  # Never repin a mutated copy.

    try:
        request_path = v.resolved(Path(request_path))
        source = v.Dataset(request_path.parent)
        request = s.read_json(source, request_path.name)
        manifest = request_contract(request)
        v.string(code_revision, "code_revision")
        destination = Path(output_root)
        v.require(not destination.is_symlink(), "output", "existing symlink")
        destination = destination.resolve()
        v.require(not destination.exists(), "output", "must be a new directory; no overwrite/resume")
        v.require(destination != source.root and source.root not in destination.parents, "output", "must be outside request directory")
        roots = [source.path(d["path"] + "/dataset.json").parent for d in request["datasets"]]
        validation = v.validate_datasets(roots, request["disjoint_references"])
        if validation["status"] == "invalid":
            return {**report, "status": validation["status"], "dataset_validation": validation}
        for expected, actual in zip(request["datasets"], validation["datasets"]):
            v.require((expected["dataset_id"], expected["split"]) == (actual["dataset_id"], actual["split"]),
                      "request", "dataset ID/split mismatch")
            s.check_hashes(actual["input_sha256"], expected["input_sha256"], "request dataset")
        if validation["status"] == "needs_review":
            return {**report, "status": "needs_review", "dataset_validation": validation}
        for row in request["runs"]:
            source.artifact(row["configuration"], "configuration")
            _, digest = e.read_config(source.path(row["configuration"]["path"]))
            v.require(digest == row["configuration"]["sha256"], "configuration", "hash changed during validation")
            s.read_policy(source, row["policy"])
        binary = v.resolved(Path(binary))
        binary_hash = e.binding(binary.parent, binary.name)["sha256"]
        trusted = {name: e.binding(SCRIPT.parent, name)["sha256"] for name in TOOLS}
        destination.mkdir(parents=True, exist_ok=False)
        output = destination
        report["output"] = str(output)
        freeze(source, request_path.name, "request.json", source.inputs[request_path.name])
        freeze(v.Dataset(binary.parent), binary.name, "frozen/replay", binary_hash)
        for name, digest in trusted.items():
            freeze(v.Dataset(SCRIPT.parent), name, "frozen/toolchain/" + name, digest)
        for root, row, target in zip(roots, request["datasets"], manifest["datasets"]):
            for path, digest in row["input_sha256"].items():
                freeze(v.Dataset(root), path, "collection/" + target["path"] + "/" + path, digest)
        for index, row in enumerate(request["runs"]):
            for key in ("configuration", "policy"):
                freeze(source, row[key]["path"], f"frozen/{index:04d}-{key}.json", row[key]["sha256"])
        immutable["initial-manifest.json"] = atomic_json(output, "initial-manifest.json", manifest)
        plan = dict(schema_version=1, code_revision_declared=code_revision, initial_manifest=copy.deepcopy(manifest),
                    input_sha256=dict(immutable), request_sha256=immutable["request.json"],
                    execution_policy="manifest order; one exporter call per run; no implicit retry or resume")
        immutable["runner-plan.json"] = atomic_json(output, "runner-plan.json", plan)
        report["plan_sha256"] = immutable["runner-plan.json"]
        report["runs"] = [{key: row[key] for key in ("run_id", "dataset_id", "path")} |
                          dict(status="pending", attempted=False, export_status=None, errors=[]) for row in manifest["runs"]]
        control("collection/collection.json", manifest)
        report["status"] = "running"
        control("runner-state.json", report)
        guard()
        initial = c.check(output / "collection", controls["collection/collection.json"])
        v.require(initial["status"] == "incomplete" and initial["catalog_verified"], "snapshot", "frozen catalog validation failed")
        catalog = {d["dataset_id"]: d for d in manifest["datasets"]}
        for index, (row, requested, state) in enumerate(zip(manifest["runs"], request["runs"], report["runs"])):
            guard()
            state.update(status="running", attempted=True)
            report["exporter_calls"] += 1
            control("runner-state.json", report)
            try:
                exported = e.export(output / "collection" / catalog[row["dataset_id"]]["path"],
                                    output / "collection" / row["path"], output / "frozen/replay",
                                    output / f"frozen/{index:04d}-configuration.json", output / f"frozen/{index:04d}-policy.json",
                                    code_revision, row["run_id"],
                                    [output / "collection" / catalog[did]["path"] for did in row["peer_ids"]],
                                    request["disjoint_references"], requested["timeout_seconds"])
                state.update(export_status=exported["status"], errors=exported.get("errors", []))
            except c.ERRORS as error:
                state.update(export_status="exception", errors=[str(error)])
            guard()  # Shared input/control drift stops subsequent launches.
            if state["export_status"] in ("exported", "replay_failed"):
                try:
                    checked = admit_bundle(output / "collection" / row["path"], row, requested, catalog, binary_hash, code_revision)
                    c.b.same(checked["peer_scope"]["disjoint_references"], request["disjoint_references"], "frozen split policy")
                    v.require((checked["status"] == "verified") == (state["export_status"] == "exported"), "exporter", "status mismatch")
                except c.ERRORS as error:
                    state.update(status="invalid", errors=[str(error)])
                else:
                    for path, digest in checked["bundle_sha256"].items():
                        immutable["collection/" + row["path"] + "/" + path] = digest
                    guard()
                    row.update(plan_sha256=checked["plan_sha256"], execution_sha256=checked["bundle_sha256"]["execution.json"])
                    control("collection/collection.json", manifest)
                    state["status"] = checked["status"]
            else:
                state["status"] = "incomplete"
            control("runner-state.json", report)
        report["status"] = "invalid" if any(r["status"] == "invalid" for r in report["runs"]) else "finished"
    except KeyboardInterrupt:
        report.update(status="incomplete", errors=["interrupted; no automatic retry or resume"])
    except c.ERRORS as error:
        report.update(status="invalid", errors=[str(error)])
    for state in report["runs"]:
        if state["status"] == "running":
            state.update(status="incomplete", errors=state["errors"] + ["attempt ended without a sealed result"])
    if output is not None and "collection/collection.json" in controls:
        try:
            guard()
            audit = c.check(output / "collection", controls["collection/collection.json"])
            guard()  # The collection checker does not cover outer frozen inputs.
            if report["status"] == "finished":
                report["status"] = audit["status"]
            elif audit["status"] == "invalid":
                report["status"] = "invalid"
            report.update(collection_status=audit["status"], manifest_sha256=controls["collection/collection.json"],
                          collection_report_sha256=atomic_json(output, "collection-report.json", audit))
        except c.ERRORS as error:
            report.update(status="invalid", errors=report["errors"] + [str(error)])
        try:
            control("runner-state.json", report)
        except c.ERRORS as error:
            report.update(status="invalid", errors=report["errors"] + ["cannot save final state: " + str(error)])
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("request", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--code-revision", required=True)
    args = parser.parse_args()
    report = run(args.request, args.output, args.binary, args.code_revision)
    print(json.dumps(report, indent=2, sort_keys=True, allow_nan=False))
    return c.EXIT_CODES[report["status"]]


if __name__ == "__main__":
    sys.exit(main())
