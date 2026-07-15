#!/usr/bin/env python3
"""Capture an ArduPilot version and parameter baseline without changing FC state.

The only outbound MAVLink operations are MAV_CMD_REQUEST_MESSAGE for
AUTOPILOT_VERSION and PARAM_REQUEST_LIST. The tool intentionally exposes no
parameter-set, arm, mode, mission, or actuator command path.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from pymavlink import mavutil


SCHEMA = "visual_homing.fc_baseline.v1"
REQUEST_COMPONENT_ID = 190
FIRMWARE_VERSION_TYPES = {
    0: "dev",
    64: "alpha",
    128: "beta",
    192: "rc",
    255: "official",
}


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def utc_text(value: datetime) -> str:
    return value.isoformat(timespec="seconds").replace("+00:00", "Z")


def packed_version(value: int) -> dict[str, Any]:
    value = int(value)
    version_type = value & 0xFF
    return {
        "raw": value,
        "major": (value >> 24) & 0xFF,
        "minor": (value >> 16) & 0xFF,
        "patch": (value >> 8) & 0xFF,
        "type": version_type,
        "type_name": FIRMWARE_VERSION_TYPES.get(version_type, "unknown"),
    }


def byte_field_hex(value: Any) -> str:
    if value is None:
        return ""
    if isinstance(value, str):
        return value.encode("latin-1", errors="replace").hex()
    return bytes(value).hex()


def parameter_name(value: Any) -> str:
    if isinstance(value, bytes):
        return value.split(b"\0", 1)[0].decode("ascii", errors="replace")
    return str(value).split("\0", 1)[0]


def parameter_type_name(value: int) -> str:
    entry = mavutil.mavlink.enums.get("MAV_PARAM_TYPE", {}).get(int(value))
    return entry.name if entry is not None else "UNKNOWN"


def status_text(message: Any) -> dict[str, Any]:
    return {
        "severity": int(message.severity),
        "text": str(message.text).split("\0", 1)[0],
    }


def atomic_write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(content, encoding="utf-8", newline="\n")
    os.replace(temporary, path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture read-only ArduPilot AUTOPILOT_VERSION and parameter artifacts."
    )
    parser.add_argument("--device", default="/dev/serial0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output-dir", type=Path, default=Path("artifacts/fc_baseline"))
    parser.add_argument("--heartbeat-timeout-s", type=float, default=10.0)
    parser.add_argument("--version-timeout-s", type=float, default=10.0)
    parser.add_argument("--parameter-timeout-s", type=float, default=240.0)
    parser.add_argument("--idle-retry-s", type=float, default=8.0)
    parser.add_argument("--max-list-requests", type=int, default=3)
    args = parser.parse_args()
    if args.baud <= 0:
        parser.error("--baud must be positive")
    for name in ("heartbeat_timeout_s", "version_timeout_s", "parameter_timeout_s", "idle_retry_s"):
        if getattr(args, name) <= 0:
            parser.error(f"--{name.replace('_', '-')} must be positive")
    if args.max_list_requests <= 0:
        parser.error("--max-list-requests must be positive")
    return args


def capture_version(connection: Any, target_system: int, target_component: int, timeout_s: float) -> tuple[Any | None, list[dict[str, Any]], list[dict[str, Any]]]:
    connection.mav.command_long_send(
        target_system,
        target_component,
        mavutil.mavlink.MAV_CMD_REQUEST_MESSAGE,
        0,
        float(mavutil.mavlink.MAVLINK_MSG_ID_AUTOPILOT_VERSION),
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
    )
    deadline = time.monotonic() + timeout_s
    statuses: list[dict[str, Any]] = []
    acknowledgements: list[dict[str, Any]] = []
    while time.monotonic() < deadline:
        message = connection.recv_match(blocking=True, timeout=min(1.0, deadline - time.monotonic()))
        if message is None or message.get_srcSystem() != target_system:
            continue
        message_type = message.get_type()
        if message_type == "AUTOPILOT_VERSION":
            return message, statuses, acknowledgements
        if message_type == "STATUSTEXT":
            statuses.append(status_text(message))
        elif message_type == "COMMAND_ACK" and int(message.command) == mavutil.mavlink.MAV_CMD_REQUEST_MESSAGE:
            acknowledgements.append({"command": int(message.command), "result": int(message.result)})
    return None, statuses, acknowledgements


def capture_parameters(
    connection: Any,
    target_system: int,
    target_component: int,
    timeout_s: float,
    idle_retry_s: float,
    max_list_requests: int,
) -> tuple[list[dict[str, Any]], int, int, list[dict[str, Any]]]:
    parameters_by_index: dict[int, dict[str, Any]] = {}
    parameters_by_name: dict[str, dict[str, Any]] = {}
    statuses: list[dict[str, Any]] = []
    expected_count = 0
    list_requests = 0
    started = time.monotonic()
    last_parameter = started

    def request_list() -> None:
        nonlocal list_requests, last_parameter
        connection.mav.param_request_list_send(target_system, target_component)
        list_requests += 1
        last_parameter = time.monotonic()
        print(f"fc_baseline_param_request request={list_requests}/{max_list_requests}", flush=True)

    request_list()
    while time.monotonic() - started < timeout_s:
        remaining = timeout_s - (time.monotonic() - started)
        message = connection.recv_match(blocking=True, timeout=min(1.0, remaining))
        now = time.monotonic()
        if message is not None and message.get_srcSystem() == target_system:
            message_type = message.get_type()
            if message_type == "STATUSTEXT":
                statuses.append(status_text(message))
            elif message_type == "PARAM_VALUE":
                name = parameter_name(message.param_id)
                entry = {
                    "name": name,
                    "value": float(message.param_value),
                    "type": int(message.param_type),
                    "type_name": parameter_type_name(message.param_type),
                    "index": int(message.param_index),
                    "count": int(message.param_count),
                }
                expected_count = max(expected_count, int(message.param_count))
                if int(message.param_index) < 65535:
                    parameters_by_index[int(message.param_index)] = entry
                parameters_by_name[name] = entry
                last_parameter = now
                received = len(parameters_by_index)
                if received == 1 or received % 100 == 0 or (expected_count and received >= expected_count):
                    print(
                        f"fc_baseline_param_progress received={received} expected={expected_count}",
                        flush=True,
                    )
                if expected_count > 0 and len(parameters_by_index) >= expected_count:
                    break
        if now - last_parameter >= idle_retry_s:
            if list_requests >= max_list_requests:
                break
            request_list()

    parameters = sorted(parameters_by_name.values(), key=lambda item: item["name"])
    return parameters, expected_count, list_requests, statuses


def main() -> int:
    args = parse_args()
    captured_at = utc_now()
    stamp = captured_at.strftime("%Y%m%dT%H%M%SZ")
    json_path = args.output_dir / f"fc-baseline-{stamp}.json"
    param_path = args.output_dir / f"fc-parameters-{stamp}.param"

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
        target_system = int(heartbeat.get_srcSystem())
        target_component = int(heartbeat.get_srcComponent())
        version, version_statuses, acknowledgements = capture_version(
            connection, target_system, target_component, args.version_timeout_s
        )
        parameters, expected_count, list_requests, parameter_statuses = capture_parameters(
            connection,
            target_system,
            target_component,
            args.parameter_timeout_s,
            args.idle_retry_s,
            args.max_list_requests,
        )
    finally:
        connection.close()

    version_data: dict[str, Any] | None = None
    if version is not None:
        version_data = {
            "capabilities": int(version.capabilities),
            "flight_sw_version": packed_version(version.flight_sw_version),
            "middleware_sw_version": packed_version(version.middleware_sw_version),
            "os_sw_version": packed_version(version.os_sw_version),
            "board_version": int(version.board_version),
            "flight_custom_version_hex": byte_field_hex(version.flight_custom_version),
            "middleware_custom_version_hex": byte_field_hex(version.middleware_custom_version),
            "os_custom_version_hex": byte_field_hex(version.os_custom_version),
            "vendor_id": int(version.vendor_id),
            "product_id": int(version.product_id),
            "uid": int(version.uid),
            "uid2_hex": byte_field_hex(getattr(version, "uid2", None)),
        }

    snapshot = {
        "schema": SCHEMA,
        "captured_at_utc": utc_text(captured_at),
        "operation": "request_only_no_parameter_writes",
        "transport": {"device": args.device, "baud": args.baud},
        "target": {"system_id": target_system, "component_id": target_component},
        "heartbeat": {
            "type": int(heartbeat.type),
            "autopilot": int(heartbeat.autopilot),
            "base_mode": int(heartbeat.base_mode),
            "custom_mode": int(heartbeat.custom_mode),
            "system_status": int(heartbeat.system_status),
            "mavlink_version": int(heartbeat.mavlink_version),
        },
        "autopilot_version": version_data,
        "request_message_acknowledgements": acknowledgements,
        "parameter_capture": {
            "complete": expected_count > 0 and len(parameters) >= expected_count,
            "received": len(parameters),
            "expected": expected_count,
            "list_requests": list_requests,
        },
        "parameters": parameters,
        "status_text": version_statuses + parameter_statuses,
    }
    atomic_write(json_path, json.dumps(snapshot, indent=2, sort_keys=True) + "\n")

    param_lines = [
        f"# schema={SCHEMA}",
        f"# captured_at_utc={utc_text(captured_at)}",
        f"# target_system={target_system} target_component={target_component}",
        "# name,value",
    ]
    param_lines.extend(f"{item['name']},{item['value']:.9g}" for item in parameters)
    atomic_write(param_path, "\n".join(param_lines) + "\n")

    complete = snapshot["parameter_capture"]["complete"]
    version_text = "missing"
    if version_data is not None:
        flight = version_data["flight_sw_version"]
        version_text = f"{flight['major']}.{flight['minor']}.{flight['patch']}-{flight['type_name']}"
    print(
        "fc_baseline_capture_done"
        f" complete={str(complete).lower()}"
        f" parameters={len(parameters)}/{expected_count}"
        f" autopilot_version={version_text}"
        f" custom_version_hex={version_data['flight_custom_version_hex'] if version_data else 'missing'}"
        f" json_path={json_path}"
        f" param_path={param_path}",
        flush=True,
    )
    return 0 if complete and version_data is not None else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"fc_baseline_capture_error={error}", file=sys.stderr, flush=True)
        raise SystemExit(1)
