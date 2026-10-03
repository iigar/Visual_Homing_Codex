#!/usr/bin/env python3
"""Read-only structural validator for the second-pass dataset v1 contract.

No replay, scoring, device access or automatic repairs. See
docs/SECOND_PASS_VALIDATOR_UA.md for the concrete schema and exit codes.
"""
import argparse
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path
import re
import stat
import struct
import sys


U64 = (1 << 64) - 1
I64 = (1 << 63) - 1
MAX_ROWS = 100000
MAX_PIXELS = 64 * 1024 * 1024
FRAME_FIELDS = "sequence frame_id timestamp_ns path sha256 width height".split()
LABEL_FIELDS = ("frame_id location reference_index_min reference_index_max "
                "endpoint direction visibility evidence_id").split()


class Invalid(ValueError):
    pass


def require(condition, where, message):
    if not condition:
        raise Invalid(f"{where}: {message}")


def obj(value, fields, where):
    require(type(value) is dict, where, "expected object")
    require(set(value) == set(fields.split()), where,
            f"expected exactly these fields: {fields}")
    return value


def string(value, where):
    require(isinstance(value, str) and bool(value.strip()), where, "expected nonempty string")
    return value


def integer(value, where, minimum=0, maximum=U64):
    require(type(value) is int and minimum <= value <= maximum, where,
            f"expected integer {minimum}..{maximum}")
    return value


def decimal(value, where, minimum=0, maximum=U64):
    require(isinstance(value, str) and re.fullmatch(r"[0-9]{1,20}", value),
            where, "expected unsigned decimal integer")
    return integer(int(value), where, minimum, maximum)


def choice(value, choices, where):
    require(isinstance(value, str) and value in choices.split(), where,
            f"expected one of: {choices}")


def boolean(value, where):
    require(type(value) is bool, where, "expected boolean")


def strings(value, where, nonempty=False):
    require(type(value) is list and (not nonempty or value), where, "expected string list")
    for item in value:
        string(item, where)
    require(len(value) == len(set(value)), where, "duplicate entries")


def sha256(value, where):
    require(isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value),
            where, "expected lowercase SHA-256")


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "JSON", f"duplicate key {key}")
        result[key] = value
    return result


def invalid_constant(value):
    raise Invalid(f"JSON: non-finite constant {value}")


def finite_float(value):
    number = float(value)
    require(math.isfinite(number), "JSON", "non-finite number")
    return number


def resolved(path):
    try:
        return path.resolve(strict=True)
    except RuntimeError as exc:
        raise Invalid(f"{path}: cannot resolve path (symlink cycle)") from exc


class HashedReader:
    """Hash the very bytes parsed, with bounded payload reads."""
    def __init__(self, stream):
        self.stream = stream
        self.digest = hashlib.sha256()

    def read(self, count):
        data = self.stream.read(count)
        self.digest.update(data)
        return data

    def exact(self, count):
        data = self.read(count)
        require(len(data) == count, "binary", "truncated input")
        return data

    def payload_hash(self, count):
        digest = hashlib.sha256()
        while count:
            data = self.exact(min(count, 1024 * 1024))
            digest.update(data)
            count -= len(data)
        return digest.hexdigest()


def pgm(reader):
    header_bytes = 0

    def byte():
        nonlocal header_bytes
        header_bytes += 1
        require(header_bytes <= 4096, "PGM", "header exceeds 4096 bytes")
        return reader.exact(1)

    def token():
        ch = byte()
        while True:
            if ch in b" \t\r\n\v\f":
                ch = byte()
            elif ch == b"#":
                while ch != b"\n":
                    ch = byte()
                ch = byte()
            else:
                break
        result = bytearray()
        while ch not in b" \t\r\n\v\f":
            result.extend(ch)
            ch = byte()
        return bytes(result), ch

    require(token()[0] == b"P5", "PGM", "expected binary P5")
    width = decimal(token()[0].decode("ascii"), "PGM.width", 1, 65535)
    height = decimal(token()[0].decode("ascii"), "PGM.height", 1, 65535)
    maximum, separator = token()
    require(maximum == b"255", "PGM", "max value must be 255")
    # Match the core reader's single separator / CRLF convention. Never strip
    # whitespace or '#' from the first pixel of the binary raster.
    if separator == b"\r" and reader.stream.peek(1)[:1] == b"\n":
        byte()
    require(width * height <= MAX_PIXELS, "PGM", "payload exceeds 64 MiB")
    pixels = reader.payload_hash(width * height)
    require(not reader.read(1), "PGM", "trailing bytes after raster")
    return width, height, pixels


