#!/usr/bin/env python3
"""Linux-only synthetic campaign scaling probe; no performance acceptance threshold.

Uses the existing campaign test fixtures and real offline replay binary. Each
phase runs in a fresh Python process. See the baseline document for scope.
"""
import argparse
import copy
import cProfile
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import pstats
import resource
import shutil
import statistics
import subprocess
import sys
import tarfile
import tempfile
import time

import test_second_pass_campaign as f

SCRIPT = Path(__file__).resolve()
REPO = SCRIPT.parents[2]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def write(path, value):
    f.e.write_json(path, value)


def verify_collection(report, request, archive, inputs):
    require(report["status"] == "verified", str(report.get("errors")))
    for flag in ("archive_integrity_verified", "saved_report_reproduced", "results_released"):
        require(report[flag] is True, flag)
    for flag in ("binary_executed", "semantic_independence_verified", "chronology_authenticated"):
        require(report[flag] is False, flag)
    collection = report["collection"]
    require(len(collection["runs"]) == len(request["runs"]), "dropped run")
    groups = [(collection["coverage"], request["runs"])]
    for split in f.SPLITS:
        rows = [row for row in request["runs"] if row["dataset_id"] == split]
        groups.extend((collection[key][split], rows) for key in ("by_dataset", "by_split"))
    for coverage, rows in groups:
        counts = {key: len(rows) if key == "verified" else 0 for key in f.c.RUN_STATES}
        require(coverage == dict(expected_runs=len(rows), runs_by_status=counts,
            expected_frame_run_pairs=f.FRAMES * len(rows),
            frame_run_pairs_by_status={key: n * f.FRAMES for key, n in counts.items()},
            all_runs_verified=True), "coverage differs from full request")
    predictions, metrics = [], []
    for index, (requested, row) in enumerate(zip(request["runs"], collection["runs"])):
        require((row["run_id"], row["dataset_id"], row["path"], row["status"]) ==
                (requested["run_id"], requested["dataset_id"], f"bundles/{index:04d}", "verified"), "run identity")
        bundle, config_policy = row["bundle"], f.variants(requested)
        require(bundle["peer_scope"]["peer_ids"] == [s for s in f.SPLITS if s != requested["dataset_id"]], "peers")
        require(bundle["peer_scope"]["disjoint_references"] is True, "reference scope")
        metric = bundle["score"]["metrics"]
        require(tuple(metric[key] for key in ("tp", "fp", "fn")) == f.expected_metrics(requested), "metric oracle")
        require(metric["recall"]["denominator"] == (6 if config_policy[1] == "inclusive" else 5), "positive denominator")
        require(metric["prediction_coverage"] == dict(numerator=8, denominator=8, value=1), "prediction coverage")
        path = archive / "collection" / row["path"] / "predictions.csv"
        with path.open(newline="") as stream:
            actual = list(csv.DictReader(stream))
        with (inputs / requested["dataset_id"] / "frames.csv").open(newline="") as stream:
            source = list(csv.DictReader(stream))
        require(len(actual) == len(source) == f.FRAMES, "frame count")
        for i, (prediction, frame) in enumerate(zip(actual, source)):
            require(all(prediction[key] == frame[key] for key in ("sequence", "frame_id", "timestamp_ns")), "source identity")
            expected = f.MATCHES[i] if config_policy[0] == "near" else None
            require(prediction["reference_index"] == ("" if expected is None else str(expected)), "prediction oracle")
            require(prediction["observation"] == "observed", "missing prediction")
        predictions.append(digest(path))
        metrics.append(metric)
    return dict(coverage=collection["coverage"], prediction_sha256=predictions, metrics=metrics)


def worker(job_path):
    job = json.loads(job_path.read_text())
    archive, inputs = Path(job["archive"]), Path(job["inputs"])
    request = f.read(inputs, "request.json")
    action = (lambda: f.r.run(inputs / "request.json", archive, Path(job["binary"]), job["revision"])) \
        if job["phase"] == "runner" else (lambda: f.a.check(archive, job["plan_sha256"], job["state_sha256"]))
    profiler = cProfile.Profile() if job["profile"] else None
    started = time.perf_counter()
    if profiler:
        profiler.enable()
    report = action()
    if profiler:
        profiler.disable()
    elapsed = time.perf_counter() - started
    # Capture before oracle checks, output serialization and profiler formatting.
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    measurement = dict(phase=job["phase"], runs=len(request["runs"]), profiled=job["profile"],
                       wall_seconds=elapsed, worker_peak_rss_kib=rss, status=report["status"])
    require(report["status"] == "verified", str(report.get("errors")))
    if job["phase"] == "runner":
        require(report == f.read(archive, "runner-state.json"), "saved state")
        require(report["exporter_calls"] == len(request["runs"]), "exporter count")
        require(all(row["status"] == "verified" for row in report["runs"]), "non-verified slot")
    else:
        measurement["oracles"] = verify_collection(report, request, archive, inputs)
    write(job_path.with_suffix(".report.json"), report)
    if profiler:
        profiler.dump_stats(str(job_path.with_suffix(".pstats")))
        stream = io.StringIO()
        stats = pstats.Stats(profiler, stream=stream)
        stats.sort_stats("cumulative").print_stats(30)
        stats.sort_stats("tottime").print_stats(30)
        job_path.with_suffix(".profile.txt").write_text(stream.getvalue())
        write(job_path.with_suffix(".profile.json"), [dict(file=file, line=line, function=name,
            primitive_calls=values[0], calls=values[1], self_seconds=values[2], cumulative_seconds=values[3])
            for (file, line, name), values in stats.stats.items()])
    write(job_path.with_suffix(".measurement.json"), measurement)


