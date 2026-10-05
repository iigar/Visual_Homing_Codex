#!/usr/bin/env python3
"""Read-only integrity/status/rescore check of an offline exporter v1 bundle.

Loads only adjacent trusted tools. Never imports bundled sources or runs a binary.
See docs/SECOND_PASS_BUNDLE_CHECKER_UA.md for scope and exit codes.
"""
import argparse
import csv
import importlib.util
import json
import os
from pathlib import Path
import stat
import sys


SCRIPT = Path(__file__).resolve()
spec = importlib.util.spec_from_file_location("trusted_second_pass_exporter", SCRIPT.with_name("export-second-pass.py"))
e = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e)
s, v = e.s, e.v
TOOLS = ("export-second-pass.py", "score-second-pass.py", "validate-second-pass-dataset.py")
OUTPUTS = {"raw-predictions.csv", "replay.log", "predictions.csv", "run.json", "score.json"}
FIXED_INPUTS = {"implementation/replay", "configuration.json", "policy.json", "manifest.csv", "clock.txt",
                *("toolchain/" + name for name in TOOLS)}
UNSUPPORTED = ["endpoint_stop", "route_readiness", "verification_accepted", "published"]
STATE = "fresh process per dataset; matcher and navigator retain state across all stored frames including gaps"
PREPROCESSING = ("Gray8ResizePreprocessor; explicit target in configuration; source-time zero-work clock; "
                 "synthetic armed Guided telemetry")
PLAN_CONSTANTS = {
    "clock": "source_timestamp_each_phase_zero_work",
    "telemetry": "synthetic heartbeat armed Guided",
    "state_reset_policy": STATE,
    "unsupported_outcomes": UNSUPPORTED,
    "adapter_contract": "second_pass_replay_v1; no camera profile; other matcher options at compiled defaults",
    "limitations": "No dynamic-library/environment pinning, binary/source attestation or adversarial filesystem protection",
}
SCORE_BYTES = 256 * 1024 * 1024
INVENTORY_LIMIT = 200000
EXIT_CODES = {"verified": 0, "invalid": 2, "replay_failed": 3, "incomplete": 4}


def inventory(root):
    """Closed bundle: no symlinks, devices, unbound files or hidden peer folders."""
    files, directories, pending = set(), set(), [root]
    while pending:
        with os.scandir(pending.pop()) as entries:
            for entry in entries:
                relative = Path(entry.path).relative_to(root).as_posix()
                mode = entry.stat(follow_symlinks=False).st_mode
                v.require(not stat.S_ISLNK(mode), relative, "bundle symlinks are not supported")
                if stat.S_ISDIR(mode):
                    directories.add(relative)
                    pending.append(Path(entry.path))
                else:
                    v.require(stat.S_ISREG(mode), relative, "expected regular file")
                    files.add(relative)
                v.require(len(files) + len(directories) <= INVENTORY_LIMIT, "bundle", "inventory limit exceeded")
    return files, directories


def hash_map(value, where):
    v.require(type(value) is dict, where, "expected path/hash object")
    for path, digest in value.items():
        v.string(path, where + ".path")
        v.sha256(digest, where + ".sha256")
    return value


def same(actual, expected, where):
    # JSON comparison preserves bool/int/float distinctions; Python == does not.
    dump = lambda value: json.dumps(value, sort_keys=True, allow_nan=False, separators=(",", ":"))
    v.require(dump(actual) == dump(expected), where, "does not match exporter v1 contract")


