#!/usr/bin/env python3
"""Compare two Linux edge-cache probes in interleaved fresh processes."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import statistics
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def summarize(samples):
    fields = {key: [row[key] for row in samples]
              for key in ("init_us", "first_edge_us", "repeat_edge_us")}
    fields["init_plus_first_edge_us"] = [row["init_us"] + row["first_edge_us"] for row in samples]
    for stage in ("initialized", "matched", "first_edge", "repeat_edge"):
        fields[f"rss_{stage}_kib"] = [row["rss_kib"][stage] for row in samples]
    fields["rss_init_delta_kib"] = [row["rss_kib"]["initialized"] - row["rss_kib"]["loaded"] for row in samples]
    fields["rss_edge_delta_kib"] = [row["rss_kib"]["first_edge"] - row["rss_kib"]["matched"] for row in samples]
    return {key: {"min": min(values), "median": statistics.median(values), "max": max(values)}
            for key, values in fields.items()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    executables = {name: getattr(args, name).resolve() for name in ("before", "after")}
    inventory = args.inventory.resolve()
    routes = sorted((inventory / "data/field_routes").glob("*.vhrs"))
    manifest = json.loads((inventory / "hash-verification.json").read_text(encoding="utf-8-sig"))
    expected = {entry["file"]: entry for entry in manifest}
    if not routes:
        raise ValueError("No routes found")
    for route in routes:
        entry = expected["field_routes/" + route.name]
        if digest(route) != entry["sha256"] or route.stat().st_size != entry["bytes"]:
            raise ValueError(f"Source hash mismatch: {route}")
    args.output_dir.mkdir(parents=True, exist_ok=False)
    report = {"platform": platform.platform(), "fresh_processes_per_variant_per_route": 7,
              "source_head": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
              "script_sha256": digest(Path(__file__)),
              "executables": {name: {"path": str(path), "sha256": digest(path)} for name, path in executables.items()},
              "method": "alternate before/after order; move loaded route into matcher; one middle-frame match; first and repeated edge probe; current RSS from smaps_rollup",
              "routes": []}
    for route in routes:
        data = {"before": [], "after": []}
        reference = None
        for repeat in range(7):
            for variant in (("before", "after") if repeat % 2 == 0 else ("after", "before")):
                result = subprocess.run([str(executables[variant]), str(route)], check=True, capture_output=True, text=True)
                row = json.loads(result.stdout)
                if row["first"] != row["repeat"]:
                    raise ValueError("Repeated diagnostics differ")
                signature = {key: row[key] for key in ("entries", "match", "first", "repeat")}
                if reference is not None and signature != reference:
                    raise ValueError(f"Before/after result mismatch: {route}")
                reference = signature
                data[variant].append(row)
        (args.output_dir / f"{route.stem}.json").write_text(json.dumps(data, indent=2) + "\n")
        report["routes"].append({"route": route.name, "sha256": digest(route), "results_equal": True,
                                 "before": summarize(data["before"]), "after": summarize(data["after"])})
        print(f"Compared {route.name}: results identical", flush=True)
    (args.output_dir / "summary.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
