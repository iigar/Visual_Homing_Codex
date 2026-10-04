#!/usr/bin/env python3
"""Controlled replay integration oracles. Imagery and labels are synthetic."""
import csv
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import test_second_pass_scorer as t


SCRIPT = Path(__file__).resolve().parents[1] / "export-second-pass.py"
e = t.load("exporter", SCRIPT)
REAL_RUN = subprocess.run
BINARY = None


def configuration():
    return dict(schema_version=1, target_width=2, target_height=2, window_radius=0,
                minimum_confidence=0.99, max_direction_shift_px=0, radians_per_pixel=0.02,
                navigator_minimum_confidence=0.95, navigator_max_match_age_ms=200,
                navigator_yaw_gain=1, navigator_max_yaw_rate_radps=0.35,
                navigator_max_yaw_accel_radps2=1, navigator_forward_speed_mps=0)


def make_oracle(root):
    meta, frames, labels, _ = t.make_inputs(root)
    references = [(20, 60, 100, 140), (80, 120, 160, 200), (200, 140, 80, 20), (10, 90, 170, 250)]
    route = struct.pack("<4sHHBBHI", b"VHRS", 1, 16, 1, 0, 0, 4)
    for i, pixels in enumerate(references):
        route += struct.pack("<QQhfHHBBHI", i, 10 + i, 0, 0.0, 2, 2, 1, 0, 0, 4) + bytes(pixels)
    (root / meta["reference"]["path"]).write_bytes(route)
    meta["reference"]["sha256"] = t.fixtures.digest(route)
    indices = [0, 1, None, 2, 0, None, 3, 1]
    for i, index in enumerate(indices):
        pixels = bytes([255 if i == 2 else 0] * 4) if index is None else bytes(p + 1 for p in references[index])
        data = b"P5\n2 2\n255\n" + pixels
        (root / frames[i]["path"]).write_bytes(data)
        frames[i]["sha256"] = t.fixtures.digest(data)
        target = [0, 2, 1, None, None, 2, 3, 1][i]
        labels[i].update(location="unknown" if i == 4 else "off_route" if i == 3 else "on_route",
                         reference_index_min="" if target is None else str(target),
                         reference_index_max="" if target is None else str(target))
    t.fixtures.save_dataset(root, meta, frames, labels)
    return meta, frames, labels