def execution_contract(execution):
    v.require(type(execution) is dict, "execution", "expected object")
    reason = execution.get("missing_reason")
    fields = "status returncode missing_reason plan_sha256 output_sha256"
    if reason in ("replay_launch_failed", "replay_output_invalid"):
        fields += " error"
    v.obj(execution, fields, "execution")
    v.sha256(execution["plan_sha256"], "execution.plan_sha256")
    hashes = hash_map(execution["output_sha256"], "execution.output_sha256")
    v.require(set(hashes) == OUTPUTS, "execution.output_sha256", "missing/extra output binding")
    v.choice(execution["status"], "complete failed", "execution.status")
    code = execution["returncode"]
    if execution["status"] == "complete":
        v.integer(code, "execution.returncode", 0, 0)
        v.require(reason is None, "execution", "complete run cannot have missing_reason")
    else:
        v.choice(reason, "replay_failed replay_timeout replay_launch_failed replay_output_invalid", "execution.missing_reason")
        if reason in ("replay_timeout", "replay_launch_failed"):
            v.require(code is None, "execution.returncode", "timeout/launch failure must have null returncode")
        else:
            v.integer(code, "execution.returncode", -(1 << 31), (1 << 32) - 1)
            v.require((code == 0) == (reason == "replay_output_invalid"), "execution", "returncode/reason mismatch")
        if "error" in execution:
            v.require(type(execution["error"]) is str, "execution.error", "expected diagnostic string")
    return hashes


def plan_contract(plan):
    v.obj(plan, "schema_version run_id code_revision_declared input_sha256 arguments timeout_seconds "
          "disjoint_references split_peer_count clock telemetry state_reset_policy unsupported_outcomes "
          "adapter_contract environment limitations", "plan")
    v.integer(plan["schema_version"], "plan.schema_version", 1, 1)
    for key in ("run_id", "code_revision_declared"):
        v.string(plan[key], "plan." + key)
    for key, value in PLAN_CONSTANTS.items():
        same(plan[key], value, "plan." + key)
    v.integer(plan["timeout_seconds"], "plan.timeout_seconds", 1, 3600)
    v.integer(plan["split_peer_count"], "plan.split_peer_count", 0, 1000)
    v.boolean(plan["disjoint_references"], "plan.disjoint_references")
    v.obj(plan["environment"], "platform python", "plan.environment")
    for key, value in plan["environment"].items():
        v.string(value, "plan.environment." + key)
    return hash_map(plan["input_sha256"], "plan.input_sha256")


def portable_score(score, prefixes):
    """Only dataset root locations may change when an intact bundle is moved."""
    v.require(type(score) is dict and type(score.get("dataset_validation")) is dict, "score", "missing dataset validation")
    validation = score["dataset_validation"]
    rows = validation.get("datasets")
    v.require(type(rows) is list and len(rows) == len(prefixes), "score", "dataset scope mismatch")
    normalized = []
    for row, prefix in zip(rows, prefixes):
        v.require(type(row) is dict, "score dataset", "expected object")
        v.string(row.get("root"), "score dataset.root")
        normalized.append({**row, "root": prefix})
    return {**score, "dataset_validation": {**validation, "datasets": normalized}}


