#!/usr/bin/env python3
"""Capture selected RC parameters and RC_CHANNELS without changing FC state.

The only outbound MAVLink operations are PARAM_REQUEST_READ for a fixed
parameter allowlist and MAV_CMD_REQUEST_MESSAGE for RC_CHANNELS. The tool has
no parameter-set, origin/home-set, arm, mode, mission, or actuator path.
"""

from __future__ import annotations

import argparse
import json
import os
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from pymavlink import mavutil


SCHEMA = "visual_homing.fc_rc_baseline.v1"
REQUEST_COMPONENT_ID = 190
CHANNELS = (7, 8, 12)
PARAMETER_NAMES = tuple(
    f"RC{channel}_{suffix}"
    for channel in CHANNELS
    for suffix in ("OPTION", "MIN", "MAX", "TRIM", "DZ", "REVERSED")
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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture request-only RC7/RC8/RC12 parameter and PWM evidence."
    )
    parser.add_argument("--device", default="/dev/serial0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output-dir", type=Path, default=Path("artifacts/fc_baseline"))
    parser.add_argument("--heartbeat-timeout-s", type=float, default=10.0)
    parser.add_argument("--parameter-timeout-s", type=float, default=2.0)
    parser.add_argument("--capture-s", type=float, default=8.0)
    parser.add_argument("--request-period-s", type=float, default=0.5)
    args = parser.parse_args()
    if args.baud <= 0:
        parser.error("--baud must be positive")
    for name in (
        "heartbeat_timeout_s",
        "parameter_timeout_s",
        "capture_s",
        "request_period_s",
    ):
        if getattr(args, name) <= 0:
            parser.error(f"--{name.replace('_', '-')} must be positive")
    return args


def parameter_name(value: Any) -> str:
    if isinstance(value, bytes):
        return value.split(b"\0", 1)[0].decode("ascii", errors="replace")
    return str(value).split("\0", 1)[0]


def request_parameter(
    connection: Any,
    target_system: int,
    target_component: int,
    name: str,
    timeout_s: float,
) -> dict[str, Any] | None:
    deadline = time.monotonic() + timeout_s
    encoded = name.encode("ascii")
    while time.monotonic() < deadline:
        connection.mav.param_request_read_send(
            target_system,
            target_component,
            encoded,
            -1,
        )
        inner_deadline = min(deadline, time.monotonic() + 0.6)
        while time.monotonic() < inner_deadline:
            message = connection.recv_match(type="PARAM_VALUE", blocking=True, timeout=0.2)
            if message is None or message.get_srcSystem() != target_system:
                continue
            if parameter_name(message.param_id) != name:
                continue
            return {
                "value": float(message.param_value),
                "type": int(message.param_type),
                "index": int(message.param_index),
                "count": int(message.param_count),
            }
    return None


def request_rc_channels(connection: Any, target_system: int, target_component: int) -> None:
    connection.mav.command_long_send(
        target_system,
        target_component,
        mavutil.mavlink.MAV_CMD_REQUEST_MESSAGE,
        0,
        float(mavutil.mavlink.MAVLINK_MSG_ID_RC_CHANNELS),
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
    )


def capture_rc_channels(
    connection: Any,
    target_system: int,
    target_component: int,
    capture_s: float,
    request_period_s: float,
) -> dict[str, Any]:
    deadline = time.monotonic() + capture_s
    next_request = 0.0
    samples: list[dict[str, Any]] = []
    acknowledgements: list[int] = []
    while time.monotonic() < deadline:
        now = time.monotonic()
        if now >= next_request:
            request_rc_channels(connection, target_system, target_component)
            next_request = now + request_period_s
        message = connection.recv_match(
            type=["RC_CHANNELS", "COMMAND_ACK"],
            blocking=True,
            timeout=min(0.2, max(0.0, deadline - time.monotonic())),
        )
        if message is None or message.get_srcSystem() != target_system:
            continue
        if message.get_type() == "COMMAND_ACK":
            if int(message.command) == mavutil.mavlink.MAV_CMD_REQUEST_MESSAGE:
                acknowledgements.append(int(message.result))
            continue
        samples.append(
            {
                "time_boot_ms": int(message.time_boot_ms),
                "chancount": int(message.chancount),
                "rssi": int(message.rssi),
                **{
                    f"channel_{channel}_pwm": int(getattr(message, f"chan{channel}_raw"))
                    for channel in CHANNELS
                },
            }
        )

    summary: dict[str, Any] = {
        "reported": bool(samples),
        "sample_count": len(samples),
        "request_ack_results": acknowledgements,
        "last": samples[-1] if samples else None,
        "channels": {},
    }
    for channel in CHANNELS:
        values = [sample[f"channel_{channel}_pwm"] for sample in samples]
        summary["channels"][str(channel)] = {
            "minimum_pwm": min(values) if values else None,
            "maximum_pwm": max(values) if values else None,
            "last_pwm": values[-1] if values else None,
            "changed": len(set(values)) > 1,
        }
    return summary


def main() -> int:
    args = parse_args()
    captured_at = utc_now()
    stamp = captured_at.strftime("%Y%m%dT%H%M%SZ")
    output_path = args.output_dir / f"fc-rc-baseline-{stamp}.json"

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
        parameters = {
            name: request_parameter(
                connection,
                target_system,
                target_component,
                name,
                args.parameter_timeout_s,
            )
            for name in PARAMETER_NAMES
        }
        rc_channels = capture_rc_channels(
            connection,
            target_system,
            target_component,
            args.capture_s,
            args.request_period_s,
        )
    finally:
        connection.close()

    snapshot = {
        "schema": SCHEMA,
        "captured_at_utc": utc_text(captured_at),
        "operation": "request_only_no_parameter_writes_no_state_change",
        "device": args.device,
        "baud": args.baud,
        "target_system": target_system,
        "target_component": target_component,
        "requested_parameters": list(PARAMETER_NAMES),
        "parameters": parameters,
        "rc_channels": rc_channels,
    }
    atomic_write(output_path, json.dumps(snapshot, indent=2, sort_keys=True) + "\n")
    print("fc_rc_baseline_capture passed=true")
    print(f"output={output_path}")
    print(f"rc_channels_reported={str(rc_channels['reported']).lower()}")
    print(f"rc_channel_samples={rc_channels['sample_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
