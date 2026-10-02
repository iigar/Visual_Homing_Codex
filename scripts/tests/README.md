# Offline readiness log tests

Run from the repository root on Linux/WSL with Python 3.9+, Bash and the standard
GNU command-line tools used by the scripts:

```sh
python3 scripts/tests/test_readiness_logs.py -v
```

The tests invoke the four readiness/reporting consumers from a temporary working
directory with spaces in its path. They need no build, camera, Pi, UART or network.
They clear inherited `VISUAL_HOMING_*` settings and set the locale explicitly.

`fixtures/readiness/ready.log` is synthetic software input, not hardware acceptance
evidence. `ready.json` records the complete exporter result before the shared
parser extraction. Tests also cover CLI errors, missing/empty/duplicate fields,
last-summary selection, literal-space and extra-equals handling, carriage returns,
strict/quality/operator policy differences, ambiguous endpoints, automatic counts,
multiple input logs and JSON file output. Checker diagnostics and exit codes are
compared exactly; the complete JSON structure is compared with the fixture.

The shared `scripts/lib/log-fields.sh` must travel with its four consumer scripts;
each resolves it relative to its own location. Readiness decisions remain in the
individual consumers. Their historical differences are characterized by these
tests, not endorsed as a common policy. The suite is separate from C++ CTest and
does not add Python/Bash requirements to Windows core builds.

## Telemetry source CLI tests

```sh
python3 scripts/tests/test_telemetry_source_cli.py path/to/visual_homing_core -v
```

Runs seven offline tests against the actual CLI with temporary binary fixtures.
Checks explicit/missing/partial/out-of-range source IDs, mixed and zero sources,
unsupported heartbeat families, structural-only inspection, CRC rejection, and
paths containing spaces. Uses only the Python standard library; no devices,
network, or pymavlink installation. `scripts/test-software.sh` runs this suite
against both Debug and Release executables.
