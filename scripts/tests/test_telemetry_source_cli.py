"""Offline source selection and heartbeat contract through the actual CLI."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

CORE_EXE = Path(sys.argv.pop(1)).resolve()
SYSTEM = "VISUAL_HOMING_TELEMETRY_SYSTEM_ID"
COMPONENT = "VISUAL_HOMING_TELEMETRY_COMPONENT_ID"
# pymavlink 2.4.49 wire fixtures, same provenance as mavlink_telemetry_golden.hpp.
HEARTBEAT = bytes.fromhex("fe09172a110004000000020380040395ac")
ATTITUDE = bytes.fromhex("fe1c172a111ee80300000000803e000000bf0000c03f000000000000000000000000e8da")
POSITION = bytes.fromhex("fe1c172a1121e803000000000000000000000000000004a60000000000000000000011f0")


def rewrite(wire, *, system=42, component=17, vehicle_type=2, autopilot=3):
    data = bytearray(wire[:-2])
    data[3:5] = bytes((system, component))
    if data[5] == 0:
        data[10:12] = bytes((vehicle_type, autopilot))
        data[12] = 129  # Armed + CUSTOM_MODE_ENABLED.
    crc = 0xffff
    for byte in data[1:] + bytes(({0: 50, 30: 39, 33: 104}[data[5]],)):
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0x8408 if crc & 1 else 0)
    return bytes(data) + crc.to_bytes(2, "little")


class TelemetrySourceCLI(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="telemetry source ")
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "telemetry data.bin"
        self.path.write_bytes(rewrite(HEARTBEAT) + ATTITUDE + POSITION)

    def run_cli(self, source=("42", "17"), inspect=False):
        env = os.environ.copy()
        for key, value in zip((SYSTEM, COMPONENT), source):
            env.pop(key, None)
            if value is not None:
                env[key] = value
        command = [str(CORE_EXE), "--inspect-mavlink-telemetry" if inspect else "--validate-mavlink-telemetry",
                   str(self.path)]
        if not inspect:
            command += ["1", "1", "1", "0"]
        return subprocess.run(command, env=env, capture_output=True, text=True, timeout=10)

    def test_explicit_source(self):
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for field in ("selected_system_id=42", "selected_component_id=17", "selected_source_frames=3",
                      "source_passed=true", "heartbeat_contract_passed=true", "mode=Guided", "passed=true"):
            self.assertIn(field, result.stdout)

    def test_unconfigured_is_structural_only(self):
        result = self.run_cli((None, None), inspect=True)
        self.assertEqual(result.returncode, 0)
        for field in ("frames_seen=3", "selected_source_frames=0", "unselected_source_frames=3",
                      "heartbeat_messages=0", "mode=Unknown"):
            self.assertIn(field, result.stdout)
        result = self.run_cli((None, None))
        self.assertEqual(result.returncode, 2)
        self.assertIn("source_passed=false", result.stdout)

    def test_partial_and_invalid_configuration(self):
        for source in (("42", None), (None, "17")):
            result = self.run_cli(source)
            self.assertEqual(result.returncode, 1)
            self.assertIn("Both VISUAL_HOMING_TELEMETRY", result.stderr)
        for value in ("0", "256", "-1", " -1", "", " ", "1x", "1.5", "4294967296", "18446744073709551616"):
            for source in ((value, "17"), ("42", value)):
                with self.subTest(source=source):
                    self.assertEqual(self.run_cli(source).returncode, 1)

    def test_source_boundaries_and_wrong_source(self):
        for value in (1, 255):
            self.path.write_bytes(b"".join(rewrite(p, system=value, component=value)
                                          for p in (HEARTBEAT, ATTITUDE, POSITION)))
            self.assertEqual(self.run_cli((str(value), str(value))).returncode, 0)
        for source in (("43", "17"), ("42", "18")):
            self.assertEqual(self.run_cli(source).returncode, 2)

    def test_mixed_and_zero_sources(self):
        for options in ({"system": 43}, {"component": 18}, {"system": 0}, {"component": 0}):
            for index in range(3):
                packets = [rewrite(HEARTBEAT), ATTITUDE, POSITION]
                packets[index] = rewrite(packets[index], **options)
                self.path.write_bytes(b"".join(packets))
                result = self.run_cli()
                self.assertEqual(result.returncode, 2, result.stdout)
                self.assertIn("selected_source_frames=2", result.stdout)

    def test_unsupported_heartbeat(self):
        for options in ({"vehicle_type": 1}, {"autopilot": 12}, {"vehicle_type": 6, "autopilot": 8}):
            self.path.write_bytes(rewrite(HEARTBEAT, **options) + ATTITUDE + POSITION)
            result = self.run_cli()
            self.assertEqual(result.returncode, 2)
            self.assertIn("heartbeat_contract_passed=false", result.stdout)
            self.assertIn("mode=Unknown", result.stdout)

    def test_crc_still_checked_without_source(self):
        corrupt = bytearray(HEARTBEAT)
        corrupt[-1] ^= 1
        self.path.write_bytes(corrupt)
        for source in (("42", "17"), (None, None)):
            result = self.run_cli(source, inspect=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn("checksum_errors=1", result.stdout)


if __name__ == "__main__":
    unittest.main()