def check(bundle_root, expected_plan_sha256=None, expected_peer_ids=None):
    report = {"report_version": 1, "status": "invalid", "errors": [],
              "bundle_integrity_verified": False, "score_reproduced": False,
              "binary_executed": False, "semantic_independence_verified": False,
              "chronology_authenticated": False}
    try:
        if expected_plan_sha256 is not None:
            v.sha256(expected_plan_sha256, "expected_plan_sha256")
        if expected_peer_ids is not None:
            v.strings(expected_peer_ids, "expected_peer_ids")
        store = v.Dataset(bundle_root)
        files, directories = inventory(store.root)
        if "execution.json" not in files:
            return {**report, "status": "incomplete", "errors": ["completion record execution.json is absent"]}
        v.require("failure.json" not in files, "bundle", "failure.json conflicts with completion record")
        execution = s.read_json(store, "execution.json")
        output_hashes = execution_contract(execution)
        plan = s.read_json(store, "plan.json")
        v.require(store.inputs["plan.json"] == execution["plan_sha256"], "plan", "completion SHA-256 mismatch")
        if expected_plan_sha256 is not None:
            v.require(store.inputs["plan.json"] == expected_plan_sha256, "plan", "external SHA-256 anchor mismatch")
        inputs = plan_contract(plan)
        v.require(not (set(inputs) & (OUTPUTS | {"plan.json", "execution.json"})), "plan", "input/output paths overlap")
        expected_files = set(inputs) | OUTPUTS | {"plan.json", "execution.json"}
        v.require(files == expected_files, "inventory", "missing/extra file, including unbound peer artifacts")
        expected_dirs = {parent.as_posix() for name in files for parent in Path(name).parents if parent != Path(".")}
        v.require(directories == expected_dirs, "inventory", "unbound directory")
        for path, digest in {**inputs, **output_hashes}.items():
            store.artifact({"path": path, "sha256": digest}, "bundle")

        # A pinned source is data, never a module to load. Rescore with adjacent
        # trusted code only, requiring byte-identical pinned tool versions.
        trusted_hashes = {name: e.binding(SCRIPT.parent, name)["sha256"] for name in TOOLS}
        for name, digest in trusted_hashes.items():
            v.require(inputs.get("toolchain/" + name) == digest, "toolchain", "pinned/trusted source mismatch: " + name)
        config, config_hash = e.read_config(store.path("configuration.json"))
        v.require(inputs.get("configuration.json") == config_hash, "configuration", "plan hash mismatch")
        prefixes = ["dataset", *[f"peers/{index:04d}" for index in range(1, plan["split_peer_count"] + 1)]]
        roots = [store.root / prefix for prefix in prefixes]
        # Validate membership before handing dataset roots to the standalone scorer.
        for prefix in prefixes:
            store.path(prefix + "/dataset.json")
        validation = v.validate_datasets(roots, plan["disjoint_references"])
        v.require(validation["status"] == "structurally_valid", "datasets", "snapshot review/structure/split validation failed")
        expected_inputs = {prefix + "/" + path: digest for prefix, dataset in zip(prefixes, validation["datasets"])
                           for path, digest in dataset["input_sha256"].items()}
        v.require(set(inputs) == set(expected_inputs) | FIXED_INPUTS, "plan.input_sha256", "incomplete/extra dataset or fixed input map")
        s.check_hashes({path: inputs[path] for path in expected_inputs}, expected_inputs, "dataset hashes")
        peer_ids = [dataset["dataset_id"] for dataset in validation["datasets"][1:]]
        if expected_peer_ids is not None:
            v.require(set(peer_ids) == set(expected_peer_ids), "peer scope", "external expected peer IDs differ")

        data = v.Dataset(roots[0])
        meta = s.read_json(data, "dataset.json")
        frames = [row for _, row in data.rows("frames.csv", v.FRAME_FIELDS)]
        # Authenticate reread metadata before using its previously validated shape.
        s.check_hashes(data.inputs, {path: inputs["dataset/" + path] for path in data.inputs}, "dataset reread")
        v.require(all(int(f["width"]) * int(f["height"]) <= 16 * 1024 * 1024 for f in frames),
                  "replay", "source image exceeds adapter pixel limit")
        _, reference_pixels = data.read(meta["reference"]["path"], v.vhrs, meta["reference"]["sha256"])
        v.require(all(p[:2] == (config["target_width"], config["target_height"]) for p in reference_pixels),
                  "replay", "reference geometry differs from configured target")
        same(plan["arguments"], ["implementation/replay", "dataset/" + meta["reference"]["path"], "manifest.csv", "clock.txt",
                                 *[str(config[key]) for key in e.PARAMETERS]], "plan.arguments")
        manifest = "".join(f'{f["frame_id"]},{f["timestamp_ns"]},dataset/{f["path"]}\n' for f in frames)
        clock = "".join(f'{f["timestamp_ns"]}\n' for f in frames)
        same(store.small("manifest.csv", 64 * 1024 * 1024), manifest, "manifest")
        # Path.write_text in the exporter uses host newline translation for clock.txt.
        v.require(store.small("clock.txt", 4 * 1024 * 1024) in (clock, clock.replace("\n", "\r\n")),
                  "clock", "source clock differs from frame timestamps")
        pin = lambda path: {"path": path, "sha256": inputs[path] if path in inputs else output_hashes[path]}
        expected_run = {"schema_version": 1, "run_id": plan["run_id"], "dataset_id": meta["dataset_id"],
                        "input_sha256": validation["datasets"][0]["input_sha256"], "inputs_frozen_before_run": True,
                        "producer": {"kind": "external_export", "code_revision": plan["code_revision_declared"],
                                     "implementation": pin("implementation/replay"), "configuration": pin("configuration.json"),
                                     "state_reset_policy": STATE, "preprocessing": PREPROCESSING},
                        "policy": pin("policy.json"), "predictions": pin("predictions.csv")}
        same(s.read_json(store, "run.json"), expected_run, "run/plan bindings")
        predictions = s.read_predictions(store, pin("predictions.csv"), frames, meta["reference"]["entry_count"])
        if execution["status"] == "complete":
            v.require(output_hashes["raw-predictions.csv"] == output_hashes["predictions.csv"], "predictions", "complete run differs from raw output")
            v.require(all(p["observation"] == "observed" and p["navigation_command_valid"] is not None
                          and all(p[k] is None for k in UNSUPPORTED) for p in predictions),
                      "predictions", "unsupported or missing complete-run outcome")
        else:
            v.require(all(p["observation"] == "missing" and p["missing_reason"] == execution["missing_reason"] for p in predictions),
                      "predictions", "failed run must give every source frame the same explicit missing reason")

        saved_score = json.loads(store.small("score.json", SCORE_BYTES), object_pairs_hook=v.unique_object,
                                 parse_constant=v.invalid_constant, parse_float=v.finite_float)
        rescored = s.score(roots[0], store.root, roots[1:], plan["disjoint_references"])
        v.require(rescored["status"] == "scored", "rescore", "trusted scorer rejected bundle")
        same(portable_score(saved_score, prefixes), portable_score(rescored, prefixes), "saved/recomputed score")
        # Preserve the initially checked hashes through all later reads/rescore.
        e.verify_frozen(store.root, dict(store.inputs))
        v.require(inventory(store.root) == (files, directories), "final inventory", "bundle membership changed during check")
        for name, digest in trusted_hashes.items():
            v.require(e.binding(SCRIPT.parent, name)["sha256"] == digest, "toolchain", "trusted source changed during check")
        report.update(status="verified" if execution["status"] == "complete" else "replay_failed",
                      bundle_integrity_verified=True, score_reproduced=True, execution_status=execution["status"],
                      run_id=plan["run_id"], plan_sha256=store.inputs["plan.json"],
                      external_plan_anchor_checked=expected_plan_sha256 is not None,
                      peer_scope={"dataset_id": meta["dataset_id"], "peer_ids": peer_ids,
                                  "external_inventory_checked": expected_peer_ids is not None,
                                  "disjoint_references": plan["disjoint_references"], "scope": "embedded datasets only"},
                      bundle_sha256=dict(store.inputs), checker_sha256=e.binding(SCRIPT.parent, SCRIPT.name)["sha256"],
                      score=rescored)
    except (ValueError, OSError, csv.Error, RuntimeError, OverflowError) as error:
        report.update(status="invalid", errors=[str(error)])
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--expected-plan-sha256", help="independent plan hash retained outside this bundle")
    parser.add_argument("--expected-peer-ids", nargs="*", help="exact external peer ID inventory; no values asserts zero peers")
    args = parser.parse_args()
    report = check(args.bundle, args.expected_plan_sha256, args.expected_peer_ids)
    print(json.dumps(report, indent=2, sort_keys=True, allow_nan=False))
    return EXIT_CODES[report["status"]]


if __name__ == "__main__":
    sys.exit(main())
