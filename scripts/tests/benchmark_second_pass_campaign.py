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
import second_pass_resolution as resolution
import second_pass_catalog as catalog

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


def frame_count(value):
    require(type(value) is int and 8 <= value <= 512 and value % 8 == 0,
            "frame counts must be multiples of 8 in [8, 512]")
    return value


def vm_hwm_kib():
    # Current executable image peak; getrusage can retain the pre-exec peak.
    for line in Path("/proc/self/status").read_text().splitlines():
        if line.startswith("VmHWM:"):
            _, value, unit = line.split()
            require(unit == "kB" and int(value) > 0, "invalid Linux VmHWM")
            return int(value)
    raise RuntimeError("Linux /proc/self/status has no VmHWM")


def make_inputs(inputs, frames):
    """Repeat the fixed eight-frame block, keeping references/config/policy fixed."""
    frame_count(frames)
    request = f.make_campaign(inputs)
    if frames == f.FRAMES:
        return request
    for dataset in request["datasets"]:
        root = inputs / dataset["path"]
        meta = f.read(root, "dataset.json")
        with (root / "frames.csv").open(newline="") as stream:
            source = list(csv.DictReader(stream))
        with (root / "labels.csv").open(newline="") as stream:
            labels = list(csv.DictReader(stream))
        expanded, annotations = [], []
        for index in range(frames):
            original, label = source[index % f.FRAMES], labels[index % f.FRAMES]
            identity = dict(sequence=str(index), frame_id=str(int(source[0]["frame_id"]) + index),
                            timestamp_ns=str(int(source[0]["timestamp_ns"]) + index * 100))
            path = f"frames/{index}.pgm"
            if index >= f.FRAMES:
                shutil.copyfile(root / original["path"], root / path)
            expanded.append({**original, **identity, "path": path})
            annotations.append({**label, "frame_id": identity["frame_id"]})
        meta["sequence"].update(acquired_count=frames, stored_count=frames)
        meta["sequence"]["actual"].update(last_frame_id=int(expanded[-1]["frame_id"]),
                                          last_timestamp_ns=int(expanded[-1]["timestamp_ns"]))
        f.t.t.fixtures.save_dataset(root, meta, expanded, annotations)
        validation = f.v.validate_datasets([root])
        require(validation["status"] == "structurally_valid", str(validation))
        dataset["input_sha256"] = validation["datasets"][0]["input_sha256"]
    write(inputs / "request.json", request)
    return request


