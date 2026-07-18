#!/usr/bin/env python3
"""Run the Visual Homing route-local estimator against exact Copter 4.3.6 SITL.

This harness is intentionally local-only. It starts ArduPilot SITL with no
MAVProxy, connects through localhost TCP, feeds raw MAVLink2 ODOMETRY frames
created by the C++ estimator/encoder producer, and never opens serial hardware.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
import time
from typing import Any


EXPECTED_ARDUPILOT_COMMIT = "0c5e999c44785b0b8f53e7758856ceea614ef01b"
SITL_LAT = -35.363261
SITL_LON = 149.165230
SITL_ALT_MSL_M = 584.0
EXPECTED_PARAMETERS = {
    "AHRS_EKF_TYPE": 3.0,
    "EK2_ENABLE": 0.0,
    "EK3_ENABLE": 1.0,
    "EK3_SRC1_POSXY": 6.0,
    "EK3_SRC1_POSZ": 6.0,
    "EK3_SRC1_VELXY": 6.0,
    "EK3_SRC1_VELZ": 0.0,
    "EK3_SRC1_YAW": 6.0,
    "GPS_TYPE": 0.0,
    "VISO_TYPE": 1.0,
}


class AcceptanceError(RuntimeError):
    pass


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[1]
    timestamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    parser = argparse.ArgumentParser()
    parser.add_argument("--ardupilot-root", type=Path, required=True)
    parser.add_argument(
        "--producer",
        type=Path,
        default=repo_root / "core" / "build-wsl-sitl" / "route_local_odometry_sitl_producer",
    )
    parser.add_argument(
        "--parameter-file",
        type=Path,
        default=repo_root / "config" / "sitl" / "arducopter-4.3.6-route-local-odometry.parm",
    )
    parser.add_argument(
        "--artifact-dir",
        type=Path,
        default=repo_root / "artifacts" / "sitl" / f"route-local-odometry-{timestamp}",
    )
    parser.add_argument("--acquire-timeout-s", type=float, default=45.0)
    parser.add_argument("--provider-timeout-s", type=float, default=12.0)
    parser.add_argument("--recovery-timeout-s", type=float, default=30.0)
    return parser.parse_args()


def run_checked(command: list[str], cwd: Path | None = None) -> str:
    completed = subprocess.run(
        command,
        cwd=cwd,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    return completed.stdout.strip()


def parse_fields(line: str) -> tuple[str, dict[str, str]]:
    parts = line.strip().split()
    if not parts:
        raise AcceptanceError("producer returned an empty response")
    fields: dict[str, str] = {}
    for part in parts[1:]:
        if "=" in part:
            key, value = part.split("=", 1)
            fields[key] = value
    return parts[0], fields


class Producer:
    def __init__(self, executable: Path):
        self.process = subprocess.Popen(
            [str(executable)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        hello = self._readline()
        kind, fields = parse_fields(hello)
        if kind != "HELLO" or fields.get("protocol") != "1":
            raise AcceptanceError(f"unexpected producer greeting: {hello}")

    def _readline(self) -> str:
        if self.process.stdout is None:
            raise AcceptanceError("producer stdout is unavailable")
        line = self.process.stdout.readline()
        if line:
            return line.rstrip("\n")
        stderr = ""
        if self.process.stderr is not None:
            stderr = self.process.stderr.read()
        raise AcceptanceError(f"producer exited unexpectedly: {stderr.strip()}")

    def command(self, command: str) -> tuple[str, dict[str, str]]:
        if self.process.stdin is None:
            raise AcceptanceError("producer stdin is unavailable")
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        line = self._readline()
        kind, fields = parse_fields(line)
        if kind == "ERROR":
            raise AcceptanceError(f"producer rejected command {command!r}: {line}")
        return kind, fields

    def update(self, health_ready: bool = True) -> tuple[str, dict[str, str]]:
        now_ms = int(time.time() * 1000)
        return self.command(
            f"UPDATE {now_ms} 0.0 0.5 0.0 {1 if health_ready else 0} forward"
        )

    def close(self) -> None:
        if self.process.poll() is None:
            try:
                self.command("QUIT")
            except AcceptanceError:
                pass
        try:
            self.process.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            self.process.kill()


def wait_for_connection(mavutil: Any, timeout_s: float):
    deadline = time.monotonic() + timeout_s
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        connection = None
        try:
            connection = mavutil.mavlink_connection(
                "tcp:127.0.0.1:5760",
                source_system=191,
                source_component=190,
                autoreconnect=False,
            )
            heartbeat = connection.wait_heartbeat(timeout=3.0)
            if heartbeat is not None:
                return connection, heartbeat
        except Exception as error:  # connection refused while SITL is starting
            last_error = error
        if connection is not None:
            connection.close()
        time.sleep(0.5)
    raise AcceptanceError(f"SITL TCP heartbeat timeout: {last_error}")


def request_message(master: Any, mavlink: Any, message_id: int) -> None:
    master.mav.command_long_send(
        master.target_system,
        master.target_component,
        mavlink.MAV_CMD_REQUEST_MESSAGE,
        0,
        float(message_id),
        0,
        0,
        0,
        0,
        0,
        0,
    )


def read_parameter(master: Any, name: str, timeout_s: float = 5.0) -> float:
    encoded = name.encode("ascii")
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        master.mav.param_request_read_send(
            master.target_system,
            master.target_component,
            encoded,
            -1,
        )
        inner_deadline = min(deadline, time.monotonic() + 0.8)
        while time.monotonic() < inner_deadline:
            message = master.recv_match(type="PARAM_VALUE", blocking=True, timeout=0.2)
            if message is None:
                continue
            param_id = message.param_id
            if isinstance(param_id, bytes):
                param_id = param_id.decode("ascii", errors="replace")
            if str(param_id).rstrip("\x00") == name:
                return float(message.param_value)
    raise AcceptanceError(f"parameter {name} was not reported")


def verify_autopilot_version(master: Any, mavlink: Any) -> dict[str, Any]:
    request_message(master, mavlink, mavlink.MAVLINK_MSG_ID_AUTOPILOT_VERSION)
    message = master.recv_match(type="AUTOPILOT_VERSION", blocking=True, timeout=8.0)
    if message is None:
        raise AcceptanceError("AUTOPILOT_VERSION was not reported")
    custom = bytes(message.flight_custom_version).rstrip(b"\x00")
    custom_text = custom.decode("ascii", errors="replace")
    if not custom_text.startswith(EXPECTED_ARDUPILOT_COMMIT[:8]):
        raise AcceptanceError(
            f"unexpected SITL firmware hash {custom_text!r}; expected {EXPECTED_ARDUPILOT_COMMIT[:8]}"
        )
    return {
        "flight_sw_version": int(message.flight_sw_version),
        "flight_custom_version": custom_text,
    }


def set_origin(master: Any) -> None:
    master.mav.system_time_send(int(time.time() * 1_000_000), 0)
    master.mav.set_gps_global_origin_send(
        master.target_system,
        int(SITL_LAT * 10_000_000),
        int(SITL_LON * 10_000_000),
        int(SITL_ALT_MSL_M * 1000),
    )


def confirm_origin_and_home(master: Any, mavlink: Any, timeout_s: float = 12.0) -> dict[str, Any]:
    expected_lat = int(SITL_LAT * 10_000_000)
    expected_lon = int(SITL_LON * 10_000_000)
    evidence: dict[str, Any] = {}
    started = time.monotonic()
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        set_origin(master)
        request_message(master, mavlink, mavlink.MAVLINK_MSG_ID_GPS_GLOBAL_ORIGIN)
        request_message(master, mavlink, mavlink.MAVLINK_MSG_ID_HOME_POSITION)
        inner_deadline = min(deadline, time.monotonic() + 0.8)
        while time.monotonic() < inner_deadline:
            message = master.recv_match(
                type=["GPS_GLOBAL_ORIGIN", "HOME_POSITION", "GLOBAL_POSITION_INT"],
                blocking=True,
                timeout=0.2,
            )
            if message is None:
                continue
            message_type = message.get_type()
            if message_type == "GPS_GLOBAL_ORIGIN":
                evidence["origin"] = {
                    "latitude": int(message.latitude),
                    "longitude": int(message.longitude),
                    "altitude": int(message.altitude),
                }
            elif message_type == "HOME_POSITION":
                evidence["home"] = {
                    "latitude": int(message.latitude),
                    "longitude": int(message.longitude),
                    "altitude": int(message.altitude),
                }
            elif int(message.lat) != 0 and int(message.lon) != 0:
                evidence["global_position"] = {
                    "latitude": int(message.lat),
                    "longitude": int(message.lon),
                    "altitude": int(message.alt),
                }

        origin = evidence.get("origin")
        home = evidence.get("home")
        global_position = evidence.get("global_position")
        origin_known = origin is not None and (
            abs(origin["latitude"] - expected_lat) <= 1
            and abs(origin["longitude"] - expected_lon) <= 1
        )
        global_known = global_position is not None
        home_known = home is not None and home["latitude"] != 0 and home["longitude"] != 0
        if (origin_known or global_known) and home_known:
            evidence["passed"] = True
            evidence["home_reported"] = True
            return evidence
        if (origin_known or global_known) and time.monotonic() - started >= 3.0:
            evidence["passed"] = True
            evidence["home_reported"] = False
            evidence["home_status"] = "not_reported_with_gps_disabled"
            return evidence
    evidence["passed"] = False
    raise AcceptanceError(f"SITL origin was not confirmed: {evidence}")


def request_home_position(
    master: Any, mavlink: Any, timeout_s: float
) -> dict[str, int] | None:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        request_message(master, mavlink, mavlink.MAVLINK_MSG_ID_HOME_POSITION)
        inner_deadline = min(deadline, time.monotonic() + 0.8)
        while time.monotonic() < inner_deadline:
            message = master.recv_match(type="HOME_POSITION", blocking=True, timeout=0.2)
            if message is not None:
                return {
                    "latitude": int(message.latitude),
                    "longitude": int(message.longitude),
                    "altitude": int(message.altitude),
                }
    return None


def home_matches_sitl_origin(home: dict[str, int]) -> bool:
    expected_lat = int(SITL_LAT * 10_000_000)
    expected_lon = int(SITL_LON * 10_000_000)
    expected_alt = int(SITL_ALT_MSL_M * 1000)
    return (
        abs(home["latitude"] - expected_lat) <= 1
        and abs(home["longitude"] - expected_lon) <= 1
        and abs(home["altitude"] - expected_alt) <= 10
    )


def set_and_confirm_home(master: Any, mavlink: Any, timeout_s: float = 8.0) -> dict[str, Any]:
    before_command = request_home_position(master, mavlink, 2.0)
    expected_lat = int(SITL_LAT * 10_000_000)
    expected_lon = int(SITL_LON * 10_000_000)
    master.mav.command_int_send(
        master.target_system,
        master.target_component,
        mavlink.MAV_FRAME_GLOBAL,
        mavlink.MAV_CMD_DO_SET_HOME,
        0,
        0,
        0.0,
        0.0,
        0.0,
        0.0,
        expected_lat,
        expected_lon,
        SITL_ALT_MSL_M,
    )

    deadline = time.monotonic() + timeout_s
    ack_result: int | None = None
    while time.monotonic() < deadline:
        message = master.recv_match(type="COMMAND_ACK", blocking=True, timeout=0.2)
        if message is None or int(message.command) != mavlink.MAV_CMD_DO_SET_HOME:
            continue
        ack_result = int(message.result)
        break
    if ack_result != mavlink.MAV_RESULT_ACCEPTED:
        raise AcceptanceError(f"MAV_CMD_DO_SET_HOME was not accepted: result={ack_result}")

    after_command = request_home_position(master, mavlink, timeout_s)
    if after_command is None:
        raise AcceptanceError("HOME_POSITION was not reported after accepted MAV_CMD_DO_SET_HOME")
    if not home_matches_sitl_origin(after_command):
        raise AcceptanceError(
            f"HOME_POSITION does not match the configured SITL origin: {after_command}"
        )
    return {
        "passed": True,
        "reported_before_command": before_command is not None,
        "before_command": before_command,
        "command": "MAV_CMD_DO_SET_HOME",
        "transport": "COMMAND_INT",
        "command_result": ack_result,
        "home": after_command,
    }


def confirm_home_persists(master: Any, mavlink: Any, timeout_s: float = 5.0) -> dict[str, int]:
    home = request_home_position(master, mavlink, timeout_s)
    if home is None:
        raise AcceptanceError("HOME_POSITION disappeared after it had been explicitly set")
    if not home_matches_sitl_origin(home):
        raise AcceptanceError(f"HOME_POSITION changed unexpectedly: {home}")
    return home


def request_ekf_status_stream(master: Any, mavlink: Any) -> None:
    master.mav.request_data_stream_send(
        master.target_system,
        master.target_component,
        mavlink.MAV_DATA_STREAM_EXTRA3,
        10,
        1,
    )
    master.mav.command_long_send(
        master.target_system,
        master.target_component,
        mavlink.MAV_CMD_SET_MESSAGE_INTERVAL,
        0,
        float(mavlink.MAVLINK_MSG_ID_EKF_STATUS_REPORT),
        100_000.0,
        0,
        0,
        0,
        0,
        0,
    )


def decode_and_verify_frame(mavlink: Any, frame: bytes) -> Any:
    parser = mavlink.MAVLink(None)
    message = None
    for byte in frame:
        candidate = parser.parse_char(bytes([byte]))
        if candidate is not None:
            message = candidate
    if message is None or message.get_type() != "ODOMETRY":
        raise AcceptanceError("C++ producer frame is not decodable as ODOMETRY")
    if message.frame_id != mavlink.MAV_FRAME_LOCAL_FRD:
        raise AcceptanceError(f"unexpected ODOMETRY frame_id={message.frame_id}")
    if message.child_frame_id != mavlink.MAV_FRAME_BODY_FRD:
        raise AcceptanceError(f"unexpected ODOMETRY child_frame_id={message.child_frame_id}")
    return message


def drain_status(master: Any, latest: dict[str, Any]) -> None:
    while True:
        message = master.recv_match(blocking=False)
        if message is None:
            return
        message_type = message.get_type()
        if message_type == "EKF_STATUS_REPORT":
            latest["ekf_flags"] = int(message.flags)
        elif message_type == "HEARTBEAT":
            latest["custom_mode"] = int(message.custom_mode)
        elif message_type == "STATUSTEXT":
            text = str(message.text)
            latest.setdefault("statustext", []).append(text)
            latest["statustext"] = latest["statustext"][-100:]
        elif message_type == "GPS_GLOBAL_ORIGIN":
            latest["origin"] = {
                "latitude": int(message.latitude),
                "longitude": int(message.longitude),
                "altitude": int(message.altitude),
            }


def position_valid(flags: int, mavlink: Any) -> bool:
    horizontal = mavlink.EKF_POS_HORIZ_REL | mavlink.EKF_POS_HORIZ_ABS
    return bool(flags & horizontal) and not bool(flags & mavlink.EKF_CONST_POS_MODE)


def stream_until(
    master: Any,
    producer: Producer,
    mavlink: Any,
    latest: dict[str, Any],
    timeout_s: float,
    predicate,
) -> tuple[bool, int, int | None]:
    deadline = time.monotonic() + timeout_s
    frames_sent = 0
    first_reset: int | None = None
    while time.monotonic() < deadline:
        kind, fields = producer.update(True)
        if kind != "FRAME":
            raise AcceptanceError(f"valid producer update did not produce a frame: {kind} {fields}")
        frame = bytes.fromhex(fields["hex"])
        decoded = decode_and_verify_frame(mavlink, frame)
        if first_reset is None:
            first_reset = int(decoded.reset_counter)
        master.write(frame)
        frames_sent += 1
        drain_status(master, latest)
        if predicate(latest):
            return True, frames_sent, first_reset
        time.sleep(0.05)
    drain_status(master, latest)
    return predicate(latest), frames_sent, first_reset


def wait_without_provider(
    master: Any,
    mavlink: Any,
    latest: dict[str, Any],
    timeout_s: float,
) -> bool:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        drain_status(master, latest)
        flags = int(latest.get("ekf_flags", 0))
        if not position_valid(flags, mavlink):
            return True
        time.sleep(0.1)
    return False


def enter_guided(master: Any, mavlink: Any, latest: dict[str, Any], timeout_s: float = 10.0) -> bool:
    mapping = master.mode_mapping()
    if not mapping or "GUIDED" not in mapping:
        raise AcceptanceError("GUIDED mode mapping is unavailable")
    guided = int(mapping["GUIDED"])
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        master.mav.set_mode_send(
            master.target_system,
            mavlink.MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
            guided,
        )
        inner_deadline = min(deadline, time.monotonic() + 0.5)
        while time.monotonic() < inner_deadline:
            message = master.recv_match(type="HEARTBEAT", blocking=True, timeout=0.1)
            if message is not None:
                latest["custom_mode"] = int(message.custom_mode)
                if int(message.custom_mode) == guided:
                    return True
    return False


def terminate_process_group(process: subprocess.Popen[Any]) -> None:
    if process.poll() is not None:
        return
    os.killpg(process.pid, signal.SIGTERM)
    try:
        process.wait(timeout=5.0)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait(timeout=5.0)


def main() -> int:
    args = parse_args()
    if platform.system() != "Linux":
        raise AcceptanceError("run this harness inside Linux/WSL; it never targets real serial hardware")

    repo_root = Path(__file__).resolve().parents[1]
    ardupilot_root = args.ardupilot_root.resolve()
    producer_path = args.producer.resolve()
    parameter_file = args.parameter_file.resolve()
    artifact_dir = args.artifact_dir.resolve()
    sitl_dir = artifact_dir / "sitl-state"
    artifact_dir.mkdir(parents=True, exist_ok=False)
    sitl_dir.mkdir(parents=True)

    actual_commit = run_checked(["git", "rev-parse", "HEAD"], cwd=ardupilot_root)
    if actual_commit != EXPECTED_ARDUPILOT_COMMIT:
        raise AcceptanceError(
            f"ArduPilot checkout is {actual_commit}; exact {EXPECTED_ARDUPILOT_COMMIT} is required"
        )
    tracked_status = run_checked(
        ["git", "status", "--porcelain", "--untracked-files=no"], cwd=ardupilot_root
    )
    if tracked_status:
        raise AcceptanceError("ArduPilot exact-version worktree has tracked modifications")
    if not producer_path.is_file() or not os.access(producer_path, os.X_OK):
        raise AcceptanceError(f"C++ producer is missing or not executable: {producer_path}")
    if not parameter_file.is_file():
        raise AcceptanceError(f"SITL parameter file is missing: {parameter_file}")

    self_test = run_checked([str(producer_path), "--self-test"])
    if "passed=true" not in self_test:
        raise AcceptanceError(f"producer self-test failed: {self_test}")

    pymavlink_root = ardupilot_root / "modules" / "mavlink"
    sys.path.insert(0, str(pymavlink_root))
    from pymavlink import mavutil  # type: ignore

    sim_vehicle = ardupilot_root / "Tools" / "autotest" / "sim_vehicle.py"
    sitl_binary = ardupilot_root / "build" / "sitl" / "bin" / "arducopter"
    if not sitl_binary.is_file():
        raise AcceptanceError(f"exact SITL binary is missing: {sitl_binary}")

    environment = os.environ.copy()
    compatibility = repo_root / "scripts" / "sitl" / "python_compat"
    environment["PYTHONPATH"] = os.pathsep.join(
        [str(compatibility), str(pymavlink_root), environment.get("PYTHONPATH", "")]
    )
    command = [
        sys.executable,
        str(sim_vehicle),
        "-v",
        "ArduCopter",
        "-f",
        "quad",
        "--vehicle-binary",
        str(sitl_binary),
        "--no-rebuild",
        "--no-mavproxy",
        "--wipe-eeprom",
        "--speedup",
        "1",
        "--use-dir",
        str(sitl_dir),
        "--add-param-file",
        str(parameter_file),
        f"--custom-location={SITL_LAT},{SITL_LON},{SITL_ALT_MSL_M},0",
    ]

    sitl_log_path = artifact_dir / "arducopter-sitl.log"
    evidence_path = artifact_dir / "acceptance.json"
    sitl_log = sitl_log_path.open("w", encoding="utf-8")
    sitl_process = subprocess.Popen(
        command,
        cwd=ardupilot_root,
        env=environment,
        stdout=sitl_log,
        stderr=subprocess.STDOUT,
        start_new_session=True,
        text=True,
    )
    master = None
    producer = None
    evidence: dict[str, Any] = {
        "passed": False,
        "ardupilot_commit": actual_commit,
        "producer_self_test": self_test,
        "parameter_file": str(parameter_file),
        "sitl_command": command,
        "hardware_accessed": False,
        "serial_opened": False,
    }

    try:
        master, heartbeat = wait_for_connection(mavutil, 90.0)
        evidence["heartbeat"] = {
            "system": int(master.target_system),
            "component": int(master.target_component),
            "custom_mode": int(heartbeat.custom_mode),
        }
        evidence["firmware"] = verify_autopilot_version(master, mavutil.mavlink)
        evidence["parameters"] = {}
        for name, expected in EXPECTED_PARAMETERS.items():
            actual = read_parameter(master, name)
            evidence["parameters"][name] = actual
            if abs(actual - expected) > 1e-6:
                raise AcceptanceError(f"SITL parameter {name}={actual}, expected {expected}")

        request_ekf_status_stream(master, mavutil.mavlink)
        evidence["origin_home_before_external_nav"] = confirm_origin_and_home(
            master, mavutil.mavlink
        )
        producer = Producer(producer_path)
        kind, fields = producer.command(f"INIT {int(time.time() * 1000)} 0.5")
        if kind != "READY" or fields.get("reset") != "0":
            raise AcceptanceError(f"producer initialization failed: {kind} {fields}")

        latest: dict[str, Any] = {"statustext": []}
        acquired, frames, reset_counter = stream_until(
            master,
            producer,
            mavutil.mavlink,
            latest,
            args.acquire_timeout_s,
            lambda state: position_valid(int(state.get("ekf_flags", 0)), mavutil.mavlink),
        )
        evidence["acquisition"] = {
            "passed": acquired,
            "frames_sent": frames,
            "first_reset_counter": reset_counter,
            "ekf_flags": int(latest.get("ekf_flags", 0)),
        }
        if not acquired:
            raise AcceptanceError(f"ExternalNav position did not become valid: {latest}")

        evidence["home_after_external_nav"] = set_and_confirm_home(master, mavutil.mavlink)

        guided = enter_guided(master, mavutil.mavlink, latest)
        evidence["guided_mode_accepted_disarmed"] = guided
        if not guided:
            raise AcceptanceError("GUIDED mode was not accepted after ExternalNav acquisition")

        kind, reset_fields = producer.command("RESET")
        if kind != "RESET" or reset_fields.get("reset") != "1":
            raise AcceptanceError(f"producer reset failed: {kind} {reset_fields}")
        reset_ok, reset_frames, first_reset = stream_until(
            master,
            producer,
            mavutil.mavlink,
            latest,
            5.0,
            lambda state: position_valid(int(state.get("ekf_flags", 0)), mavutil.mavlink),
        )
        evidence["reset"] = {
            "passed": reset_ok and first_reset == 1,
            "frames_sent": reset_frames,
            "first_reset_counter": first_reset,
            "ekf_flags": int(latest.get("ekf_flags", 0)),
        }
        if not evidence["reset"]["passed"]:
            raise AcceptanceError(f"reset-counter scenario failed: {evidence['reset']}")

        invalid_results = []
        for _ in range(3):
            invalid_kind, invalid_fields = producer.update(False)
            invalid_results.append({"kind": invalid_kind, **invalid_fields})
            time.sleep(0.05)
        evidence["estimator_invalid_gate"] = invalid_results
        if invalid_results[-1].get("reset_required") != "1":
            raise AcceptanceError(f"invalid streak did not require reset: {invalid_results}")

        timed_out = wait_without_provider(
            master,
            mavutil.mavlink,
            latest,
            args.provider_timeout_s,
        )
        evidence["provider_timeout"] = {
            "passed": timed_out,
            "ekf_flags": int(latest.get("ekf_flags", 0)),
        }
        if not timed_out:
            raise AcceptanceError("EKF position validity did not time out after provider frames stopped")
        evidence["home_after_provider_timeout"] = confirm_home_persists(
            master, mavutil.mavlink
        )

        kind, recovery_reset = producer.command("RESET")
        if kind != "RESET" or recovery_reset.get("reset") != "2":
            raise AcceptanceError(f"recovery reset failed: {kind} {recovery_reset}")
        recovered, recovery_frames, first_recovery_reset = stream_until(
            master,
            producer,
            mavutil.mavlink,
            latest,
            args.recovery_timeout_s,
            lambda state: position_valid(int(state.get("ekf_flags", 0)), mavutil.mavlink),
        )
        evidence["recovery"] = {
            "passed": recovered and first_recovery_reset == 2,
            "frames_sent": recovery_frames,
            "first_reset_counter": first_recovery_reset,
            "ekf_flags": int(latest.get("ekf_flags", 0)),
        }
        if not evidence["recovery"]["passed"]:
            raise AcceptanceError(f"provider recovery failed: {evidence['recovery']}")
        evidence["home_after_recovery"] = confirm_home_persists(master, mavutil.mavlink)

        evidence["latest"] = latest
        evidence["passed"] = True
        return 0
    except Exception as error:
        evidence["error"] = str(error)
        return 1
    finally:
        if producer is not None:
            producer.close()
        if master is not None:
            master.close()
        terminate_process_group(sitl_process)
        sitl_log.close()
        evidence_path.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"route_local_odometry_sitl_acceptance passed={str(evidence['passed']).lower()}")
        print(f"evidence={evidence_path}")
        print(f"sitl_log={sitl_log_path}")
        if "error" in evidence:
            print(f"error={evidence['error']}", file=sys.stderr)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AcceptanceError as error:
        print(f"route_local_odometry_sitl_acceptance passed=false error={error}", file=sys.stderr)
        raise SystemExit(1)
