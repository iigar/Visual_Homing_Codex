"""Synthetic quadrant geometry and separate trusted replay RSS diagnostics."""
import csv
import hashlib
import os
from pathlib import Path
import signal
import struct
import subprocess

import test_second_pass_campaign as f


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def side_length(value):
    require(type(value) is int and 2 <= value <= 256 and value % 2 == 0,
            "square sides must be even integers in [2, 256]")
    return value


def quadrants(pixels, side):
    side_length(side)
    require(len(pixels) == 4, "expected four quadrant values")
    half = side // 2
    return ((bytes([pixels[0]]) * half + bytes([pixels[1]]) * half) * half
            + (bytes([pixels[2]]) * half + bytes([pixels[3]]) * half) * half)


def rows(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def apply_resolution(inputs, request, side, source_side=None):
    """Keep labels/IDs/config thresholds; expand pixels by exact quadrants."""
    side_length(side)
    source_side = side if source_side is None else side_length(source_side)
    if side == source_side == 2:
        return request
    for name in f.CONFIGURATIONS:
        config = f.read(inputs, name + ".json")
        config.update(target_width=side, target_height=side)
        f.e.write_json(inputs / (name + ".json"), config)
    for index, dataset in enumerate(request["datasets"]):
        root = inputs / dataset["path"]
        meta = f.read(root, "dataset.json")
        frames, labels = rows(root / "frames.csv"), rows(root / "labels.csv")
        route = bytearray(struct.pack("<4sHHBBHI", b"VHRS", 1, 16, 1, 0, 0, 4))
        for reference, values in enumerate(f.REFERENCES):
            payload = quadrants(values[index:] + values[:index], side)
            route.extend(struct.pack("<QQhfHHBBHI", reference, 10 + reference, 0, 0.0, side, side, 1, 0, 0, len(payload)))
            route.extend(payload)
        (root / meta["reference"]["path"]).write_bytes(route)
        meta["reference"]["sha256"] = f.t.t.fixtures.digest(route)
        for frame in frames:
            path = root / frame["path"]
            original = path.read_bytes()
            require(original.startswith(b"P5\n2 2\n255\n") and len(original) == 15, "expected original 2x2 fixture")
            data = f"P5\n{source_side} {source_side}\n255\n".encode() + quadrants(original[-4:], source_side)
            path.write_bytes(data)
            frame.update(width=str(source_side), height=str(source_side), sha256=f.t.t.fixtures.digest(data))
        meta["capture"].update(native_width=source_side, native_height=source_side,
                                profile_id=f"synthetic-quadrants-{source_side}")
        f.t.t.fixtures.save_dataset(root, meta, frames, labels)
        validation = f.v.validate_datasets([root])
        require(validation["status"] == "structurally_valid", str(validation))
        dataset["input_sha256"] = validation["datasets"][0]["input_sha256"]
    for requested in request["runs"]:
        requested["configuration"] = f.e.binding(inputs, requested["configuration"]["path"])
    f.e.write_json(inputs / "request.json", request)
    return request


def run_timed(command, cwd, stdout, stderr, timeout):
    """Kill the entire diagnostic group on timeout/interruption, including replay."""
    with subprocess.Popen(command, cwd=cwd, stdout=stdout, stderr=stderr, start_new_session=True) as process:
        try:
            process.communicate(timeout=timeout)
        except BaseException:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
            raise
        return process.returncode


def measure_replays(archive, output, binary, time_tool, checked):
    """Additional executions outside API timing; never run the archived binary."""
    require(checked["status"] == "verified" and checked["archive_integrity_verified"] is True, "unverified archive")
    archive, output, binary, time_tool = (Path(p).resolve() for p in (archive, output, binary, time_tool))
    require(output != archive and archive not in output.parents, "diagnostics must be outside archive")
    require(binary != archive and archive not in binary.parents, "diagnostics require the trusted external binary")
    require(time_tool != archive and archive not in time_tool.parents, "diagnostics require the trusted external time tool")
    binary_hash, tool_hash = digest(binary), digest(time_tool)
    before = f.snapshot(archive)
    output.mkdir()
    records = []
    for index, row in enumerate(checked["collection"]["runs"]):
        bundle = archive / "collection" / row["path"]
        require(digest(bundle / "plan.json") == row["bundle"]["plan_sha256"], "plan changed before diagnostic")
        f.e.verify_frozen(bundle, row["bundle"]["bundle_sha256"])
        plan = f.read(bundle, "plan.json")
        require(plan["input_sha256"]["implementation/replay"] == binary_hash, "trusted binary differs from plan")
        require(plan["arguments"][0] == "implementation/replay", "unexpected executable in plan")
        prefix = output / f"{index:04d}"
        stats, stdout_path, stderr_path = (prefix.with_suffix(s) for s in (".rss.txt", ".csv", ".stderr"))
        command = [str(time_tool), "-q", "-f", "%M", "-o", str(stats), "--", str(binary), *plan["arguments"][1:]]
        with stdout_path.open("xb") as stdout, stderr_path.open("xb") as stderr:
            code = run_timed(command, bundle, stdout, stderr, plan["timeout_seconds"])
        require(code == 0, f"diagnostic replay failed: {row['run_id']} exit {code}")
        rss = int(stats.read_text().strip())
        require(rss > 0, "missing child peak RSS")
        prediction_hash = digest(stdout_path)
        require(prediction_hash == row["bundle"]["bundle_sha256"]["raw-predictions.csv"], "diagnostic predictions differ")
        records.append(dict(run_id=row["run_id"], plan_sha256=row["bundle"]["plan_sha256"],
                            max_rss_kib=rss, raw_predictions_sha256=prediction_hash, returncode=code))
    require(f.snapshot(archive) == before, "diagnostics mutated archive")
    require(digest(binary) == binary_hash and digest(time_tool) == tool_hash, "diagnostic executable changed")
    result = dict(records=records, binary_sha256=binary_hash, time_sha256=tool_hash,
                  executions=len(records), peak_max_rss_kib=max(row["max_rss_kib"] for row in records),
                  scope="GNU Time %M per additional trusted replay process, after API timing; not simultaneous tree peak")
    f.e.write_json(output / "summary.json", result)
    return result