def run_pair(root, count, binary, revision, copied_script, profile=False):
    root.mkdir()
    inputs, archive = root / "inputs", root / "archive"
    request = f.make_campaign(inputs)
    base = request["runs"]
    request["runs"] = [dict(copy.deepcopy(row), run_id=f"repeat-{repeat:02d}-" + row["run_id"])
                       for repeat in range(count // 12) for row in base]
    write(inputs / "request.json", request)
    source_before = f.snapshot(inputs)
    results = {}
    for phase in ("runner", "audit"):
        job = dict(phase=phase, inputs=str(inputs), archive=str(archive), binary=str(binary),
                   revision=revision, profile=profile)
        if phase == "audit":
            before = f.snapshot(archive)
            job.update(plan_sha256=digest(archive / "runner-plan.json"), state_sha256=digest(archive / "runner-state.json"))
        job_path = root / (phase + ".job.json")
        write(job_path, job)
        subprocess.run([sys.executable, str(copied_script), "--worker", str(job_path)], check=True,
                       env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"})
        results[phase] = json.loads(job_path.with_suffix(".measurement.json").read_text())
        if phase == "audit":
            require(before == f.snapshot(archive), "audit mutated archive")
            write(root / "archive-inventory.json", before)
    require(source_before == f.snapshot(inputs), "mutated inputs")
    print(json.dumps(dict(sample=root.name, **{key: {k: v for k, v in value.items() if k != "oracles"}
                                             for key, value in results.items()})), flush=True)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--runs", type=int, nargs="+", default=[12, 24, 48])
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--work-dir", type=Path, default=Path("/tmp"))
    parser.add_argument("--worker", type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()
    require(sys.platform == "linux", "Linux ru_maxrss units required")
    if args.worker:
        worker(args.worker)
        return
    require(args.binary is not None and args.output is not None, "--binary and --output required")
    require(all(12 <= n <= 96 and n % 12 == 0 for n in args.runs) and len(set(args.runs)) == len(args.runs),
            "unique run counts must be multiples of 12 in [12, 96]")
    require(1 <= args.repeats <= 5, "repeats must be in [1, 5]")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip()
    sources = [REPO / "scripts" / name for name in f.a.TOOLS]
    sources += [SCRIPT.with_name(name) for name in (SCRIPT.name, "test_second_pass_campaign.py",
                "test_second_pass_exporter.py", "test_second_pass_scorer.py", "test_second_pass_dataset.py")]
    source_hashes = {p.relative_to(REPO).as_posix(): digest(p) for p in sources}
    binary_hash = digest(args.binary)
    with tempfile.TemporaryDirectory(prefix="campaign-scaling-", dir=args.work_dir) as temporary:
        work = Path(temporary)
        for relative, expected in source_hashes.items():
            target = work / "source" / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(REPO / relative, target)
            require(digest(target) == expected, "source copy differs")
        binary = work / "replay"
        shutil.copy2(args.binary, binary)
        binary.chmod(0o700)
        require(digest(binary) == binary_hash, "binary copy differs")
        script = work / "source" / SCRIPT.relative_to(REPO)
        summary = dict(schema_version=1, revision_declared=revision, source_sha256=source_hashes,
            binary_sha256=binary_hash, environment=dict(python=sys.version, executable=sys.executable,
            kernel=platform.release(), machine=platform.machine(), cpu_count=os.cpu_count(),
            cpu_affinity=sorted(os.sched_getaffinity(0)), work_root=str(work),
            filesystem=subprocess.check_output(["stat", "-f", "-c", "%T", str(work)], text=True).strip(),
            cpu_model=next((line.split(":", 1)[1].strip() for line in Path("/proc/cpuinfo").read_text().splitlines()
                            if line.startswith("model name")), "unknown")),
            scope="API wall time; fresh Python worker self peak RSS in KiB, excludes replay children; warm cache, no eviction",
            run_counts=args.runs, repeats=args.repeats, samples=[], aggregates=[])
        run_pair(work / "warmup", 12, binary, revision, script)
        # Round-robin sizes reduce confounding from a monotonic size/time order.
        for repeat in range(args.repeats):
            for count in args.runs:
                name = f"runs-{count:02d}-repeat-{repeat:02d}"
                result = run_pair(work / name, count, binary, revision, script)
                summary["samples"].append(dict(sample=name, runs=count, phases=result))
                write(output / "summary.partial.json", summary)
        for count in args.runs:
            for phase in ("runner", "audit"):
                samples = [s["phases"][phase] for s in summary["samples"] if s["runs"] == count]
                summary["aggregates"].append(dict(runs=count, phase=phase, **{key: dict(
                    median=statistics.median(s[key] for s in samples), minimum=min(s[key] for s in samples),
                    maximum=max(s[key] for s in samples)) for key in ("wall_seconds", "worker_peak_rss_kib")}))
        summary["profile"] = run_pair(work / "profile", max(args.runs), binary, revision, script, profile=True)
        require(source_hashes == {p.relative_to(REPO).as_posix(): digest(p) for p in sources}, "source changed during probe")
        require(digest(args.binary) == binary_hash, "binary changed during probe")
        for phase in ("runner", "audit"):
            for suffix in ("profile.txt", "profile.json", "pstats"):
                shutil.copy2(work / "profile" / f"{phase}.job.{suffix}", output / f"{phase}.{suffix}")
        # Archive preservation is outside all measured intervals. No extraction/execution here.
        archive_path = output / "native-evidence.tar.gz"
        with tarfile.open(archive_path, "w:gz") as tar:
            tar.add(work, arcname="evidence")
        summary["evidence_sha256"] = digest(archive_path)
        write(output / "summary.json", summary)
        print(json.dumps(summary["aggregates"], indent=2), flush=True)


if __name__ == "__main__":
    main()
