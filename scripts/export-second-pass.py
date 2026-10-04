#!/usr/bin/env python3
"""Freeze validated inputs, run the offline replay adapter, then score its CSV.

Only use a trusted locally built second_pass_replay binary. This is a reproducible
local workflow, not a sandbox for executables or adversarial tamper protection.
"""
import argparse
import csv
import importlib.util
import json
import math
from pathlib import Path
import platform
import stat
import subprocess
import sys


SCRIPT = Path(__file__).resolve()
spec = importlib.util.spec_from_file_location("second_pass_scorer", SCRIPT.with_name("score-second-pass.py"))
s = importlib.util.module_from_spec(spec)
spec.loader.exec_module(s)
v = s.v
PARAMETERS = ("target_width target_height window_radius minimum_confidence max_direction_shift_px radians_per_pixel "
              "navigator_minimum_confidence navigator_max_match_age_ms navigator_yaw_gain "
              "navigator_max_yaw_rate_radps navigator_max_yaw_accel_radps2 navigator_forward_speed_mps").split()


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8")


def binding(root, relative):
    store = v.Dataset(root)
    def consume(reader):
        while reader.read(1024 * 1024):
            pass
    store.read(relative, consume)
    return {"path": relative, "sha256": store.inputs[relative]}


def copy_checked(store, relative, destination, expected):
    v.sha256(expected, relative + ".sha256")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("xb") as output:
        def copy(reader):
            while block := reader.read(1024 * 1024):
                output.write(block)
        store.read(relative, copy, expected)


def read_config(path):
    store = v.Dataset(path.parent)
    config = s.read_json(store, path.name)
    v.obj(config, "schema_version " + " ".join(PARAMETERS), "configuration")
    v.integer(config["schema_version"], "configuration.schema_version", 1, 1)
    for key in ("target_width", "target_height"):
        # Bounds also avoid signed multiplication overflow in the current resize.
        v.integer(config[key], key, 1, 4096)
    v.integer(config["window_radius"], "window_radius", 0, v.MAX_ROWS)
    v.integer(config["max_direction_shift_px"], "max_direction_shift_px", 0, config["target_width"] - 1)
    for key in PARAMETERS:
        if key in ("target_width", "target_height", "window_radius", "max_direction_shift_px"):
            continue
        value = config[key]
        v.require(type(value) in (int, float) and math.isfinite(value) and value >= 0, key, "expected finite nonnegative number")
        if key in ("minimum_confidence", "navigator_minimum_confidence"):
            v.require(value <= 1, key, "confidence exceeds 1")
    return config, store.inputs[path.name]


def verify_frozen(root, hashes):
    store = v.Dataset(root)
    for path, digest in hashes.items():
        store.artifact({"path": path, "sha256": digest}, "frozen input")


def missing_predictions(path, frames, reason):
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=s.PREDICTION_FIELDS, lineterminator="\n")
        writer.writeheader()
        for frame in frames:
            writer.writerow({**{k: frame[k] for k in ("sequence", "frame_id", "timestamp_ns")},
                             "observation": "missing", "missing_reason": reason})


