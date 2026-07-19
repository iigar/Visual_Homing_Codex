# Visual Homing RTL/HOVER Plan

Цей документ є self-contained handoff для продовження роботи після 2026-07-10/2026-07-12 сесій. Його мета: щоб нова Codex/операторська сесія могла продовжити без додаткових уточнень, розуміючи поточний стан, прийняті артефакти, команди, межі безпеки і послідовність до повноцінної роботи Visual_Homing у режимах RTL та HOVER.

Цей документ не авторизує політ. Будь-який armed/tethered/free-flight крок потребує окремого reviewed test plan, фізичної safety-підготовки і явного operator approval.

## Поточний Стан

Hardware/current field setup:

```text
Pi: Raspberry Pi Zero 2W class companion
FC: Matek H743 Slim V3 / ArduPilot family setup
Camera: Arducam/OV9281 wide mono
Camera profile: ov9281-160-wide
Capture path: 640x400 -> matcher target 160x100 Gray8
Telemetry: MAVLink on /dev/serial0 at 115200
Current route direction accepted for next steps: forward
```

Current route for the next endpoint-ambiguity tests:

```text
/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260712T164651Z.vhrs
entries=600
duration=about 20.03 s
target_size=160x100
route_self_match passed=true
route_perturb_check passed=true
route_distinctiveness quality_pass=true warning=false
ambiguous_nearest_entries=0/600
```

Previous accepted provider-send route:

```text
/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260710T155821Z.vhrs
entries=600
duration=about 20.02 s
target_size=160x100
route_self_match passed=true
route_perturb_check passed=true
route_distinctiveness quality_pass=true warning=true
ambiguous_nearest_entries=4/600 near start
```

Detailed evidence:

```text
docs/HARDWARE_ACCESS_BASELINE_UA.md
docs/COORDINATE_FRAME_CONTRACT_UA.md
docs/FIELD_EVIDENCE_2026-07-10_OV9281_UA.md
docs/FIELD_EVIDENCE_2026-07-12_OV9281_UA.md
docs/JT_ZERO_HANDOFF_INTEGRATION_PLAN_UA.md
docs/SESSION_LOG.md
```

`docs/HARDWARE_ACCESS_BASELINE_UA.md` is the source-of-truth for SSH, Pi/FC hardware, physical-vs-logical UART mapping, current verification status, installed firmware questions, and the parameter snapshot required before the next FC/JT_Zero acceptance probe.

