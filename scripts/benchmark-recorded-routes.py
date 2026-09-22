#!/usr/bin/env python3
"""Offline diagnostics for a hash-verified board inventory; never connects to hardware."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def distribution(values):
    values = sorted(values)
    if not values:
        return None
    return {"count": len(values), "min_us": values[0], "max_us": values[-1],
            **{f"p{p}_us": values[math.ceil(len(values) * p / 100) - 1]
               for p in (50, 95, 99)}}


def stop_bindings(log_path):
    """Bind by explicit stop image path in each log, never by filename date/index."""
    sessions = {}
    for line in log_path.read_text().splitlines():
        log, marker, content = line.partition(":live_route_match_")
        if marker:
            event, _, fields = content.partition(" ")
            sessions.setdefault(log, []).append(
                (event, dict(token.split("=", 1) for token in fields.split() if "=" in token)))
    bindings = {}
    for log, events in sessions.items():
        start = None
        for event, fields in events:
            if event == "start":
                start = fields
            elif event == "done" and fields.get("endpoint_stop_frame_written") == "true":
                if start is None:
                    raise ValueError(f"Missing start record for {log}")
                image = Path(fields["endpoint_stop_frame_path"]).name
                binding = {"route": Path(start["route"]).name,
                           "historical_index": int(fields["endpoint_stop_route_index"]),
                           "historical_confidence": float(fields["endpoint_stop_confidence"]),
                           "source_log": log, "historical_start": start}
                if image in bindings and bindings[image] != binding:
                    raise ValueError(f"Ambiguous stop image binding: {image}")
                bindings[image] = binding
    return bindings


def benchmark(binary, route, mode, output, image=None):
    command = [str(binary), str(route), mode] + ([str(image)] if image else [])
    result = subprocess.run(command, capture_output=True, text=True, check=True)
    output.write_text(result.stdout)
    rows = [json.loads(line) for line in result.stdout.splitlines()]
    config = rows[0]
    init = [r["init_us"] for r in rows if r["kind"] == "init"]
    matches = [r for r in rows if r["kind"] == "match"]
    passes = [[r for r in matches if r["pass"] == p] for p in range(config["measured_passes"])]
    expected = config["queries_per_pass"]
    if len(init) != len(passes) or len(matches) != len(passes) * expected:
        raise ValueError("Incomplete benchmark output")
    signatures = []
    for samples in passes:
        if [r["query"] for r in samples] != list(range(expected)):
            raise ValueError("Missing/reordered benchmark samples")
        signatures.append([{k: v for k, v in r.items() if k not in ("pass", "match_us")}
                           for r in samples])
    if any(s != signatures[0] for s in signatures[1:]):
        raise ValueError("Matcher results differ between repeated passes")
    if any(not math.isfinite(r["match_us"]) or r["match_us"] < 0
           or not 0 <= r["confidence"] <= 1 for r in matches):
        raise ValueError("Invalid benchmark measurement")
    return {"config": config, "stable_results": True,
            "result_sha256": hashlib.sha256(json.dumps(signatures[0], sort_keys=True).encode()).hexdigest(),
            "init_us": init, "first_query_us": [p[0]["match_us"] for p in passes],
            "steady": distribution([r["match_us"] for p in passes for r in p[1:]]),
            "steady_per_pass": [distribution([r["match_us"] for r in p[1:]]) for p in passes],
            "valid_per_pass": sum(r["valid"] for r in passes[0]),
            "self_exact_indices_per_pass": None if image else sum(
                r["route_index"] == r["query"] for r in passes[0]),
            "first_result": signatures[0][0]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True,
                        help="New directory; existing results are never overwritten")
    args = parser.parse_args()
    build, inventory, output = (p.resolve() for p in (args.build_dir, args.inventory, args.output_dir))
    cache = (build / "CMakeCache.txt").read_text()
    if "CMAKE_BUILD_TYPE:STRING=Release\n" not in cache:
        raise ValueError("Use a Release build for timing")
    flags = [line for line in cache.splitlines() if line.startswith("VISUAL_HOMING_") and ":BOOL=" in line]
    if len(flags) != 7 or any(not line.endswith("=OFF") for line in flags):
        raise ValueError("All seven camera/output build flags must be OFF")
    binary, core = build / "gray8_matcher_benchmark", build / "visual_homing_core"
    verified = json.loads((inventory / "hash-verification.json").read_text(encoding="utf-8-sig"))
    for entry in verified:
        path = inventory / "data" / entry["file"]
        if path.stat().st_size != entry["bytes"] or sha256(path) != entry["sha256"]:
            raise ValueError(f"Inventory hash mismatch: {path}")
    routes = sorted((inventory / "data/field_routes").glob("*.vhrs"))
    images = sorted((inventory / "data/stop_frames").glob("*.pgm"))
    actual = {p.relative_to(inventory / "data").as_posix() for p in routes + images}
    if not routes or not images or actual != {e["file"] for e in verified}:
        raise ValueError("Inventory file set differs from verified manifest")
    bindings = stop_bindings(inventory / "session-summaries.log")
    for image in images:
        if image.name not in bindings or bindings[image.name]["route"] not in {r.name for r in routes}:
            raise ValueError(f"Missing route evidence for {image.name}")
    subprocess.run([str(binary), "--self-test"], check=True)
    output.mkdir(parents=True, exist_ok=False)
    repo = Path(__file__).resolve().parents[1]
    report = {"schema": 1, "platform": platform.platform(), "machine": platform.machine(),
              "cpu": next((line.split(":", 1)[1].strip() for line in Path("/proc/cpuinfo").read_text().splitlines()
                           if line.startswith("model name")), "unknown"),
              "source_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip(),
              "binary_sha256": {p.name: sha256(p) for p in (binary, core)},
              "tool_source_sha256": {str(p.relative_to(repo)): sha256(p) for p in
                                     (Path(__file__).resolve(), repo / "core/tools/gray8_matcher_benchmark.cpp")},
              "cache_sha256": sha256(build / "CMakeCache.txt"), "build_flags": flags,
              "inventory": verified, "session_summary_sha256": sha256(inventory / "session-summaries.log"),
              "percentiles": "nearest rank; excludes first query of each pass",
              "timing_scope": "steady_clock around matcher.match only; initialization includes route copy and edge cache; no I/O, frame copy, pacing, camera or live pipeline",
              "routes": [], "stop_frames": []}
    commands_path = build / "compile_commands.json"
    if commands_path.exists():
        report["compile_commands"] = [c for c in json.loads(commands_path.read_text())
                                       if c["file"].endswith(("gray8_route_matcher.cpp", "gray8_matcher_benchmark.cpp"))]
    env = {k: v for k, v in os.environ.items() if not k.startswith("VISUAL_HOMING_")}
    for route in routes:
        print(f"Checking {route.name}", flush=True)
        controls = {}
        for option in ("--self-match-route", "--perturb-route", "--route-distinctiveness"):
            result = subprocess.run([str(core), option, str(route)], text=True, capture_output=True, env=env)
            (output / f"{route.stem}{option}.log").write_text(result.stdout + result.stderr)
            if result.returncode not in (0, 2) or (option == "--self-match-route" and result.returncode != 0):
                raise ValueError(f"Control execution failed: {option} {route.name}")
            controls[option] = {"exit_code": result.returncode, "output": result.stdout.strip()}
        modes = {mode: benchmark(binary, route, mode, output / f"{route.stem}-{mode}.jsonl")
                 for mode in ("self-global", "self-window30", "self-window30-scale")}
        report["routes"].append({"route": route.name, "controls": controls, "modes": modes})
    for image in images:
        binding = bindings[image.name]
        route = inventory / "data/field_routes" / binding["route"]
        measured = benchmark(binary, route, "pgm", output / f"{image.stem}.jsonl", image)
        candidates = measured["first_result"]["candidates"]
        gap = candidates[0]["confidence"] - candidates[1]["confidence"] if len(candidates) > 1 else None
        report["stop_frames"].append({"image": image.name, **binding, "measured": measured,
                                      "top_two_gap": gap,
                                      "same_as_historical_index": measured["first_result"]["route_index"] == binding["historical_index"]})
    (output / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Saved {len(routes)} route reports and {len(images)} stop-frame reports: {output / 'summary.json'}")


if __name__ == "__main__":
    main()
