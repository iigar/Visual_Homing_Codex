# JT_Zero Handoff Integration Plan

Цей документ описує наступний інженерний етап після польового evidence від 2026-07-02. Він не авторизує політ і не вмикає live output. Мета: розділити вже доведений dry-run external-nav readiness від майбутнього handoff до JT_Zero / flight controller і зафіксувати порядок робіт без домислів.

## Поточний Доведений Стан

Прийнятий польовий evidence:

```text
docs/FIELD_EVIDENCE_2026-07-02_UA.md
```

Два accepted logs:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260702T171401Z.log
/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260702T172806Z.log
```

Обидва logs пройшли:

```text
external_nav_valid=240/240
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_operator_readiness=ready
external_nav_operator_reason=valid
telemetry_dropped=0
dry_run_quality=true
live_output_gate_block_reasons=vehicle_not_armed:240
```

Readiness JSON для першого accepted run показав:

```text
operator.readiness=ready
handoff.candidate=true
handoff.decision=candidate_only
handoff.reason=jt_zero_not_integrated
jt_zero.available=false
jt_zero.ready=false
jt_zero.reason=not_integrated
```

Це означає: Visual Homing вже може формувати dry-run external-nav estimates, які проходять readiness gates на цьому маршруті. Це не означає, що estimates пишуться у flight controller.

## Межа, Яку Не Можна Розмити

Є два різні output контури:

1. Existing live command output path
   - файли: `LiveMavlinkBridge`, `LiveMavlinkOutputSafetyGate`, `LiveMavlinkOutputSession`, `LiveMavlinkSerialCommandWriter`;
   - message shape: MAVLink2 `SET_POSITION_TARGET_LOCAL_NED`;
   - scope: yaw-rate-only command, `vx=0`, `vy=0`;
   - керується existing bench props-off attach/send wrappers;
   - це command-output контур.

2. Future external-nav provider / JT_Zero handoff path
   - джерело: `ExternalNavEstimate`;
   - поточний стан: log-only, `external_nav_estimate` lines;
   - майбутній output: provider-повідомлення для FC/JT_Zero, не yaw command;
   - це position-provider / handoff контур.

Ці два контури не треба змішувати. Existing yaw-rate command writer не вирішує `jt_zero_not_integrated`; для цього потрібен окремий external-nav writer або JT_Zero provider path.

## Ціль Наступного Етапу

Мінімальна ціль: додати fail-closed, feature-flagged external-nav handoff boundary, який може пройти bench/props-off evidence без вільного польоту.

Нецілі цього етапу:

- не робити free flight;
- не робити autonomous return;
- не додавати forward velocity live authority;
- не замінювати ArduPilot failsafe/hold logic;
- не використовувати visual scale як safety gate;
- не приховувати `candidate_only`, поки JT_Zero writer/provider реально не інтегрований.

## Candidate Message Path

Перший provider message path для encode-only boundary: `VISION_POSITION_ESTIMATE`.

Кандидати:

```text
VISION_POSITION_ESTIMATE
ODOMETRY
```

Рішення для першого reviewed implementation pass: почати з `VISION_POSITION_ESTIMATE` як library-only encoder. Причина: поточний `ExternalNavEstimate` вже має мінімальні поля `x_m`, `y_m`, `z_m`, `yaw_rad`, timestamp, confidence і validity reason. `ODOMETRY` має сенс як наступний крок, якщо потрібні covariance/frame semantics або якщо JT_Zero очікує саме odometry stream.

Стан на 2026-07-03:

- додано encode-only `VISION_POSITION_ESTIMATE` writer boundary;
- message id `102`, payload length `117`, CRC extra `158`;
- `time_usec` передається явно, без неявного перетворення internal steady-clock timestamp;
- runtime live-route loop, Pi wrappers, live send, attach/send gates не змінені;
- output залишається disabled/unattached by default.

Перед runtime attach/send треба підтвердити:

- що JT_Zero/ArduPilot у цьому setup реально приймає цей provider path;
- які FC params потрібні для прийняття external-nav source;
- чи `VISION_POSITION_ESTIMATE` достатньо для mode/provider readiness, або треба перейти на `ODOMETRY`;
- який component id/source system безпечний для Pi-owned provider.

## Proposed Runtime Gates

Новий external-nav writer має мати окремі flags від yaw command output.

Compile-time flags, proposed:

```text
VISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT=ON
VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT=ON
VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER=ON
```

Status on 2026-07-03:

- compile-time flags added to CMake;
- `VISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT=ON` without bench scope fails CMake configure;
- `VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT=ON` without external-nav output scope fails CMake configure;
- `VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER=ON` requires both scope flags;
- default desktop and Pi scripts force these flags `OFF`;
- bench-scope `ON/ON/OFF` configures locally but keeps `VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_AVAILABLE=0`.
- attach-scope `ON/ON/ON` builds locally and sets compile-time `external_nav_writer_attached=true`;
- `test-core-pi.sh` now logs `external_nav_attach_writer_cmake=<ON|OFF>` in `pi_test_run_start`;
- `live_route_match_start` now logs external-nav output compile state: `external_nav_output_build_requested`, `external_nav_output_bench_scope`, `external_nav_output_available`, and `external_nav_writer_attached`.

Runtime flags, proposed:

```bash
VISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT=1
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BENCH_PROPS_OFF_CONFIRM=I_UNDERSTAND_EXTERNAL_NAV_WRITER_IS_ATTACHED_AND_PROPS_ARE_REMOVED
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_MAX_MESSAGES=240
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_MAX_SECONDS=10
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_MESSAGE=VISION_POSITION_ESTIMATE
```

Defaults:

```text
compile-time output disabled
runtime output disabled
writer not attached
live send unavailable
```

Wrong or missing confirmation must block. Missing route readiness, telemetry health, altitude validity, scale known, nominal route length, or external-nav estimate validity must block per-frame send.

## Safety Gates For External-Nav Output

Per-frame external-nav send can be allowed only if all are true:

- external-nav writer compiled and attached in reviewed bench scope;
- runtime output enabled;
- exact props-off confirmation supplied;
- audit log enabled and ready;
- single-writer ownership true;
- current `ExternalNavEstimate.valid_for_fc=true`;
- estimate reason is `valid`;
- route match is valid and fresh;
- match confidence is above configured external-nav minimum;
- telemetry is fresh;
- relative altitude is seen and positive;
- expected altitude window passes for the session;
- nominal route length is positive and explicitly configured;
- max message count and max duration not exceeded;
- vehicle state is one of the reviewed bench/props-off states for this phase.

For the first props-off provider evidence, vehicle armed state should remain explicit in the log. If we keep `vehicle_not_armed` as a blocker for provider send, the first external-nav writer test is attach-only/no-send. If we allow provider send while unarmed, that must be a separate reviewed decision because it changes the meaning of the safety gate from the yaw command path.

## Implementation Phases

### Phase 0 - Design Confirmation

Deliverables:

- decide `VISION_POSITION_ESTIMATE` vs `ODOMETRY`;
- record required ArduPilot/JT_Zero params;
- define source system/component ids;
- decide whether unarmed provider messages are allowed in props-off bench evidence;
- define the audit fields before code.

Exit criteria:

- updated plan with final message choice;
- no runtime behavior change.

### Phase 1 - Encode-Only External-Nav Writer Library

Add an external-nav writer boundary separate from `LiveMavlinkSerialCommandWriter`.

Suggested files:

```text
core/include/visual_homing/live_mavlink_external_nav_writer.hpp
core/src/live_mavlink_external_nav_writer.cpp
core/tests/live_mavlink_external_nav_writer_test.cpp
```

Scope:

- encode chosen MAVLink message from `ExternalNavEstimate`;
- reject invalid estimates before byte write;
- reject non-finite fields;
- reject `valid_for_fc=false`;
- reject missing scale/altitude;
- injectable byte transport, same pattern as `LiveMavlinkByteTransport`;
- no attachment to live route runtime yet.

Exit criteria:

- unit tests pass;
- default builds still do not expose any external-nav output;
- no Pi runtime change.

Status on 2026-07-03:

- implemented:
  - `core/include/visual_homing/live_mavlink_external_nav_writer.hpp`
  - `core/src/live_mavlink_external_nav_writer.cpp`
  - `core/tests/live_mavlink_external_nav_writer_test.cpp`
- writer uses injectable `LiveMavlinkByteTransport`;
- writer rejects non-running sends, invalid/non-finite estimates, `valid_for_fc=false`, non-`valid` reason, stale route/telemetry/altitude/scale flags;
- only byte encoding and validation are covered; no runtime attachment exists yet;
- WSL CMake/Ninja CTest passed `25/25`.

### Phase 2 - External-Nav Output Bridge And Session Gate

Add a bridge/session layer for external-nav output, separate from yaw command `LiveMavlinkOutputSession`.

Suggested files:

```text
core/include/visual_homing/live_external_nav_output_session.hpp
core/src/live_external_nav_output_session.cpp
core/tests/live_external_nav_output_session_test.cpp
```

Scope:

- start/stop lifecycle;
- audit every blocked/allowed estimate;
- enforce max messages and max seconds;
- fail closed on audit failure;
- fail closed on writer start/send failure;
- preserve ordinary dry-run behavior unless explicit compile/runtime flags are set.

Exit criteria:

- tests cover blocked/no writer/audit failure/send failure/max count/max duration;
- default `test-core-pi.sh` still logs estimates only and sends nothing.

Status on 2026-07-03:

- implemented first session boundary:
  - `core/include/visual_homing/live_external_nav_output_session.hpp`
  - `core/src/live_external_nav_output_session.cpp`
  - `core/tests/live_external_nav_output_session_test.cpp`
- session is separate from `LiveMavlinkOutputSession` and does not use `NavigationCommand`;
- session gates `output_available`, `runtime_enabled`, `operator_confirmed`, `audit_log_enabled`, `audit_log_ready`, `single_writer`, and FC-ready `ExternalNavEstimate`;
- session records every allowed/blocked estimate through an injectable audit sink;
- session fails closed on audit record failure, writer start failure, writer send failure, max message count, and max duration;
- implemented file audit boundary:
  - `core/include/visual_homing/live_external_nav_output_audit_log.hpp`
  - `core/src/live_external_nav_output_audit_log.cpp`
  - `core/tests/live_external_nav_output_audit_log_test.cpp`
- implemented audit checker:
  - `scripts/check-external-nav-output-audit-log.sh`
- audit records `external_nav_output_audit` start/estimate/stop lines with allowed/sent decision, reason, `time_usec`, FC-ready state, pose, route progress, altitude, telemetry freshness, and scale flags;
- checker can validate expected estimate count, allowed/sent/blocked counts, blocker reason, stop reason, FC-ready state, and required readiness flags;
- no Pi runtime attachment or provider send wrapper exists yet;
- WSL CMake/Ninja CTest passed `27/27`.

### Phase 3 - Pi Wrapper: Attach-Only Props-Off Evidence

Add a Pi wrapper for reviewed external-nav writer attach, analogous to but separate from command-output attach:

```text
scripts/run-external-nav-output-bench-props-off-attach-pi.sh
```

Expected first result:

```text
external_nav_writer_attached=true
external_nav_output_allowed=0
external_nav_output_blocked=<auto>
reason=<reviewed blocker, e.g. vehicle_not_armed or runtime_send_disabled>
```

This phase proves the writer can be built and attached without sending provider messages.

Wrapper local status on 2026-07-03:

- build/log fields required for wrapper grep evidence exist;
- local attach-scope WSL CMake/Ninja CTest passed `27/27`;
- runtime external-nav output audit wiring exists behind explicit env controls;
- `test-core-pi.sh` refuses runtime external-nav output unless external-nav estimates, live telemetry, attach CMake, props-off confirmation, single-provider confirmation, and positive message/time limits are present;
- `scripts/run-external-nav-output-bench-props-off-attach-pi.sh` exists and forces external-nav output attach CMake `ON/ON/ON`;
- the attach-only wrapper enables external-nav output audit but keeps `VISUAL_HOMING_ENABLE_EXTERNAL_NAV_MAVLINK_OUTPUT=0`;
- the wrapper checks `external_nav_writer_attached=true`, `external_nav_output_allowed=0`, `external_nav_output_sent=0`, and audit `reason=runtime_disabled`;
- default and attach-scope WSL CTest both passed `27/27`;
- no Pi wrapper command has been run yet.

Exit criteria:

- attach build logs prove writer attached;
- audit logs prove no sends;
- route/external-nav readiness still passes in the same session;
- no free flight, no props installed.

### Phase 4 - Provider Send Bench Props-Off Evidence

Only after Phase 3 evidence, add a reviewed send wrapper:

```text
scripts/run-external-nav-output-bench-props-off-send-pi.sh
```

Required confirmations:

```text
I_UNDERSTAND_THIS_WILL_SEND_EXTERNAL_NAV_PROVIDER_MESSAGES_WITH_PROPS_REMOVED
I_HAVE_VERIFIED_REVIEWED_BENCH_FC_STATE
```

Expected result:

```text
external_nav_output_allowed=<positive>
external_nav_output_blocked=0
external_nav_valid_for_fc=<same count or explicitly explained>
jt_zero/provider status observed separately
```

This still does not authorize free flight. It only proves provider messages can be emitted in a controlled bench state.

### Phase 5 - JT_Zero Provider Readiness Evidence

After provider send works, collect evidence that FC/JT_Zero accepts or rejects it:

- FC mode/provider messages before, during, after send;
- EKF/external-nav acceptance indicators if available;
- reason if Guided still rejects position;
- no automatic movement commands.

Exit criteria:

- readiness JSON can report something more specific than `jt_zero_not_integrated`;
- if accepted, JSON may move from `candidate_only` to a reviewed pre-flight candidate state;
- if rejected, JSON must report the exact blocker.

### Phase 6 - Restrained / Flight-Adjacent Planning

Only after bench provider evidence:

- props-off send evidence clean;
- restrained/tether plan written;
- manual abort plan written;
- max authority reviewed;
- no forward command authority unless explicitly reviewed later.

Free flight is out of scope until restrained evidence exists.

## Required Documentation Updates During Implementation

Update these as the phases move:

```text
docs/ROADMAP.md
docs/LIVE_OUTPUT_SAFETY_PLAN.md
docs/LIVE_OUTPUT_BENCH_PROPS_OFF_PLAN.md
docs/FIELD_EVIDENCE_2026-07-02_UA.md
docs/SESSION_LOG.md
```

If a new message path is selected, add a short decision entry to:

```text
docs/DECISIONS.md
```

## Acceptance Bar Before Any Flight Test

Minimum evidence before discussing free flight:

- two clean external-nav readiness dry-runs already exist for the current route;
- external-nav writer attach-only props-off evidence passes;
- external-nav provider send props-off evidence passes;
- FC/JT_Zero acceptance or blocker is documented;
- restrained/tethered test plan exists;
- operator abort path exists;
- live output remains bounded and auditable.

Until then, all results remain dry-run / bench / candidate evidence.