Key accepted forward attach-only evidence:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260710T163641Z.log
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260710T171252Z.log
```

Both forward attach-only runs reached:

```text
progress=...1
tracked_progress=...1
endpoint_stop=true
endpoint_dwell_passed=true
external_nav_valid=all/all
external_nav_max_invalid_streak=0
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_operator_readiness=ready
external_nav_output_blocked=all
final_external_nav_output_reason=runtime_disabled
```

First accepted send-enabled provider bench evidence:

```text
run_log=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-send-20260710T174235Z.log
audit_log=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-send-audit-20260710T174235Z.log
json=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-send-20260710T174235Z.json
```

Accepted fields:

```text
passed=true
frames=585/1200
valid_matches=585
progress=0.00166945..1
tracked_progress=0.00166945..1
endpoint_stop=true
endpoint_dwell_ms=1231.4
endpoint_dwell_required_ms=1200
external_nav_valid=585/585
external_nav_max_invalid_streak=0
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_operator_readiness=ready
external_nav_latest_telemetry_mode=AltHold
external_nav_latest_telemetry_armed=false
external_nav_output_allowed=585
external_nav_output_sent=585
external_nav_output_blocked=0
final_external_nav_output_reason=allowed
audit_check: estimates=585 allowed=585 sent=585 blocked=0 reason=allowed
```

Meaning:

- Visual Homing can produce FC-ready external-nav estimates on the accepted forward route.
- The external-nav provider writer can send bounded/audited provider messages in props-off bench state.
- This is not proof that ArduPilot/JT_Zero accepts the provider as a usable navigation source.
- This is not armed, tethered, hover, RTL, or free-flight evidence.

2026-07-12 endpoint ambiguity finding:

```text
route=/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260712T164651Z.vhrs
attach_log=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260712T170240Z.log
send_log=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-send-20260712T170735Z.log
diagnostic_log=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260712T171244Z.log
```

Meaning:

- The route quality is good (`warning=false`) and forward attach-only matching reached strict readiness.
- The send-enabled run stopped physically about 1 m early even though `endpoint_stop=true`, `route_index=599`, and `external_nav_output_sent=490`.
- The diagnostic no-stop run at the real finish stabilized near `tracked_progress=0.994992` with tiny top/edge candidate gaps.
- Endpoint completion must be ambiguity-aware. Do not use pure `progress=1 + dwell` as flight-adjacent endpoint evidence on this route.

Current code response:

```text
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_REQUIRE_UNAMBIGUOUS=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_MIN_TOP_MATCH_GAP=<double>
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_MIN_EDGE_TOP_MATCH_GAP=<double>
```

This gate is opt-in. First test it attach-only. If it blocks the current ambiguous endpoint with `endpoint_confirmation_reason=top_match_gap_low` or `edge_top_match_gap_low`, that is a correct fail-closed result and should replace early physical endpoint completion as the next safety baseline.

Reverse status:

```text
best_reverse_log=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260710T170003Z.log
endpoint_stop=true
endpoint_stop_frame=endpoint-stop-frame-20260710T170046Z-id-699-route-0.pgm
external_nav_valid=534/547
external_nav_max_invalid_streak=9
external_nav_session_reason=external_nav_invalid_streak_high
```

Meaning:

- Reverse endpoint/dwell physically stops in the correct endpoint window near `route_index=0`.
- Reverse is not accepted for strict provider-send evidence yet.
- Do not run reverse send-enabled until reverse attach-only gets short/zero invalid streaks.

## Must-Read At New Session Start

At the start of a new Codex session, read:

```text
docs/PROJECT_MEMORY.md
docs/SESSION_LOG.md
docs/FIELD_EVIDENCE_2026-07-10_OV9281_UA.md
docs/FIELD_EVIDENCE_2026-07-12_OV9281_UA.md
docs/JT_ZERO_HANDOFF_INTEGRATION_PLAN_UA.md
docs/VISUAL_HOMING_RTL_HOVER_PLAN_UA.md
git log -5 --oneline
```

Expected latest commits include:

```text
87b53ec Record external nav send bench evidence
aa7e79e Add external nav send bench wrapper
fe1d9e1 Record forward repeat field evidence
2ab63dc Export endpoint stop frame
c878788 Add endpoint dwell confirmation
```

Ignore untracked local helper files unless explicitly asked:

```text
.claude/
AGENTS.md
CLAUDE.md
```

## Current Known Good Forward Send Command

This command is props-off bench only. It can send external-nav provider MAVLink messages. Use only after physically confirming propellers removed and reviewed FC bench state.

```bash
cd ~/Visual_Homing_Codex
ROUTE=/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260710T155821Z.vhrs
SEND_CONFIRM=I_UNDERSTAND_THIS_WILL_SEND_EXTERNAL_NAV_PROVIDER_MESSAGES_WITH_PROPS_REMOVED
FC_CONFIRM=I_HAVE_VERIFIED_REVIEWED_BENCH_FC_STATE