def vhrs(reader):
    magic, version, size, endian, _, _, count = struct.unpack("<4sHHBBHI", reader.exact(16))
    require((magic, version, size, endian) == (b"VHRS", 1, 16, 1), "VHRS", "invalid v1 header")
    require(1 <= count <= MAX_ROWS, "VHRS", "entry count must be 1..100000")
    pixels = set()
    for index in range(count):
        entry = struct.unpack("<QQhfHHBBHI", reader.exact(34))
        width, height, fmt, payload_size = entry[4], entry[5], entry[6], entry[9]
        require(fmt == 1 and width > 0 and height > 0, f"VHRS[{index}]", "expected Gray8 dimensions")
        require(payload_size == width * height and payload_size <= MAX_PIXELS,
                f"VHRS[{index}]", "invalid payload size")
        pixels.add((width, height, reader.payload_hash(payload_size)))
    require(not reader.read(1), "VHRS", "trailing bytes after entries")
    return count, pixels


class Dataset:
    def __init__(self, root):
        self.root = resolved(Path(root))
        require(self.root.is_dir(), "dataset", "expected root directory")
        self.inputs = {}
        self.review_reasons = []
        self.unknown_used = set()
        self.frame_hashes = set()
        self.pixel_hashes = set()

    def path(self, value):
        string(value, "path")
        require(not any(c in value for c in "\\:,\r\n") and
                not any(ord(c) < 32 for c in value) and value == value.strip(),
                value, "expected portable relative path without comma/control/colon/backslash")
        parts = value.split("/")
        require(all(p not in ("", ".", "..") and p == p.strip() for p in parts), value,
                "absolute, empty, dot or parent path component")
        path = resolved(self.root / value)
        require(self.root in path.parents, value, "path escapes dataset root")
        require(stat.S_ISREG(path.stat().st_mode), value, "expected regular file")
        return path

    def read(self, relative, parser, expected=None):
        path = self.path(relative)
        with path.open("rb") as stream:
            before = os.fstat(stream.fileno())
            reader = HashedReader(stream)
            result = parser(reader)
            digest = reader.digest.hexdigest()
            after = os.fstat(stream.fileno())
            require((before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns),
                    relative, "file changed while reading")
        require(relative not in self.inputs or self.inputs[relative] == digest,
                relative, "file changed between reads")
        self.inputs[relative] = digest
        if expected is not None:
            sha256(expected, relative + ".sha256")
            require(expected == digest, relative, "SHA-256 mismatch")
        return result

    def small(self, relative, limit):
        def parse(reader):
            data = reader.read(limit + 1)
            require(len(data) <= limit, relative, "file exceeds size limit")
            return data.decode("utf-8")
        return self.read(relative, parse)

    def artifact(self, record, where):
        sha256(record["sha256"], where + ".sha256")
        def parse(reader):
            while reader.read(1024 * 1024):
                pass
        self.read(record["path"], parse, record["sha256"])

    def unknown(self, value, where, validate):
        if value is None:
            string(self.meta["unknown_reasons"].get(where), where + " unknown reason")
            self.unknown_used.add(where)
        else:
            validate(value, where)

    def parameters(self, value, where):
        require(type(value) is dict, where, "expected object")

        def visit(item, path):
            if item is None:
                self.unknown(item, path, string)
            elif isinstance(item, (dict, list)):
                children = item.items() if isinstance(item, dict) else enumerate(item)
                for key, child in children:
                    visit(child, path + "." + str(key))
        visit(value, where)

    def review(self, record, where):
        choice(record["review_status"], "pending reviewed rejected", where + ".review_status")
        self.unknown(record["reviewer_id"], where + ".reviewer_id", string)
        if record["review_status"] == "reviewed":
            string(record["reviewer_id"], where + ".reviewer_id")
        else:
            self.review_reasons.append(where + ": " + record["review_status"])

    def evidence_link(self, value, where):
        string(value, where)
        require(value in self.evidence, where, "unknown evidence_id")

    def metadata(self):
        m = json.loads(self.small("dataset.json", 4 * 1024 * 1024),
                       object_pairs_hook=unique_object, parse_constant=invalid_constant, parse_float=finite_float)
        obj(m, "schema_version dataset_id query_session_id capture_group_id split reference "
               "capture clock sequence independence annotation unknown_reasons", "dataset.json")
        self.meta = m
        integer(m["schema_version"], "schema_version", 1, 1)
        require(type(m["unknown_reasons"]) is dict, "unknown_reasons", "expected object")
        for key, reason in m["unknown_reasons"].items():
            string(key, "unknown_reasons key")
            string(reason, key)
        for key in ("dataset_id", "query_session_id", "capture_group_id"):
            string(m[key], key)
        choice(m["split"], "development validation test", "split")
        annotation = obj(m["annotation"], "revision annotator_id reviewer_id review_status method "
                         "endpoint_definition visibility_definition source_frame_binding unresolved evidence", "annotation")
        for key in ("revision", "annotator_id", "method", "endpoint_definition", "visibility_definition", "source_frame_binding"):
            string(annotation[key], "annotation." + key)
        self.review(annotation, "annotation")
        strings(annotation["unresolved"], "annotation.unresolved")
        if annotation["unresolved"]:
            self.review_reasons.append("annotation: unresolved items")
        require(type(annotation["evidence"]) is list and annotation["evidence"], "annotation.evidence", "expected nonempty list")
        self.evidence = {}
        for record in annotation["evidence"]:
            obj(record, "id path sha256 method uncertainty synchronization review_status reviewer_id", "evidence")
            eid = string(record["id"], "evidence.id")
            require(eid not in self.evidence, "evidence.id", "duplicate ID")
            for key in ("method", "uncertainty", "synchronization"):
                string(record[key], "evidence." + eid + "." + key)
            self.review(record, "evidence." + eid)
            self.artifact(record, "evidence." + eid)
            self.evidence[eid] = record
        ref = obj(m["reference"], "path sha256 entry_count recording_session_id provenance_evidence_id", "reference")
        sha256(ref["sha256"], "reference.sha256")
        integer(ref["entry_count"], "reference.entry_count", 1, MAX_ROWS)
        string(ref["recording_session_id"], "reference.recording_session_id")
        require(ref["recording_session_id"] != m["query_session_id"], "reference", "query and reference session must differ")
        self.evidence_link(ref["provenance_evidence_id"], "reference.provenance_evidence_id")
        count, self.reference_pixels = self.read(ref["path"], vhrs, ref["sha256"])
        require(count == ref["entry_count"], "reference.entry_count", "does not match VHRS")
        capture = obj(m["capture"], "camera_id profile_id config_path config_sha256 native_width native_height pixel_format "
                      "preprocessing conditions conversion", "capture")
        for key in ("camera_id", "profile_id", "pixel_format"):
            string(capture[key], "capture." + key)
        sha256(capture["config_sha256"], "capture.config_sha256")
        self.artifact({"path": capture["config_path"], "sha256": capture["config_sha256"]}, "capture.config")
        for key in ("native_width", "native_height"):
            integer(capture[key], "capture." + key, 1, 65535)
        preprocessing = obj(capture["preprocessing"], "method parameters", "capture.preprocessing")
        string(preprocessing["method"], "capture.preprocessing.method")
        self.parameters(preprocessing["parameters"], "capture.preprocessing.parameters")
        conditions = obj(capture["conditions"], "lighting altitude motion", "capture.conditions")
        for key, value in conditions.items():
            self.unknown(value, "capture.conditions." + key, string)
        clock = obj(m["clock"], "clock_id unit source epoch timestamp_provenance alignment_uncertainty_ns", "clock")
        for key in ("clock_id", "epoch", "timestamp_provenance"):
            string(clock[key], "clock." + key)
        choice(clock["unit"], "ns", "clock.unit")
        choice(clock["source"], "host_callback sensor_exposure host_read", "clock.source")
        self.unknown(clock["alignment_uncertainty_ns"], "clock.alignment_uncertainty_ns", integer)
        independence = obj(m["independence"], "not_reference_derived review_status reviewer_id evidence_ids unresolved", "independence")
        boolean(independence["not_reference_derived"], "independence.not_reference_derived")
        require(independence["not_reference_derived"], "independence", "reference-derived data is not a second pass")
        self.review(independence, "independence")
        strings(independence["unresolved"], "independence.unresolved")
        if independence["unresolved"]:
            self.review_reasons.append("independence: unresolved items")
        strings(independence["evidence_ids"], "independence.evidence_ids", nonempty=True)
        for eid in independence["evidence_ids"]:
            self.evidence_link(eid, "independence.evidence_ids")

    def rows(self, name, fields):
        reader = csv.reader(io.StringIO(self.small(name, 64 * 1024 * 1024), newline=""), strict=True)
        require(next(reader, None) == fields, name, "unexpected CSV header")
        for index, row in enumerate(reader):
            require(index < MAX_ROWS, name, "row count exceeds 100000")
            require(len(row) == len(fields), f"{name}:{reader.line_num}", "wrong column count")
            yield index, dict(zip(fields, row))

    def frames(self):
        self.frames_by_id = {}
        self.frame_list = []
        previous_time = 0
        previous_id = -1
        for index, row in self.rows("frames.csv", FRAME_FIELDS):
            where = f"frames.csv row {index + 1}"
            require(decimal(row["sequence"], where) == index, where, "sequence must be 0..N-1 in capture order")
            fid = decimal(row["frame_id"], where + ".frame_id")
            timestamp = decimal(row["timestamp_ns"], where + ".timestamp_ns", 1, I64)
            require(fid > previous_id, where, "frame IDs must be unique and increasing within a session")
            require(timestamp > previous_time, where, "timestamps must strictly increase")
            width = decimal(row["width"], where + ".width", 1, 65535)
            height = decimal(row["height"], where + ".height", 1, 65535)
            decoded = self.read(row["path"], pgm, row["sha256"])
            require(decoded[:2] == (width, height), where, "dimensions do not match decoded PGM")
            self.frame_hashes.add(row["sha256"])
            self.pixel_hashes.add(decoded)
            frame = {"frame_id": fid, "timestamp_ns": timestamp, "width": width, "height": height}
            self.frames_by_id[fid] = frame
            self.frame_list.append(frame)
            previous_time, previous_id = timestamp, fid
        require(self.frame_list, "frames.csv", "empty sequence")
        require(len({(r["width"], r["height"]) for r in self.frame_list}) == 1, "frames.csv", "geometry changed within session")
        if self.pixel_hashes & self.reference_pixels:
            self.review_reasons.append("query/reference exact pixel overlap; physical independence not established")

    def conversion(self):
        capture = self.meta["capture"]
        conversion = capture["conversion"]
        if conversion is None:
            require(capture["pixel_format"] == "Gray8" and
                    capture["preprocessing"] == {"method": "identity", "parameters": {}},
                    "capture.conversion", "processed/non-Gray8 input requires original mapping")
            first = self.frame_list[0]
            require((first["width"], first["height"]) == (capture["native_width"], capture["native_height"]),
                    "capture", "native dimensions differ without conversion")
            return
        obj(conversion, "method parameters mapping_evidence_id originals", "capture.conversion")
        string(conversion["method"], "capture.conversion.method")
        self.parameters(conversion["parameters"], "capture.conversion.parameters")
        self.evidence_link(conversion["mapping_evidence_id"], "capture.conversion.mapping_evidence_id")
        require(type(conversion["originals"]) is list, "capture.conversion.originals", "expected list")
        seen = set()
        for record in conversion["originals"]:
            obj(record, "frame_id path sha256", "original")
            fid = integer(record["frame_id"], "original.frame_id")
            require(fid in self.frames_by_id and fid not in seen, "original.frame_id", "extra or duplicate ID")
            seen.add(fid)
            self.artifact(record, "original")
            self.frame_hashes.add(record["sha256"])
        require(seen == set(self.frames_by_id), "originals", "incomplete source frame coverage")

    def sequence(self):
        seq = obj(self.meta["sequence"], "complete planned_start planned_end boundaries_confirmed actual "
                  "acquired_count stored_count dropped_count counter_evidence_id gaps", "sequence")
        for key in ("complete", "boundaries_confirmed"):
            boolean(seq[key], "sequence." + key)
        for key in ("planned_start", "planned_end"):
            string(seq[key], "sequence." + key)
        for key in ("acquired_count", "dropped_count"):
            self.unknown(seq[key], "sequence." + key, integer)
        integer(seq["stored_count"], "sequence.stored_count", 1, MAX_ROWS)
        require(seq["stored_count"] == len(self.frame_list), "sequence.stored_count", "does not match frames.csv")
        self.evidence_link(seq["counter_evidence_id"], "sequence.counter_evidence_id")
        actual = obj(seq["actual"], "first_frame_id last_frame_id first_timestamp_ns last_timestamp_ns", "sequence.actual")
        for prefix, frame in (("first", self.frame_list[0]), ("last", self.frame_list[-1])):
            for key in ("frame_id", "timestamp_ns"):
                name = prefix + "_" + key
                integer(actual[name], "sequence.actual." + name)
                require(actual[name] == frame[key], "sequence.actual." + name, "does not match frames.csv")
        require(type(seq["gaps"]) is list, "sequence.gaps", "expected list")
        adjacent = {(a["frame_id"], b["frame_id"]): (a["timestamp_ns"], b["timestamp_ns"])
                    for a, b in zip(self.frame_list, self.frame_list[1:])}
        adjacent[(None, self.frame_list[0]["frame_id"])] = (0, self.frame_list[0]["timestamp_ns"])
        adjacent[(self.frame_list[-1]["frame_id"], None)] = (self.frame_list[-1]["timestamp_ns"], I64 + 1)
        seen = set()
        dropped = 0
        unknown_count = False
        for index, gap in enumerate(seq["gaps"]):
            where = f"sequence.gaps.{index}"
            obj(gap, "after_frame_id before_frame_id start_timestamp_ns end_timestamp_ns missing_count reason evidence_id", where)
            for key in ("after_frame_id", "before_frame_id"):
                if gap[key] is not None:
                    integer(gap[key], where + "." + key)
            pair = (gap["after_frame_id"], gap["before_frame_id"])
            require(pair in adjacent and pair not in seen, where, "gap must name unique adjacent stored frames or a boundary")
            seen.add(pair)
            lo, hi = adjacent[pair]
            start = integer(gap["start_timestamp_ns"], where + ".start_timestamp_ns", 1, I64)
            end = integer(gap["end_timestamp_ns"], where + ".end_timestamp_ns", 1, I64)
            require(lo < start <= end < hi, where, "gap times must lie strictly between anchors")
            self.unknown(gap["missing_count"], where + ".missing_count", integer)
            if gap["missing_count"] is None:
                unknown_count = True
            else:
                dropped += gap["missing_count"]
            if None not in pair:
                require(gap["missing_count"] == pair[1] - pair[0] - 1, where, "missing count disagrees with source ID gap")
            elif gap["missing_count"] is not None:
                capacity = pair[1] if pair[0] is None else U64 - pair[0]
                require(gap["missing_count"] <= capacity, where, "missing IDs exceed unsigned source range")
            string(gap["reason"], where + ".reason")
            self.evidence_link(gap["evidence_id"], where + ".evidence_id")
        for pair in adjacent:
            if None not in pair and pair[1] != pair[0] + 1:
                require(pair in seen, "sequence.gaps", "unexplained source ID gap")
        acquired, declared_dropped = seq["acquired_count"], seq["dropped_count"]
        if acquired is not None:
            require(acquired >= len(self.frame_list) + dropped, "sequence.acquired_count", "less than stored plus known gaps")
        if acquired is not None and declared_dropped is not None:
            require(acquired == len(self.frame_list) + declared_dropped, "sequence", "acquired != stored + dropped")
        if declared_dropped is not None:
            require(declared_dropped >= dropped, "sequence.gaps", "gap total exceeds dropped count")
            if not unknown_count:
                require(declared_dropped == dropped, "sequence.gaps", "gap total differs from dropped count")
        self.complete_consistent = (seq["complete"] and seq["boundaries_confirmed"] and
                                    acquired is not None and declared_dropped is not None and not unknown_count)
        require(not seq["complete"] or self.complete_consistent, "sequence.complete", "completeness claim has unknown counts/boundaries")
        if not self.complete_consistent:
            self.review_reasons.append("sequence completeness unconfirmed")

    def labels(self):
        seen = set()
        counts = {"on_route_with_interval": 0, "on_route_without_interval": 0, "off_route": 0, "unknown": 0}
        for index, row in self.rows("labels.csv", LABEL_FIELDS):
            where = f"labels.csv row {index + 1}"
            fid = decimal(row["frame_id"], where + ".frame_id")
            require(fid in self.frames_by_id and fid not in seen, where, "extra or duplicate frame ID")
            seen.add(fid)
            for key, options in (("location", "on_route off_route unknown"), ("endpoint", "start end neither unknown"),
                                 ("direction", "forward reverse stationary unknown"), ("visibility", "clear degraded unobservable unknown")):
                choice(row[key], options, where + "." + key)
            self.evidence_link(row["evidence_id"], where + ".evidence_id")
            lower, upper = row["reference_index_min"], row["reference_index_max"]
            require(bool(lower) == bool(upper), where, "both interval cells must be present or empty")
            if lower:
                lo = decimal(lower, where + ".reference_index_min")
                hi = decimal(upper, where + ".reference_index_max")
                require(row["location"] == "on_route" and lo <= hi < self.meta["reference"]["entry_count"],
                        where, "interval inconsistent with location/reference range")
            if row["endpoint"] in ("start", "end"):
                require(row["location"] == "on_route", where, "endpoint requires on_route location")
            category = row["location"]
            if category == "on_route":
                category += "_with_interval" if lower else "_without_interval"
            counts[category] += 1
        require(seen == set(self.frames_by_id), "labels.csv", "missing frame labels")
        self.label_counts = counts

    def validate(self):
        self.metadata()
        self.frames()
        self.conversion()
        self.sequence()
        self.labels()
        require(set(self.meta["unknown_reasons"]) == self.unknown_used, "unknown_reasons", "unused or misspelled unknown field")
        return {"root": str(self.root), "dataset_id": self.meta["dataset_id"], "split": self.meta["split"],
                "structural_valid": True, "errors": [], "review_reasons": self.review_reasons,
                "complete_claim_consistent": self.complete_consistent, "frames": len(self.frame_list),
                "label_counts": self.label_counts, "input_sha256": self.inputs,
                "declared_review": {key: self.meta[key]["review_status"] for key in ("independence", "annotation")}}


