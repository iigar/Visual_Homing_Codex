#!/usr/bin/env python3
"""Capture FC origin/home/local-frame evidence without changing FC state.

The only outbound MAVLink operation is MAV_CMD_REQUEST_MESSAGE for a fixed
allowlist of diagnostic messages. This tool intentionally exposes no origin or
home setters, parameter writes, arm, mode, mission, or actuator command path.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from pymavlink import mavutil


SCHEMA = "visual_homing.fc_local_frame.v1"
REQUEST_COMPONENT_ID = 190
REQUESTS = (
    ("GPS_GLOBAL_ORIGIN", 49),
    ("HOME_POSITION", 242),
    ("LOCAL_POSITION_NED", 32),
    ("ESTIMATOR_STATUS", 230),
    ("EKF_STATUS_REPORT", 193),
)
PASSIVE_MESSAGES = {
    "AHRS2",
    "ATTITUDE",
    "EKF_STATUS_REPORT",
    "ESTIMATOR_STATUS",
    "GLOBAL_POSITION_INT",
    "GPS_RAW_INT",
    "STATUSTEXT",
}
ESTIMATOR_FLAGS = (
    (1, "attitude"),
    (2, "velocity_horizontal"),
    (4, "velocity_vertical"),
    (8, "position_horizontal_relative"),
    (16, "position_horizontal_absolute"),
    (32, "position_vertical_absolute"),
    (64, "position_vertical_above_ground"),
    (128, "constant_position_mode"),
    (256, "predicted_position_horizontal_relative"),
    (512, "predicted_position_horizontal_absolute"),
    (1024, "gps_glitch"),
    (2048, "accelerometer_error"),
)


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def utc_text(value: datetime) -> str:
    return value.isoformat(timespec="seconds").replace("+00:00", "Z")


def atomic_write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(content, encoding="utf-8", newline="\n")
    os.replace(temporary, path)


def json_value(value: Any) -> Any:
    if isinstance(value, bytes):
        return value.hex()
    if isinstance(value, float) and not math.isfinite(value):
        return None
    if isinstance(value, dict):
        return {str(key): json_value(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [json_value(item) for item in value]
    return value


def message_data(message: Any) -> dict[str, Any]:
    return json_value(message.to_dict())


def decode_estimator_flags(value: int) -> dict[str, Any]:
    value = int(value)
    return {
        "raw": value,
        "set": [name for bit, name in ESTIMATOR_FLAGS if value & bit],
        "position_horizontal_relative_valid": bool(value & 8),
        "position_horizontal_absolute_valid": bool(value & 16),
        "constant_position_mode": bool(value & 128),
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture request-only ArduPilot origin, home, local-position, and estimator evidence."
    )
    parser.add_argument("--device", default="/dev/serial0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output-dir", type=Path, default=Path("artifacts/fc_baseline"))
    parser.add_argument("--heartbeat-timeout-s", type=float, default=10.0)
    parser.add_argument("--request-timeout-s", type=float, default=3.0)
    parser.add_argument("--passive-capture-s", type=float, default=3.0)
    args = parser.parse_args()
    if args.baud <= 0:
        parser.error("--baud must be positive")
    for name in ("heartbeat_timeout_s", "request_timeout_s", "passive_capture_s"):
        if getattr(args, name) <= 0:
            parser.error(f"--{name.replace('_', '-')} must be positive")
    return args


def retain_message(
    message: Any,
    target_system: int,
    latest: dict[str, dict[str, Any]],
    status_texts: list[dict[str, Any]],
    counts: dict[str, int],
) -> None:
    if message.get_srcSystem() != target_system:
        return
    message_type = message.get_type()
    counts[message_type] = counts.get(message_type, 0) + 1
    if message_type == "STATUSTEXT":
        status_texts.append(message_data(message))
    elif message_type in PASSIVE_MESSAGES:
        latest[message_type] = message_data(message)


def request_message(
    connection: Any,
    target_system: int,
    target_component: int,
    message_name: str,
    message_id: int,
    timeout_s: float,
    latest: dict[str, dict[str, Any]],
    status_texts: list[dict[str, Any]],
    counts: dict[str, int],
) -> dict[str, Any]:
    connection.mav.command_long_send(
        target_system,
        target_component,
        mavutil.mavlink.MAV_CMD_REQUEST_MESSAGE,
        0,
        float(message_id),
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
    )
    acknowledgement: dict[str, Any] | None = None
    response: dict[str, Any] | None = None
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        message = connection.recv_match(blocking=True, timeout=min(0.5, remaining))
        if message is None or message.get_srcSystem() != target_system:
            continue
        message_type = message.get_type()
        if message_type == "COMMAND_ACK" and int(message.command) == mavutil.mavlink.MAV_CMD_REQUEST_MESSAGE:
            acknowledgement = message_data(message)
        elif message_type == message_name:
            response = message_data(message)
            latest[message_type] = response
        retain_message(message, target_system, latest, status_texts, counts)
        if acknowledgement is not None and response is not None:
            break
    return {
        "name": message_name,
        "message_id": message_id,
        "acknowledgement": acknowledgement,
        "response_received": response is not None,
        "response": response,
    }


def capture_local_frame(connection: Any, args: argparse.Namespace, heartbeat: Any) -> dict[str, Any]:
    target_system = int(heartbeat.get_srcSystem())
    target_component = int(heartbeat.get_srcComponent())
    latest: dict[str, dict[str, Any]] = {}
    status_texts: list[dict[str, Any]] = []
    counts: dict[str, int] = {}
    requests = [
        request_message(
            connection,
            target_system,
            target_component,
            name,
            message_id,
            args.request_timeout_s,
            latest,
            status_texts,
            counts,
        )
        for name, message_id in REQUESTS
    ]

    deadline = time.monotonic() + args.passive_capture_s
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        message = connection.recv_match(blocking=True, timeout=min(0.5, remaining))
        if message is not None:
            retain_message(message, target_system, latest, status_texts, counts)

    estimator = latest.get("ESTIMATOR_STATUS") or latest.get("EKF_STATUS_REPORT")
    estimator_flags = decode_estimator_flags(estimator["flags"]) if estimator is not None else None
    return {
        "target": {"system_id": target_system, "component_id": target_component},
        "heartbeat": message_data(heartbeat),
        "requests": requests,
        "latest_messages": latest,
        "message_counts": counts,
        "estimator_flags": estimator_flags,
        "status_text": status_texts,
        "local_frame_evidence": {
            "gps_global_origin_reported": "GPS_GLOBAL_ORIGIN" in latest,
            "home_position_reported": "HOME_POSITION" in latest,
            "local_position_ned_reported": "LOCAL_POSITION_NED" in latest,
        },
    }


def main() -> int:
    args = parse_args()
    captured_at = utc_now()
    stamp = captured_at.strftime("%Y%m%dT%H%M%SZ")
    output_path = args.output_dir / f"fc-local-frame-{stamp}.json"
    connection = mavutil.mavlink_connection(
        args.device,
        baud=args.baud,
        autoreconnect=False,
        source_system=255,
        source_component=REQUEST_COMPONENT_ID,
    )
    try:
        heartbeat = connection.wait_heartbeat(timeout=args.heartbeat_timeout_s)
        if heartbeat is None:
            raise RuntimeError("heartbeat timeout")
        capture = capture_local_frame(connection, args, heartbeat)
    finally:
        connection.close()

    artifact = {
        "schema": SCHEMA,
        "captured_at_utc": utc_text(captured_at),
        "operation": "request_only_no_state_change",
        "outbound_operations": ["MAV_CMD_REQUEST_MESSAGE"],
        "transport": {"device": args.device, "baud": args.baud},
        **capture,
    }
    atomic_write(output_path, json.dumps(artifact, indent=2, sort_keys=True, allow_nan=False) + "\n")
    evidence = artifact["local_frame_evidence"]
    flags = artifact["estimator_flags"] or {}
    print(
        "fc_local_frame_capture_done"
        f" gps_global_origin_reported={str(evidence['gps_global_origin_reported']).lower()}"
        f" home_position_reported={str(evidence['home_position_reported']).lower()}"
        f" local_position_ned_reported={str(evidence['local_position_ned_reported']).lower()}"
        f" estimator_flags={flags.get('raw', 'missing')}"
        f" horizontal_relative_valid={str(flags.get('position_horizontal_relative_valid', False)).lower()}"
        f" horizontal_absolute_valid={str(flags.get('position_horizontal_absolute_valid', False)).lower()}"
        f" constant_position_mode={str(flags.get('constant_position_mode', False)).lower()}"
        f" output_path={output_path}",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"fc_local_frame_capture_error={error}", file=sys.stderr, flush=True)
        raise SystemExit(1)
