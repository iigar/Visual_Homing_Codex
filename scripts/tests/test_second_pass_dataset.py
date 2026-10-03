#!/usr/bin/env python3
"""Synthetic contract tests; these fixtures are not independent capture evidence."""
import copy
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "validate-second-pass-dataset.py"
spec = importlib.util.spec_from_file_location("second_pass", SCRIPT)
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def write_csv(path, fields, rows):
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def make_dataset(root, name="one", split="development", pixel_offset=0):
    root.mkdir(parents=True)
    (root / "reference").mkdir()
    (root / "frames").mkdir()
    (root / "evidence").mkdir()
    # Encode the documented VHRS wire layout independently of the validator.
    route = struct.pack("<4sHHBBHI", b"VHRS", 1, 16, 1, 0, 0, 4)
    for index in range(4):
        route += struct.pack("<QQhfHHBBHI", index, 10 + index, 0, 0.0, 2, 2, 1, 0, 0, 4)
        route += bytes([180 + index, 200, 210, 220])
    (root / "reference/route.vhrs").write_bytes(route)
    evidence = b"SYNTHETIC fixture only; no physical capture or semantic acceptance.\n"
    (root / "evidence/review.txt").write_bytes(evidence)
    (root / "evidence/capture.conf").write_bytes(b"fixture config")
    frames, labels = [], []
    for index in range(3):
        data = b"P5\n2 2\n255\n" + bytes([10 + index + pixel_offset, 32, 35, 10])
        path = f"frames/{index}.pgm"
        (root / path).write_bytes(data)
        frames.append(dict(sequence=str(index), frame_id=str(10 + index), timestamp_ns=str(100 + index * 100),
                           path=path, sha256=digest(data), width="2", height="2"))
        labels.append(dict(frame_id=str(10 + index), location="on_route", reference_index_min=str(index),
                           reference_index_max=str(index), endpoint="neither", direction="forward", visibility="clear", evidence_id="manual"))
    meta = {
        "schema_version": 1, "dataset_id": name, "query_session_id": "query-" + name,
        "capture_group_id": "group-" + name, "split": split,
        "reference": {"path": "reference/route.vhrs", "sha256": digest(route), "entry_count": 4,
                      "recording_session_id": "reference-session", "provenance_evidence_id": "manual"},
        "capture": {"camera_id": "synthetic", "profile_id": "synthetic-2x2", "config_path": "evidence/capture.conf",
                    "config_sha256": digest(b"fixture config"),
                    "native_width": 2, "native_height": 2, "pixel_format": "Gray8",
                    "preprocessing": {"method": "identity", "parameters": {}},
                    "conditions": {"lighting": None, "altitude": None, "motion": "synthetic"}, "conversion": None},
        "clock": {"clock_id": "synthetic-clock", "unit": "ns", "source": "host_callback", "epoch": "synthetic origin",
                  "timestamp_provenance": "scripted fixture", "alignment_uncertainty_ns": None},
        "sequence": {"complete": True, "planned_start": "synthetic first", "planned_end": "synthetic last",
                     "boundaries_confirmed": True, "actual": {"first_frame_id": 10, "last_frame_id": 12,
                     "first_timestamp_ns": 100, "last_timestamp_ns": 300}, "acquired_count": 3, "stored_count": 3,
                     "dropped_count": 0, "counter_evidence_id": "manual", "gaps": []},
        "independence": {"not_reference_derived": True, "review_status": "reviewed", "reviewer_id": "synthetic-reviewer",
                         "evidence_ids": ["manual"], "unresolved": []},
        "annotation": {"revision": "synthetic-v1", "annotator_id": "synthetic-author", "reviewer_id": "synthetic-reviewer",
                       "review_status": "reviewed", "method": "synthetic labels", "endpoint_definition": "synthetic region",
                       "visibility_definition": "synthetic enum", "source_frame_binding": "frame_id",
                       "unresolved": [], "evidence": [{"id": "manual", "path": "evidence/review.txt", "sha256": digest(evidence),
                       "method": "synthetic", "uncertainty": "not a measurement", "synchronization": "scripted same clock",
                       "review_status": "reviewed", "reviewer_id": "synthetic-reviewer"}]},
        "unknown_reasons": {"capture.conditions.lighting": "synthetic", "capture.conditions.altitude": "synthetic",
                            "clock.alignment_uncertainty_ns": "no real clock alignment"}}
    save_dataset(root, meta, frames, labels)
    return meta, frames, labels