VISUAL_HOMING_ROUTE_OUTPUT="$ROUTE" \
VISUAL_HOMING_CAMERA_TARGET_WIDTH=160 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=100 \
VISUAL_HOMING_CAMERA_FRAMES=1200 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_WINDOW_RADIUS=30 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=forward \
VISUAL_HOMING_LIVE_ROUTE_MATCH_INITIAL_PROGRESS_MIN=0 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_INITIAL_PROGRESS_MAX=0.08 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_DIRECTIONAL_SEARCH=1 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_DIRECTIONAL_BIAS=0 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.985 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_DWELL_MS=1200 \
VISUAL_HOMING_MATCH_LIVE_ROUTE_TELEMETRY_MAX_AGE_MS=3000 \
VISUAL_HOMING_MAVLINK_MAX_MALFORMED_FRAMES=2 \
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=1 \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=3 \
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=5 \
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_MAX_MESSAGES=700 \
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_MAX_SECONDS=25 \
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BENCH_SEND_CONFIRM="$SEND_CONFIRM" \
VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BENCH_FC_STATE_CONFIRM="$FC_CONFIRM" \
./scripts/run-external-nav-output-bench-props-off-send-pi.sh
```

Acceptance criteria for forward send repeat:

```text
passed=true
external_nav_valid=<same>/<same>
external_nav_max_invalid_streak=0
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_operator_readiness=ready
endpoint_stop=true
endpoint_dwell_passed=true
external_nav_output_allowed>0
external_nav_output_sent>0
external_nav_output_blocked=0
final_external_nav_output_reason=allowed
external_nav_output_audit_log_check passed=true
```

If any of these fail, do not proceed to FC acceptance or flight-adjacent tests. Preserve the run log and diagnose.

## Current Known Good Reverse Attach-Only Command

Use this before attempting any reverse send-enabled run. Reverse send is blocked until strict readiness is clean enough.

```bash
cd ~/Visual_Homing_Codex
ROUTE=/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260710T155821Z.vhrs

