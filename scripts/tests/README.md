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

## Second-pass evaluation collection checker

```sh
python3 scripts/tests/test_second_pass_collection.py -v
```

27 standard-library synthetic tests, included once in `scripts/test-software.sh`.
Cover explicit dataset/run inventory, complete input maps, plan/completion and
external manifest anchors, missing/unsealed/failed runs, per-dataset/split
frame-run coverage, distinct policies without pooled accuracy, catalog-wide
cross-split leakage beyond embedded peers, exact embedded snapshots, review,
schema/types/limits, closed inventory, symlinks/FIFO, changes during checking,
relocation and deterministic read-only CLI exits 0/2/3/4/5. Archived binaries are
inert data; no replay is executed. See [the collection contract](../../docs/SECOND_PASS_COLLECTION_UA.md).

## Second-pass collection runner

```sh
python3 scripts/tests/test_second_pass_collection_runner.py path/to/second_pass_replay -v
```

23 integration tests run against both Debug and Release by `test-software.sh`.
Cover actual C++ oracle results, full plan before launch, immutable snapshots,
all catalog peers, request/config/review/split preflight, nonzero/timeout/launch/
malformed-output failures, partial exporter exceptions, interrupted and pending
slots, no retry/resume/overwrite, shared controls and previous-result drift,
failed manifest writes, wrong-policy admission, deterministic repeats, archive
relocation and CLI exits 0/2/3/4/5. No devices or network. See
[the runner contract](../../docs/SECOND_PASS_COLLECTION_RUNNER_UA.md).

## Second-pass runner archive audit

```sh
python3 scripts/tests/test_second_pass_runner_audit.py -v
```

30 synthetic tests run once in `test-software.sh`. Inert archived executables;
no runner/exporter or child process may execute during the API audit. Cover
request/initial/final inventory, full frozen map and tools, bundle admission
against requested binary/config/policy/revision/timeout, entire catalog peers,
ordered progress/counts, failed/interrupted/pending slots and denominators,
atomic-write crash windows, missing/null/malformed controls, full typed saved
report equality, external plan versus final state anchors, readonly relocation,
closed inventory, symlinks/FIFO, mutation during checking and CLI exits 0/2/3/4.
See [the audit contract](../../docs/SECOND_PASS_RUNNER_AUDIT_UA.md).

## Second-pass multi-dataset campaign

```sh
python3 scripts/tests/test_second_pass_campaign.py path/to/second_pass_replay -v
```

Four integration tests run against both Debug and Release in `test-software.sh`.
Exercise an explicit 3-split x 2-config x 2-policy matrix (12 runs, 96 frame/run
pairs) through actual replay, runner and read-only archive audit. Independent
per-run TP/FP/FN and prediction oracles distinguish config effects from policy
effects. Cover frozen request before every child, all catalog peers/disjoint
references, full source IDs/times, repeats/relocation, failed children,
partial export followed by interruption, future pending slots, no resume,
all denominator groups, and campaign-wide review/split rejection before launch.
Fixtures are related synthetic pixels/labels, not independent physical captures.
See [the campaign evidence](../../docs/SECOND_PASS_CAMPAIGN_2026-10-13_UA.md).

## Second-pass campaign scaling probe (manual, Linux)

```sh
python3 scripts/tests/benchmark_second_pass_campaign.py \
  --binary path/to/second_pass_replay --output path/to/new-evidence \
  --runs 12 24 48 --repeats 3 --work-dir /tmp
```

Copies the seven trusted Python tools, four fixture modules, probe and local
binary to temporary storage. Uses one excluded 12-run warmup, fresh workers
for runner/audit, round-robin sizes, and a separate largest-size cProfile run.
Each matrix repeats the existing 12-run synthetic fixture with unique run IDs;
dataset/frame sizes stay fixed. Every sample must pass full coverage, prediction
and metric oracles; audits must leave bytes/mtime unchanged. Failures abort the
probe rather than being dropped from averages. No retries or performance gates.

Wall time covers the API call; imports, fixture creation, oracle checks and
evidence persistence are excluded. Peak RSS is the Linux Python worker's own
high-water mark (KiB), including imports and excluding replay child processes;
it is not simultaneous whole-process-tree memory. Warm cache, no cache eviction.
The native evidence tarball contains all samples, inputs, copied sources/binary,
reports and profiles. Timing runs are deliberately outside `test-software.sh`.
See [the measured baseline](../../docs/SECOND_PASS_SCALING_2026-10-18_UA.md).