def verify_collection(report, request, archive, inputs, frames=f.FRAMES):
    blocks = frame_count(frames) // f.FRAMES
    require(report["status"] == "verified", str(report.get("errors")))
    for flag in ("archive_integrity_verified", "saved_report_reproduced", "results_released"):
        require(report[flag] is True, flag)
    for flag in ("binary_executed", "semantic_independence_verified", "chronology_authenticated"):
        require(report[flag] is False, flag)
    collection = report["collection"]
    require(len(collection["runs"]) == len(request["runs"]), "dropped run")
    datasets = {row["dataset_id"]: row for row in request["datasets"]}
    splits = {row["split"] for row in datasets.values()}
    require(set(collection["by_dataset"]) == set(datasets), "dataset coverage keys")
    require(set(collection["by_split"]) == splits, "split coverage keys")
    groups = [(collection["coverage"], request["runs"])]
    for did in datasets:
        rows = [row for row in request["runs"] if row["dataset_id"] == did]
        require(rows, "catalog dataset without runs")
        groups.append((collection["by_dataset"][did], rows))
    for split in splits:
        rows = [row for row in request["runs"] if datasets[row["dataset_id"]]["split"] == split]
        groups.append((collection["by_split"][split], rows))
    for coverage, rows in groups:
        counts = {key: len(rows) if key == "verified" else 0 for key in f.c.RUN_STATES}
        require(coverage == dict(expected_runs=len(rows), runs_by_status=counts,
            expected_frame_run_pairs=frames * len(rows),
            frame_run_pairs_by_status={key: n * frames for key, n in counts.items()},
            all_runs_verified=True), "coverage differs from full request")
    predictions, metrics = [], []
    for index, (requested, row) in enumerate(zip(request["runs"], collection["runs"])):
        require((row["run_id"], row["dataset_id"], row["path"], row["status"]) ==
                (requested["run_id"], requested["dataset_id"], f"bundles/{index:04d}", "verified"), "run identity")
        bundle, config_policy = row["bundle"], f.variants(requested)
        require(bundle["peer_scope"]["peer_ids"] == [did for did in datasets if did != requested["dataset_id"]], "peers")
        require(bundle["peer_scope"]["disjoint_references"] is True, "reference scope")
        metric = bundle["score"]["metrics"]
        require(tuple(metric[key] for key in ("tp", "fp", "fn")) ==
                tuple(n * blocks for n in f.expected_metrics(requested)), "metric oracle")
        require(metric["frames"] == frames, "metric frame count")
        require(metric["recall"]["denominator"] == (6 if config_policy[1] == "inclusive" else 5) * blocks, "positive denominator")
        require(metric["prediction_coverage"] == dict(numerator=frames, denominator=frames, value=1), "prediction coverage")
        require(metric["unscorable_fraction"]["numerator"] ==
                (1 if config_policy[1] == "inclusive" else 2) * blocks, "unscorable denominator")
        require(metric["negative_prediction_coverage"] == dict(numerator=blocks, denominator=blocks, value=1), "negative coverage")
        path = archive / "collection" / row["path"] / "predictions.csv"
        with path.open(newline="") as stream:
            actual = list(csv.DictReader(stream))
        with (inputs / datasets[requested["dataset_id"]]["path"] / "frames.csv").open(newline="") as stream:
            source = list(csv.DictReader(stream))
        require(len(actual) == len(source) == frames, "frame count")
        for i, (prediction, frame) in enumerate(zip(actual, source)):
            require(all(prediction[key] == frame[key] for key in ("sequence", "frame_id", "timestamp_ns")), "source identity")
            expected = f.MATCHES[i % f.FRAMES] if config_policy[0] == "near" else None
            require(prediction["reference_index"] == ("" if expected is None else str(expected)), "prediction oracle")
            require(prediction["observation"] == "observed", "missing prediction")
            valid = "true" if expected is not None else "false"
            require(prediction["match_valid"] == prediction["navigation_command_valid"] == valid, "validity oracle")
            require(all(prediction[key] == "" for key in f.c.b.UNSUPPORTED), "unsupported gate")
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
    initial_hwm, initial_rusage = vm_hwm_kib(), resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    started = time.perf_counter()
    if profiler:
        profiler.enable()
    report = action()
    if profiler:
        profiler.disable()
    elapsed = time.perf_counter() - started
    # Capture before oracle checks, output serialization and profiler formatting.
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    image_rss = vm_hwm_kib()
    measurement = dict(phase=job["phase"], runs=len(request["runs"]), profiled=job["profile"],
                       frames_per_dataset=job["frames_per_dataset"],
                       side_pixels=job["side_pixels"],
                       dataset_count=len(request["datasets"]),
                       worker_vm_hwm_kib=image_rss, worker_initial_vm_hwm_kib=initial_hwm,
                       worker_initial_rusage_maxrss_kib=initial_rusage,
                       wall_seconds=elapsed, worker_peak_rss_kib=rss, status=report["status"])
    require(report["status"] == "verified", str(report.get("errors")))
    if job["phase"] == "runner":
        require(report == f.read(archive, "runner-state.json"), "saved state")
        require(report["exporter_calls"] == len(request["runs"]), "exporter count")
        require(all(row["status"] == "verified" for row in report["runs"]), "non-verified slot")
    else:
        measurement["oracles"] = verify_collection(report, request, archive, inputs, job["frames_per_dataset"])
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