class ExporterTests(unittest.TestCase):
    def setUp(self):
        # platform may call subprocess once; keep failure-injection mocks scoped
        # to the replay child even when a single test is run in isolation.
        e.platform.platform()
        self.temp = tempfile.TemporaryDirectory(prefix="second pass exporter ")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "dataset"
        self.output = self.base / "run with spaces"
        self.meta, self.frames, self.labels = make_oracle(self.root)
        self.config_path, self.policy_path = self.base / "configuration.json", self.base / "policy.json"
        self.config = configuration()
        e.write_json(self.config_path, self.config)
        e.write_json(self.policy_path, t.default_policy())

    def save(self):
        t.fixtures.save_dataset(self.root, self.meta, self.frames, self.labels)

    def export(self, **kwargs):
        return e.export(self.root, self.output, BINARY, self.config_path, self.policy_path,
                        "synthetic-test-build-declared", "synthetic-replay-oracle", **kwargs)

    def read(self, path):
        return json.loads((self.output / path).read_text())

    def predictions(self):
        with (self.output / "predictions.csv").open(newline="") as stream:
            return list(csv.DictReader(stream))

    def fake_success(self, args, **kwargs):
        text = io.StringIO()
        rows = []
        for frame in self.frames:
            rows.append({**{k: frame[k] for k in ("sequence", "frame_id", "timestamp_ns")},
                         "observation": "observed", "match_valid": "true", "reference_index": "0",
                         "navigation_command_valid": "true"})
        writer = csv.DictWriter(text, fieldnames=e.s.PREDICTION_FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
        kwargs["stdout"].write(text.getvalue().encode())
        return subprocess.CompletedProcess(args, 0)

    def test_actual_replay_to_scorer_hand_calculated_oracle(self):
        before = e.v.validate_datasets([self.root])["datasets"][0]["input_sha256"]
        result = self.export()
        self.assertEqual(result["status"], "exported", result)
        rows = self.predictions()
        self.assertEqual([r["reference_index"] for r in rows], ["0", "1", "", "2", "0", "", "3", "1"])
        self.assertEqual([r["match_valid"] for r in rows], ["true", "true", "false", "true", "true", "false", "true", "true"])
        self.assertEqual([r["navigation_command_valid"] for r in rows], [r["match_valid"] for r in rows])
        for row, source in zip(rows, self.frames):
            for key in ("sequence", "frame_id", "timestamp_ns"):
                self.assertEqual(row[key], source[key])
            for key in ("endpoint_stop", "route_readiness", "verification_accepted", "published"):
                self.assertEqual(row[key], "")
        report = self.read("score.json")
        metric = report["metrics"]
        self.assertEqual((metric["tp"], metric["fp"], metric["fn"]), (3, 2, 3))
        self.assertEqual(metric["precision"]["value"], 3 / 5)
        self.assertEqual(metric["recall"]["value"], 1 / 2)
        self.assertFalse(report["semantic_independence_verified"])
        self.assertEqual(self.read("execution.json")["status"], "complete")
        plan = self.read("plan.json")
        e.verify_frozen(self.output, plan["input_sha256"])
        self.assertEqual(self.read("run.json")["input_sha256"], before)
        self.assertEqual(e.v.validate_datasets([self.root])["datasets"][0]["input_sha256"], before)

    def test_actual_cli_and_repeat_are_deterministic(self):
        args = [sys.executable, str(SCRIPT), str(self.root), str(self.output), "--binary", str(BINARY),
                "--configuration", str(self.config_path), "--policy", str(self.policy_path),
                "--code-revision", "synthetic-declared", "--run-id", "repeat"]
        process = REAL_RUN(args, capture_output=True, text=True)
        self.assertEqual(process.returncode, 0, process.stderr + process.stdout)
        self.assertEqual(json.loads(process.stdout)["status"], "exported")
        first = (self.output / "predictions.csv").read_bytes()
        first_metrics = self.read("score.json")["metrics"]
        self.output = self.base / "repeat"
        self.assertEqual(self.export()["status"], "exported")
        self.assertEqual(first, (self.output / "predictions.csv").read_bytes())
        self.assertEqual(first_metrics, self.read("score.json")["metrics"])

    def test_frozen_plan_exists_before_launch_and_originals_are_not_consumed(self):
        def launch(args, **kwargs):
            plan = self.read("plan.json")
            e.verify_frozen(self.output, plan["input_sha256"])
            self.assertFalse((self.output / "run.json").exists())
            self.assertEqual(kwargs["cwd"], self.output)
            self.assertEqual(Path(args[0]), self.output / "implementation/replay")
            (self.root / self.frames[0]["path"]).write_bytes(b"original changed after freezing")
            self.config_path.write_text("changed after freezing")
            self.policy_path.write_text("changed after freezing")
            return REAL_RUN(args, **kwargs)
        with patch.object(e.subprocess, "run", side_effect=launch):
            result = self.export()
        self.assertEqual(result["status"], "exported", result)
        self.assertEqual(self.read("score.json")["metrics"]["tp"], 3)

    def test_existing_output_is_never_overwritten(self):
        self.output.mkdir()
        (self.output / "keep.txt").write_text("keep")
        with patch.object(e.subprocess, "run") as launch:
            self.assertEqual(self.export()["status"], "invalid")
            launch.assert_not_called()
        self.assertEqual((self.output / "keep.txt").read_text(), "keep")
        self.assertEqual(len(list(self.output.iterdir())), 1)

    def test_output_inside_input_rejected(self):
        self.output = self.root / "run"
        with patch.object(e.subprocess, "run") as launch:
            self.assertEqual(self.export()["status"], "invalid")
            launch.assert_not_called()
        self.assertFalse(self.output.exists())

    def test_invalid_and_review_pending_inputs_do_not_execute(self):
        with patch.object(e.subprocess, "run") as launch:
            self.meta["independence"]["review_status"] = "pending"
            self.save()
            self.assertEqual(self.export()["status"], "needs_review")
            self.meta["independence"]["review_status"] = "reviewed"
            self.frames[0]["sha256"] = "0" * 64
            self.save()
            self.assertEqual(self.export()["status"], "invalid")
            launch.assert_not_called()
        self.assertFalse(self.output.exists())

    def test_config_and_policy_invalid_before_execution(self):
        for key, value in (("window_radius", True), ("minimum_confidence", 1.1), ("navigator_yaw_gain", -1),
                           ("target_width", 4097), ("max_direction_shift_px", 2), ("extra", 3),
                           ("navigator_max_match_age_ms", "200"), ("navigator_yaw_gain", 10 ** 400)):
            with self.subTest(key=key, value=value), patch.object(e.subprocess, "run") as launch:
                invalid = dict(self.config, **{key: value})
                e.write_json(self.config_path, invalid)
                self.assertEqual(self.export()["status"], "invalid")
                launch.assert_not_called()
        self.config_path.write_text('{"schema_version": NaN}')
        self.assertEqual(self.export()["status"], "invalid")
        e.write_json(self.config_path, self.config)
        self.policy_path.write_text('{"schema_version": 1, "schema_version": 1}')
        with patch.object(e.subprocess, "run") as launch:
            self.assertEqual(self.export()["status"], "invalid")
            launch.assert_not_called()

    def test_target_geometry_mismatch_does_not_execute(self):
        self.config["target_width"] = 3
        e.write_json(self.config_path, self.config)
        with patch.object(e.subprocess, "run") as launch:
            result = self.export()
            self.assertEqual(result["status"], "invalid", result)
            launch.assert_not_called()
        self.assertIn("geometry", str(result))
        self.assertFalse((self.output / "plan.json").exists())

    def test_snapshot_mutation_or_binary_mutation_blocks_completion(self):
        for name in ("dataset/frames/0.pgm", "implementation/replay", "policy.json", "plan.json"):
            with self.subTest(name=name):
                self.output = self.base / name.replace("/", "-")
                def launch(args, **kwargs):
                    result = self.fake_success(args, **kwargs)
                    with (self.output / name).open("ab") as stream:
                        stream.write(b"tampered")
                    return result
                with patch.object(e.subprocess, "run", side_effect=launch):
                    result = self.export()
                self.assertEqual(result["status"], "invalid", result)
                self.assertFalse((self.output / "run.json").exists())
                self.assertFalse((self.output / "execution.json").exists())

    def test_input_change_between_validation_and_copy_is_rejected(self):
        real_copy = e.copy_checked
        def mutate(store, relative, destination, expected):
            if relative == "frames/0.pgm" and store.root == self.root:
                (self.root / relative).write_bytes(b"changed before copying")
            return real_copy(store, relative, destination, expected)
        with patch.object(e, "copy_checked", side_effect=mutate), patch.object(e.subprocess, "run") as launch:
            result = self.export()
            self.assertEqual(result["status"], "invalid", result)
            launch.assert_not_called()
        self.assertFalse((self.output / "plan.json").exists())

    def test_validated_inputs_and_predictions_cannot_be_repinned(self):
        real_copy = e.copy_checked
        for name in ("configuration.json", "policy.json"):
            with self.subTest(name=name):
                self.output = self.base / ("changed-" + name)
                def mutate(store, relative, destination, expected):
                    real_copy(store, relative, destination, expected)
                    if destination == self.output / name:
                        # Still valid JSON; must retain the originally admitted hash.
                        with destination.open("ab") as stream:
                            stream.write(b"\n")
                with patch.object(e, "copy_checked", side_effect=mutate), patch.object(e.subprocess, "run", side_effect=self.fake_success) as launch:
                    result = self.export()
                    self.assertEqual(result["status"], "invalid", result)
                    launch.assert_not_called()
                self.assertFalse((self.output / "execution.json").exists())

        self.output = self.base / "changed-validated-predictions"
        real_read = e.s.read_predictions
        def mutate_result(store, record, frames, entry_count):
            predictions = real_read(store, record, frames, entry_count)
            if record["path"] == "raw-predictions.csv":
                with (self.output / record["path"]).open("ab") as stream:
                    stream.write(b"\n")
            return predictions
        with patch.object(e.s, "read_predictions", side_effect=mutate_result):
            self.assertEqual(self.export()["status"], "invalid")
        self.assertFalse((self.output / "execution.json").exists())

    def assert_missing_failure(self, reason):
        self.assertEqual(self.read("execution.json")["status"], "failed")
        for row in self.predictions():
            self.assertEqual(row["observation"], "missing")
            self.assertEqual(row["missing_reason"], reason)
            self.assertTrue(all(row[key] == "" for key in e.s.PREDICTION_FIELDS[5:]))
        self.assertEqual(self.read("score.json")["metrics"]["prediction_coverage"]["value"], 0)
        self.assertEqual(self.read("score.json")["metrics"]["fn"], 6)

    def test_partial_replay_failure_discards_all_partial_results(self):
        def fail(args, **kwargs):
            self.fake_success(args, **kwargs)
            return subprocess.CompletedProcess(args, 2)
        with patch.object(e.subprocess, "run", side_effect=fail):
            self.assertEqual(self.export()["status"], "replay_failed")
        self.assert_missing_failure("replay_failed")

    def test_timeout_and_launch_failure_are_explicit_missing(self):
        for reason, error in (("replay_timeout", subprocess.TimeoutExpired("synthetic", 1)),
                              ("replay_launch_failed", OSError("synthetic launch failure"))):
            with self.subTest(reason=reason):
                self.output = self.base / reason
                with patch.object(e.subprocess, "run", side_effect=error):
                    self.assertEqual(self.export(timeout_seconds=1)["status"], "replay_failed")
                self.assert_missing_failure(reason)

    def test_malformed_omitted_reordered_or_invented_outcomes_are_missing(self):
        for kind in ("empty", "missing", "wrong_id", "wrong_time", "endpoint", "publication", "no_navigation", "extra"):
            with self.subTest(kind=kind):
                self.output = self.base / kind
                def launch(args, **kwargs):
                    fake = io.BytesIO()
                    self.fake_success(args, **{**kwargs, "stdout": fake})
                    rows = list(csv.DictReader(io.StringIO(fake.getvalue().decode())))
                    if kind == "empty":
                        rows = []
                    elif kind == "missing":
                        rows.pop()
                    elif kind == "wrong_id":
                        rows[0]["frame_id"] = rows[1]["frame_id"]
                    elif kind == "wrong_time":
                        rows[0]["timestamp_ns"] = str(int(rows[0]["timestamp_ns"]) + 1)
                    elif kind == "endpoint":
                        rows[0]["endpoint_stop"] = "false"
                    elif kind == "publication":
                        rows[0]["published"] = "true"
                    elif kind == "no_navigation":
                        rows[0]["navigation_command_valid"] = ""
                    else:
                        rows.append(rows[-1])
                    text = io.StringIO()
                    writer = csv.DictWriter(text, fieldnames=e.s.PREDICTION_FIELDS, lineterminator="\n")
                    writer.writeheader()
                    writer.writerows(rows)
                    kwargs["stdout"].write(text.getvalue().encode())
                    return subprocess.CompletedProcess(args, 0)
                with patch.object(e.subprocess, "run", side_effect=launch):
                    result = self.export()
                self.assertEqual(result["status"], "replay_failed", result)
                self.assert_missing_failure("replay_output_invalid")

    def test_peer_is_frozen_and_cross_split_leakage_rejected(self):
        peer = self.base / "peer"
        t.fixtures.make_dataset(peer, name="peer", split="test", pixel_offset=40)
        result = self.export(peers=[peer])
        self.assertEqual(result["status"], "exported", result)
        self.assertTrue((self.output / "peers/0001/dataset.json").is_file())
        self.assertEqual(len(self.read("score.json")["dataset_validation"]["datasets"]), 2)
        peer_meta, peer_frames, peer_labels = t.make_inputs(self.base / "leak")[:3]
        peer_meta["dataset_id"] = "leak"
        peer_meta["split"] = "test"
        t.fixtures.save_dataset(self.base / "leak", peer_meta, peer_frames, peer_labels)
        self.output = self.base / "rejected"
        with patch.object(e.subprocess, "run") as launch:
            self.assertEqual(self.export(peers=[self.base / "leak"])["status"], "invalid")
            launch.assert_not_called()

    def test_actual_maximum_source_ids_and_timestamps_preserved(self):
        for i, (frame, label) in enumerate(zip(self.frames, self.labels)):
            frame["frame_id"] = label["frame_id"] = str(e.v.U64 - 7 + i)
            frame["timestamp_ns"] = str(e.v.I64 - 700 + 100 * i)
        self.meta["sequence"]["actual"] = dict(first_frame_id=e.v.U64 - 7, last_frame_id=e.v.U64,
                                                first_timestamp_ns=e.v.I64 - 700, last_timestamp_ns=e.v.I64)
        self.save()
        result = self.export()
        self.assertEqual(result["status"], "exported", result)
        self.assertEqual(self.predictions()[-1]["frame_id"], str(e.v.U64))
        self.assertEqual(self.predictions()[-1]["timestamp_ns"], str(e.v.I64))

    def test_actual_resize_preserves_source_identity(self):
        for frame in self.frames:
            path = self.root / frame["path"]
            pixels = path.read_bytes()[-4:]
            data = b"P5\r\n4 4\r\n255\r\n" + bytes(pixels[(y // 2) * 2 + x // 2] for y in range(4) for x in range(4))
            path.write_bytes(data)
            frame.update(width="4", height="4", sha256=t.fixtures.digest(data))
        self.meta["capture"].update(native_width=4, native_height=4)
        self.save()
        result = self.export()
        self.assertEqual(result["status"], "exported", result)
        self.assertEqual(self.read("score.json")["metrics"]["tp"], 3)

    def test_actual_matcher_state_is_retained_across_a_declared_gap(self):
        data = b"P5\n2 2\n255\n" + bytes([11, 91, 171, 251])  # Closest to reference 3.
        (self.root / self.frames[1]["path"]).write_bytes(data)
        self.frames[1]["sha256"] = t.fixtures.digest(data)
        self.meta["sequence"]["gaps"] = [dict(after_frame_id=10, before_frame_id=11,
            start_timestamp_ns=150, end_timestamp_ns=150, missing_count=0,
            reason="synthetic interruption with no acquired drop", evidence_id="manual")]
        self.save()
        self.config.update(window_radius=1, minimum_confidence=0)
        e.write_json(self.config_path, self.config)
        result = self.export()
        self.assertEqual(result["status"], "exported", result)
        # Matcher keeps previous index 0, searches only 0..1, so cannot select 3.
        self.assertEqual(self.predictions()[1]["reference_index"], "1")
        self.assertEqual(self.read("score.json")["temporal_coverage"]["boundaries"][0]["reasons"], ["declared_gap"])

    def test_real_process_timeout_and_nonzero_exit_have_no_partial_credit(self):
        shim = self.base / "synthetic-adapter"
        for mode in ("timeout", "exit"):
            with self.subTest(mode=mode):
                shim.write_text("#!/usr/bin/env python3\nimport sys, time\n"
                                "print('synthetic partial output', flush=True)\n"
                                + ("time.sleep(10)\n" if mode == "timeout" else "sys.exit(7)\n"))
                shim.chmod(0o700)
                self.output = self.base / mode
                args = [sys.executable, str(SCRIPT), str(self.root), str(self.output), "--binary", str(shim),
                        "--configuration", str(self.config_path), "--policy", str(self.policy_path),
                        "--code-revision", "synthetic-shim", "--run-id", mode, "--timeout-seconds", "1"]
                result = REAL_RUN(args, capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(json.loads(result.stdout)["status"], "replay_failed")
                self.assert_missing_failure("replay_timeout" if mode == "timeout" else "replay_failed")
                self.assertIn(b"synthetic partial", (self.output / "raw-predictions.csv").read_bytes())

    def test_cli_invalid_and_pending_exit_codes(self):
        self.meta["independence"]["review_status"] = "pending"
        self.save()
        args = [sys.executable, str(SCRIPT), str(self.root), str(self.output), "--binary", str(BINARY),
                "--configuration", str(self.config_path), "--policy", str(self.policy_path),
                "--code-revision", "synthetic", "--run-id", "pending"]
        result = REAL_RUN(args, capture_output=True, text=True)
        self.assertEqual(result.returncode, 3, result.stderr)
        self.assertEqual(json.loads(result.stdout)["status"], "needs_review")
        self.config_path.write_text("invalid json")
        result = REAL_RUN(args, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2, result.stderr)

    def test_adapter_rejects_bad_clock_and_numeric_arguments(self):
        self.assertEqual(self.export()["status"], "exported")
        args = self.read("plan.json")["arguments"]
        args[0] = str(self.output / args[0])
        for index, value in ((4, "0"), (4, "2junk"), (7, "nan"), (8, "-1"), (10, "1.1")):
            with self.subTest(index=index, value=value):
                changed = args.copy()
                changed[index] = value
                result = REAL_RUN(changed, cwd=self.output, capture_output=True)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, b"")
        for text in ("", "0\n", "100\n100\n", str(e.v.I64 + 1) + "\n", "100\n"):
            (self.output / "clock.txt").write_text(text)
            result = REAL_RUN(args, cwd=self.output, capture_output=True)
            self.assertEqual(result.returncode, 2)


if __name__ == "__main__":
    BINARY = Path(sys.argv.pop(1)).resolve()
    unittest.main()
