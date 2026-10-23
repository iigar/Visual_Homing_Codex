#!/usr/bin/env python3
"""Linux resolution and separate child-RSS contracts; synthetic replay only."""
import copy
from pathlib import Path
import signal
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import MagicMock, patch

import benchmark_second_pass_campaign as p

f, geometry = p.f, p.resolution
BINARY = None


class ResolutionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temporary = tempfile.TemporaryDirectory(prefix="resolution tests ")
        cls.addClassCleanup(temporary.cleanup)
        cls.base = Path(temporary.name)
        cls.cases = {}
        for name, source_side in (("identity", 32), ("downsample", 64)):
            inputs, archive = cls.base / (name + "-inputs"), cls.base / (name + "-archive")
            request = p.make_inputs(inputs, 16)
            geometry.apply_resolution(inputs, request, 32, source_side)
            state = f.r.run(inputs / "request.json", archive, BINARY, f.REVISION)
            p.require(state["status"] == "verified", str(state))
            report = f.a.check(archive)
            p.require(report["status"] == "verified", str(report))
            cls.cases[name] = inputs, archive, request, report

    def test_quadrants_and_invalid_sizes_before_writes(self):
        self.assertEqual(geometry.quadrants((10, 20, 30, 40), 4),
                         bytes([10, 10, 20, 20] * 2 + [30, 30, 40, 40] * 2))
        with patch.object(f.e, "write_json", side_effect=AssertionError("invalid size wrote files")):
            for value in (None, True, "32", 32.0, 0, 1, 3, 257, 258):
                with self.subTest(value=value), self.assertRaises(RuntimeError):
                    geometry.apply_resolution(self.base / "invalid", {}, value)

    def test_original_two_pixel_fixture_is_unchanged(self):
        inputs = self.base / "original"
        request = p.make_inputs(inputs, 8)
        before = f.snapshot(inputs)
        self.assertEqual(geometry.apply_resolution(inputs, copy.deepcopy(request), 2), request)
        self.assertEqual(before, f.snapshot(inputs))

    def test_fixture_records_geometry_and_configuration_bindings(self):
        for name, (inputs, _, request, _) in self.cases.items():
            side = 32 if name == "identity" else 64
            for index, split in enumerate(f.SPLITS):
                root = inputs / split
                frames = geometry.rows(root / "frames.csv")
                self.assertEqual(len(frames), 16)
                self.assertEqual([int(row["frame_id"]) for row in frames], list(range((index + 1) * 1000, (index + 1) * 1000 + 16)))
                self.assertEqual([int(row["timestamp_ns"]) for row in frames], [(index + 1) * 100000 + n * 100 for n in range(16)])
                for frame in frames:
                    self.assertEqual((frame["width"], frame["height"]), (str(side), str(side)))
                    self.assertEqual(p.digest(root / frame["path"]), frame["sha256"])
                    self.assertEqual(len((root / frame["path"]).read_bytes().split(b"\n", 3)[3]), side * side)
                meta = f.read(root, "dataset.json")
                self.assertEqual(meta["capture"]["native_width"], side)
                route = (root / meta["reference"]["path"]).read_bytes()
                self.assertEqual(len(route), 16 + 4 * (34 + 32 * 32))
                for reference in range(4):
                    offset = 16 + reference * (34 + 32 * 32)
                    fields = struct.unpack_from("<QQhfHHBBHI", route, offset)
                    self.assertEqual((fields[0], fields[4], fields[5], fields[-1]), (reference, 32, 32, 1024))
                    pixels = route[offset + 34:offset + 34 + 1024]
                    values = f.REFERENCES[reference]
                    expected = values[index:] + values[:index]
                    self.assertEqual(tuple(pixels[i] for i in (0, 31, 31 * 32, 1023)), expected)
            for requested in request["runs"]:
                config = f.read(inputs, requested["configuration"]["path"])
                original = f.t.configuration()
                original.update(target_width=32, target_height=32,
                                minimum_confidence=0.99 if f.variants(requested)[0] == "near" else 1.0)
                self.assertEqual(config, original)
                self.assertEqual(p.digest(inputs / requested["configuration"]["path"]), requested["configuration"]["sha256"])

    def test_actual_identity_and_downsampling_have_same_oracles(self):
        oracles = [p.verify_collection(report, request, archive, inputs, 16)
                   for inputs, archive, request, report in self.cases.values()]
        self.assertEqual(oracles[0], oracles[1])
        self.assertEqual([(row["tp"], row["fp"], row["fn"]) for row in oracles[0]["metrics"]],
                         [(6, 4, 6), (4, 4, 6), (0, 0, 12), (0, 0, 10)] * 3)

    def test_diagnostic_replays_match_without_archive_mutation(self):
        _, archive, _, report = self.cases["identity"]
        before = f.snapshot(archive)
        with patch.object(geometry, "run_timed", wraps=geometry.run_timed) as launch:
            result = geometry.measure_replays(archive, self.base / "rss", BINARY, Path("/usr/bin/time"), report)
        self.assertEqual(result["executions"], 12)
        self.assertEqual(launch.call_count, 12)
        for call in launch.call_args_list:
            command = call.args[0]
            self.assertEqual(command[command.index("--") + 1], str(BINARY))
        self.assertTrue(all(row["max_rss_kib"] > 0 and row["returncode"] == 0 for row in result["records"]))
        self.assertEqual(before, f.snapshot(archive))

    def test_changed_plan_and_wrong_binary_prevent_execution(self):
        _, archive, _, report = self.cases["identity"]
        path = archive / "collection/bundles/0000/plan.json"
        original = path.read_bytes()
        with patch.object(geometry, "run_timed", side_effect=AssertionError("unbound execution")):
            try:
                path.write_bytes(original + b" ")
                with self.assertRaisesRegex(RuntimeError, "plan changed"):
                    geometry.measure_replays(archive, self.base / "changed-plan", BINARY, Path("/usr/bin/time"), report)
            finally:
                path.write_bytes(original)
            wrong = self.base / "wrong-binary"
            wrong.write_bytes(b"inert untrusted bytes")
            with self.assertRaisesRegex(RuntimeError, "binary differs"):
                geometry.measure_replays(archive, self.base / "wrong-binary-rss", wrong, Path("/usr/bin/time"), report)

    def test_timeout_and_interruption_kill_diagnostic_group_and_reap(self):
        for error in (subprocess.TimeoutExpired("replay", 1), KeyboardInterrupt()):
            process = MagicMock(pid=12345)
            process.communicate.side_effect = error
            manager = MagicMock()
            manager.__enter__.return_value = process
            with patch.object(geometry.subprocess, "Popen", return_value=manager) as popen, \
                 patch.object(geometry.os, "killpg") as kill:
                with self.assertRaises(type(error)):
                    geometry.run_timed(["fake"], self.base, None, None, 1)
            self.assertTrue(popen.call_args.kwargs["start_new_session"])
            kill.assert_called_once_with(12345, signal.SIGKILL)
            process.wait.assert_called_once()


if __name__ == "__main__":
    BINARY = Path(sys.argv.pop(1)).resolve()
    unittest.main()
