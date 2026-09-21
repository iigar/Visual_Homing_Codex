#!/usr/bin/env python3
"""Offline CLI contract tests. Run with Python 3 on a host with Bash/GNU tools."""

import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


SCRIPTS = Path(__file__).resolve().parents[1]
FIXTURES = Path(__file__).resolve().parent / "fixtures" / "readiness"
LIVE = "check-live-readiness-log.sh"
EXTERNAL = "check-external-nav-readiness-log.sh"
EXPORT = "export-external-nav-readiness-json.sh"
OPERATOR = "operator-readiness-summary.sh"
CONSUMERS = (LIVE, EXTERNAL, EXPORT, OPERATOR)
AUTO = {
    "VISUAL_HOMING_EXPECTED_LIVE_ROUTE_FRAMES": "auto",
    "VISUAL_HOMING_EXPECTED_LIVE_ROUTE_VALID_MATCHES": "auto",
    "VISUAL_HOMING_EXPECTED_LIVE_ROUTE_DRY_RUN_VALID": "auto",
    "VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_ALLOWED": "auto",
    "VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCKED": "auto",
    "VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS": "vehicle_not_armed:auto",
}


class ReadinessLogsTest(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory(prefix="readiness logs ")
        self.addCleanup(self.workspace.cleanup)
        self.cwd = Path(self.workspace.name)
        self.log = "input route.log"
        self.ready = (FIXTURES / "ready.log").read_text()
        self.write_log(self.ready)
        self.environment = {
            key: value for key, value in os.environ.items()
            if not key.startswith("VISUAL_HOMING_")
            and key not in ("BASH_ENV", "ENV", "SHELLOPTS", "BASHOPTS")
        }
        self.environment["LC_ALL"] = "C"

    def write_log(self, contents):
        (self.cwd / self.log).write_bytes(contents.encode())

    def change_fields(self, **fields):
        lines = self.ready.splitlines()
        tokens = lines[1].split(" ")
        for key, value in fields.items():
            self.assertTrue(any(token.startswith(key + "=") for token in tokens), key)
            tokens = [
                (key + "=" + value) if token.startswith(key + "=") else token
                for token in tokens
                if value is not None or not token.startswith(key + "=")
            ]
        lines[1] = " ".join(tokens)
        self.write_log("\n".join(lines) + "\n")

    def invoke(self, script, args=None, env=None):
        result = subprocess.run(
            ["bash", str(SCRIPTS / script), *(args if args is not None else [self.log])],
            cwd=self.cwd, env=self.environment | (env or {}), capture_output=True,
            timeout=30,
        )
        return result.returncode, result.stdout.decode(), result.stderr.decode()

    def check(self, script, code=0, stdout="", stderr="", args=None, env=None):
        self.assertEqual(self.invoke(script, args, env), (code, stdout, stderr))

    def success(self, script, strict="true", readiness="ready", reason="valid",
                valid="150/150", gates="vehicle_not_armed:150"):
        if script == LIVE:
            return (f"readiness_log_check path={self.log} passed=true "
                    f"live_output_gate_block_reasons={gates}\n")
        return (f"external_nav_readiness_log_check path={self.log} passed=true "
                f"external_nav_valid={valid} external_nav_session_ready=true "
                f"external_nav_strict_session_ready={strict} "
                f"external_nav_operator_readiness={readiness} "
                f"external_nav_operator_reason={reason}\n")

    def failure(self, script, detail):
        prefix = "readiness_log_check" if script == LIVE else "external_nav_readiness_log_check"
        return f"{prefix} path={self.log} passed=false {detail}\n"

    def report(self, label="READY", reason="valid", complete=True):
        return (
            f"{label} | route {'complete' if complete else 'incomplete'} | endpoint 9.03s "
            f"| alt 0.5m | FPS 16.60 | conf 0.81/0.93 | reason {reason}\n"
            f"detail | frames 150/150 | tracked_delta 0.88 | endpoint_stop true "
            f"| stop endpoint_progress | log {self.log}\n"
        )

    def exported(self, updates=None, env=None):
        expected = json.loads((FIXTURES / "ready.json").read_text())
        for section, fields in (updates or {}).items():
            expected[section].update(fields)
        code, stdout, stderr = self.invoke(EXPORT, env=env)
        self.assertEqual((code, stderr), (0, ""))
        self.assertEqual(json.loads(stdout), expected)
        return stdout

    def test_ready(self):
        for script in (LIVE, EXTERNAL):
            self.check(script, stdout=self.success(script))
        self.check(OPERATOR, stdout=self.report())
        self.exported()

    def test_usage(self):
        for script in CONSUMERS:
            kind = "external-nav-dry-run.log" if script == OPERATOR else "test-core-pi-log"
            suffix = " [output.json]" if script == EXPORT else f" [<{kind}> ...]"
            self.check(script, 2, stderr=f"usage: {SCRIPTS / script} <{kind}>{suffix}\n", args=[])
        self.check(EXPORT, 2, stderr=f"usage: {SCRIPTS / EXPORT} <test-core-pi-log> [output.json]\n",
                   args=[self.log, "output.json", "extra"])

    def test_missing_log_and_summary(self):
        for missing_file in (True, False):
            with self.subTest(missing_file=missing_file):
                if missing_file:
                    (self.cwd / self.log).unlink()
                else:
                    self.write_log("noise passed=true\n")
                error = "log_not_found" if missing_file else "missing_live_route_match_compact"
                for script in (LIVE, EXTERNAL):
                    self.check(script, 2, stderr=self.failure(script, f"error={error}"))
                self.check(EXPORT, 2, stderr=f"external_nav_readiness_json passed=false error={error} path={self.log}\n")
                message = "log missing" if missing_file else "missing route summary"
                self.check(OPERATOR, 1, stdout=f"BLOCKED | {message} | {self.log}\n")

    def test_missing_and_empty_required_field(self):
        for value in (None, ""):
            with self.subTest(value=value):
                self.change_fields(endpoint_passed=value)
                for script in (LIVE, EXTERNAL):
                    self.check(script, 2, stderr=self.failure(script, "missing_field=endpoint_passed"))
                self.check(OPERATOR, stdout=self.report(complete=False))
                self.exported({
                    "route": {"endpoint_passed": None},
                    "handoff": {"route_complete": False, "candidate": False,
                                "decision": "blocked", "reason": "route_not_complete"},
                })

    def test_first_duplicate_and_last_summary_win(self):
        self.write_log(self.ready.replace("passed=true frames=", "passed=false frames=", 1)
                       + self.ready.replace("passed=true frames=", "passed=true passed=false frames=", 1))
        for script in (LIVE, EXTERNAL):
            self.check(script, stdout=self.success(script))
        self.check(OPERATOR, stdout=self.report())
        self.exported()
        self.write_log(self.ready.replace("passed=true frames=", "passed=false passed=true frames=", 1))
        for script in (LIVE, EXTERNAL):
            self.check(script, 2, stderr=self.failure(script, "field=passed expected=true actual=false"))
        self.exported({"route": {"passed": False}})

    def test_literal_space_and_equals_contract(self):
        self.write_log(self.ready.replace(" ", "  ").replace("frames=150/150", "frames=150/150=ignored"))
        for script in (LIVE, EXTERNAL):
            self.check(script, stdout=self.success(script))
        self.check(OPERATOR, stdout=self.report())
        self.exported()
        # A tab is not a field separator in the existing log contract.
        self.write_log(self.ready.replace(" telemetry_health=true", "\ttelemetry_health=true"))
        for script in (LIVE, EXTERNAL):
            self.check(script, 2, stderr=self.failure(script, "missing_field=telemetry_health"))

    def test_carriage_return_is_not_trimmed(self):
        self.change_fields(telemetry_health="true\r")
        for script in (LIVE, EXTERNAL):
            self.check(script, 2, stderr=self.failure(script, "field=telemetry_health expected=true actual=true\r"))
        self.exported({"telemetry": {"health": None}})

    def test_strict_and_operator_readiness_remain_distinct(self):
        self.change_fields(external_nav_strict_session_ready="false",
                           external_nav_strict_session_reason="invalid_streak",
                           external_nav_operator_readiness="marginal",
                           external_nav_operator_reason="invalid_streak")
        self.check(LIVE, stdout=self.success(LIVE))
        self.check(EXTERNAL, stdout=self.success(EXTERNAL, strict="false", readiness="marginal", reason="invalid_streak"))
        self.check(EXTERNAL, 2, env={"VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_STRICT_SESSION_READY": "1"}, stderr=(
            self.failure(EXTERNAL, "field=external_nav_strict_session_ready expected=true actual=false")
            + self.failure(EXTERNAL, "field=external_nav_strict_session_reason expected=valid actual=invalid_streak")
        ))
        self.check(OPERATOR, stdout=self.report("MARGINAL", "invalid_streak"))
        self.exported({
            "external_nav": {"strict_session_ready": False, "strict_session_reason": "invalid_streak"},
            "operator": {"readiness": "marginal", "reason": "invalid_streak"},
            "handoff": {"visual_homing_ready": False, "candidate": False,
                        "decision": "blocked", "reason": "visual_homing_marginal"},
        })

    def test_invalid_strict_configuration(self):
        self.check(EXTERNAL, 2, env={"VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_STRICT_SESSION_READY": "true"},
                   stderr="VISUAL_HOMING_EXPECTED_EXTERNAL_NAV_STRICT_SESSION_READY must be 0 or 1, got 'true'\n")

    def test_altitude_blocker(self):
        self.change_fields(external_nav_relative_altitude_window_passed="false",
                           external_nav_altitude_blocker="out_of_window",
                           external_nav_operator_readiness="blocked",
                           external_nav_operator_reason="out_of_window")
        self.check(LIVE, stdout=self.success(LIVE))
        self.check(EXTERNAL, 2, stderr=(
            self.failure(EXTERNAL, "field=external_nav_relative_altitude_window_passed expected=true actual=false")
            + self.failure(EXTERNAL, "field=external_nav_altitude_blocker expected=none actual=out_of_window")
        ))
        self.check(OPERATOR, stdout=self.report("BLOCKED", "out_of_window"))
        self.exported({
            "altitude": {"window_passed": False, "blocker": "out_of_window"},
            "operator": {"readiness": "blocked", "reason": "out_of_window"},
            "handoff": {"visual_homing_ready": False, "candidate": False,
                        "decision": "blocked", "reason": "out_of_window"},
        })

    def test_ambiguous_endpoint_reporting(self):
        self.change_fields(ambiguous_endpoint_hold="true")
        # The CLI label and export apply different criteria; preserve both.
        self.check(OPERATOR, stdout=self.report())
        self.exported({
            "route": {"ambiguous_endpoint_hold": True},
            "handoff": {"route_complete": False, "decision": "ambiguous_endpoint_hold",
                        "reason": "endpoint_match_ambiguous"},
        })

    def test_auto_counts_and_early_endpoint(self):
        self.change_fields(frames="128/150", valid_matches="128", dry_run_valid="128/128",
                           live_output_gate_blocked="128", live_output_gate_block_reasons="vehicle_not_armed:128")
        for script in (LIVE, EXTERNAL):
            self.check(script, stdout=self.success(script, gates="vehicle_not_armed:128"), env=AUTO)
        self.check(LIVE, stdout=self.success(LIVE, gates="vehicle_not_armed:128"), env=AUTO | {
            "VISUAL_HOMING_EXPECTED_LIVE_ROUTE_ENDPOINT_STOP": "true",
            "VISUAL_HOMING_EXPECTED_LIVE_ROUTE_STOP_REASON": "endpoint_progress",
        })
        self.check(LIVE, 2, env=AUTO | {"VISUAL_HOMING_EXPECTED_LIVE_ROUTE_STOP_REASON": "frame_limit"},
                   stderr=self.failure(LIVE, "field=stop_reason expected=frame_limit actual=endpoint_progress"))

    def test_count_policies_remain_distinct(self):
        self.change_fields(frames="150/128")
        self.check(EXTERNAL, stdout=self.success(EXTERNAL), env=AUTO)
        self.check(LIVE, 2, env=AUTO, stderr=self.failure(LIVE, "field=frames expected=captured_lte_requested actual=150/128"))
        self.change_fields(live_output_gate_blocked="149")
        self.check(EXTERNAL, stdout=self.success(EXTERNAL), env=AUTO)
        self.check(LIVE, 2, env=AUTO, stderr=self.failure(LIVE, "field=live_output_gate_counts expected=sum_eq_frames actual=0+149/150"))
        self.change_fields(live_output_gate_allowed="oops")
        self.check(LIVE, 2, env=AUTO, stderr=self.failure(LIVE, "field=live_output_gate_allowed expected=uint actual=oops"))

    def test_multiple_logs_continue_after_failure(self):
        args = ["missing.log", self.log]
        for script in (LIVE, EXTERNAL):
            self.check(script, 2, stdout=self.success(script), args=args,
                       stderr=self.failure(script, "error=log_not_found").replace(self.log, "missing.log"))
        self.check(OPERATOR, 1, stdout="BLOCKED | log missing | missing.log\n" + self.report(), args=args)

    def test_export_to_file_and_explicit_inputs(self):
        stdout = self.exported(env={
            "VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET": "custom",
            "VISUAL_HOMING_EXTERNAL_NAV_NOMINAL_ROUTE_LENGTH_M": "10",
            "VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M": "2e0",
            "VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M": "bad",
        }, updates={
            "operator_inputs": {"altitude_preset": "custom", "requested_handoff_distance_m": 2},
            "resolved_config": {"nominal_route_length_m": 10},
            "altitude": {"preset": "custom"},
        })
        self.assertIn('"requested_handoff_distance_m": 2e0', stdout)
        default_stdout = self.exported()
        output = "new directory/readiness.json"
        self.check(EXPORT, stdout=f"external_nav_readiness_json path={output} schema=visual_homing.external_nav_readiness.v1\n",
                   args=[self.log, output])
        self.assertEqual((self.cwd / output).read_bytes(), default_stdout.encode())


if __name__ == "__main__":
    unittest.main()
