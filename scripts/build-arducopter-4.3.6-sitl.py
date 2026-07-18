#!/usr/bin/env python3
"""Build exact Copter 4.3.6 SITL on modern Ubuntu without patching its source."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import platform
import subprocess
import sys


EXPECTED_ARDUPILOT_COMMIT = "0c5e999c44785b0b8f53e7758856ceea614ef01b"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ardupilot-root", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=4)
    return parser.parse_args()


def checked_output(command: list[str], cwd: Path) -> str:
    return subprocess.run(
        command,
        cwd=cwd,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    ).stdout.strip()


def main() -> int:
    args = parse_args()
    if platform.system() != "Linux":
        raise RuntimeError("run this helper inside Linux/WSL")
    if args.jobs <= 0:
        raise ValueError("--jobs must be positive")

    repo_root = Path(__file__).resolve().parents[1]
    ardupilot_root = args.ardupilot_root.resolve()
    actual_commit = checked_output(["git", "rev-parse", "HEAD"], ardupilot_root)
    if actual_commit != EXPECTED_ARDUPILOT_COMMIT:
        raise RuntimeError(
            f"ArduPilot checkout is {actual_commit}; exact {EXPECTED_ARDUPILOT_COMMIT} is required"
        )
    tracked_status = checked_output(
        ["git", "status", "--porcelain", "--untracked-files=no"], ardupilot_root
    )
    if tracked_status:
        raise RuntimeError("ArduPilot exact-version worktree has tracked modifications")
    submodules = checked_output(["git", "submodule", "status", "--recursive"], ardupilot_root)
    incomplete = [line for line in submodules.splitlines() if line.startswith(("-", "+", "U"))]
    if incomplete:
        raise RuntimeError(f"ArduPilot submodules are not pinned and initialized: {incomplete}")

    compatibility = repo_root / "scripts" / "sitl" / "python_compat"
    environment = os.environ.copy()
    environment["PYTHONPATH"] = os.pathsep.join(
        [str(compatibility), environment.get("PYTHONPATH", "")]
    )
    environment["CXXFLAGS"] = " ".join(
        value for value in [environment.get("CXXFLAGS", ""), "-include cstdint"] if value
    )
    waf = ardupilot_root / "waf"
    subprocess.run(
        [sys.executable, str(waf), "configure", "--board", "sitl"],
        cwd=ardupilot_root,
        env=environment,
        check=True,
    )
    subprocess.run(
        [sys.executable, str(waf), "copter", "-j", str(args.jobs)],
        cwd=ardupilot_root,
        env=environment,
        check=True,
    )

    binary = ardupilot_root / "build" / "sitl" / "bin" / "arducopter"
    if not binary.is_file():
        raise RuntimeError(f"SITL build completed without expected binary: {binary}")
    print("arducopter_4_3_6_sitl_build passed=true")
    print(f"commit={actual_commit}")
    print(f"binary={binary}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
