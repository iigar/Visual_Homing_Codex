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

## Second-pass dataset validator

```sh
python3 scripts/tests/test_second_pass_dataset.py -v
```

31 standard-library synthetic tests for the standalone dataset validator;
included in `scripts/test-software.sh`. Temporary fixtures independently encode
small VHRS/PGM files, JSON metadata and CSV rows. Cover corruption/truncation,
path and symlink escape/cycles, strict integer clocks/order, complete labels,
gaps/counts, review states, conversion originals, split leakage and CLI exits.
Symlink tests skip only when the host cannot create links; WSL runs them.
These fixtures are explicitly synthetic and do not establish physical data
independence. See [the schema and CLI contract](../../docs/SECOND_PASS_VALIDATOR_UA.md).

## Second-pass scorer

```sh
python3 scripts/tests/test_second_pass_scorer.py -v
```

27 synthetic tests, including a hand-calculated confusion matrix and 40 exhaustive
interval/index oracle cases. Cover pinned input/policy hashes, malformed rows,
missing/unknown coverage, independent downstream outcomes, endpoint tolerances
and +1 ns, censored events, consecutive-correct reacquisition, gap boundaries,
large exact timestamps, read-only deterministic CLI and exit 0/2/3. Reuses the
dataset fixture factory; no matcher, hardware or external dependencies are run.
Included in `scripts/test-software.sh`. See [the scorer contract](../../docs/SECOND_PASS_SCORER_UA.md).

## Second-pass replay exporter

```sh
python3 scripts/tests/test_second_pass_exporter.py path/to/second_pass_replay -v
```

21 synthetic integration tests, run against both Debug and Release by
`scripts/test-software.sh`. Real C++ replay produces source-bound predictions
with a hand-calculated TP=3/FP=2/FN=3 oracle; tests cover deterministic repeats,
resize, maximum integer identities, state across gaps, frozen inputs before
launch, later mutation, review/split/schema guards and invalid/unsupported rows.
Controlled test shims also exercise actual process timeout and nonzero exits;
all partial results become explicit missing. No devices or network. The existing
C++ replay timing test verifies observer identity and unchanged legacy logs.
See [the exporter contract](../../docs/SECOND_PASS_EXPORTER_UA.md).

## Saved second-pass bundle checker

```sh
python3 scripts/tests/test_second_pass_bundle.py -v
```

24 standard-library synthetic tests, included once in `scripts/test-software.sh`.
Fixtures encode saved bundles with inert binary contents; no executable is run.
Cover all bound file hashes, closed inventory/peers, strict schemas/types,
simulation/argv/manifest consistency, failed and incomplete outcomes, relocated
roots, exact rescoring, symlinks/FIFO, limits, external anchors, mutation during
checking and deterministic read-only CLI exits 0/2/3/4. Exporter integration
tests also check real C++ success and actual timeout/nonzero bundles against
both Debug and Release workflows. See [the checker contract](../../docs/SECOND_PASS_BUNDLE_CHECKER_UA.md).