def export(dataset_root, output_root, binary, configuration, policy, code_revision, run_id,
           peers=(), disjoint_references=False, timeout_seconds=300):
    output = None
    report = {"report_version": 1, "status": "invalid", "errors": [], "semantic_independence_verified": False}
    try:
        roots = [v.resolved(Path(root)) for root in (dataset_root, *peers)]
        requested_output = Path(output_root)
        v.require(not requested_output.is_symlink(), "output", "must not be an existing symlink")
        destination = requested_output.resolve()
        v.require(all(destination != root and root not in destination.parents for root in roots),
                  "output", "must be outside input datasets")
        v.require(not destination.exists(), "output", "must be a new directory; existing runs are never overwritten")
        v.string(code_revision, "code_revision")
        v.string(run_id, "run_id")
        v.integer(timeout_seconds, "timeout_seconds", 1, 3600)
        binary, configuration, policy = (v.resolved(Path(p)) for p in (binary, configuration, policy))
        config, config_hash = read_config(configuration)
        policy_record = binding(policy.parent, policy.name)
        s.read_policy(v.Dataset(policy.parent), policy_record)
        implementation = binding(binary.parent, binary.name)
        validation = v.validate_datasets(roots, disjoint_references)
        report["dataset_validation"] = validation
        if validation["status"] != "structurally_valid":
            report["status"] = validation["status"]
            return report

        destination.mkdir(parents=True, exist_ok=False)
        output = destination
        frozen_hashes = {}

        def freeze(store, relative, target, expected):
            copy_checked(store, relative, output / target, expected)
            # Retain the admitted hash, never repin a changed copy when planning.
            frozen_hashes[target] = expected

        frozen_roots = []
        for index, (root, validated) in enumerate(zip(roots, validation["datasets"])):
            prefix = "dataset" if index == 0 else f"peers/{index:04d}"
            frozen = output / prefix
            for path, digest in validated["input_sha256"].items():
                freeze(v.Dataset(root), path, prefix + "/" + path, digest)
            frozen_roots.append(frozen)
        frozen_validation = v.validate_datasets(frozen_roots, disjoint_references)
        v.require(frozen_validation["status"] == "structurally_valid", "snapshot", "frozen dataset validation failed")
        for old, new in zip(validation["datasets"], frozen_validation["datasets"]):
            s.check_hashes(new["input_sha256"], old["input_sha256"], "snapshot")
        data = v.Dataset(frozen_roots[0])
        meta = s.read_json(data, "dataset.json")
        frames = [row for _, row in data.rows("frames.csv", v.FRAME_FIELDS)]
        v.require(all(int(f["width"]) * int(f["height"]) <= 16 * 1024 * 1024 for f in frames),
                  "replay", "source image exceeds adapter 16 Mi-pixel limit")
        _, reference_pixels = data.read(meta["reference"]["path"], v.vhrs, meta["reference"]["sha256"])
        v.require(all(p[:2] == (config["target_width"], config["target_height"]) for p in reference_pixels),
                  "replay", "reference geometry differs from configured resize target")

        freeze(v.Dataset(binary.parent), binary.name, "implementation/replay", implementation["sha256"])
        executable = output / "implementation/replay"
        executable.chmod(executable.stat().st_mode | stat.S_IXUSR)
        freeze(v.Dataset(configuration.parent), configuration.name, "configuration.json", config_hash)
        freeze(v.Dataset(policy.parent), policy.name, "policy.json", policy_record["sha256"])
        for path in (SCRIPT, SCRIPT.with_name("score-second-pass.py"), SCRIPT.with_name("validate-second-pass-dataset.py")):
            source = binding(path.parent, path.name)
            freeze(v.Dataset(path.parent), path.name, "toolchain/" + path.name, source["sha256"])
        # Generated paths stay relative to the new run directory, including spaces.
        with (output / "manifest.csv").open("w", encoding="utf-8", newline="") as manifest:
            for frame in frames:
                manifest.write(f'{frame["frame_id"]},{frame["timestamp_ns"]},dataset/{frame["path"]}\n')
        (output / "clock.txt").write_text("".join(f'{f["timestamp_ns"]}\n' for f in frames), encoding="utf-8")
        arguments = ["implementation/replay", "dataset/" + meta["reference"]["path"], "manifest.csv", "clock.txt",
                     *[str(config[key]) for key in PARAMETERS]]
        for generated in ("manifest.csv", "clock.txt"):
            frozen_hashes[generated] = binding(output, generated)["sha256"]
        plan = {"schema_version": 1, "run_id": run_id, "code_revision_declared": code_revision,
                "input_sha256": frozen_hashes, "arguments": arguments, "timeout_seconds": timeout_seconds,
                "disjoint_references": disjoint_references, "split_peer_count": len(peers),
                "clock": "source_timestamp_each_phase_zero_work", "telemetry": "synthetic heartbeat armed Guided",
                "state_reset_policy": "fresh process per dataset; matcher and navigator retain state across all stored frames including gaps",
                "unsupported_outcomes": ["endpoint_stop", "route_readiness", "verification_accepted", "published"],
                "adapter_contract": "second_pass_replay_v1; no camera profile; other matcher options at compiled defaults",
                "environment": {"platform": platform.platform(), "python": platform.python_version()},
                "limitations": "No dynamic-library/environment pinning, binary/source attestation or adversarial filesystem protection"}
        write_json(output / "plan.json", plan)  # Closed and hashed BEFORE launch.
        frozen_hashes = dict(frozen_hashes, **{"plan.json": binding(output, "plan.json")["sha256"]})
        verify_frozen(output, frozen_hashes)
        reason = None
        execution = {"plan_sha256": frozen_hashes["plan.json"], "returncode": None}
        try:
            with (output / "raw-predictions.csv").open("wb") as stdout, (output / "replay.log").open("wb") as stderr:
                process = subprocess.run([str(executable), *arguments[1:]], cwd=output,
                                         stdout=stdout, stderr=stderr, timeout=timeout_seconds, check=False)
            execution["returncode"] = process.returncode
            if process.returncode != 0:
                reason = "replay_failed"
        except subprocess.TimeoutExpired:
            reason = "replay_timeout"
        except OSError as error:
            reason = "replay_launch_failed"
            execution["error"] = str(error)
        verify_frozen(output, frozen_hashes)
        if reason is None:
            try:
                raw = binding(output, "raw-predictions.csv")
                predictions = s.read_predictions(v.Dataset(output), raw,
                                                 frames, meta["reference"]["entry_count"])
                v.require(all(p["observation"] == "observed" and p["navigation_command_valid"] is not None
                              and all(p[k] is None for k in plan["unsupported_outcomes"]) for p in predictions),
                          "replay", "adapter output contract mismatch")
            except (ValueError, OSError, csv.Error) as error:
                reason = "replay_output_invalid"
                execution["error"] = str(error)
        if reason:
            missing_predictions(output / "predictions.csv", frames, reason)
        else:
            copy_checked(v.Dataset(output), raw["path"], output / "predictions.csv", raw["sha256"])
        run = {"schema_version": 1, "run_id": run_id, "dataset_id": meta["dataset_id"],
               "input_sha256": validation["datasets"][0]["input_sha256"], "inputs_frozen_before_run": True,
               "producer": {"kind": "external_export", "code_revision": code_revision,
                            "implementation": binding(output, "implementation/replay"),
                            "configuration": binding(output, "configuration.json"),
                            "state_reset_policy": plan["state_reset_policy"],
                            "preprocessing": "Gray8ResizePreprocessor; explicit target in configuration; source-time zero-work clock; synthetic armed Guided telemetry"},
               "policy": binding(output, "policy.json"), "predictions": binding(output, "predictions.csv")}
        write_json(output / "run.json", run)
        score = s.score(frozen_roots[0], output, frozen_roots[1:], disjoint_references)
        write_json(output / "score.json", score)
        v.require(score["status"] == "scored", "scorer", json.dumps(score.get("errors", [])))
        verify_frozen(output, frozen_hashes)
        execution.update(status="failed" if reason else "complete", missing_reason=reason,
                         output_sha256={p: binding(output, p)["sha256"] for p in
                                        ("raw-predictions.csv", "replay.log", "predictions.csv", "run.json", "score.json")})
        write_json(output / "execution.json", execution)  # Completion marker is written last.
        report.update(status="replay_failed" if reason else "exported", execution=execution,
                      run_id=run_id, output=str(output), score_status=score["status"])
    except (ValueError, OSError, csv.Error, RuntimeError, OverflowError) as error:
        report.update(status="invalid", errors=[str(error)])
        if output is not None:
            try:
                write_json(output / "failure.json", report)
            except OSError as diagnostic_error:
                report["errors"].append("cannot write failure.json: " + str(diagnostic_error))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--configuration", type=Path, required=True)
    parser.add_argument("--policy", type=Path, required=True)
    parser.add_argument("--code-revision", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--split-peer", type=Path, action="append", default=[])
    parser.add_argument("--disjoint-references", action="store_true")
    parser.add_argument("--timeout-seconds", type=int, default=300)
    args = parser.parse_args()
    report = export(args.dataset, args.output, args.binary, args.configuration, args.policy, args.code_revision,
                    args.run_id, args.split_peer, args.disjoint_references, args.timeout_seconds)
    print(json.dumps(report, indent=2, sort_keys=True, allow_nan=False))
    return 0 if report["status"] == "exported" else 3 if report["status"] == "needs_review" else 2


if __name__ == "__main__":
    sys.exit(main())