def validate_datasets(roots, disjoint_references=False):
    datasets, results, overlaps, errors = [], [], [], []
    if not roots:
        errors.append("at least one dataset root is required")
    for root in roots:
        dataset = None
        try:
            dataset = Dataset(root)
            result = dataset.validate()
            datasets.append(dataset)
        except (Invalid, OSError, ValueError, csv.Error, RecursionError) as exc:
            result = {"root": str(root), "structural_valid": False, "errors": [str(exc)],
                      "review_reasons": [], "input_sha256": dataset.inputs if dataset else {}}
        results.append(result)
    identities = {}
    sessions = {}
    for dataset in datasets:
        m = dataset.meta
        did = m["dataset_id"]
        if did in identities:
            errors.append(f"duplicate dataset_id: {did}")
        identities[did] = dataset
        session = m["query_session_id"]
        if session in sessions and sessions[session] != m["capture_group_id"]:
            errors.append(f"query_session_id {session} assigned to different capture groups")
        sessions[session] = m["capture_group_id"]
    for index, left in enumerate(datasets):
        for right in datasets[index + 1:]:
            a, b = left.meta, right.meta
            if a["split"] == b["split"]:
                continue
            kinds = []
            for key in ("capture_group_id", "query_session_id"):
                if a[key] == b[key]:
                    kinds.append(key)
            if left.frame_hashes & right.frame_hashes:
                kinds.append("query_or_original_sha256")
            if left.pixel_hashes & right.pixel_hashes:
                kinds.append("query_pixels")
            if (a["query_session_id"] == b["reference"]["recording_session_id"] or
                    b["query_session_id"] == a["reference"]["recording_session_id"]):
                kinds.append("query_reference_session")
            if (left.frame_hashes & {b["reference"]["sha256"]} or
                    right.frame_hashes & {a["reference"]["sha256"]} or
                    left.pixel_hashes & right.reference_pixels or right.pixel_hashes & left.reference_pixels):
                kinds.append("query_reference_content")
            shared_reference = (a["reference"]["sha256"] == b["reference"]["sha256"] or
                                a["reference"]["recording_session_id"] == b["reference"]["recording_session_id"])
            if shared_reference and disjoint_references:
                kinds.append("reference")
            if kinds or shared_reference:
                overlaps.append({"datasets": [a["dataset_id"], b["dataset_id"]],
                                 "violations": kinds, "shared_reference": shared_reference})
    valid = all(r["structural_valid"] for r in results) and not errors and not any(o["violations"] for o in overlaps)
    needs_review = any(r["review_reasons"] for r in results)
    status = "invalid" if not valid else "needs_review" if needs_review else "structurally_valid"
    return {"report_version": 1, "scope": "structural_only", "status": status,
            "semantic_independence_verified": False, "split_check_scope": "only supplied dataset roots",
            "disjoint_references": disjoint_references, "datasets": results,
            "collection_errors": errors, "cross_split_overlaps": overlaps}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("roots", nargs="+", type=Path, help="dataset root directories; supply all splits together")
    parser.add_argument("--disjoint-references", action="store_true", help="also reject reference sharing across splits")
    args = parser.parse_args(argv)
    report = validate_datasets(args.roots, args.disjoint_references)
    print(json.dumps(report, indent=2, sort_keys=True, ensure_ascii=True))
    return {"structurally_valid": 0, "invalid": 2, "needs_review": 3}[report["status"]]


if __name__ == "__main__":
    sys.exit(main())