def save_dataset(root, meta, frames, labels):
    (root / "dataset.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    write_csv(root / "frames.csv", validator.FRAME_FIELDS, frames)
    write_csv(root / "labels.csv", validator.LABEL_FIELDS, labels)


class DatasetTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="second pass tests ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "one"
        self.meta, self.frames, self.labels = make_dataset(self.root)

    def report(self, expected="structurally_valid", message=None):
        save_dataset(self.root, self.meta, self.frames, self.labels)
        report = validator.validate_datasets([self.root])
        self.assertEqual(report["status"], expected, report)
        if message:
            self.assertIn(message, json.dumps(report))
        return report

    def update_frame(self, index, data):
        (self.root / self.frames[index]["path"]).write_bytes(data)
        self.frames[index]["sha256"] = digest(data)

    def update_route(self, data):
        (self.root / "reference/route.vhrs").write_bytes(data)
        self.meta["reference"]["sha256"] = digest(data)

    def test_valid_report_is_deterministic_and_read_only(self):
        before = {p.relative_to(self.root).as_posix(): digest(p.read_bytes()) for p in self.root.rglob("*") if p.is_file()}
        report = validator.validate_datasets([self.root])
        self.assertEqual(report, validator.validate_datasets([self.root]))
        self.assertEqual(report["status"], "structurally_valid")
        self.assertFalse(report["semantic_independence_verified"])
        result = report["datasets"][0]
        self.assertTrue(result["complete_claim_consistent"])
        self.assertEqual(result["input_sha256"], before)
        self.assertEqual(result["label_counts"]["on_route_with_interval"], 3)
        after = {p.relative_to(self.root).as_posix(): digest(p.read_bytes()) for p in self.root.rglob("*") if p.is_file()}
        self.assertEqual(before, after)

    def test_missing_duplicate_reordered_and_extra_frames(self):
        original = copy.deepcopy(self.frames)
        cases = [original[:-1], [original[0], original[0], original[2]],
                 [original[1], original[0], original[2]], original + [original[2]], []]
        for rows in cases:
            with self.subTest(rows=rows):
                self.frames = copy.deepcopy(rows)
                self.report("invalid")

    def test_integer_and_clock_boundaries(self):
        for key, values in {"frame_id": ["-1", "+10", "1.0", " 10", str(1 << 64), "true"],
                            "timestamp_ns": ["0", "-1", str(1 << 63), "1e3", "100.0", "\u0661"]}.items():
            original = self.frames[0][key]
            for value in values:
                with self.subTest(key=key, value=value):
                    self.frames[0][key] = value
                    self.report("invalid")
            self.frames[0][key] = original
        self.frames[1]["timestamp_ns"] = "100"
        self.report("invalid", "timestamps must strictly increase")
        self.frames[1]["timestamp_ns"] = "99"
        self.report("invalid", "timestamps must strictly increase")

    def test_maximum_ids_and_timestamps_are_not_float_rounded(self):
        for index in range(3):
            self.frames[index]["frame_id"] = str(validator.U64 - 2 + index)
            self.frames[index]["timestamp_ns"] = str(validator.I64 - 2 + index)
            self.labels[index]["frame_id"] = self.frames[index]["frame_id"]
        self.meta["sequence"]["actual"] = dict(first_frame_id=validator.U64 - 2, last_frame_id=validator.U64,
                                                first_timestamp_ns=validator.I64 - 2, last_timestamp_ns=validator.I64)
        self.report()

    def test_missing_files_hashes_and_dimensions(self):
        for field, value, message in (("path", "frames/missing.pgm", "missing.pgm"),
                                       ("sha256", "a" * 64, "SHA-256 mismatch"),
                                       ("sha256", "A" * 64, "lowercase SHA-256"),
                                       ("width", "3", "dimensions do not match")):
            original = self.frames[0][field]
            with self.subTest(field=field, value=value):
                self.frames[0][field] = value
                self.report("invalid", message)
            self.frames[0][field] = original
        (self.root / "evidence/review.txt").write_bytes(b"changed")
        self.report("invalid", "SHA-256 mismatch")

    def test_path_escapes_and_nonregular_files(self):
        for path in ("../outside.pgm", "/tmp/outside.pgm", "C:/outside.pgm", "C:outside.pgm",
                     "\\\\server\\share", "frames/../frames/0.pgm", "frames\\0.pgm", "frames/0.pgm,extra",
                     "frames/0.pgm\n", "frames//0.pgm", "frames/./0.pgm", "frames"):
            with self.subTest(path=path):
                self.frames[0]["path"] = path
                self.report("invalid")

    def test_symlink_escapes_in_all_artifact_roles(self):
        outside = Path(self.temp.name) / "outside"
        outside.write_bytes(b"outside")
        link = self.root / "escape"
        try:
            link.symlink_to(outside)
        except OSError as exc:
            self.skipTest(f"symlinks unavailable: {exc}")
        for record in (self.frames[0], self.meta["reference"], self.meta["annotation"]["evidence"][0]):
            original = record["path"]
            record["path"] = "escape"
            self.report("invalid", "path escapes dataset root")
            record["path"] = original
        save_dataset(self.root, self.meta, self.frames, self.labels)
        outside.write_bytes((self.root / "dataset.json").read_bytes())
        (self.root / "dataset.json").unlink()
        (self.root / "dataset.json").symlink_to(outside)
        report = validator.validate_datasets([self.root])
        self.assertEqual(report["status"], "invalid")
        self.assertIn("escapes", str(report))

    def test_pgm_comments_crlf_and_binary_whitespace_pixels(self):
        self.update_frame(0, b"P5\r\n# synthetic comment\r\n2\t2\n255\r\n" + b"\n #\x00")
        self.report()

    def test_pgm_corruption_and_all_truncations(self):
        good = b"P5\n2 2\n255\n\x00\x01\x02\x03"
        cases = [good[:i] for i in range(len(good))]
        cases += [good + b"\x00", good.replace(b"P5", b"P2"), good.replace(b"255", b"254"),
                  good.replace(b"2 2", b"0 2"), good.replace(b"2 2", b"65535 65535"),
                  b"P5\n#" + b"x" * 4096, good.replace(b"2 2", b"2x 2")]
        for data in cases:
            with self.subTest(data=data[:30], size=len(data)):
                self.update_frame(0, data)
                self.report("invalid")

    def test_vhrs_corruption_count_and_truncations(self):
        good = (self.root / "reference/route.vhrs").read_bytes()
        cases = [good[:i] for i in range(len(good))]
        for offset, data in ((0, b"NOPE"), (4, b"\x02\x00"), (6, b"\x11\x00"), (8, b"\x02"),
                             (12, struct.pack("<I", 100001)), (38, b"\x00\x00"), (42, b"\x02"),
                             (46, struct.pack("<I", 5))):
            cases.append(good[:offset] + data + good[offset + len(data):])
        cases.append(good + b"extra")
        for data in cases:
            with self.subTest(size=len(data), prefix=data[:16]):
                self.update_route(data)
                self.report("invalid")
        self.update_route(good)
        self.meta["reference"]["entry_count"] = 3
        self.report("invalid", "does not match VHRS")

    def test_schema_missing_extra_types_and_unknowns(self):
        original = copy.deepcopy(self.meta)
        for transform in (lambda m: m.pop("clock"), lambda m: m.update(typo=True),
                          lambda m: m.update(schema_version=True), lambda m: m.update(schema_version=1.0),
                          lambda m: m["reference"].update(sha256=None),
                          lambda m: m["clock"].update(unit="ms"), lambda m: m["clock"].update(source="file_mtime"),
                          lambda m: m["clock"].update(clock_id=None), lambda m: m["capture"].update(native_width=True),
                          lambda m: m["unknown_reasons"].clear(), lambda m: m["unknown_reasons"].update(typo="unknown")):
            self.meta = copy.deepcopy(original)
            transform(self.meta)
            self.report("invalid")

    def test_malformed_json_duplicates_nonfinite_and_wrong_root(self):
        good = (self.root / "dataset.json").read_text()
        for text in ("[]", "null", "{", good.replace('"schema_version": 1', '"schema_version": 1, "schema_version": 1'),
                     good.replace('"parameters": {}', '"parameters": {"x": NaN}'),
                     good.replace('"parameters": {}', '"parameters": {"x": 1e999}')):
            with self.subTest(text=text[:20]):
                (self.root / "dataset.json").write_text(text)
                self.assertEqual(validator.validate_datasets([self.root])["status"], "invalid")

    def test_csv_headers_extra_columns_and_malformed_quoting(self):
        for name in ("frames.csv", "labels.csv"):
            original = (self.root / name).read_text()
            for text in (original.replace("frame_id", "frame", 1), original.replace("\n", ",extra\n", 1),
                         original + "\n", original + '"unfinished', original.replace(",", ",extra,", 1)):
                with self.subTest(name=name, text=text[:40]):
                    (self.root / name).write_text(text)
                    self.assertEqual(validator.validate_datasets([self.root])["status"], "invalid")
            (self.root / name).write_text(original)

    def test_labels_coverage_duplicates_and_unordered_rows(self):
        self.labels.reverse()
        self.report()  # join by frame_id; annotation CSV need not be capture-ordered
        good = copy.deepcopy(self.labels)
        for rows in (good[:-1], good + [good[0]], []):
            self.labels = copy.deepcopy(rows)
            self.report("invalid")
        self.labels = good
        self.labels[0]["frame_id"] = "999"
        self.report("invalid", "extra or duplicate")

    def test_labels_ranges_enum_evidence_and_contradictions(self):
        good = copy.deepcopy(self.labels[0])
        for patch in ({"reference_index_min": ""}, {"reference_index_min": "2", "reference_index_max": "1"},
                      {"reference_index_max": "4"}, {"reference_index_min": "-1"}, {"location": "off_route"},
                      {"direction": "backwards"}, {"visibility": "perfect"}, {"endpoint": "finish"},
                      {"evidence_id": ""}, {"evidence_id": "missing"},
                      {"location": "unknown", "endpoint": "start", "reference_index_min": "", "reference_index_max": ""}):
            with self.subTest(patch=patch):
                self.labels[0] = dict(good, **patch)
                self.report("invalid")

    def test_unknown_intervals_are_absent_not_zero(self):
        for row, location in zip(self.labels, ("on_route", "off_route", "unknown")):
            row.update(location=location, reference_index_min="", reference_index_max="", visibility="unknown")
        counts = self.report()["datasets"][0]["label_counts"]
        self.assertEqual(counts, {"on_route_with_interval": 0, "on_route_without_interval": 1, "off_route": 1, "unknown": 1})

    def add_gap(self):
        self.frames[1]["frame_id"] = self.labels[1]["frame_id"] = "12"
        self.frames[2]["frame_id"] = self.labels[2]["frame_id"] = "13"
        seq = self.meta["sequence"]
        seq["actual"]["last_frame_id"] = 13
        seq.update(acquired_count=4, dropped_count=1)
        seq["gaps"] = [dict(after_frame_id=10, before_frame_id=12, start_timestamp_ns=150, end_timestamp_ns=150,
                            missing_count=1, reason="synthetic dropped frame", evidence_id="manual")]
        return seq["gaps"][0]

    def test_explained_gaps_and_inconsistent_gap_metadata(self):
        gap = self.add_gap()
        self.report()
        good = dict(gap)
        for patch in ({"before_frame_id": 13}, {"start_timestamp_ns": 100}, {"end_timestamp_ns": 200},
                      {"end_timestamp_ns": 149}, {"missing_count": 0}, {"reason": ""}, {"evidence_id": "absent"}):
            with self.subTest(patch=patch):
                gap.clear()
                gap.update(good, **patch)
                self.report("invalid")
        self.meta["sequence"]["gaps"] = [good, good]
        self.report("invalid", "unique adjacent")
        self.meta["sequence"]["gaps"] = []
        self.report("invalid", "unexplained source ID gap")

    def test_counts_actual_boundaries_and_incomplete_sequence(self):
        seq = self.meta["sequence"]
        good = copy.deepcopy(seq)
        for patch in ({"acquired_count": 4}, {"stored_count": 4}, {"dropped_count": 1},
                      {"boundaries_confirmed": False}, {"complete": "true"}):
            self.meta["sequence"] = dict(good, **patch)
            self.report("invalid")
        self.meta["sequence"] = copy.deepcopy(good)
        self.meta["sequence"]["actual"]["last_timestamp_ns"] = 301
        self.report("invalid", "does not match frames.csv")
        self.meta["sequence"] = dict(good, complete=False, acquired_count=None, dropped_count=None)
        self.meta["unknown_reasons"].update({"sequence.acquired_count": "counter unavailable", "sequence.dropped_count": "counter unavailable"})
        self.report("needs_review", "sequence completeness unconfirmed")
        self.meta["sequence"]["complete"] = True
        self.report("invalid", "completeness claim")

    def test_boundary_gap_with_unknown_count_remains_reviewable(self):
        self.meta["sequence"].update(complete=False, acquired_count=None, dropped_count=None)
        self.meta["sequence"]["gaps"] = [dict(after_frame_id=None, before_frame_id=10, start_timestamp_ns=1,
            end_timestamp_ns=99, missing_count=None, reason="clip starts late", evidence_id="manual")]
        self.meta["unknown_reasons"].update({"sequence.acquired_count": "unknown", "sequence.dropped_count": "unknown",
                                              "sequence.gaps.0.missing_count": "counter unavailable"})
        self.report("needs_review")

    def test_review_states_unresolved_and_false_independence(self):
        for section in ("independence", "annotation"):
            record = self.meta[section]
            for state in ("pending", "rejected"):
                record["review_status"] = state
                self.report("needs_review", section + ": " + state)
            record["review_status"] = "reviewed"
            record["unresolved"] = ["needs physical review"]
            self.report("needs_review", "unresolved")
            record["unresolved"] = []
        self.meta["independence"]["not_reference_derived"] = False
        self.report("invalid", "reference-derived")

    def test_missing_review_identity_and_duplicate_evidence(self):
        self.meta["annotation"]["reviewer_id"] = None
        self.meta["unknown_reasons"]["annotation.reviewer_id"] = "not reviewed"
        self.report("invalid", "reviewer_id")
        self.meta["annotation"]["review_status"] = "pending"
        self.report("needs_review")
        self.meta["annotation"]["evidence"] *= 2
        self.report("invalid", "duplicate ID")

    def test_reference_session_and_exact_pixels_do_not_establish_independence(self):
        self.meta["query_session_id"] = "reference-session"
        self.report("invalid", "query and reference session")
        self.meta["query_session_id"] = "query-one"
        self.update_frame(0, b"P5\n2 2\n255\n" + bytes([180, 200, 210, 220]))
        self.report("needs_review", "query/reference exact pixel overlap")

    def add_conversion(self):
        originals = []
        for index, frame in enumerate(self.frames):
            path = f"frames/original-{index}.raw"
            data = bytes([index, 50, 60, 70])
            (self.root / path).write_bytes(data)
            originals.append(dict(frame_id=int(frame["frame_id"]), path=path, sha256=digest(data)))
        self.meta["capture"]["conversion"] = dict(method="synthetic raw to PGM", parameters={},
                                                   mapping_evidence_id="manual", originals=originals)
        return originals

    def test_conversion_requires_complete_hashed_original_mapping(self):
        self.meta["capture"]["preprocessing"]["method"] = "synthetic conversion"
        self.report("invalid", "requires original mapping")
        originals = self.add_conversion()
        self.report()
        originals.pop()
        self.report("invalid", "incomplete source frame coverage")
        originals.append(originals[0])
        self.report("invalid", "extra or duplicate ID")

    def test_cross_split_group_session_and_hash_leakage(self):
        other = Path(self.temp.name) / "two"
        m, frames, labels = make_dataset(other, "two", "test", 30)
        report = validator.validate_datasets([self.root, other])
        self.assertEqual(report["status"], "structurally_valid", report)
        self.assertTrue(report["cross_split_overlaps"][0]["shared_reference"])
        self.assertEqual(validator.validate_datasets([self.root, other], True)["status"], "invalid")
        for key in ("capture_group_id", "query_session_id"):
            old = m[key]
            m[key] = self.meta[key]
            save_dataset(other, m, frames, labels)
            report = validator.validate_datasets([self.root, other])
            self.assertEqual(report["status"], "invalid")
            self.assertIn(key, str(report))
            m[key] = old
        data = (self.root / self.frames[0]["path"]).read_bytes()
        (other / frames[0]["path"]).write_bytes(data)
        frames[0]["sha256"] = digest(data)
        save_dataset(other, m, frames, labels)
        report = validator.validate_datasets([self.root, other])
        self.assertEqual(report["status"], "invalid")
        self.assertIn("query_or_original_sha256", str(report))
        self.assertIn("query_pixels", str(report))
        # Re-encoding the PGM header defeats byte equality, not pixel equality.
        data = data.replace(b"P5\n", b"P5\n# other header\n")
        (other / frames[0]["path"]).write_bytes(data)
        frames[0]["sha256"] = digest(data)
        save_dataset(other, m, frames, labels)
        report = validator.validate_datasets([self.root, other])
        self.assertEqual(report["status"], "invalid")
        self.assertNotIn("query_or_original_sha256", str(report))
        self.assertIn("query_pixels", str(report))

    def test_original_hash_leakage_and_duplicate_dataset_identity(self):
        originals = self.add_conversion()
        save_dataset(self.root, self.meta, self.frames, self.labels)
        other = Path(self.temp.name) / "two"
        m, frames, labels = make_dataset(other, "two", "test", 30)
        m["capture"]["conversion"] = copy.deepcopy(self.meta["capture"]["conversion"])
        for original in originals:
            (other / original["path"]).write_bytes((self.root / original["path"]).read_bytes())
        save_dataset(other, m, frames, labels)
        report = validator.validate_datasets([self.root, other])
        self.assertEqual(report["status"], "invalid")
        self.assertIn("query_or_original_sha256", str(report))
        self.assertEqual(validator.validate_datasets([self.root, self.root])["status"], "invalid")

    def test_invalid_dataset_does_not_hide_other_results(self):
        report = validator.validate_datasets([Path(self.temp.name) / "missing", self.root])
        self.assertEqual(report["status"], "invalid")
        self.assertFalse(report["datasets"][0]["structural_valid"])
        self.assertTrue(report["datasets"][1]["structural_valid"])
        self.assertEqual(validator.validate_datasets([])["status"], "invalid")

    def test_nested_unknown_parameters_and_impossible_partial_counts(self):
        self.add_conversion()
        self.meta["capture"]["conversion"]["parameters"] = {"exposure": None}
        self.report("invalid", "unknown reason")
        self.meta["unknown_reasons"]["capture.conversion.parameters.exposure"] = "not recorded"
        self.report()
        self.meta["sequence"].update(complete=False, acquired_count=2, dropped_count=None)
        self.meta["unknown_reasons"]["sequence.dropped_count"] = "unknown"
        self.report("invalid", "less than stored")

    def test_symlink_cycle_is_a_diagnostic_not_a_traceback(self):
        path = self.root / "loop"
        try:
            path.symlink_to(path)
        except OSError as exc:
            self.skipTest(f"symlinks unavailable: {exc}")
        self.frames[0]["path"] = "loop"
        self.report("invalid")

    def test_reference_query_leakage_across_splits(self):
        other = Path(self.temp.name) / "two"
        m, frames, labels = make_dataset(other, "two", "test", 30)
        m["reference"]["recording_session_id"] = self.meta["query_session_id"]
        save_dataset(other, m, frames, labels)
        report = validator.validate_datasets([self.root, other])
        self.assertEqual(report["status"], "invalid")
        self.assertIn("query_reference_session", str(report))
        m["reference"]["recording_session_id"] = "reference-session"
        route = (other / "reference/route.vhrs").read_bytes()
        pixels = (self.root / self.frames[0]["path"]).read_bytes()[-4:]
        route = route[:50] + pixels + route[54:]
        (other / "reference/route.vhrs").write_bytes(route)
        m["reference"]["sha256"] = digest(route)
        save_dataset(other, m, frames, labels)
        report = validator.validate_datasets([self.root, other])
        self.assertEqual(report["status"], "invalid")
        self.assertIn("query_reference_content", str(report))

    def test_schema_and_row_size_limits(self):
        original_limit = validator.MAX_ROWS
        try:
            validator.MAX_ROWS = 2
            with self.assertRaisesRegex(validator.Invalid, "row count exceeds"):
                list(validator.Dataset(self.root).rows("frames.csv", validator.FRAME_FIELDS))
        finally:
            validator.MAX_ROWS = original_limit
        (self.root / "dataset.json").write_bytes(b" " * (4 * 1024 * 1024 + 1))
        report = validator.validate_datasets([self.root])
        self.assertEqual(report["status"], "invalid")
        self.assertIn("file exceeds size limit", str(report))

    def test_cli_json_exit_codes_and_path_with_spaces(self):
        for expected, status in ((0, "structurally_valid"), (3, "needs_review"), (2, "invalid")):
            if expected == 3:
                self.meta["independence"]["review_status"] = "pending"
            if expected == 2:
                self.frames[0]["sha256"] = "a" * 64
            save_dataset(self.root, self.meta, self.frames, self.labels)
            result = subprocess.run([sys.executable, str(SCRIPT), str(self.root)], capture_output=True, text=True)
            self.assertEqual(result.returncode, expected, result.stderr + result.stdout)
            self.assertEqual(result.stderr, "")
            self.assertEqual(json.loads(result.stdout)["status"], status)


if __name__ == "__main__":
    unittest.main()