def run_pair(root, count, binary, revision, copied_script, profile=False, frames=f.FRAMES, side=2, time_tool=None, datasets=3):
    root.mkdir()
    inputs, archive = root / "inputs", root / "archive"
    request = make_inputs(inputs, frames)
    request = resolution.apply_resolution(inputs, request, side)
    request = catalog.expand_catalog(inputs, request, datasets)
    base = request["runs"]
    request["runs"] = [dict(copy.deepcopy(row), run_id=f"repeat-{repeat:02d}-" + row["run_id"])
                       for repeat in range(count // 12) for row in base]
    write(inputs / "request.json", request)
    source_before = f.snapshot(inputs)
    results = {}
    for phase in ("runner", "audit"):
        job = dict(phase=phase, inputs=str(inputs), archive=str(archive), binary=str(binary),
                   revision=revision, profile=profile, frames_per_dataset=frames, side_pixels=side)
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
    if time_tool is not None:
        checked = json.loads((root / "audit.job.report.json").read_text())
        results["replay_rss"] = resolution.measure_replays(archive, root / "replay-rss", binary, time_tool, checked)
    print(json.dumps(dict(sample=root.name, **{key: {k: v for k, v in value.items() if k not in ("oracles", "records")}
                                             for key, value in results.items()})), flush=True)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--output", type=Path)
    dimension = parser.add_mutually_exclusive_group()
    dimension.add_argument("--runs", type=int, nargs="+", help="vary runs at fixed 8 frames (default: 12 24 48)")
    dimension.add_argument("--frames", type=int, nargs="+", help="vary frames per dataset at fixed 12 runs")
    dimension.add_argument("--resolution", type=int, nargs="+", help="vary square sides at fixed 64 frames and 12 runs")
    dimension.add_argument("--datasets", type=int, nargs="+", help="vary 3/6/12 catalog entries at fixed 12 runs, 8 frames, 32x32 pixels")
    parser.add_argument("--large-archive", action="store_true", help="with --frames: fix 12 catalog entries and 32x32 pixels")
    parser.add_argument("--replay-rss", action="store_true", help="additional trusted replays under GNU Time, outside API timing")
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--work-dir", type=Path, default=Path("/tmp"))
    parser.add_argument("--worker", type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()
    require(sys.platform == "linux", "Linux ru_maxrss units required")
    if args.worker:
        worker(args.worker)
        return
    require(args.binary is not None and args.output is not None, "--binary and --output required")
    require(not args.large_archive or args.frames is not None, "--large-archive requires --frames")
    axis, counts = next(((axis, values) for axis, values in (
        ("dataset_count", args.datasets), ("side_pixels", args.resolution),
        ("frames_per_dataset", args.frames), ("runs", args.runs)) if values is not None), ("runs", [12, 24, 48]))
    require(len(set(counts)) == len(counts), "counts must be unique")
    if axis == "runs":
        require(all(12 <= n <= 96 and n % 12 == 0 for n in counts), "run counts must be multiples of 12 in [12, 96]")
    elif axis == "frames_per_dataset":
        for count in counts:
            frame_count(count)
    elif axis == "side_pixels":
        for count in counts:
            resolution.side_length(count)
    else:
        for count in counts:
            catalog.dataset_count(count)
    require(not args.replay_rss or axis == "side_pixels", "--replay-rss requires --resolution")
    def shape(count):
        if axis == "runs":
            return count, f.FRAMES, 2, 3
        if axis == "frames_per_dataset":
            return 12, count, 32 if args.large_archive else 2, 12 if args.large_archive else 3
        if axis == "side_pixels":
            return 12, 64, count, 3
        return 12, f.FRAMES, 32, count
    require(1 <= args.repeats <= 5, "repeats must be in [1, 5]")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip()
    sources = [REPO / "scripts" / name for name in f.a.TOOLS]
    sources += [SCRIPT.with_name(name) for name in (SCRIPT.name, "second_pass_resolution.py", "second_pass_catalog.py", "test_second_pass_campaign.py",
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
        time_tool, time_hash = None, None
        if args.replay_rss:
            system_time = Path("/usr/bin/time")
            time_hash = digest(system_time)
            time_tool = work / "gnu-time"
            shutil.copy2(system_time, time_tool)
            time_tool.chmod(0o700)
            require(digest(time_tool) == time_hash, "GNU Time copy differs")
        script = work / "source" / SCRIPT.relative_to(REPO)
        summary = dict(schema_version=1, revision_declared=revision, source_sha256=source_hashes,
            binary_sha256=binary_hash, environment=dict(python=sys.version, executable=sys.executable,
            kernel=platform.release(), machine=platform.machine(), cpu_count=os.cpu_count(),
            cpu_affinity=sorted(os.sched_getaffinity(0)), work_root=str(work),
            filesystem=subprocess.check_output(["stat", "-f", "-c", "%T", str(work)], text=True).strip(),
            cpu_model=next((line.split(":", 1)[1].strip() for line in Path("/proc/cpuinfo").read_text().splitlines()
                            if line.startswith("model name")), "unknown")),
            scope="API wall time; Linux VmHWM current image peak includes imports; legacy getrusage peak may include pre-exec parent; both exclude replay children; warm cache, no eviction",
            dimension=axis, run_counts=counts if axis == "runs" else [12],
            frame_counts=counts if axis == "frames_per_dataset" else [64 if axis == "side_pixels" else f.FRAMES],
            square_sides=counts if axis == "side_pixels" else [32 if axis == "dataset_count" or args.large_archive else 2],
            dataset_counts=counts if axis == "dataset_count" else [12 if args.large_archive else 3],
            large_archive=args.large_archive, time_sha256=time_hash,
            repeats=args.repeats, samples=[], aggregates=[])
        run_pair(work / "warmup", 12, binary, revision, script,
                 frames=64 if axis == "side_pixels" else f.FRAMES,
                 side=32 if axis == "dataset_count" or args.large_archive else 2,
                 datasets=12 if args.large_archive else 3, time_tool=time_tool)
        # Round-robin sizes reduce confounding from a monotonic size/time order.
        for repeat in range(args.repeats):
            for count in counts:
                runs, frames, side, datasets = shape(count)
                name = f"{axis}-{count:02d}-repeat-{repeat:02d}"
                result = run_pair(work / name, runs, binary, revision, script, frames=frames, side=side, time_tool=time_tool, datasets=datasets)
                summary["samples"].append(dict(sample=name, runs=runs, frames_per_dataset=frames, side_pixels=side, dataset_count=datasets, phases=result))
                write(output / "summary.partial.json", summary)
        for count in counts:
            for phase in ("runner", "audit"):
                samples = [s["phases"][phase] for s in summary["samples"] if s[axis] == count]
                runs, frames, side, datasets = shape(count)
                summary["aggregates"].append(dict(runs=runs, frames_per_dataset=frames, side_pixels=side, dataset_count=datasets, phase=phase, **{key: dict(
                    median=statistics.median(s[key] for s in samples), minimum=min(s[key] for s in samples),
                    maximum=max(s[key] for s in samples)) for key in ("wall_seconds", "worker_peak_rss_kib", "worker_vm_hwm_kib")}))
        if time_tool is not None:
            summary["replay_rss_aggregates"] = []
            for side in counts:
                records = [row for sample in summary["samples"] if sample["side_pixels"] == side
                           for row in sample["phases"]["replay_rss"]["records"]]
                values = [row["max_rss_kib"] for row in records]
                summary["replay_rss_aggregates"].append(dict(side_pixels=side, executions=len(records),
                    max_rss_kib=dict(median=statistics.median(values), minimum=min(values), maximum=max(values))))
        runs, frames, side, datasets = shape(max(counts))
        summary["profile"] = run_pair(work / "profile", runs, binary, revision, script, profile=True, frames=frames, side=side, time_tool=time_tool, datasets=datasets)
        require(source_hashes == {p.relative_to(REPO).as_posix(): digest(p) for p in sources}, "source changed during probe")
        require(digest(args.binary) == binary_hash, "binary changed during probe")
        if time_tool is not None:
            require(digest(time_tool) == time_hash and digest(Path("/usr/bin/time")) == time_hash, "GNU Time changed during probe")
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
