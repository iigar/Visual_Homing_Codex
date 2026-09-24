#!/usr/bin/env python3
"""Compare identical matcher benchmark harnesses linked to before/after implementations."""
import argparse
import importlib.util
import json
from pathlib import Path
import platform
import subprocess


spec = importlib.util.spec_from_file_location(
    "recorded_benchmark", Path(__file__).with_name("benchmark-recorded-routes.py"))
recorded = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recorded)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    inventory = args.inventory.resolve()
    executables = {name: getattr(args, name).resolve() for name in ("before", "after")}
    manifest = json.loads((inventory / "hash-verification.json").read_text(encoding="utf-8-sig"))
    routes = sorted((inventory / "data/field_routes").glob("*.vhrs"))
    images = sorted((inventory / "data/stop_frames").glob("*.pgm"))
    if not routes or not images or {p.relative_to(inventory / "data").as_posix() for p in routes + images} != {e["file"] for e in manifest}:
        raise ValueError("Inventory file set differs from manifest")
    for entry in manifest:
        path = inventory / "data" / entry["file"]
        if path.stat().st_size != entry["bytes"] or recorded.sha256(path) != entry["sha256"]:
            raise ValueError(f"Inventory hash mismatch: {path}")
    bindings = recorded.stop_bindings(inventory / "session-summaries.log")
    cases = [(route, mode, None) for route in routes
             for mode in ("self-global", "self-window30", "self-window30-scale")]
    for image in images:
        route = inventory / "data/field_routes" / bindings[image.name]["route"]
        if route not in routes:
            raise ValueError(f"Missing route for {image}")
        cases.extend((route, mode, image) for mode in ("pgm", "pgm-scale"))
    for binary in executables.values():
        subprocess.run([str(binary), "--self-test"], check=True)
    args.output_dir.mkdir(parents=True, exist_ok=False)
    report = {"platform": platform.platform(), "fresh_processes_per_variant_per_case": 3,
              "method": "alternate before/after order; one warmup and three measured passes per process; exact result hashes include all match/candidate fields; no timestamp pacing",
              "source_head": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
              "source_sha256": {str(p): recorded.sha256(p) for p in (
                  Path(__file__), Path("scripts/benchmark-recorded-routes.py"),
                  Path("core/tools/gray8_matcher_benchmark.cpp"), Path("core/src/gray8_route_matcher.cpp"))},
              "executables": {name: {"path": str(path), "sha256": recorded.sha256(path)} for name, path in executables.items()},
              "inventory": manifest, "stop_bindings": bindings, "cases": []}
    for route, mode, image in cases:
        name = f"{(image or route).stem}-{mode}"
        data = {"before": [], "after": []}
        reference = None
        for repeat in range(3):
            for variant in (("before", "after") if repeat % 2 == 0 else ("after", "before")):
                result = recorded.benchmark(executables[variant], route, mode,
                    args.output_dir / f"{name}-{variant}-{repeat}.jsonl", image)
                signature = (result["config"], result["result_sha256"])
                if reference is not None and signature != reference:
                    raise ValueError(f"Result mismatch: {name}, {variant}, repeat {repeat}")
                reference = signature
                data[variant].append(result)
        report["cases"].append({"route": route.name, "image": image.name if image else None,
                                "mode": mode, "results_equal": True, **data})
        print(f"Compared {name}: identical results", flush=True)
    (args.output_dir / "summary.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
