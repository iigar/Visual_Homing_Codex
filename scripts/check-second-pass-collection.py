#!/usr/bin/env python3
"""Read-only evaluation collection audit; never executes archived programs.

See docs/SECOND_PASS_COLLECTION_UA.md for the manifest and coverage contract.
"""
import argparse
import csv
import importlib.util
import json
from pathlib import Path
import sys


SCRIPT = Path(__file__).resolve()
spec = importlib.util.spec_from_file_location("trusted_bundle_checker", SCRIPT.with_name("check-second-pass-bundle.py"))
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)
e, s, v = b.e, b.s, b.v
EXIT_CODES = {"verified": 0, "invalid": 2, "replay_failed": 3, "incomplete": 4, "needs_review": 5}
RUN_STATES = ("verified", "replay_failed", "incomplete", "invalid")
ERRORS = (ValueError, OSError, csv.Error, RuntimeError, OverflowError)
TOOLS = (SCRIPT.name, "check-second-pass-bundle.py", *b.TOOLS)


def relative_path(value):
    """Lexical validation also works for an expected, not-yet-created run."""
    v.string(value, "path")
    v.require(not any(c in value for c in "\\:,") and not any(ord(c) < 32 for c in value),
              value, "expected portable relative path")
    v.require(all(p not in ("", ".", "..") and p == p.strip() for p in value.split("/")),
              value, "absolute, empty, dot, parent or whitespace path component")
    return value


def manifest_contract(manifest):
    v.obj(manifest, "schema_version collection_id disjoint_references datasets runs", "collection")
    v.integer(manifest["schema_version"], "schema_version", 1, 1)
    v.string(manifest["collection_id"], "collection_id")
    v.boolean(manifest["disjoint_references"], "disjoint_references")
    for key, limit in (("datasets", 256), ("runs", 4096)):
        v.require(type(manifest[key]) is list and 1 <= len(manifest[key]) <= limit, key, "invalid list size")
    datasets, run_ids, paths, used = {}, set(), ["collection.json"], set()
    for row in manifest["datasets"]:
        v.obj(row, "dataset_id split path input_sha256", "dataset")
        did = v.string(row["dataset_id"], "dataset_id")
        v.require(did not in datasets, "datasets", "duplicate dataset_id")
        v.choice(row["split"], "development validation test", "split")
        paths.append(relative_path(row["path"]))
        hashes = b.hash_map(row["input_sha256"], "dataset.input_sha256")
        v.require(hashes, "dataset.input_sha256", "empty binding map")
        for path in hashes:
            relative_path(path)
        datasets[did] = row
    for row in manifest["runs"]:
        v.obj(row, "run_id dataset_id path plan_sha256 execution_sha256 peer_ids", "run")
        rid = v.string(row["run_id"], "run_id")
        v.require(rid not in run_ids, "runs", "duplicate run_id")
        run_ids.add(rid)
        did = v.string(row["dataset_id"], "run.dataset_id")
        v.require(did in datasets, "run.dataset_id", "not in expected catalog")
        used.add(did)
        paths.append(relative_path(row["path"]))
        v.strings(row["peer_ids"], "peer_ids")
        v.require(set(row["peer_ids"]) <= set(datasets) - {did}, "peer_ids", "unknown or self peer")
        for key in ("plan_sha256", "execution_sha256"):
            if row[key] is not None:
                v.sha256(row[key], "run." + key)
        v.require(row["execution_sha256"] is None or row["plan_sha256"] is not None,
                  "run", "completion anchor requires a plan anchor")
    v.require(used == set(datasets), "runs", "every catalog dataset needs an expected run")
    # Casefold also excludes path aliases when a collection is moved to Windows.
    folded = sorted(p.casefold() for p in paths)
    for index, path in enumerate(folded):
        v.require(not any(other == path or other.startswith(path + "/") for other in folded[index + 1:]),
                  "paths", "duplicate, nested or case-aliased dataset/bundle roots")
    return datasets


def closed_inventory(files, directories, manifest):
    catalog_files = {"collection.json"} | {row["path"] + "/" + path for row in manifest["datasets"]
                                            for path in row["input_sha256"]}
    bundles = [row["path"] for row in manifest["runs"]]
    parents = {parent.as_posix() for path in catalog_files | set(bundles)
               for parent in Path(path).parents if parent != Path(".")}
    v.require(not (files & set(bundles)), "inventory", "bundle path is a file")
    v.require(all(path in catalog_files or any(path.startswith(root + "/") for root in bundles) for path in files),
              "inventory", "unlisted file or dataset/bundle")
    v.require(all(path in parents or any(path == root or path.startswith(root + "/") for root in bundles)
                  for path in directories), "inventory", "unlisted directory")