VISUAL_HOMING_ROUTE_OUTPUT="$ROUTE" \
VISUAL_HOMING_CAMERA_TARGET_WIDTH=160 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=100 \
VISUAL_HOMING_CAMERA_FRAMES=1200 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_WINDOW_RADIUS=30 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=reverse \
VISUAL_HOMING_LIVE_ROUTE_MATCH_INITIAL_PROGRESS_MIN=0.84 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_INITIAL_PROGRESS_MAX=0.92 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_DIRECTIONAL_SEARCH=1 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_DIRECTIONAL_BIAS=0 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_START_PROGRESS=0.08 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.84 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_DWELL_MS=1200 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPORT_STOP_FRAME=1 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_FRAME_DIR=/home/pi/Visual_Homing_Codex/artifacts/stop_frames \
VISUAL_HOMING_MATCH_LIVE_ROUTE_TELEMETRY_MAX_AGE_MS=3000 \
VISUAL_HOMING_MAVLINK_MAX_MALFORMED_FRAMES=2 \
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=1 \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=3 \
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=5 \
./scripts/run-external-nav-output-bench-props-off-attach-pi.sh
```

Reverse acceptance before any reverse send:

```text
endpoint_stop=true
endpoint_dwell_passed=true
external_nav_valid_fraction=1 or very close to 1
external_nav_max_invalid_streak <= 3
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_operator_readiness=ready
```

If `external_nav_max_invalid_streak` stays `9+`, reverse remains endpoint/dwell-only evidence and must not be used for provider-send or flight-adjacent planning.

## Phase Ladder To Full RTL/HOVER

### Phase A - Bench Repeatability

Goal: prove the accepted forward provider-send behavior repeats, not just one lucky run.

Actions:

1. Run forward send-enabled bench repeat x1.
2. If clean, run forward send-enabled bench repeat x2 or proceed to acceptance probe if battery/time constrained.
3. Run reverse attach-only x1 to measure whether reverse invalid streak improved.

Exit criteria:

```text
forward_send_repeat_count >= 2 total accepted send runs
each forward send run: allowed=sent>0, blocked=0, reason=allowed
each forward send run: external_nav_valid=all/all, max_invalid_streak=0
reverse status recorded as accepted or still blocked
```

Stop conditions:

```text
any send run with blocked>0
any send run with external_nav_invalid_reasons != none
telemetry_health=false
endpoint_dwell_passed=false
operator sees physical endpoint mismatch
```

### Phase B - FC/JT_Zero Acceptance Probe

Goal: distinguish "Pi sent messages" from "FC/JT_Zero accepted messages as usable navigation source".

Required evidence to collect:

```text
pre-send FC mode, armed state, EKF/provider status if available
during-send FC mode, EKF/provider status if available
post-send FC mode, EKF/provider status if available
MAVLink message counts including inbound provider stream and FC status messages
reason if Guided/position readiness still rejects external nav
```

Implementation direction:

- Run `scripts/check-pi-field-readiness.sh` first and proceed only if it reports `visual_homing_pi_field_readiness_done passed=true`.
- Use `scripts/run-external-nav-output-acceptance-probe-pi.sh` as the reviewed probe wrapper around the accepted forward send command.
- Capture a telemetry window before send, during send, and after send.
- Decode `HEARTBEAT`, `STATUSTEXT`, EKF status, local/global position status, GPS/external-nav related status, and any ArduPilot messages indicating external nav acceptance/rejection.
- Do not infer acceptance only from Pi-side `sent=true`.
- Do not change FC params blindly. First read and log current params/status. Parameter names and required EKF source settings must be verified against the installed ArduPilot firmware before changing anything.

The current probe wrapper produces:

```text
external-nav-acceptance-probe-<stamp>.log
external-nav-acceptance-pre-<stamp>.log
external-nav-acceptance-send-<stamp>.log
external-nav-acceptance-send-audit-<stamp>.log
external-nav-acceptance-post-<stamp>.log
```

Its automatic result is intentionally conservative:

```text
acceptance_probe_result=probe_complete_requires_fc_status_review
```

This means the wrapper completed the before/send/after evidence bundle, but FC/JT_Zero acceptance must still be reviewed from telemetry/status signals. `external_nav_output_sent>0` alone is not acceptance.

Exit criteria:

```text
Pi-side send audit clean
FC-side acceptance signal explicit, or explicit rejection/blocker captured
readiness JSON can say more specific than jt_zero_not_integrated
```

If accepted:

```text
handoff.decision can move from candidate_only to reviewed_provider_candidate
```

If rejected:

```text
handoff.decision remains blocked/candidate_only
reason records exact FC/JT_Zero blocker
```

### Phase C - Stationary Props-Off Integration

Goal: prove repeated provider-send does not destabilize FC state while vehicle remains props-off and stationary.

Actions:

1. Run accepted forward provider-send command while stationary at known route positions.
2. Verify FC state remains stable: no unexpected mode changes, failsafe, EKF resets, arming side effects, or high-rate errors.
3. Repeat at route start, middle, and endpoint if practical.

Exit criteria:

```text
provider messages sent and accepted or explicitly rejected
no unexpected FC state transitions
audit clean
operator abort path verified
```

### Phase D - Restrained / Tethered Hover-Adjacent Test

Goal: test provider acceptance and HOVER candidate behavior without free-flight autonomy.

Prerequisites:

```text
Phase B accepted or blocker resolved
props-off send repeatability clean
restrained/tether plan written
manual abort plan written
max authority reviewed
battery/logging/safety observers ready
```

HOVER definition for this phase:

- ArduPilot remains responsible for stabilization.
- Visual_Homing does not yet command free movement.
- Visual_Homing may provide external-nav pose estimates if FC acceptance is proven.
- Any counter-command/velocity station-keeping remains disabled unless a separate command-authority plan is approved.

Exit criteria:

```text
FC remains stable with Visual_Homing provider active
operator can abort immediately
logs show whether provider improves/harms hold behavior
no uncontrolled motion
```

### Phase E - Visual HOVER Mode

Goal: add Visual_Homing-assisted station keeping as a controlled mode, not route return.

Required design before implementation:

- Define image-displacement or route-local hover target.
- Define deadband, max velocity/yaw authority, max duration, confidence gates, altitude gates, telemetry freshness gates, and loss-of-visual failsafe.
- Decide whether HOVER uses external-nav provider only, command output, or both. Do not mix without explicit ownership rules.
- Keep ArduPilot responsible for stabilization and motor mixing.

Dry-run first:

```text
visual_hover_estimate valid/invalid
hover_error_px or route-local error
candidate command or provider correction
gate reason
no live authority
```

Bench/tether gates before flight:

```text
bounded authority
audit every command/provider update
manual abort
timeout to neutral
visual loss -> neutral/blocked
telemetry stale -> neutral/blocked
```

### Phase F - Visual RTL Dry-Run

Goal: prove route-following logic for return without live authority.

Mode concept:

- Recorded route is outbound.
- Visual RTL uses live matching to reacquire route progress.
- If returning along the route, expected progress is usually reverse.
- Since reverse strict readiness is not accepted yet on the 2026-07-10 route, reverse must be improved before RTL authority.

Dry-run requirements:

```text
route reacquire from realistic offsets
progress direction coherent
endpoint dwell at home/start zone
external_nav estimates valid through route
invalid streaks bounded
operator-visible state machine: SEARCHING, TRACKING, ENDPOINT_DWELL, COMPLETE, BLOCKED
```

No live RTL until:

```text
reverse or selected return direction strict readiness accepted repeatedly
provider acceptance proven
HOVER/hold behavior reviewed
abort/failsafe plan tested
```

### Phase G - Restrained Visual RTL

Goal: test Visual RTL behavior with physical restraint and strict authority bounds.

Prerequisites:

```text
Phase F dry-run accepted
Phase E hover/hold accepted
FC provider acceptance explicit
route endpoint and stop-frame evidence current
max authority reviewed
```

Test shape:

- Short route only.
- Low altitude.
- Tether/restraining setup.
- Manual pilot/kill path ready.
- Visual_Homing authority bounded by low speed/yaw/timeout.
- Abort on any confidence drop, telemetry stale, invalid streak, endpoint mismatch, or operator command.

### Phase H - Free-Flight Visual RTL/HOVER

This is out of current evidence scope.

Minimum prerequisites before discussing this phase:

```text
multiple accepted forward and return-direction provider-send bench runs
FC/JT_Zero acceptance proven
restrained hover accepted
restrained RTL accepted
documented failsafe behavior
manual abort rehearsed
fresh route quality evidence
weather/lighting/scene conditions documented
```

## Immediate Next Actions

Engineering priority added on `2026-07-19` before global reacquisition/runtime attachment:

1. Replace the short-test-only in-memory route-recording assumption with a library-only streaming `VHRS v1` writer using `.partial`, bounded buffering, periodic checkpoints and explicit finalize. Completed on desktop.
2. Integrate a bounded streaming recorder for kilometer-scale routes; batch SD writes, preserve incomplete evidence after power loss, and add queue/write latency evidence. Desktop integration is complete; Pi benchmark and recovery scanner/chunked-v2 remain pending.
3. Define a multiscale route manifest with a frequent `160x100` tracking layer, compact global-search descriptors, and sparse native `1280x800` Gray8 keyframes selected by JT_Zero local displacement, altitude/scale-band change, attitude/yaw and scene novelty.
4. During recovery, treat JT_Zero as the local motion/hold anchor, not the global route finder. Visual Homing performs coarse full-route search, top-N high-resolution verification and multi-frame consistency; only then may route-local ODOMETRY resume with an incremented `reset_counter`.
5. Keep recovery within the recorded altitude/scale envelope. A high-altitude image contains wider ground coverage that cannot be synthesized from one low-altitude frame by resize alone.
6. Benchmark sparse native-resolution capture on Pi for 10-15 minutes with FPS/latency/RSS/SD-write/temperature/frequency/`get_throttled` evidence before selecting its rate. No flight authority follows from a successful recording benchmark.
7. Preserve a future portable-route/off-corridor-entry mode: a second compatible vehicle may join a transferred route after segment/progress/direction reacquisition. Approximate bearing/distance to an unseen corridor is a separate operator/shared-frame/optional-geographic/search prior, not information recoverable from visual signatures alone.
8. Use sparse native-resolution gate-keyframes as optional bridge anchors: a versioned `LOCAL_ENU|LOCAL_NED` pose may guide approach to a gate search envelope, but route ownership/handoff remains blocked until high-resolution and multi-frame visual confirmation creates a valid future `reset_reference`.

Current carry-forward state before the next field day:

```text
latest_pi_validated_code_checkout=586a55f
latest_pi_test_log=/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260715T224654Z.log (28/28, output flags OFF)
latest_field_readiness_log=/home/pi/Visual_Homing_Codex/artifacts/logs/pi-field-readiness-20260715T213722Z.log
field_readiness=passed true
route=/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260712T164651Z.vhrs
telemetry=opened true, malformed_frames=0, heartbeat_seen=true, mode=AltHold, armed=false
relative_altitude_min_avg_max_m=0.494/0.5256/0.542
fc=ArduCopter 4.3.6 official hash 0c5e999c parameters 1288/1288
independent_yaw=implemented and desktop/Pi unit-tested as route_heading_ned_rad + bounded image direction_error_rad; boundary saturation fails closed
route_heading_audit=entry 0->1 startup jump 1.469628 rad (84.20 deg); span after entry 5 is 0.249147 rad
physical_wiring=operator visually confirmed Pi TX->Matek RX3, Pi RX<-Matek TX3, common GND
physical_route_geometry=operator confirmed this artifact was recorded on a straight route
fc_local_frame_artifact=/home/pi/Visual_Homing_Codex/artifacts/fc_baseline/fc-local-frame-20260715T221923Z.json
fc_local_frame=origin/home/local_position_ned not reported; EKF flags 167; horizontal position invalid; constant-position mode
next_action=obtain explicit WGS84 EKF-origin/home and measured geographic route bearing; do not run acceptance send
```

Recommended next field/bench sequence:

1. Keep `./scripts/run-external-nav-output-acceptance-probe-pi.sh` blocked. FC telemetry yaw is now diagnostic-only, but the FC still has no reported local origin/XY and the straight route's geographic bearing is not yet measured.
2. Validate the implemented forward-only image-derived yaw offline and attach-only. It uses bounded horizontal pixel shift, rejects boundary saturation, wraps `route_heading_ned_rad + direction_error_rad`, and never accepts FC `ATTITUDE.yaw` or stored route heading hints as authority. Define reverse heading/camera semantics separately before any reverse acceptance.
3. Horizontal geometry is operator-confirmed straight for `field-route-20260712T164651Z.vhrs`, so the current `x=progress*length,y=0` model may be evaluated for this artifact only. Do not use its first heading hint: entries `0 -> 1` contain an `84.20 deg` startup discontinuity, and the remaining stable hints still describe recorded FC telemetry rather than an independent measured yaw.
4. Physical Pi TX -> Matek RX3, Pi RX <- Matek TX3, and common GND were visually confirmed by the operator on 2026-07-16. Reconfirm only after any wiring change.
5. The request-only local-frame snapshot found no reported origin, home, or `LOCAL_POSITION_NED`; EKF flags `167` have no valid horizontal position and do have constant-position mode. Separately plan any state-changing `SET_GPS_GLOBAL_ORIGIN`/home operation using explicit WGS84 latitude/longitude/MSL altitude. Do not guess `0/0/0`, and do not set or move EKF origin as part of a read-only check.
6. Pi default build/test is accepted at `586a55f` (`28/28`, all output flags `OFF`). After real origin/bearing inputs exist, repeat `./scripts/check-pi-field-readiness.sh` and validate the yaw/geometry path attach-only before any send-enabled probe.
7. Only after all gates are independently evidenced may a new props-off acceptance-probe run be reviewed. Record its manifest, pre/send/audit/post logs, and exact FC acceptance/rejection signal.
8. Do not proceed to tether/armed tests until that probe gives an explicit accepted/rejected signal and a separate reviewed test plan exists.

## Documentation Rules

Every accepted or failed evidence run should record:

```text
absolute run_log path
absolute audit_log path if any
readiness_json path if any
route path
direction
frames captured/requested
valid_matches
progress/tracked_progress range
endpoint dwell
external_nav_valid and max_invalid_streak
operator_readiness
telemetry mode/armed
output allowed/sent/blocked
final output reason
physical operator observation
```

After any code/doc change:

```text
git diff --check
GitNexus detect_changes
commit
push
```