def check_run(store, row, catalog, files):
    result = {key: row[key] for key in ("run_id", "dataset_id", "path")}
    result.update(status="invalid", errors=[])
    try:
        missing = []
        for name, key in (("plan.json", "plan_sha256"), ("execution.json", "execution_sha256")):
            path = row["path"] + "/" + name
            if row[key] is None:
                missing.append(key + " is not sealed")
            elif path not in files:
                missing.append(name + " is absent")
            else:
                store.artifact({"path": path, "sha256": row[key]}, "run anchor")
        if missing:
            return {**result, "status": "incomplete", "errors": missing}
        checked = b.check(store.root / row["path"], row["plan_sha256"], row["peer_ids"])
        v.require(checked["status"] in ("verified", "replay_failed"), "bundle", str(checked["errors"]))
        v.require(checked["run_id"] == row["run_id"], "run_id", "bundle differs from manifest")
        scope = checked["peer_scope"]
        v.require(scope["dataset_id"] == row["dataset_id"], "dataset_id", "bundle differs from manifest")
        # The exact completion bytes, including all output hashes, are anchored.
        v.require(checked["bundle_sha256"]["execution.json"] == row["execution_sha256"],
                  "execution", "completion changed during bundle check")
        for dataset in checked["score"]["dataset_validation"]["datasets"]:
            expected = catalog[dataset["dataset_id"]]
            v.require(dataset["split"] == expected["split"], "split", "embedded/catalog mismatch")
            s.check_hashes(dataset["input_sha256"], expected["input_sha256"], "embedded/catalog dataset")
        # Preserve every byte admitted by the checker through the collection audit.
        for path, digest in checked["bundle_sha256"].items():
            store.artifact({"path": row["path"] + "/" + path, "sha256": digest}, "checked bundle")
        result.update(status=checked["status"], bundle=checked)
    except ERRORS as error:
        result.update(status="invalid", errors=[str(error)])
    return result


def coverage(rows, frames):
    counts = {state: sum(row["status"] == state for row in rows) for state in RUN_STATES}
    frame_counts = {state: sum(frames[row["dataset_id"]] for row in rows if row["status"] == state)
                    for state in RUN_STATES}
    return {"expected_runs": len(rows), "runs_by_status": counts,
            "expected_frame_run_pairs": sum(frame_counts.values()), "frame_run_pairs_by_status": frame_counts,
            "all_runs_verified": counts["verified"] == len(rows)}


def check(collection_root, expected_manifest_sha256=None):
    report = {"report_version": 1, "status": "invalid", "errors": [], "runs": [],
              "results_released": False, "catalog_verified": False, "binary_executed": False,
              "semantic_independence_verified": False, "chronology_authenticated": False,
              "scope": "only datasets and runs explicitly listed in collection.json; no pooled accuracy"}
    try:
        if expected_manifest_sha256 is not None:
            v.sha256(expected_manifest_sha256, "expected_manifest_sha256")
        trusted = {name: e.binding(SCRIPT.parent, name)["sha256"] for name in TOOLS}
        store = v.Dataset(collection_root)
        inventory = b.inventory(store.root)
        manifest = s.read_json(store, "collection.json")
        if expected_manifest_sha256 is not None:
            v.require(store.inputs["collection.json"] == expected_manifest_sha256, "collection", "external manifest anchor mismatch")
        catalog = manifest_contract(manifest)
        report.update(collection_id=manifest["collection_id"], manifest_sha256=store.inputs["collection.json"],
                      external_manifest_anchor_checked=expected_manifest_sha256 is not None)
        closed_inventory(*inventory, manifest)
        for row in catalog.values():
            for path, digest in row["input_sha256"].items():
                store.artifact({"path": row["path"] + "/" + path, "sha256": digest}, "catalog")
        validation = v.validate_datasets([store.root / row["path"] for row in catalog.values()], manifest["disjoint_references"])
        report["dataset_validation"] = validation
        v.require(validation["status"] != "invalid", "catalog", "dataset structure or cross-split validation failed")
        for expected, actual in zip(catalog.values(), validation["datasets"]):
            v.require((actual["dataset_id"], actual["split"]) == (expected["dataset_id"], expected["split"]),
                      "catalog", "dataset ID/split differs from manifest")
            s.check_hashes(actual["input_sha256"], expected["input_sha256"], "catalog input map")
        frames = {row["dataset_id"]: row["frames"] for row in validation["datasets"]}
        if validation["status"] == "needs_review":
            report.update(status="needs_review", errors=["catalog review is unresolved; runs were not assessed"])
        else:
            report["runs"] = [check_run(store, row, catalog, inventory[0]) for row in manifest["runs"]]
            states = {row["status"] for row in report["runs"]}
            report["status"] = next((state for state in ("invalid", "incomplete", "replay_failed") if state in states), "verified")
        e.verify_frozen(store.root, dict(store.inputs))
        v.require(b.inventory(store.root) == inventory, "collection", "membership changed during check")
        for name, digest in trusted.items():
            v.require(e.binding(SCRIPT.parent, name)["sha256"] == digest, "toolchain", "trusted source changed during check")
        report.update(observed_sha256=dict(store.inputs), toolchain_sha256=trusted)
        if validation["status"] == "structurally_valid":
            report.update(catalog_verified=True, results_released=True, coverage=coverage(report["runs"], frames),
                          by_dataset={did: coverage([r for r in report["runs"] if r["dataset_id"] == did], frames) for did in catalog},
                          by_split={split: coverage([r for r in report["runs"] if catalog[r["dataset_id"]]["split"] == split], frames)
                                    for split in sorted({d["split"] for d in catalog.values()})})
    except ERRORS as error:
        report.update(status="invalid", errors=[str(error)], results_released=False, catalog_verified=False)
        for row in report["runs"]:
            row.pop("bundle", None)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("collection", type=Path, help="directory containing collection.json")
    parser.add_argument("--expected-manifest-sha256", help="independently retained collection manifest hash")
    args = parser.parse_args()
    report = check(args.collection, args.expected_manifest_sha256)
    print(json.dumps(report, indent=2, sort_keys=True, allow_nan=False))
    return EXIT_CODES[report["status"]]


if __name__ == "__main__":
    sys.exit(main())
