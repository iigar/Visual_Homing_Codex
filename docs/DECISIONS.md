# Decisions

## 2026-07-18 - Compose RC12 And Reset Safety As An Unattached Dry-Run Session

Decision:
- Add `LocalResetDryRunSession` as a library-only composition of `RcSwitchTriggerDecoder` and `InflightHomeResetSafetyGate` for `LocalEstimatorReset` only.
- Accept explicit current time, telemetry, RC sample time and reset-reference validity. Evaluate the safety gate only for an accepted one-shot edge; invalid RC samples fail closed before it.
- Limit outcomes to `observe_only`, `blocked` and `would_reset_local_estimator`. Do not add an estimator executor, FC Home action, writer, runtime or UART caller.

Why:
- The accepted Pi trace proved the physical RC12 edge but did not include the fresh heartbeat/armed state, audit readiness or reset reference required to authorize even a hypothetical local reset.
- A small composition boundary makes action separation and fail-closed ordering testable before any state-changing implementation exists.

Impact:
- Tests cover fresh and stale inputs, missing heartbeat/audit/reference, armed default denial, explicit armed local-reset permission, FC-Home/local-reset permission separation, held-HIGH suppression and invalid RC samples.
- WSL/Ninja and MSVC 19.44/Ninja pass `36/36`; existing decoder and gate symbols remain unchanged and GitNexus reports LOW upstream risk with no affected production execution flows.

Risk:
- `audit_log_ready` is injected configuration, not proof that a concrete audit file was opened and flushed; live integration must attach and verify that resource separately.
- `would_reset_local_estimator` is not reset authority. Reset does not correct wind drift or navigate toward the route: it only clears stale local tracking so a future global route relocalizer can seek a trustworthy lock. Without reacquisition, ODOMETRY must stay suppressed and recovery remains manual/failsafe or a separately reviewed search behavior.

## 2026-07-18 - Decode RC12 In A Standalone Dry-Run Boundary Before Any Reset Attachment

Decision:
- Add `RcSwitchTriggerDecoder` with fixed default hysteresis, debounce, low-before-high arming, one-shot edge behavior, cooldown and fail-closed input validation.
- Add a trace-driven `rc12_local_reset_dry_run` executable whose strongest result is `would_request_local_estimator_reset`; keep both local-reset and FC-Home executors absent.
- Extend the request-only RC capture with optional sample trace output and an explicit `RC12_OPTION=0` requirement. Provide a confirmation-gated Pi wrapper for the later live dry-run.

Why:
- A raw `PWM >= threshold` level check would retrigger while held, could fire at process startup with the switch already HIGH, and would not reject bounce, the middle band or rapid repeat cycles.
- Reusing the core decoder on desktop and Pi trace evidence avoids treating a Python-only diagnostic heuristic as the future safety policy.

Impact:
- Default thresholds are valid `800..2200 us`, LOW `<=1200`, HIGH `>=1800`, debounce `150 ms`, cooldown `3000 ms`, based on the confirmed `999..2000 us` RC12 range.
- WSL/Ninja and MSVC/Ninja pass `35/35`; the synthetic CLI trace produces exactly one request event, blocks a rapid second cycle through cooldown, and rejects a negative expected-count argument.
- No existing camera, route, telemetry, writer, estimator, Home or flight execution flow calls the decoder.

Risk:
- This is still input decoding and dry-run evidence only. It does not prove live Pi timing and does not authorize reset, Home change or flight use.
- The trace-driven tool does not replace the `InflightHomeResetSafetyGate`; future attachment must combine the edge with fresh live telemetry/RC, audit readiness and a valid reset reference.

## 2026-07-18 - Reserve Live-Confirmed RC12 For The Future Visual Homing Trigger

Decision:
- Use RC12 as the only candidate among RC7/RC8/RC12 for a future Visual Homing edge trigger in the current transmitter/FC setup.
- Keep RC7 reserved for its installed `RTL` aux function (`RC7_OPTION=4`) and RC8 reserved for `GPS Disable` (`RC8_OPTION=65`). Do not overwrite either mapping as part of this work.
- Treat RC12 only as input evidence. A future runtime must add hysteresis, debounce, one-shot edge detection, cooldown, fresh-RC checks, audit-before-action, and an explicit choice between local reset and FC Home change.

Why:
- A request-only live capture observed RC12 moving cleanly from `999` to `2000 us` under the operator's intended switch while RC7 stayed `999 us` and RC8 stayed `1503 us`.
- `RC12_OPTION=0` means the FC currently assigns no aux action to that channel, while RC7/RC8 are safety-relevant and already occupied.

Impact:
- The historical JT_Zero move from channel 8 to channel 12 now matches current live evidence, but the new system remains independently designed and gated.
- Evidence is `/home/pi/Visual_Homing_Codex/artifacts/fc_baseline/fc-rc-baseline-20260718T160510Z.json`: 41 samples, RC12 `changed=true`, last `999 us`, all request ACK results accepted.

Risk:
- This decision does not attach RC12 to runtime and does not authorize reset, Home change, parameter writes, or flight use.
- Transmitter/receiver/FC remapping invalidates the observation and requires a new request-only capture.

## 2026-07-18 - Gate In-Flight Local Reset And FC Home Change Independently

Decision:
- Preserve an emergency in-flight override for adverse/non-standard conditions, but keep local estimator reset and FC Home change as different actions with independent default-OFF allow flags and operator confirmations.
- Add a library-only `InflightHomeResetSafetyGate`. Armed actions require master availability, fresh telemetry and RC input, an edge trigger, ready audit, action-specific permission/confirmation, and a valid action reference/target.
- Keep normal disarmed preflight reset/Home setup available through the same common freshness/audit/reference checks without enabling the in-flight flags.
- Do not attach the gate to RC input, estimator reset, writer, runtime, UART, or FC yet. Prepare a separate request-only RC7/8/12 capture tool for live mapping evidence.

Why:
- Resetting route-local tracking can be a recovery action, while changing FC Home moves the RTL target. A shared flag could turn a limited estimator recovery into an unintended navigation-target change.
- An adverse-condition override can be useful, but a held switch, stale RC sample, stale armed state, missing audit, or implicit target must not repeatedly or silently execute it.

Impact:
- The new gate defaults every armed capability to unavailable/disabled and proves in tests that enabling one action cannot enable the other.
- WSL/Ninja and MSVC/Ninja pass `32/32`; no runtime caller exists.
- `scripts/capture-fc-rc-baseline-pi.py` can later read fixed RC parameters and request `RC_CHANNELS` without parameter writes or state changes.

Risk:
- This is policy scaffolding, not an implemented emergency control. No in-flight use is authorized.
- RC12 is now live-confirmed as the free candidate; any executor still needs edge/debounce/cooldown implementation plus separate SITL and props-off real-FC acceptance.

## 2026-07-18 - Require Exact-Version SITL Before Real FC Attachment

Decision:
- Validate the route-local estimator and raw ODOMETRY encoder against a detached exact `Copter-4.3.6`/`0c5e999c` SITL worktree before any real-FC writer attachment.
- Use the actual C++ estimator+encoder as the frame producer; use pinned 4.3.6 `pymavlink` only to orchestrate localhost TCP, inspect telemetry, set a known SITL-only global origin/Home, and request disarmed mode changes.
- Require exact firmware hash and parameter readback, correct `LOCAL_FRD/BODY_FRD` decode, EKF ExternalNav acquisition, post-acquisition Home confirmation, explicit `MAV_CMD_DO_SET_HOME` acknowledgement, disarmed GUIDED acceptance, invalid-estimate suppression, reset-counter propagation, provider timeout, Home persistence, and explicit recovery.
- Keep EKF origin and Home distinct. An absent pre-acquisition `HOME_POSITION` must not be converted into fabricated telemetry; request it again after ExternalNav becomes valid, explicitly set it through `COMMAND_INT`, require `MAV_RESULT_ACCEPTED`, and compare the FC-reported Home with the configured origin.

Why:
- Unit tests can prove byte layout and estimator gates but cannot prove that the installed ArduPilot handler/EKF accepts the message and changes navigation readiness.
- Exact-version SITL closes that software-contract gap without exposing a real FC, UART, motors, or vehicle state.
- Home and EKF origin are related but distinct ArduPilot states. ArduCopter can derive Home after an ExternalNav location exists, while the explicit accepted command gives the preflight procedure a deterministic, verifiable lock point.

Impact:
- Repeated SITL runs at exact hash passed: flags `831` with both EKF3 IMUs using external nav, automatic post-acquisition Home, accepted integer-coordinate `MAV_CMD_DO_SET_HOME`, disarmed GUIDED accepted, timeout flags `39` with Home preserved, and recovery to `831` after reset counter `2` with Home unchanged.
- MSVC/Ninja and WSL/Ninja pass `31/31` core tests. The new producer and runner remain test-only and have no runtime writer/session/UART callers.
- The procedure, build compatibility, commands, evidence and scope are recorded in `docs/ROUTE_LOCAL_ODOMETRY_SITL_UA.md`.

Risk:
- SITL does not prove real Matek timing, serial transport, Pi load, real-FC origin/Home, armed GUIDED, physical reverse yaw, RTL behavior, motor behavior, or flight safety.
- The next boundary is a separately reviewed props-off real-FC acceptance with output paths still isolated; runtime attachment is not authorized by this decision.

## 2026-07-18 - Keep The Route-Local Estimator Explicit, Stateful, And Unattached

Decision:
- Add `RouteLocalOdometryEstimator` as a library-only state machine between route matching and the exact ArduCopter 4.3.6 ODOMETRY encoder; do not attach it to writer/session/runtime/UART yet.
- Require explicit, fresh route-start altitude initialization. Map pose as `x=progress*nominal_route_length`, `y=0`, and `z=start_altitude-current_altitude` in fixed route-local FRD.
- Use the independent bounded image direction residual as forward yaw. Keep reverse yaw unavailable by default; allow nose-toward-route-start yaw only through an explicit policy with a configurable residual sign.
- Require explicit reset when traversal direction changes. Preserve the route-start altitude across tracking reset, but require reinitialization after the vertical origin is cleared.
- Fail closed on invalid configuration, stale/future observations, low match confidence, unhealthy input, invalid direction residual, excessive update/rate changes, wrong-way progress, or a configured consecutive-invalid streak.

Why:
- Implicitly choosing the first observed altitude as Z origin can silently move the EKF frame after restart or reacquisition.
- Forward and reverse camera headings are not interchangeable. A default-disabled reverse policy makes the unproven case visible instead of fabricating yaw.
- A stateful reset counter and bounded temporal gates are required before route progress can be treated as a pose observation rather than a stateless match result.

Impact:
- Deterministic tests now cover initialization, forward/reverse semantics, direction-change reset, freshness, progress, horizontal/vertical/yaw-rate limits, invalid streaks, reset-counter behavior, recovery, and integration with the existing encoder.
- MSVC 19.44 + Ninja passed `30/30` desktop CTest tests. The prior Pi evidence remains encoder-only at `29/29`; no Pi or FC validation was claimed for the estimator.
- Existing `VISION_POSITION_ESTIMATE`, live external-nav writer/session, Pi wrappers, and all runtime output paths remain unchanged.

Risk:
- The estimator is now exercised against exact-version ArduPilot SITL origin/Home/disarmed-mode behavior, but not a real FC EKF; reverse residual sign still needs physical acceptance for the actual camera orientation.
- No runtime integration is authorized until a separate reviewed props-off real-FC step verifies reset, timestamp, frame, origin/Home, mode, transport, and fail-closed behavior.

## 2026-07-18 - Keep The GitNexus Reference Audit Separate And Extract Selectively

Decision:
- Keep the tracked legacy baseline under `reference/` unchanged and keep the sibling `_gitnexus_indexed_copy` local-only as a code-intelligence aid.
- Treat the external conflict snapshot as the same baseline, not as an additional upstream: SHA-256 comparison found `242/242` identical files and no differences.
- Reuse only bounded concepts or parser/UI patterns after reimplementation behind current `core/` interfaces and current safety contracts; do not merge reference flight/navigation code wholesale.
- Prioritize future extraction review in this order: sensor protocol test vectors/parsers, small-candidate visual refinement/reacquisition ideas, Android Pi-owned API/readiness UI patterns, then non-flight tooling. Keep command authority, estimator frames, health/readiness, and output gating owned by the current core.

Why:
- GitNexus shows the reference Python sensor, visual-odometry, and route-following classes are mostly isolated from the main execution graph; the C++ route/MAVLink path contains scaffolding and TODOs, while backend/frontend tests primarily exercise simulation/web behavior.
- Several prototypes violate current requirements: missing MSP V2 CRC validation, hard-coded camera geometry, implicit/unclear frames, direct homography-to-command conversion, and an unsafe assumption that IMU plus barometer can supply horizontal GPS-denied home navigation.
- The Android source still contains useful Retrofit/DTO/discovery/telemetry/UI structure, provided the Pi remains the owner of resolved configuration and safety/readiness decisions.

Impact:
- No existing `core/` code, route artifact, Pi checkout, FC state, or accepted evidence changes.
- The local indexed copy provides symbol/context/impact/cypher navigation with a clean baseline at `405e7ac`; its graph has `3436` symbols and `84` flows.
- Detailed component classification and known hazards are preserved in `docs/REFERENCE_REPOSITORY_AUDIT_UA.md`.

Risk:
- BM25/FTS is unavailable on the current Windows LadybugDB setup, so GitNexus natural-language query is degraded; known-symbol graph navigation remains usable.
- Copying a prototype because it appears complete in documentation can silently reintroduce frame, checksum, freshness, scale, or command-authority defects. Every future symbol edit still requires current-repo impact analysis, tests, and staged safety evidence.

## 2026-07-17 - Use Route-Local ODOMETRY Without Geographic Bearing

Decision:
- Represent the confirmed-straight route in a fixed `ROUTE_FRD`/MAVLink `MAV_FRAME_LOCAL_FRD` pose frame: X forward along the recorded route, Y right, Z down from the route-start origin.
- Use the operator-confirmed nominal route length of approximately `10 m` for the later estimator mapping `x=progress*10 m`, `y=0`; do not hard-code route length into the wire encoder.
- Treat the previous approximately `0.5 m` altitude as AGL/readiness context, not route-local Z. When route start and return have the same height, `z` is approximately zero; later vertical estimation must use displacement from the route-start height.
- Encode ODOMETRY for exact ArduCopter `4.3.6` with pose frame `LOCAL_FRD(20)`, child/twist frame `BODY_FRD(12)`, yaw-only quaternion, reset counter, and estimator type `VISION(2)`.
- Send velocity and angular-rate fields as `NaN`, not zero, and leave both covariance arrays unknown. Keep this first change library-only and unattached to runtime/UART/FC.

Why:
- A GPS-denied route-following system needs a consistent local route frame, not a bearing relative to geographic North.
- The exact installed firmware accepts this ODOMETRY frame pair. Its EKF rejects non-finite ExternalNav velocity, which prevents the position/yaw-only boundary from falsely asserting zero velocity while `EK3_SRC1_VELXY=6` is configured.
- The installed MAVLink schema predates the ODOMETRY `quality` extension; implementing the exact `232`-byte contract avoids silently targeting a newer dialect than the FC.

Impact:
- Geographic route bearing and WGS84 route coordinates are no longer prerequisites for the new route-local encoder or its future route estimator.
- The old `VISION_POSITION_ESTIMATE` runtime path remains unchanged and still fails closed unless its existing `LOCAL_NED` requirements are satisfied.
- The first ODOMETRY module has no writer/session/runtime callers. MSVC build and CTest passed `29/29`; exact pinned MAVLink generation matched the `244`-byte frame and checksum.

Risk:
- This does not yet prove EKF provider acceptance, mode readiness, origin/home behavior, reverse-route yaw semantics, or vertical-origin tracking.
- Before any UART attachment, add a route-local estimator with explicit start-height state, independent forward/reverse yaw semantics, reset handling, rate/health gates, and SITL/props-off acceptance evidence.
- No FC state or hardware output was changed by this decision.

## 2026-07-16 - Derive Forward-Route Yaw From Bounded Image Shift

Decision:
- Keep FC `ATTITUDE.yaw` diagnostic-only while `EK3_SRC1_YAW=6` selects ExternalNav yaw.
- For a physically straight forward route, compute ExternalNav yaw as the configured geographic route bearing plus the matched frame's horizontal image-shift residual.
- Treat the direction observation as independent only when route match and pixel calibration are valid and the best shift lies strictly inside the configured search window.
- Reject boundary-saturated, disabled, or non-finite observations; do not use stored route heading hints or provide an operator override for independence.
- Keep reverse yaw semantics outside this decision until camera orientation and route-direction behavior are separately defined and tested.

Why:
- Feeding FC yaw back into its own ExternalNav source is a feedback loop, not an independent observation.
- The matcher already measures a small per-frame visual direction residual from current pixels against the matched route frame.
- A best result on the search boundary means the real angular error may be larger than the bounded search can observe, so accepting it would overstate yaw validity.

Impact:
- A valid forward image observation can now satisfy `yaw_source_independent`; FC telemetry yaw is logged separately for comparison.
- Route alignment, geographic bearing, altitude-origin alignment, metric scale, match, telemetry, and altitude gates remain mandatory for this older `LOCAL_NED`/`VISION_POSITION_ESTIMATE` path. The later `2026-07-17` `LOCAL_FRD` ODOMETRY decision removes geographic bearing from the route-local contract without changing this historical boundary.
- The operator-confirmed straightness of `field-route-20260712T164651Z.vhrs` closes only its straight-axis assumption; it does not supply geographic bearing or WGS84 origin, and those geographic values are not prerequisites for the later route-local ODOMETRY path.

Risk:
- Image-shift sign/calibration and physical bearing still require attach-only field evidence before provider send.
- The FC reports no horizontal local position/origin and constant-position mode; no acceptance probe is authorized by this change.
- Desktop MSVC build and CTest passed `28/28`. Pi default build at `586a55f` kept all output flags `OFF` and passed `28/28`; log: `/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260715T224654Z.log`.

## 2026-07-15 - Require Explicit LOCAL_NED Alignment Before External-Nav Output

Decision:
- Treat visual route progress as `ROUTE_FRD`, never as implicit North/East/Down.
- Require explicit route origin, NED heading, and altitude-origin alignment before an `ExternalNavEstimate` may become FC-ready.
- Make the output session, MAVLink writer, and direct `VISION_POSITION_ESTIMATE` encoder independently reject any estimate that is not explicit `LOCAL_NED` with known alignment.
- Default all new Pi controls to unaligned/blocked and require explicit alignment confirmations for runtime provider send.
- Keep MAVLink `ODOMETRY` and its `MAV_FRAME_LOCAL_FRD`/`MAV_FRAME_BODY_FRD` semantics outside this change.

Why:
- Route-forward/right/down axes are not geographic North/East/Down until a measured transform is applied.
- ROS ENU/FLU and ArduPilot NED/FRD differ in both axis order and sign; implicit conversion can create a plausible but physically wrong provider pose.
- The previously accepted send logs prove bounded byte output only. They do not prove that the route pose had the correct local-frame alignment.

Impact:
- Old CLI blocks still parse, but without the new alignment fields their estimates fail closed with `frame_alignment_not_known`.
- Pi runtime send additionally requires `VISUAL_HOMING_EXTERNAL_NAV_ROUTE_FRAME_ALIGNMENT_KNOWN=1` and `VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_ORIGIN_ALIGNED=1`.
- Per-frame/audit/compact logs and readiness JSON expose pose frame, route origin/heading, and altitude-origin state.
- Default and external-nav attach-capable WSL CMake/Ninja configurations each passed CTest `28/28`; shell syntax checks passed for the five modified Pi/readiness scripts, and both dedicated wrappers refused early when alignment inputs were absent.

Risk:
- No Pi, SITL, or FC acceptance test was run. Correct numeric origin/heading and vertical-origin equivalence still require a reviewed physical procedure before provider send.
- The current protocol remains `VISION_POSITION_ESTIMATE`; future `ODOMETRY` support needs its own frame and twist contract.

## 2026-07-03 - Wire External-Nav Output Runtime Audit Before Pi Wrapper

Decision:
- Wire `LiveExternalNavOutputSession` into live route matching behind explicit external-nav output audit/runtime controls.
- Keep default behavior off: no audit, no writer start, and no provider send unless external-nav output env controls are explicitly supplied.
- Use a disabled no-op writer by default and attach `LiveMavlinkExternalNavWriter` only in builds where external-nav output is compile-time available.
- Add Pi script guards for runtime send: external-nav estimates, live telemetry, attach CMake, props-off confirmation, single-position-provider confirmation, and positive message/time limits are required.
- Record successful external-nav output audit entries after `send_vision_position_estimate`, so `sent=true` means the writer send call returned successfully.

Why:
- The future Pi attach wrapper needs real runtime audit records, not just compile-state logs.
- External-nav output must remain separate from live yaw command output and must fail closed until explicitly enabled.
- The audit log must distinguish `allowed` from actually `sent`; otherwise the evidence would overstate provider-output behavior.

Impact:
- `live_route_match_start`, `live_route_match_done`, and `live_route_match_compact` now expose external-nav output audit/runtime counters and blocker reasons.
- `test-core-pi.sh` can pass external-nav output audit controls through the environment and refuses unsafe runtime combinations.
- Default WSL CTest passed `27/27`.
- External-nav attach-scope WSL CTest passed `27/27`.
- No Pi wrapper, Pi attach run, FC provider integration, or field test was run.

Risk:
- Runtime output still depends on a future Pi wrapper and field-reviewed procedure; local tests only prove build/runtime gating and audit semantics.

## 2026-07-03 - Add Attach-Only External-Nav Output Pi Wrapper

Decision:
- Add `scripts/run-external-nav-output-bench-props-off-attach-pi.sh` as the first external-nav output Pi wrapper.
- The wrapper forces external-nav output attach CMake `ON/ON/ON`, enables external-nav output audit, and keeps `VISUAL_HOMING_ENABLE_EXTERNAL_NAV_MAVLINK_OUTPUT=0`.
- It checks compile/runtime evidence after the run: external-nav output build requested, bench scope enabled, output available, writer attached, output audit started, allowed `0`, sent `0`, and final reason `runtime_disabled`.
- It also runs `scripts/check-external-nav-output-audit-log.sh` against the produced audit log.
- It requires strict external-nav readiness evidence from the underlying dry-run wrapper before accepting attach-only output evidence.

Why:
- Phase 3 needs a repeatable operator command for attach-only evidence before any provider-send bench work.
- The wrapper proves the writer can be compiled/attached and audited without sending `VISION_POSITION_ESTIMATE`.

Impact:
- The wrapper is available locally and documented.
- It has not been run on `jtzero` and is not field evidence yet.

Risk:
- The wrapper still requires a reviewed props-off Pi run before it can count as accepted attach evidence.

## 2026-07-03 - Log External-Nav Output Compile State Before Attach Wrapper

Decision:
- Add external-nav output compile-state fields to existing run logs before introducing the Pi attach wrapper.
- `test-core-pi.sh` reports external-nav CMake state in `pi_test_run_start`.
- `live_route_match_start` reports `external_nav_output_build_requested`, `external_nav_output_bench_scope`, `external_nav_output_available`, and `external_nav_writer_attached`.

Why:
- The future attach-only wrapper needs machine-checkable evidence that the attach-capable build was selected and that the external-nav writer compile state is attached.
- Adding observability first keeps the next wrapper small and avoids relying on chat notes or manual CMake cache inspection.
- This remains separate from provider-send behavior; log visibility is not runtime integration.

Impact:
- Default WSL build and CTest passed `27/27`.
- External-nav attach-scope WSL build with attach `ON` passed `27/27`.
- No Pi wrapper, runtime output path, provider send, or field evidence was added.

Risk:
- `external_nav_writer_attached=true` will mean compile-time attach state only until runtime session wiring is added and separately audited.

## 2026-07-03 - Add External-Nav Output Audit Checker Before Pi Wrapper

Decision:
- Add `scripts/check-external-nav-output-audit-log.sh` before adding any Pi attach/send wrapper.
- Validate `external_nav_output_audit` files by start/stop count, estimate count, allowed/sent/blocked counts, reason, stop reason, FC-ready state, and readiness flags.
- Default the checker toward attach-only evidence: `allowed=0`, `sent=0`, `blocked=auto`, and reason wildcard unless a wrapper sets a stricter expected reason.

Why:
- Phase 3 attach-only evidence should be machine-checkable before the operator is asked to run anything on `jtzero`.
- The checker gives the future wrapper a clear pass/fail contract for "writer/session attached but no provider messages were sent".
- Keeping this separate from `check-live-session-audit-log.sh` preserves the command-output versus provider-output boundary.

Impact:
- A synthetic local attach-only audit passed with `estimates=2`, `allowed=0`, `sent=0`, `blocked=2`, and `reason=runtime_disabled`.
- No Pi runtime path, wrapper, or field evidence was added.

Risk:
- The checker validates audit shape and counts only. It does not prove FC/JT_Zero provider acceptance and must not be used as provider-send readiness.

## 2026-07-03 - Add Cache-Safe External-Nav Output Compile-Time Flags

Decision:
- Add three CMake flags for the external-nav provider output path:
  - `VISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT`
  - `VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT`
  - `VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER`
- Require the bench props-off scope whenever external-nav output is requested.
- Require both scope flags before the attach flag can configure.
- Define default build macros with external-nav provider output unavailable and unattached.
- Make desktop and Pi default build scripts explicitly pass all external-nav output flags as `OFF`.

Why:
- CMake cache state must not be able to silently carry an attach-capable external-nav writer into ordinary builds.
- The project needs a reviewed bench scope before any provider-output runtime wiring, matching the existing command-output safety pattern.
- This step should allow local bench-scope compilation work while keeping default runtime behavior unchanged.

Impact:
- Default WSL build and CTest passed `27/27`.
- External-nav bench-scope build with attach `OFF` passed `27/27` and still compiled with provider output unavailable.
- Invalid configure with `VISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT=ON` and bench scope `OFF` failed at CMake configure time.
- No Pi runtime path, wrapper, or field evidence was added.

Risk:
- The attach flag can now be represented at compile time, but no accepted attach-only props-off evidence exists yet. It must not be treated as provider-send readiness.

## 2026-07-03 - Use Separate External-Nav Output Audit Log Format

Decision:
- Add `LiveExternalNavOutputAuditLog` instead of reusing `LiveMavlinkOutputAuditLog`.
- Use `external_nav_output_audit` start/estimate/stop records for external-nav provider evidence.
- Record provider-specific fields: allowed/sent decision, reason, `time_usec`, `valid_for_fc`, estimate reason/source, pose, yaw, confidence, route progress/index, relative altitude, route/telemetry/altitude validity, and scale flags.
- Keep this file-audit boundary library-only until the Pi attach wrapper is implemented.

Why:
- The command-output audit log records `NavigationCommand` fields such as `vx_mps`, `vy_mps`, and `yaw_rate_radps`; those are the wrong primary fields for a position-provider handoff.
- Phase 3 attach-only evidence needs an audit artifact that can prove "session existed and blocked/sent zero provider estimates" without implying command-output authority.

Impact:
- Added:
  - `core/include/visual_homing/live_external_nav_output_audit_log.hpp`
  - `core/src/live_external_nav_output_audit_log.cpp`
  - `core/tests/live_external_nav_output_audit_log_test.cpp`
- WSL CMake/Ninja validation passed with CTest `27/27`.

Risk:
- The audit format is still a key-value text contract. Future wrapper/checker code must parse it conservatively and should not depend on field order beyond the event prefix.

## 2026-07-03 - Keep External-Nav Output Session Separate From Command Output Session

Decision:
- Add a dedicated `LiveExternalNavOutputSession` for external-nav provider estimates instead of reusing `LiveMavlinkOutputSession`.
- Gate `ExternalNavEstimate` output with external-nav-specific fields: output availability, runtime enablement, operator confirmation, audit readiness, single-writer ownership, max message count, max duration, and FC-ready estimate validity.
- Keep the session testable through injectable audit and writer interfaces.
- Do not attach the session to Pi runtime or send provider messages in this phase.

Why:
- The existing `LiveMavlinkOutputSession` is intentionally shaped around `NavigationCommand` and yaw-rate command output.
- The JT_Zero handoff path is a position-provider path; reusing the command session would blur the boundary between "command authority" and "external-nav estimate provider".
- Phase 2 needs fail-closed lifecycle/audit behavior without changing current field wrappers or enabling serial output.

Impact:
- Added:
  - `core/include/visual_homing/live_external_nav_output_session.hpp`
  - `core/src/live_external_nav_output_session.cpp`
  - `core/tests/live_external_nav_output_session_test.cpp`
- Added an `ExternalNavWriter` interface so the concrete `LiveMavlinkExternalNavWriter` can be used behind a session boundary.
- WSL CMake/Ninja validation passed with CTest `26/26`.

Risk:
- This is still not FC/JT_Zero acceptance evidence. The next stage needs a file audit/log format and attach-only props-off wrapper before any provider-send bench work.

## 2026-07-03 - Start External-Nav Writer With VISION_POSITION_ESTIMATE Encode-Only Boundary

Decision:
- Use MAVLink2 `VISION_POSITION_ESTIMATE` as the first external-nav provider message encoder for the JT_Zero handoff path.
- Keep this as an encode-only library boundary for Phase 1: no live-route runtime attachment, no Pi wrapper send path, and no change to the existing yaw-rate command-output writer.
- Pass `time_usec` explicitly into the encoder instead of deriving it from the internal steady-clock timestamp.
- Keep `ODOMETRY` as a later option if JT_Zero/ArduPilot acceptance requires stronger frame/covariance semantics.

Why:
- The accepted 2026-07-02 external-nav dry-runs already produce `ExternalNavEstimate` fields that map directly to `VISION_POSITION_ESTIMATE`: `x_m`, `y_m`, `z_m`, `yaw_rad`, route confidence, route progress, telemetry freshness, altitude validity, and scale-known readiness.
- `VISION_POSITION_ESTIMATE` is a smaller first integration target than `ODOMETRY` and matches the current project stage: prove byte encoding and estimate rejection first, then test provider acceptance separately.
- Explicit timestamp input avoids clock-domain assumptions before the flight controller/JT_Zero acceptance path is measured.

Impact:
- Added a new external-nav writer boundary:
  - `core/include/visual_homing/live_mavlink_external_nav_writer.hpp`
  - `core/src/live_mavlink_external_nav_writer.cpp`
  - `core/tests/live_mavlink_external_nav_writer_test.cpp`
- The writer uses an injectable byte transport, encodes MAVLink2 message id `102` with full payload length `117` and CRC extra `158`, and rejects invalid/non-finite/non-ready estimates before byte write.
- Desktop/WSL CMake/Ninja validation passed with CTest `25/25`.
- Runtime behavior remains unchanged; no external-nav MAVLink provider messages are sent by default or by the current Pi wrappers.

Risk:
- ArduPilot/JT_Zero provider acceptance is still unproven. The next evidence must be attach-only props-off and then reviewed provider-send props-off work, not a flight test.
- `VISION_POSITION_ESTIMATE` may not be the final provider message if JT_Zero requires `ODOMETRY` or different covariance/frame handling.

## 2026-06-23 - Keep Android Handoff Inputs Pi-Owned

Decision:
- Treat Android readiness controls as per-run operator inputs, not as a second gate-policy implementation.
- Extend the external-nav readiness JSON contract with `operator_inputs`, `resolved_config`, `handoff`, and a placeholder `jt_zero` block.
- Keep handoff as `candidate_only` until a same-Pi JT_Zero module is integrated and reports its own availability/readiness through the Pi-owned contract.
- Do not let Android decide whether Visual Homing output, JT_Zero handoff, or live output is allowed.

Why:
- Android should make field operation clearer by showing camera/feature diagnostics, readiness cards, selected altitude preset, requested handoff intent, and the Pi's resolved decision.
- The safety-relevant thresholds and handoff state must stay close to the process that owns telemetry, route matching, JT_Zero integration, and live-output gates.
- JT_Zero is planned as code running on the same Raspberry Pi, not separate hardware, so handoff should be a local Pi coordination contract exposed to Android for display/control.

Impact:
- `scripts/run-external-nav-dry-run-pi.sh` can forward optional requested handoff distance/altitude values into the readiness artifact.
- `scripts/export-external-nav-readiness-json.sh` now exposes handoff candidate state without changing any route, altitude, external-nav, or live-output pass/fail gates.
- Android can later consume one JSON/API shape instead of parsing raw logs or duplicating readiness policy.

Risk:
- Requested handoff distance is informational until route metric scale and JT_Zero readiness are authoritative. The JSON explicitly reports `handoff_distance_supported=false` for the current stage.

## 2026-06-14 - Add External Navigation Provider Milestone

Decision:
- Do not run the send-enabled bench wrapper while ArduPilot rejects armed `Guided` with `requires position`.
- Treat the absence of an external navigation provider as a separate system blocker, not a route-recording issue.
- Add Milestone 6.9 for an external-navigation provider path that starts dry-run/log-only before any `VISION_POSITION_ESTIMATE` or `ODOMETRY` writer is attached.
- Keep external-nav writing separate from command-output writing; the first external-nav writer evidence must not also send guided yaw-rate commands.

Why:
- ArduPilot can be configured to use `ExternalNav`, but the FC still needs an actual accepted position/odometry stream before armed `Guided` is available.
- Setting Home/EKF origin is not enough; it supplies an origin, not a continuous position estimate.
- The current C++ system reads telemetry and can match visual route progress, but it does not yet provide EKF position estimates to the FC.

Impact:
- `scripts/run-live-output-bench-props-off-send-pi.sh` remains prepared but should not be run until the FC can enter armed `Guided` without `requires position`.
- Roadmap now has an explicit Milestone 6.9 before the flight ladder.
- The next engineering work should model, log, and validate external-nav estimates before adding any writer.

Risk:
- A visual route-progress estimate is not automatically a metric external-nav pose. Scale, yaw/attitude compensation, altitude/range, route ambiguity, covariance/quality, EKF origin, and stale data must be handled before FC authority is considered.

## 2026-06-14 - Prepare Separate Send-Enabled Bench Wrapper

Decision:
- Add `scripts/run-live-output-bench-props-off-send-pi.sh` as a separate reviewed props-off bench wrapper for the first allowed-send evidence attempt.
- Require two confirmations before it can run: one acknowledging bounded MAVLink sends with propellers removed, and one acknowledging the operator has verified the armed `Guided` bench state.
- Reuse the reviewed attach wrapper internally so attach evidence remains mandatory.
- Require `live_output_gate_allowed`/audit allowed counts to be positive, `blocked=0`, and `reason=allowed`.
- Extend the readiness/session-audit checkers to accept `allowed=auto` for endpoint-stop sessions where the exact frame count is only known after the run.

Why:
- The project needs to distinguish attach evidence from allowed-send evidence without making the ordinary wrapper or attach-default wrapper send-enabled.
- The exact endpoint-stop frame count depends on the physical route pass, so requiring a hard-coded allowed count would make the first send-enabled bench run brittle.
- The current live route loop expects endpoint-stop evidence; the send wrapper therefore refuses `VISUAL_HOMING_LIVE_OUTPUT_MAX_COMMANDS < VISUAL_HOMING_CAMERA_FRAMES` instead of trying to create an early max-command stop mode.

Impact:
- A future operator-reviewed bench run has a dedicated command and evidence contract.
- Default and attach-default wrappers remain conservative and unchanged in purpose.
- This is preparation only; no send-enabled bench evidence has been accepted yet.

Risk:
- The send wrapper can write real MAVLink commands when the FC is armed and Guided. It remains props-off, physically restrained, and bench-only; it does not authorize flight, tethered flight, ground movement, or autonomous return.

## 2026-06-14 - Add Separate Reviewed Attach-Build Bench Wrapper

Decision:
- Add `scripts/run-live-output-bench-props-off-attach-pi.sh` as a separate reviewed bench command for the serial-writer attach build.
- Require a different confirmation string: `VISUAL_HOMING_LIVE_OUTPUT_BENCH_ATTACH_CONFIRM=I_UNDERSTAND_SERIAL_WRITER_IS_ATTACHED_AND_PROPS_ARE_REMOVED`.
- Configure all three Pi CMake live-output attach flags only inside this wrapper and verify `attach_writer_cmake=ON` plus `live_output_writer_attached=true` in the run log.
- Keep the ordinary `run-live-output-bench-props-off-pi.sh` unchanged as the default fail-closed unavailable-boundary wrapper.
- Default the attach wrapper to `allowed=0 blocked=<auto> reason=vehicle_not_armed`; any send-enabled bench attempt must override expected allowed/blocked counts and gate reasons explicitly.

Why:
- The project needs a reviewed way to prove writer attachment without making the ordinary wrapper or default builds attach-capable.
- A different confirmation string reduces accidental invocation of the attach-capable path.
- The first attach-build evidence should distinguish writer attachment from FC acceptance, allowed sends, vehicle movement, or flight behavior.

Impact:
- The attach wrapper can build and run the separate `core/build-pi-live-output-attach` path through `test-core-pi.sh`.
- Post-run checks now include attach evidence in addition to readiness and session-audit checks.
- No default wrapper behavior changes; default builds still keep live output unavailable.

Risk:
- The attach wrapper can attach the serial writer in a props-off bench build. It must remain physically restrained, propellers removed, and operator-reviewed. It does not authorize flight, tethered flight, ground movement, or autonomous return.

## 2026-06-11 - Record Modularity, Trust, And Extension Principles

Decision:
- State explicitly that the current system is operator-in-the-loop command assist, not an autonomous controller.
- Keep module boundaries replaceable and independently testable across capture, preprocessing, route I/O, matching, telemetry, navigation, command output, audit, and safety gates.
- Treat route artifacts and MAVLink serial bytes as untrusted inputs until validated, freshness-checked, and gated.
- Record altitude/range versus resolution as an operational risk: higher altitude increases ground meters per pixel and can erase route texture.

Why:
- These principles were partly implicit in existing interfaces and fail-closed gates, but future work needs them written down before attach-build, thermal, rangefinder, stronger matcher, or field-readiness work.
- Route artifact modification, malformed serial traffic, wrong sysid/compid, or stale telemetry should not become command permission.
- Extension points for thermal cameras, rangefinders, optical flow, VIO/UWB, and alternative matchers should not require rewriting the realtime scheduler.

Impact:
- `docs/ARCHITECTURE.md`, `docs/ROADMAP.md`, `docs/LIVE_OUTPUT_SAFETY_PLAN.md`, and `docs/PROJECT_MEMORY.md` now carry the same architecture intent.
- No runtime behavior changes.

Risk:
- This records design intent only. Route digest/integrity checks, sysid/compid hardening, explicit watchdog timers, and scale-mismatch logs remain future implementation work.

## 2026-06-10 - Record Route-Speed, FC-Mode, And Terrain-Matcher Risks

Decision:
- Treat route recording speed versus return/match speed as a validation variable for live-route dry-runs.
- Require the reviewed attach bench step to document and log the FC mode expected to accept `SET_POSITION_TARGET_LOCAL_NED`, currently `Guided`.
- Keep `64x48` Gray8/MAD matching as a deterministic baseline, while documenting that low-texture outdoor terrain may require higher-resolution profiles, normalization, or stronger descriptors before field/flight-ladder use.

Why:
- Windowed matching tolerates local timing differences, but large speed mismatches can still cause progress jumps, regressions, or endpoint misses.
- The serial writer can encode a valid MAVLink packet while the flight controller rejects or ignores it due to mode/configuration; safety evidence must distinguish writer bytes from FC acceptance.
- Indoor route evidence does not prove outdoor route distinctiveness, especially over monotonic ground or repeated structure.

Impact:
- Current attach/bench planning must include mode evidence, not just serial-writer attachment evidence.
- Future outdoor route work should use route-quality diagnostics as a filter and may need camera/profile/matcher upgrades before readiness evidence is meaningful.
- No runtime behavior changes; live output remains blocked in default builds.

Risk:
- These are documented residual risks only. The current code still uses the baseline matcher and current progress gates until a later implementation specifically addresses speed adaptation, FC acceptance feedback, or stronger visual descriptors.

## 2026-06-09 - Keep Visual Scale Altitude As Diagnostic Groundwork

Decision:
- Treat barometer/rangefinder-derived ground scale and image-scale drift as diagnostics before they can affect live commands.
- Add `derive_camera_ground_footprint` to compute approximate ground footprint and meters-per-pixel from a validated camera profile and positive altitude.
- Do not change `VHRS` v1 for precise altitude or visual-scale metadata yet.

Why:
- The current route format already carries coarse altitude bands and heading hints, and the telemetry adapter already exposes FC relative altitude through `NavigationEstimate::altitude_m`.
- A small deterministic camera-profile utility makes future barometer-vs-visual-scale analysis easier without introducing new live authority or route-format compatibility risk.
- Estimating altitude from visual scale alone is plausible only as a relative signal and needs dry-run comparison against FC altitude/rangefinder data before it can be trusted.

Impact:
- Future logs and matchers can derive expected ground meters-per-pixel from camera FOV and FC altitude.
- Visual scale ratio work can start as offline/live dry-run telemetry, comparing current image scale against route-frame scale assumptions.
- Live output behavior is unchanged and remains governed by the bench props-off safety gates.

Risk:
- Barometer altitude is relative and can drift; visual scale is scene-dependent and can be confused by texture, perspective, yaw/pitch, or repeated objects. Neither source is sufficient for hover/station-keeping authority without later validation and a separate safety decision.

## 2026-06-09 - Force Default Build Scripts To Clear Live-Output CMake Flags

Decision:
- Make the default desktop and Pi test scripts pass all live-output CMake options explicitly as `OFF`.
- Add explicit Pi env controls for reviewed non-default live-output CMake scopes, using separate build directories for bench-scope and attach-capable builds unless the operator intentionally overrides `VISUAL_HOMING_PI_BUILD_DIR`.

Why:
- CMake option values are cached per build directory. If an attach-capable build ever reused `core/build-pi`, the ordinary wrapper could otherwise inherit `VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER=ON` from cache even when the command line looked like the default fail-closed wrapper.
- The default path must be deterministic and fail-closed regardless of prior local experiments.

Impact:
- Ordinary `scripts/test-core.ps1`, `scripts/test-core-pi.sh`, and `scripts/run-live-output-bench-props-off-pi.sh` runs now actively force the unavailable/default build unless explicit Pi CMake env flags request a separate reviewed build scope.
- Attach-capable Pi builds are separated into `core/build-pi-live-output-attach` by default.

Risk:
- Any future attach-build procedure must be explicit about the build directory and cannot rely on changing the default wrapper implicitly.

## 2026-06-09 - Accept Post-Attach-Flag Default Wrapper Evidence

Decision:
- Accept the `jtzero` bench props-off endpoint-stop fail-closed run at commit `375f6cd` as the current default-wrapper evidence after adding the explicit serial-writer attach flag.
- Keep the ordinary Pi wrapper on the default unavailable build path where `live_output_writer_attached=false`.
- Do not accept the later `20260609T064000Z` repeat as readiness evidence because the route pass failed before endpoint completion.

Why:
- The accepted run proves the attach flag did not accidentally attach the serial writer in the default Pi wrapper.
- Pi CTest passed 23/23, live route matching stopped at `frames=117/225`, route progress gates passed, read-only telemetry stayed healthy, dry-run command quality passed, and both readiness and session-audit checkers passed.
- All 117 live-output decisions remained blocked with `live_output_unavailable`.

Impact:
- The default/pre-attach boundary is now validated after the attach flag landed.
- The next reviewed step can focus on an explicit attach-build bench validation rather than re-proving the ordinary unavailable wrapper.

Risk:
- This evidence still proves only the unavailable default-wrapper path. It does not prove serial writer attachment, hardware serial writes, flight-controller command acceptance, or flight behavior.

## 2026-06-09 - Add Explicit Serial Writer Attach Build Flag

Decision:
- Add `VISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER` as a third compile-time flag for attaching the concrete serial writer to runtime live-route sessions.
- Require both `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON` and `VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=ON` before this attach flag can configure.
- Keep the default build and the ordinary Pi wrapper on `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0`.

Why:
- The writer library existed, but the runtime live-route session still used a dry-run bridge. A separate attach flag makes the transition from library-present to runtime-attached explicit and reviewable.
- The current endpoint-stop fail-closed evidence should remain stable until a deliberate bench writer build is requested.

Impact:
- Triple-flag bench builds compile with `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=1` and create a `LiveMavlinkBridge` backed by `LiveMavlinkSerialCommandWriter` for runtime-controlled live-route sessions.
- Default builds still block as before, and the dedicated Pi wrapper still proves the unavailable boundary unless it is run from a separately configured attach build.

Risk:
- This is the first code path that can open a command serial writer in a reviewed bench-scope build. It must only be used with propellers removed, physical restraint, explicit runtime/operator confirmation, and the existing audit/safety gates.

## 2026-06-09 - Accept Endpoint-Stop Fail-Closed Bench Evidence

Decision:
- Accept the `jtzero` bench props-off endpoint-stop fail-closed run at commit `98d5407` as the current pre-attach live-output boundary evidence.
- Keep live output unavailable and keep the serial writer unattached to runtime sessions.

Why:
- The run proves the dedicated bench wrapper now stops command generation at endpoint completion instead of collecting post-endpoint tail frames.
- Pi CTest passed 23/23, live route matching stopped at `frames=129/150`, route progress gates passed, read-only telemetry stayed healthy, dry-run command quality passed, and both readiness and session-audit checkers passed.
- All 129 live-output decisions remained blocked with `live_output_unavailable`.

Impact:
- The next implementation step can focus on the reviewed writer attach/availability phase with endpoint-tail command generation removed from the pre-attach baseline.
- Checker expectations for this wrapper are variable-length by design and must be derived from the compact run log/audit log rather than assuming 150 commands.

Risk:
- This evidence still proves only the unavailable-runtime boundary. It does not prove serial writer attachment, hardware serial writes during a live session, or any flight behavior.

## 2026-06-09 - Enable Endpoint-Complete Stop In Bench Wrapper

Decision:
- Enable opt-in endpoint-complete stop for the dedicated bench props-off live-output wrapper.
- Keep the behavior tied to explicit runtime live-output session args and `expected_progress=forward|reverse`; `any` progress remains invalid for endpoint-stop.
- Allow readiness and session-audit checkers to validate variable-length endpoint-stop sessions through explicit `auto` expectations.

Why:
- The recent `jtzero` runs reached endpoint progress before the fixed 150-frame window ended, so tail frames after endpoint completion created regressions/rollback unrelated to the intended return segment.
- Before any runtime writer attachment, command generation should stop when the reviewed endpoint condition is reached rather than continuing to produce post-endpoint command attempts.

Impact:
- The bench props-off wrapper now expects `stop_reason=endpoint_progress_reached`, `endpoint_stop=true`, and fail-closed `live_output_unavailable` decisions for however many frames were captured before endpoint stop.
- Legacy readiness and audit checker defaults still validate the old 150-frame fixed-window logs unless `auto` expectations are explicitly set.
- Live MAVLink output remains unavailable; this changes session length, not writer availability.

Risk:
- Shorter sessions reduce endpoint-tail noise but make command counts route/operator dependent, so downstream tooling must use the compact run log and audit log together instead of assuming 150 commands.

## 2026-06-08 - Defer Visual Brake And Station-Keeping Assist

Decision:
- Record visual braking / station-keeping assist as a later milestone, separate from the first bench props-off live-output return boundary.
- Define the later mode as dry-run-first: estimate image displacement against a reference frame/window, compensate with FC IMU/attitude telemetry, and log proposed bounded counter-commands before any live output is considered.
- Keep ArduPilot responsible for hover, attitude/altitude hold, motor mixing, and failsafe behavior.

Why:
- The current route matcher supports coarse visual return, not metric hover or position hold.
- Immediate opposite-direction commands without damping can oscillate. Any future assist mode needs deadband, gain/rate/slew limits, confidence gates, telemetry freshness gates, cooldowns, max duration, max command count, and timeout-to-autopilot-hold.
- Real station keeping requires a scale source such as rangefinder/altitude model, optical-flow-like scale, VIO, UWB, or another reviewed input.

Impact:
- Milestone 6.8 remains focused on fail-closed bench props-off yaw-rate-only return output.
- Useful groundwork can be kept now: read-only IMU/attitude telemetry, frame timing, confidence gates, bounded command models, and audit logs.

Risk:
- This feature must not be treated as hover capability until dry-run logs show stable non-oscillatory behavior and a separate safety plan exists.

## 2026-06-08 - Harden Pre-Attach Live Output Session Safety

Decision:
- Set the dedicated bench props-off wrapper's progress tolerance defaults to `VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_PROGRESS_REGRESSIONS=10` and `VISUAL_HOMING_LIVE_ROUTE_MATCH_MAX_PROGRESS_ROLLBACK=0.30` while leaving the core defaults at `5` and `0.25`.
- Start `LiveMavlinkOutputSession` audit without opening the bridge/writer; start the bridge lazily only after an allowed safety decision.
- Require zero lateral speed in `LiveMavlinkOutputSafetyGate` for the first yaw-rate-only boundary.
- Convert bridge start failures into audited blocked command decisions, require an audit record before writer send, and stop sessions explicitly on writer send failures.
- Update the bench props-off wrapper banner to reflect that the serial writer library exists but is not attached or available.

Why:
- The `2026-06-08` `jtzero` post-hardening runs reached endpoint progress and preserved the expected fail-closed live-output result, but fixed-frame endpoint-tail ambiguity exceeded inherited wrapper progress tolerances: first `progress_rollback=0.261745` exceeded `0.25`, then `progress_regressions=8` exceeded `5`.
- The next attach phase must not open the command serial writer while `live_output_available=false`.
- The safety gate should enforce the full yaw-rate-only command contract independently of the serial writer's own validation.
- An allowed safety decision followed by a writer failure must leave an explicit audit trail instead of throwing past the session boundary.

Impact:
- Current fail-closed Pi wrapper behavior should remain `allowed=0 blocked=150 reason=live_output_unavailable`.
- The progress tolerance changes affect the dedicated bench wrapper readiness gate only; they do not make live MAVLink output available and do not alter writer safety gating.
- Future writer-enabled sessions will distinguish `bridge_start_failed` and `send_failed` from ordinary safety blocks.
- Audit record failures now stop the session before a writer send can occur.
- Live MAVLink output remains blocked.

Risk:
- Lazy bridge start changes session timing semantics: max-duration still starts at audit/session start, but the writer is opened only at the first allowed command.
- The wrapper tolerance is a short-term practical guard against fixed-frame endpoint-tail ambiguity. Before runtime writer attachment, add endpoint-complete stop semantics so matching stops after the route endpoint instead of collecting tail frames.

## 2026-06-07 - Accept Post-Writer-Library Fail-Closed Pi Evidence

Decision:
- Accept the `jtzero` bench props-off runtime-controlled fail-closed run at commit `6fc9cd2` as post-writer-library boundary evidence.
- Record both the main run log and session audit log in `docs/LIVE_OUTPUT_READINESS_RECORD.md`.
- Keep `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0` and leave the serial writer unattached to runtime sessions.

Why:
- The run was collected after `LiveMavlinkSerialCommandWriter` existed as a tested concrete library boundary, so it proves the new code did not accidentally make runtime live output available.
- Pi CTest passed 23/23, live route matching captured 150/150 valid matches, directional and endpoint progress gates passed, read-only telemetry health passed with zero dropped bytes, dry-run command quality passed, and both wrapper checkers passed.
- All 150 live-output decisions remained blocked with `live_output_unavailable`.

Impact:
- The next implementation step can focus on the explicit writer attach/availability phase with a clean fail-closed baseline after the writer library landed.
- Live MAVLink output remains blocked.

Risk:
- This evidence still proves only the unavailable-runtime boundary. It does not prove flight-controller acceptance, hardware serial send behavior in-session, or any flight behavior.

## 2026-06-06 - Wire Compile-Time Availability Into Safety Gate

Decision:
- Add `live_output_available` to `LiveMavlinkOutputSafetyConfig`.
- Check availability before runtime/operator/telemetry/match/command gates.
- Wire runtime-controlled live-route audit sessions to `LiveMavlinkBridge::command_output_available()`.
- Preserve historical dry-run readiness diagnostics by using the older non-runtime diagnostic path unless runtime controls are explicitly supplied.

Why:
- Runtime enable and operator confirmation are insufficient if the build still has no reviewed writer.
- The safety gate should make the unavailable compiled state visible as an explicit block reason before any writer send.
- Existing 3/3 readiness evidence should remain stable and continue to validate as `vehicle_not_armed` for the old dry-run audit path.

Impact:
- Runtime-controlled live-output attempts currently block with `live_output_unavailable`.
- Tests prove an unavailable live-output build blocks before fake writer send.
- Live output remains unavailable because no concrete writer exists and `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0`.

Risk:
- `live_output_unavailable` is now an earlier gate for the new runtime-controlled path. Checkers for future runtime-gate tests must expect that reason until the concrete writer phase changes availability.

## 2026-06-06 - Add Runtime Controls And Session Limits Before Writer Send

Decision:
- Add explicit runtime/operator/max-limit controls to the live-route audit path.
- Require the exact `I_UNDERSTAND_PROPS_ARE_REMOVED` confirmation string when runtime live output is requested through the Pi wrapper.
- Enforce max command count and max duration inside `LiveMavlinkOutputSession` before any writer send.
- Preserve existing dry-run readiness diagnostics unless the new runtime controls are explicitly supplied.

Why:
- Runtime/operator controls must be testable before a concrete serial writer exists.
- Hard command and duration limits are part of the first bench props-off boundary and must live in the session that owns writer calls.
- Existing readiness checks still need stable `vehicle_not_armed` dry-run blocking unless a new test intentionally exercises runtime gates.

Impact:
- Future bench commands can pass runtime enable, exact confirmation, and hard limits through `scripts/test-core-pi.sh`.
- `LiveMavlinkOutputSession` now records and stops on `max_command_count_reached` or `max_duration_reached`.
- Live output remains unavailable because no concrete writer exists and `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0`.

Risk:
- Runtime flags could look like authorization. They are only one gate; compile-time availability, concrete writer implementation, safety gate pass, audit readiness, and bench-only procedure remain required.

## 2026-06-06 - Add Live Bridge Writer Interface Without Serial Writer

Decision:
- Add `LiveMavlinkCommandWriter` as an injectable writer interface behind `LiveMavlinkBridge`.
- Keep the default `LiveMavlinkBridge` fail-closed when no writer is attached.
- Keep `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0`; do not add a concrete serial MAVLink command writer in this step.

Why:
- The live bridge start/stop/send contract needs tests before a real writer exists.
- Fake-writer tests can validate stopped-send rejection, deterministic double start/stop behavior, and safety-gate blocking without emitting MAVLink commands.

Impact:
- Phase 2 can be reviewed independently from runtime/operator gates and serial command output.
- The application path still has no live writer attached and cannot send live MAVLink commands.

Risk:
- An injectable writer boundary could be mistaken for an active writer. The default constructor, availability macro, docs, and tests must continue to keep real output unavailable until the concrete writer phase is reviewed.

## 2026-06-06 - Split Live Output Compile-Time Bench Scope

Decision:
- Add `VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT` as the reviewed companion CMake option for future bench props-off live-output work.
- Keep `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON` invalid unless the bench props-off scope is also enabled.
- Allow the paired bench-scope build to configure only while `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0` and `LiveMavlinkBridge` remains fail-closed.

Why:
- The next implementation phases need a narrow build boundary for tests without accidentally turning on command output.
- A two-option build boundary makes accidental live-output enablement less likely and keeps the safety distinction visible in CMake, tests, and docs.

Impact:
- Default builds remain blocked.
- The bench-scope build can validate compile-time wiring before any writer exists.
- The real writer phase must explicitly change the availability macro and add tests before commands can be emitted.

Risk:
- Future work could misread the bench-scope build as authorization to send commands. Documentation and tests must continue to assert that availability is false until the writer phase is reviewed.

## 2026-06-05 - Define Bench Props-Off Live Output Boundary Plan

Decision:
- Add `docs/LIVE_OUTPUT_BENCH_PROPS_OFF_PLAN.md` as Milestone 6.8, the first reviewed live-output implementation scope after 3/3 readiness evidence.
- Keep live output blocked until the plan is implemented and reviewed.

Why:
- The 3/3 evidence count is complete, but enabling live output without a narrow implementation contract would skip the remaining safety boundary design.
- The first live-output step must be bench-only, props-off, yaw-rate-only, zero-forward-speed, short-duration, audited, and explicitly operator-confirmed.
- A separate plan makes the next code changes reviewable without mixing them with flight-test assumptions.

Impact:
- The next implementation work has a bounded scope: compile-time boundary split, concrete writer interface, runtime/operator gates, safety-gate wiring, audit contract, documented Pi command, and tests.
- Milestone 7 remains deferred.

Risk:
- The plan still introduces future live command output. Implementation must keep defaults blocked and must not add pitch, roll, forward velocity, altitude, mode changes, arm/disarm, or flight authorization.

## 2026-06-05 - Mark Milestone 6.7 Readiness Evidence Complete

Decision:
- Record the `jtzero` 2026-06-05T20:39:07Z live-route match/audit run as readiness evidence 3/3.
- Keep live MAVLink output blocked until a separate reviewed bench props-off implementation plan changes the current boundary.

Why:
- The third run used a route-quality prechecked route and passed both the compact readiness checker and session audit checker.
- It produced 150/150 valid matches, strict monotonic forward progress, endpoint/progress gates, healthy read-only telemetry, 150/150 valid dry-run commands, and expected live-output blocking for every command.
- The evidence-count requirement is satisfied, but the first live-output boundary still needs an explicit implementation plan, operator checklist, and reviewed blocker change.

Impact:
- `docs/LIVE_OUTPUT_READINESS_RECORD.md` now records 3/3 accepted clean Pi dry-runs.
- The next work should design the first bench props-off yaw-only live-output boundary without relaxing route, telemetry, command-quality, audit, or zero-forward-speed gates.

Risk:
- Treating 3/3 evidence as automatic permission to enable live output would bypass the remaining safety review. Documentation must keep distinguishing evidence completion from live-output authorization.

## 2026-06-05 - Add Route Quality Log Checker Before Final Readiness Run

Decision:
- Add `scripts/check-route-quality-log.sh` to validate route recording/validation logs offline.
- Require self-match pass, perturbation pass, malformed-payload rejection, zero exact duplicate entries, and `route_distinctiveness quality_pass=true` by default.

Why:
- The second accepted readiness run proved safety plumbing but used a route whose distinctiveness diagnostics returned `quality_pass=false`.
- The remaining 3/3 readiness evidence should avoid wasting match/audit time on an obviously weak route when the route recording log already contains the needed diagnostics.
- A small log checker gives the operator a short `passed=true/false` result instead of manually scanning long Pi output.

Impact:
- Future readiness collection should record a route, run `check-route-quality-log.sh` on that recording log, and only then run the match/audit readiness command.
- The checker is a route-quality prefilter only; it does not authorize live output, flight, or Milestone 7.

Risk:
- The current route-quality policy is still Gray8-baseline-specific and may reject routes that a future improved matcher could handle. Use diagnostic overrides only for analysis, not for readiness evidence.

## 2026-06-05 - Track Session Audit Evidence Separately From Pi Compact Logs

Decision:
- Add `scripts/check-live-session-audit-log.sh` for non-live `LiveMavlinkOutputSession` audit artifacts.
- Record the successful `jtzero` session-audited dry-run as readiness evidence 2/3 while keeping live MAVLink output blocked.

Why:
- The Pi compact log proves route matching, telemetry health, dry-run command quality, and live-output blocking.
- The session audit log proves the future writer-shaped path can create a real audit artifact with one command record per dry-run command and a final stop record.
- Keeping separate checkers makes failures easier to diagnose and avoids treating the audit file as a substitute for the main readiness gate.

Impact:
- Future session-audited readiness runs can be validated with both `check-live-readiness-log.sh` and `check-live-session-audit-log.sh`.
- The readiness record now has 2/3 accepted clean dry-run logs; one more clean run is still required before changing any live-output blocker.

Risk:
- The accepted route still had a distinctiveness warning, so this evidence supports pre-live safety plumbing only. It is not approval for route quality, live output, flight, or Milestone 7.

## 2026-05-22 - Keep Baseline Separate From New Core

Decision:
- Preserve the existing project under `reference/`.
- Build the new flight-critical implementation under `core/`.

Why:
- The inherited implementation contains useful documentation, UI, diagnostics, and prototype logic.
- Its realtime architecture is not suitable as the foundation for a deterministic long-range GPS-denied return core.
- A separate core keeps future flight-critical changes reviewable and prevents accidental coupling to prototype assumptions.

Impact:
- The repository is larger because it includes the full reference tree.
- Future development can extract ideas from the reference implementation without modifying it directly.

Risk:
- Developers may accidentally treat `reference/` as active production code. Documentation should keep reinforcing that it is a baseline/reference area.

## 2026-05-22 - Use Replay-First C++ Core

Decision:
- Build the new core around replayable frame inputs before hardware camera integration.

Why:
- Flight behavior must be testable without risking hardware on every change.
- Replay makes route matching, preprocessing, timing, and failure handling easier to debug.
- Pi Zero 2W constraints require deterministic CPU and memory behavior from the beginning.

Impact:
- Early milestones focus on data formats, frame preprocessing, matching, and timing rather than immediate real camera control.

Risk:
- Replay data can hide hardware-specific latency or exposure problems. Hardware capture remains a separate milestone.

## 2026-05-27 - Start Replay With Manifest Plus PGM Gray8

Decision:
- Define the first replay input as a simple CSV manifest with `id,timestamp_ns,path`.
- Load image frames initially from binary PGM P5 grayscale files.

Why:
- The replay path must work without camera hardware, Python, OpenCV, or heavyweight codecs.
- PGM Gray8 is easy to generate in tests and maps directly to the first preprocessing and route-signature work.
- A manifest keeps monotonic frame timestamps explicit instead of deriving timing from filenames or video decode.

Impact:
- Milestone 1 can progress with deterministic image-sequence tests on desktop and constrained hardware.
- Video containers, Pi camera capture, BGR, and thermal formats remain later extensions behind the same `CameraSource` interface.

Risk:
- PGM is not a production capture format. It is intentionally a low-dependency replay seed and should be extended once timing and preprocessing behavior are stable.

## 2026-05-27 - Add Milestone 1.5 Before Route Signatures

Decision:
- Insert a short infrastructure and integration milestone before starting the Visual Route Signature format.
- The milestone should add `.gitignore`, `docs/BUILDING.md`, `scripts/test-core.ps1`, and a minimal end-to-end replay pipeline harness.

Why:
- Milestone 1 unit coverage is now validated, but the modules still need a simple integrated loop before route files become a stable contract.
- Local validation should be one command so future C++ changes are checked consistently.
- Build artifacts such as `core/build/` should be kept out of git by policy instead of manual cleanup.

Impact:
- Milestone 2 starts slightly later but with lower integration risk.
- Future sessions can immediately build/test the C++ core with the documented MSVC/CMake path.
- The pipeline harness gives a place to emit timing and health metrics from real replay input before route recording is added.

Risk:
- Infrastructure work can expand if it tries to become CI too early. Keep Milestone 1.5 limited to local developer hygiene and a minimal replay/preprocess/health loop.

## 2026-05-27 - Route Signature V1 Is Explicit Little-Endian Binary

Decision:
- Start Milestone 2 with a binary route signature file format using magic `VHRS`, version `1`, explicit little-endian integer fields, fixed-size entry metadata, and length-prefixed payload bytes.

Why:
- Route files are a core contract for matching and replay, so versioning and byte order need to be explicit before algorithms depend on them.
- Fixed metadata plus length-prefixed payloads keeps the format stream-friendly while allowing future Gray8, BGR, and thermal signatures.
- A dependency-free binary writer/reader is small enough for Pi Zero class hardware and simple to round-trip in tests.

Impact:
- Matching work can consume a stable `RouteSignatureFile` structure instead of inventing storage concerns later.
- The first implementation supports compact preprocessed payloads and route metadata: frame id, timestamp, altitude band, heading hint, dimensions, and pixel format.

Risk:
- The v1 format is intentionally minimal and may need extension for richer route metadata. Future changes must bump the version or add clearly reserved fields.

## 2026-05-27 - Start Matching With Gray8 Byte Distance

Decision:
- Start Milestone 3 with a simple Gray8 route matcher using normalized mean absolute pixel difference against route signature entries.
- Keep the matcher offline and deterministic, with optional route-window limiting and a minimum confidence gate.

Why:
- A basic matcher gives an end-to-end contract for route progress and confidence before adding more expensive or complex visual methods.
- Byte-level distance on preprocessed small frames is easy to test with synthetic perturbations and provides a baseline for later algorithm comparisons.
- Window limiting lets future navigation constrain matching near the last known progress without changing the matcher interface.

Impact:
- Early matching will be coarse and sensitive to lighting/contrast changes, but it is sufficient to validate route storage, replay, and confidence plumbing.
- Direction error remains `0.0` until a later step adds lateral/heading estimation.

Risk:
- This matcher should not be mistaken for final flight behavior. It is a deterministic baseline and must be improved or guarded before hardware flight tests.

## 2026-05-29 - Estimate Direction Error With Horizontal Pixel Shift

Decision:
- Complete the first Milestone 3 direction-error estimate by searching small horizontal pixel shifts around the matched Gray8 route entry.
- Convert the best shift to radians with a fixed `radians_per_pixel` scale in the matcher configuration.

Why:
- Coarse homing needs a signed correction signal, not only route progress and confidence.
- Small-shift matching is deterministic, cheap on constrained hardware, and easy to validate with synthetic left/right perturbations.
- Keeping the scale configurable avoids pretending that the current estimate is camera-calibrated.

Impact:
- `RouteMatch.direction_error_rad` is now populated by the baseline matcher.
- `--match-route` reports direction error in its per-frame metrics.

Risk:
- This is not a calibrated bearing estimate. It is a coarse lateral visual mismatch signal and must be bounded and confidence-gated before flight use.

## 2026-05-29 - Start Navigation With Bounded Yaw Commands

Decision:
- Start Milestone 4 with `BoundedNavigator`, converting valid route matches into bounded yaw-rate commands plus optional forward speed.
- Gate commands on health state, camera/MAVLink/navigation link health, match validity, confidence, and match age.

Why:
- Navigation output must fail closed before any MAVLink integration exists.
- Bounded yaw-rate commands are the smallest useful control output from the current route matcher.
- Command age and confidence gates prevent stale or weak visual matches from reaching the command path.

Impact:
- The core now has a first complete path from replay frame to match to navigation command model in unit-testable pieces.
- MAVLink integration can later consume `NavigationCommand` without owning confidence or stale-data policy.

Risk:
- The initial model is yaw-only and does not yet include acceleration limiting across successive commands. That remains a required Milestone 4 extension.

## 2026-05-29 - Prepare MAVLink With Dry-Run Command Sink

Decision:
- Add a no-MAVLink `DryRunCommandSink` before starting live MAVLink integration.
- Keep command output single-writer and observable through logs/tests before any ArduPilot transport is connected.

Why:
- Navigation commands must be auditable before they can reach a flight controller.
- A dry-run sink exercises the same `MavlinkBridge` interface without introducing MAVLink dependencies or live command risk.
- Pipeline-level negative tests can verify that low-confidence or stale matches produce invalid commands in the output path.

Impact:
- Milestone 5 can start from a tested command-output boundary.
- Live MAVLink work can focus on transport and ArduPilot protocol details instead of command policy.

Risk:
- Dry-run success does not validate MAVLink timing, modes, or ArduPilot acceptance behavior. Live output remains a separate gated step.

## 2026-05-30 - Map Dry-Run MAVLink Telemetry Into Core State

Decision:
- Add `MavlinkTelemetryAdapter` as the boundary between MAVLink telemetry samples and core health/navigation state.
- Treat heartbeat presence and telemetry freshness as the source of `mavlink_ok`.
- Map attitude yaw and relative altitude into `NavigationEstimate` so route metadata and future navigation gates can consume telemetry without depending on a live transport.

Why:
- The previous replay match harness forced `mavlink_ok` manually, which bypassed the health gate that live MAVLink integration will need.
- A separate adapter keeps dry-run telemetry handling deterministic and unit-testable before any ArduPilot command output exists.
- Freshness gating gives a clear fail-closed behavior for stale or missing telemetry.

Impact:
- Replay route matching now polls `DryRunMavlinkBridge` telemetry and applies it to `HealthMonitor` before generating commands.
- The core has a testable path from scripted heartbeat/mode/attitude/altitude data to health snapshots and navigation estimates.

Risk:
- Armed and flight-mode permissions are not yet enforced by the navigator. They remain the next Milestone 5 safety gate before live output is considered.

## 2026-05-30 - Gate Dry-Run Commands On Armed Guided State

Decision:
- Extend `MavlinkTelemetryAdapter` with command-permission checks.
- Require fresh heartbeat telemetry, armed state, and `Guided` mode before health is allowed to report navigation as OK.
- Keep the actual command rejection inside the existing `BoundedNavigator` health gate rather than adding a second command policy path.

Why:
- MAVLink mode and armed state are transport-derived permissions, but the navigator already owns the final fail-closed command decision through `HealthSnapshot`.
- Mapping permission failure to `navigation_ok=false` keeps command output invalid without adding live MAVLink behavior.
- Dry-run replay now exercises the same permission concept that live ArduPilot output would need later.

Impact:
- Disarmed, wrong-mode, stale, or no-heartbeat telemetry blocks commands through the normal health gate.
- Replay route matching scripts armed Guided dry-run telemetry explicitly for positive command tests.

Risk:
- Only `Guided` is accepted for now. Future ArduPilot integration may need a more nuanced allowed-mode policy, but that should remain explicit and tested.

## 2026-05-30 - Start Hardware Capture With A Fail-Closed Pi Camera Boundary

Decision:
- Add `PiCameraSource` as the first Milestone 6 hardware capture boundary implementing `CameraSource`.
- Validate dimensions, frame rate, and initial Gray8 output format in the constructor.
- Keep the desktop/default backend fail-closed: without a compiled libcamera implementation, `start()` returns false, `running()` remains false, and `poll()` returns no frame.

Why:
- The core needs a stable camera-source contract before hardware-specific code is added.
- Desktop CI/local testing must keep working without Pi hardware or libcamera packages.
- A fail-closed source prevents accidental assumptions that live capture is available before hardware validation.

Impact:
- Future Pi camera work has a concrete class and tests to extend.
- Replay remains the only active capture path in the validated desktop baseline.

Risk:
- This does not capture real camera frames yet. The next hardware step must be done on Pi/libcamera-capable hardware and must preserve the fail-closed behavior on unsupported builds.

## 2026-05-30 - Make Pi Builds Explicit And Scripted

Decision:
- Add `VISUAL_HOMING_ENABLE_LIBCAMERA` as an explicit CMake option, defaulting to `OFF`.
- Add `scripts/bootstrap-pi.sh` to install expected Raspberry Pi OS build/camera packages and run validation.
- Add `scripts/test-core-pi.sh` to configure `core/build-pi` in Release with the libcamera option enabled, build, and run CTest.

Why:
- Pi-specific dependencies should not leak into Windows or desktop replay-first builds.
- A one-command Pi setup reduces manual setup drift and makes hardware validation reproducible.
- Live capture can be introduced behind an explicit hardware flag without changing the validated desktop baseline.

Impact:
- Developers can bootstrap a Pi build path from the repository root.
- `core/build-pi/` is ignored as a generated artifact.
- Shell scripts are tracked with LF line endings for bash compatibility.

Risk:
- The scripts install packages but cannot prove camera runtime behavior until executed on real Pi hardware with a connected camera.

## 2026-05-30 - Add Camera Smoke As An Explicit Hardware Check

Decision:
- Add `run_pi_camera_smoke` and CLI `--pi-camera-smoke <width> <height> <fps> <frames>`.
- Keep camera smoke optional in `scripts/test-core-pi.sh` through `VISUAL_HOMING_RUN_CAMERA_SMOKE=1`.

Why:
- CTest should remain hardware-independent by default.
- A dedicated smoke command gives Pi validation a stable entry point once the real libcamera backend exists.
- Optional execution avoids failing package/build validation on systems where a camera is not attached or backend work is not complete.

Impact:
- Desktop validation still passes without camera hardware.
- Pi operators can run build/tests only, or opt into live camera smoke with explicit environment variables.

Risk:
- The smoke command only verifies capture availability and basic frame delivery; it does not validate visual matching quality or flight behavior.

## 2026-05-30 - Require Libcamera Package Discovery For Hardware Builds

Decision:
- When `VISUAL_HOMING_ENABLE_LIBCAMERA=ON`, require CMake `PkgConfig` discovery of the `libcamera` package and link the core target against `PkgConfig::LIBCAMERA`.

Why:
- The hardware flag should prove the Pi build environment has the camera development package available.
- Failing at configure time is clearer than compiling a nominal hardware build with missing backend prerequisites.

Impact:
- Default desktop builds remain unaffected because the option defaults to `OFF`.
- Pi builds now validate that `libcamera-dev` and `pkg-config` are installed before backend work proceeds.

Risk:
- This path still needs to be executed on real Raspberry Pi hardware; local Windows validation only covers the default `OFF` path.

## 2026-05-30 - Keep Pi Builds Memory-Conservative By Default

Decision:
- Default `scripts/test-core-pi.sh` to `MinSizeRel` and `VISUAL_HOMING_BUILD_JOBS=1`.
- Allow overrides through `VISUAL_HOMING_PI_BUILD_TYPE` and `VISUAL_HOMING_BUILD_JOBS`.

Why:
- Pi Zero 2W class hardware can run out of RAM during parallel C++ compilation, especially with libcamera headers and optimized builds.
- A slower one-job build is preferable to nondeterministic OS kills during setup.

Impact:
- Pi bootstrap is more likely to complete on constrained hardware.
- Larger Pi boards can still opt into higher parallelism explicitly.

Risk:
- Pi builds take longer by default, but this is acceptable for reproducible setup.

## 2026-05-30 - Keep Assertions Active In CTest Builds

Decision:
- Undefine `NDEBUG` for CTest executable targets even when the core is configured as `Release`, `MinSizeRel`, or another release-like build type.

Why:
- The tests intentionally use `assert` for compact deterministic checks.
- Pi uses `MinSizeRel` to reduce compile memory pressure, which would normally disable `assert` and can hide checks or skip side effects inside assertions.

Impact:
- The production library and CLI still build with the selected build type.
- Test executables keep assertions active across desktop and Pi configurations.

Risk:
- Test binaries differ slightly from production optimization flags, but this is appropriate because tests must enforce invariants.

## 2026-05-30 - Implement Initial Libcamera Capture Behind PiCameraSource

Decision:
- Implement the first libcamera backend behind `PiCameraSource` when `VISUAL_HOMING_ENABLE_LIBCAMERA` is enabled.
- Use libcamera `CameraManager`, a single Viewfinder stream, `FrameBufferAllocator`, request completion callbacks, and request reuse.
- Request YUV420 from libcamera and copy the first luma plane into the core `Frame` as Gray8.

Why:
- The core wants Gray8 frames and deterministic preprocessing, so using the luma plane gives the smallest useful live camera path.
- Keeping the backend behind the existing compile flag preserves the fail-closed desktop baseline.
- Reusing requests matches libcamera's capture model and avoids per-frame allocation in the live path.

Impact:
- `--pi-camera-smoke` can now exercise real camera capture on Pi hardware once compiled with libcamera.
- The callback copies frame bytes into a bounded two-frame queue; if the consumer lags, older frames are dropped.

Risk:
- The libcamera-enabled path still needs Pi-side compile/runtime validation and may require format/stride adjustments for specific camera pipelines.

## 2026-05-30 - Keep Live Camera Capture Opt-In At Runtime

Decision:
- Add `PiCameraConfig::enable_live_capture`, defaulting to `false`.
- Require both compile-time `VISUAL_HOMING_ENABLE_LIBCAMERA` and runtime `enable_live_capture=true` before `PiCameraSource::start()` touches live camera hardware.
- Set `enable_live_capture=true` only from the explicit `--pi-camera-smoke` CLI path.

Why:
- CTest must remain deterministic and offline even on Pi hardware.
- The libcamera-enabled build should be able to run unit tests without opening the physical camera.
- Live camera access should be a deliberate hardware validation action.

Impact:
- `pi_camera_source_test` and `camera_smoke_test` keep exercising fail-closed behavior under CTest.
- Optional Pi smoke still reaches the live backend through the CLI.

Risk:
- Future callers must explicitly enable live capture when they are truly operating in a hardware validation or live capture mode.

## 2026-05-31 - Record Live Routes Through Explicit Hardware Mode

Decision:
- Add `record_live_camera_route` and CLI `--record-live-route` as an explicit Pi camera hardware validation path.
- Reuse `PiCameraSource`, `Gray8ResizePreprocessor`, `HealthMonitor`, `RouteSignatureRecorder`, and `VHRS` v1 instead of inventing a separate live route file path.
- Keep live route recording outside CTest by default and opt in from `scripts/test-core-pi.sh` with `VISUAL_HOMING_RECORD_LIVE_ROUTE=1`.

Why:
- After live camera preprocessing passed on `jtzero`, the next useful hardware artifact is an actual `VHRS` route file recorded from the camera.
- Recording routes through the same recorder used by replay keeps offline matching and route inspection deterministic.
- Disk writes are acceptable in this explicit recording tool, but must stay out of future realtime command loops.

Impact:
- Pi operators can build, run CTest, and optionally capture a live route signature with one script.
- Desktop/default builds still fail closed because runtime live capture remains disabled unless an explicit hardware CLI enables it.

Risk:
- A recorded live route proves capture and serialization, not visual homing quality. The next step must validate the route with offline reader/matcher checks before any flight-test ladder work.

## 2026-05-31 - Add Offline Route Inspection Before More Live Work

Decision:
- Add `summarize_route_signature`, `inspect_route_signature_file`, and CLI `--inspect-route <route.vhrs>`.
- Make `scripts/test-core-pi.sh` inspect the route file immediately after optional live route recording.
- Keep inspection offline and dependency-free through the existing `VHRS` reader.

Why:
- Live route recording needs a cheap artifact check before route matching or flight-test planning.
- Header, entry count, dimensions, timestamps, payload sizes, and Gray8 status catch malformed recordings early.
- The same inspection path works for replay-generated and live-generated route files.

Impact:
- Pi validation can now prove that a recorded `VHRS` file is readable and internally consistent.
- Desktop tests cover summary behavior, including non-monotonic timestamps and mixed entry shapes.

Risk:
- Inspection only validates file structure and basic invariants; it does not prove route match quality.

## 2026-05-31 - Keep Hardware Artifacts Persistent But Untracked

Decision:
- Default Pi live route artifacts to `artifacts/visual_homing_live_route.vhrs` instead of `/tmp/visual_homing_live_route.vhrs`.
- Ignore `artifacts/` in git.
- Let the Pi script create the parent directory before recording or inspecting.

Why:
- `/tmp` can lose the route file between sessions, reboots, or cleanup, which makes later offline inspection fail even when recording previously succeeded.
- Route artifacts should persist locally for repeated inspection and matching, but they should not be committed.

Impact:
- Operators can run `VISUAL_HOMING_RECORD_LIVE_ROUTE=1 ./scripts/test-core-pi.sh` once and then rerun `VISUAL_HOMING_INSPECT_ROUTE=1 ./scripts/test-core-pi.sh` against the default artifact path.

Risk:
- Local artifacts can accumulate over time. They remain outside git and can be manually deleted when no longer needed.

## 2026-05-31 - Self-Match Route Artifacts Before Perturbation Checks

Decision:
- Add `RouteSelfMatchSummary`, `self_match_route_signature`, `self_match_route_signature_file`, and CLI `--self-match-route <route.vhrs> [minimum_confidence]`.
- Make Pi live route recording automatically inspect and self-match the written route artifact.
- Add optional `VISUAL_HOMING_SELF_MATCH_ROUTE=1` for checking an existing route artifact without opening camera hardware.

Why:
- After structural inspection passes, the next cheapest check is proving that each route entry can match back through the baseline matcher with high confidence and monotonic progress.
- Self-match catches matcher/format integration problems before adding noisy perturbation checks or flight-test preparation.
- Keeping this offline preserves replay-first validation and avoids coupling to live camera timing.

Impact:
- Desktop validation now includes a focused route artifact self-match test.
- Pi route artifacts can be checked for matcher compatibility with one script flag.

Risk:
- Self-match is an optimistic lower-bound check; it does not prove robustness to lighting, motion blur, viewpoint changes, or route reversal.

## 2026-05-31 - Add Deterministic Route Perturbation Checks

Decision:
- Extend route artifact checks with `perturbation_check_route_signature`, `perturbation_check_route_signature_file`, and CLI `--perturb-route <route.vhrs> [minimum_confidence]`.
- Check brightness offset, small deterministic byte noise, one-pixel horizontal shift, and malformed-payload rejection.
- Run perturbation checks automatically after live route recording and expose `VISUAL_HOMING_PERTURB_ROUTE=1` for existing artifacts.

Why:
- Self-match proves format/matcher compatibility but does not exercise tolerance to small visual changes.
- Deterministic perturbations give a cheap offline baseline for brightness/noise/shift behavior before using real flight logs.
- Malformed payload rejection keeps the fail-closed boundary visible in artifact validation.

Impact:
- Desktop validation covers perturbation checks in CTest and direct CLI.
- Pi artifacts can now be checked for basic robustness without opening camera hardware.

Risk:
- Synthetic perturbations are not a substitute for real viewpoint, motion blur, exposure, or terrain variation testing.

## 2026-05-31 - Use Stateless Matching For Route Artifact Checks

Decision:
- Run route self-match and perturbation artifact checks with full-route stateless matching instead of the normal stateful route window.
- Keep exact index matches and monotonic progress as diagnostics for route distinctiveness.
- Gate artifact-check pass/fail on high-confidence valid matches for all checked entries plus malformed-payload rejection for perturbation checks.

Why:
- A low-texture live `jtzero` route exposed that a stateful artifact-check window can lock onto an earlier ambiguous entry and then exclude later exact entries.
- Artifact checks validate whether the stored signatures remain matchable under controlled offline cases; they should not fail solely because repeated frames are ambiguous.
- The normal stateful route window remains relevant for route-following behavior, but artifact validation needs to separate file/matcher health from route distinctiveness.

Impact:
- Repetitive live route artifacts can pass self-match and perturbation checks when every entry is still high-confidence matchable.
- Exact index and progress monotonicity still show when a route lacks visual distinctiveness and can inform later field-test gating.

Risk:
- Passing stateless artifact checks does not prove that sequential route following will disambiguate repetitive terrain. Route distinctiveness needs a later dedicated metric before flight-test expansion.

## 2026-05-31 - Add Offline Route Distinctiveness Diagnostics

Decision:
- Add `analyze_route_distinctiveness`, file/CLI `--route-distinctiveness <route.vhrs>`, and `VISUAL_HOMING_ROUTE_DISTINCTIVENESS=1` Pi script support.
- Report low-texture entries, exact duplicate entries, ambiguous nearest-neighbor entries, payload range, adjacent mean absolute byte difference, nearest-neighbor mean absolute byte difference, and a warning flag.
- Run the diagnostic automatically after live route recording, inspection, self-match, and perturbation checks.

Why:
- Self-match and perturbation checks can pass on visually repetitive routes because every frame remains matchable, but that does not prove route progress is distinguishable.
- A lightweight offline diagnostic gives operators early feedback about low-texture or duplicate route signatures before moving toward bench replay or field-test planning.
- Keeping this outside the live camera/command loop avoids extra Pi realtime load and disk I/O in future flight-critical paths.

Impact:
- Pi validation can now surface repetitive route artifacts without blocking the existing artifact pass/fail checks.
- The diagnostic is cheap for current route sizes because it compares compact 32x24 Gray8 payloads offline.

Risk:
- The thresholds are heuristic and should not be treated as flight permission gates. They are intended to guide route capture quality and future test planning.

## 2026-05-31 - Add Route-Quality Policy To Distinctiveness Diagnostics

Decision:
- Extend route distinctiveness output with low-texture fraction, ambiguous nearest-neighbor fraction, and `quality_pass`.
- Start with conservative offline policy defaults: low-texture fraction `<= 0.05`, ambiguous nearest-neighbor fraction `<= 0.10`, average nearest mean absolute byte difference `>= 5.0`, and no exact duplicate entries.
- Keep this policy as an artifact-quality gate for bench/field route capture, not as a live flight or MAVLink command gate.

Why:
- The static table capture and the hand-carried bench route proved that raw diagnostic metrics are useful but need an operator-facing pass/fail summary.
- The hand-carried bench route had 8/180 ambiguous nearest entries and average nearest mean absolute byte difference about 7.44, so it should pass an initial route-quality policy while the static table captures should fail.
- This is still Gray8 route-signature distinctiveness, not keypoint feature detection; it evaluates whether compact route signatures are distinguishable enough for the current matcher baseline.

Impact:
- Pi route validation now prints both detailed diagnostics and a single `quality_pass` value.
- Operators can quickly tell whether a captured route artifact is suitable for bench replay/field-test planning.

Risk:
- The thresholds are empirical and should be revisited after real outdoor route captures. A passing artifact still does not authorize live MAVLink output or flight testing by itself.

## 2026-05-31 - Insert Calibration And Review Milestones Before Flight Tests

Decision:
- Add Milestone 6.5 before the flight-test ladder for camera profiles, FOV-aware direction conversion, read-only real MAVLink telemetry, altitude-aware route metadata, and thermal profile preparation.
- Add Milestone 6.6 before the flight-test ladder for baseline review, weak-point audit, safety boundary review, and security/operational hardening.
- Save a complete Claude Code handoff prompt in `docs/CLAUDE_CODE_PROMPT.md` so parallel/alternate implementations keep the same replay-first, StabX-like, safety-oriented direction.

Why:
- The long-term target is StabX-like coarse GPS-denied return over large distances, not a short-range toy matcher.
- Long-distance behavior needs camera/FOV profiles, ArduPilot attitude/altitude telemetry, altitude/scale-aware metadata, thermal/visible camera profiles, route segmentation, reacquire behavior, and confidence gates.
- A review/security pass before Milestone 7 reduces the chance that hidden safety or operational weaknesses get carried into bench/field testing.

Impact:
- Milestone 7 is not the immediate next engineering step after Milestone 6 closure.
- The next work should first make camera calibration and read-only flight-controller telemetry explicit, then review the system before any flight-test ladder expansion.

Risk:
- This delays visible flight-test progress, but it lowers integration risk and keeps live MAVLink command output blocked until the surrounding telemetry and safety model is credible.

## 2026-05-31 - Start Camera Profiles With FOV-Derived Angular Scale

Decision:
- Add an in-core `CameraProfile` model with camera id, sensor type, pixel format, capture size, target size, horizontal/vertical FOV, and route-quality thresholds.
- Add profile validation and derived angular scale values for capture and target pixels.
- Document profile fields in `docs/CAMERA_PROFILES.md`.

Why:
- StabX-like longer-distance behavior needs per-camera FOV and preprocessing assumptions instead of ad hoc direction scaling.
- Different visible-light and thermal cameras require different FOV, normalization, and threshold profiles.
- FOV-derived angular scale is a prerequisite for replacing manual `radians_per_pixel` settings in route matching and future hold/return modes.

Impact:
- The core now has a typed place for camera calibration assumptions.
- Existing matcher behavior is unchanged until profile-derived values are explicitly wired into match configs.

Risk:
- The first profile model is in-code only. Profile file loading, operator UI/templates, thermal normalization, and real telemetry integration remain pending.

## 2026-05-31 - Use Camera Profile FOV For Replay Direction Scaling

Decision:
- Allow `RouteMatchingConfig` to carry an optional `CameraProfile`.
- When present, derive horizontal `radians_per_pixel` from `horizontal_fov_rad / target_width`.
- Add an inline profile/FOV form to `--match-route` while preserving the legacy explicit `radians_per_pixel` form.

Why:
- Direction-error scale should come from camera calibration, not a magic number.
- Keeping the legacy form avoids breaking existing tests and diagnostics while M6.5 profile loading is still pending.
- Inline CLI support lets replay tests validate profile-derived scaling before adding config files or UI templates.

Impact:
- Replay route matching can now report and use profile-derived angular scale.
- Future camera profile files can plug into the same `RouteMatchingConfig` path.

Risk:
- Inline CLI profiles are a bridge, not the final operator experience. Profile file loading and validation remain required before field workflows.

## 2026-05-31 - Keep Camera Profile Selection Pi-Owned

Decision:
- Store supported camera profiles as Pi/core-owned configuration files rather than Android-owned state.
- Expose profile selection through future Pi API endpoints for listing available profiles, reading the active profile, and setting the active profile.
- Reuse source patterns from the reference Android Kotlin/Compose app for a future Settings selector, but do not use the old APK artifact as a maintainable base.

Why:
- Camera profiles affect route matching, direction scaling, route-quality thresholds, and future thermal/altitude behavior, so they must be validated where the deterministic core runs.
- Android should be a companion UI for operator selection, not the source of flight-critical calibration truth.
- The reference Android codebase already has useful Retrofit, preferences, and Settings-screen structure, so reusing source ideas is cheaper than starting from a blank app after the Pi API contract exists.

Impact:
- Milestone 6.5 should add profile file loading and profile-selection API before serious Android work.
- The Android app can later show a profile dropdown/list and submit only a selected profile id.
- Active camera profile state remains reproducible on the Pi and visible to scripts, replay tools, and logs.

Risk:
- UI work before the Pi API exists could create a second configuration path. Keep Android profile selection blocked on Pi-owned profile file loading and API validation.

## 2026-06-20 - Keep External-Nav Altitude Presets Pi-Owned

Decision:
- External-nav dry-run readiness uses Pi-owned altitude presets for common operator setups: `floor` and `stand`.
- The Pi wrapper maps those presets to expected relative-altitude windows and logs the resolved preset, expected altitude, and tolerance.
- A future Android companion should present these as operator choices and send the selected preset to the Pi instead of owning independent height calibration values.
- The standard readiness wrapper uses a short preflight and cue by default, while long telemetry captures remain explicit debug/drift tests.

Why:
- The barometer/EKF height origin can shift after power, waiting, or arm/disarm, so the physical placement used for preflight must be explicit and logged immediately before route dry-run.
- Android should reduce operator mistakes, not become a second source of calibration truth.
- The Pi/core already owns the deterministic readiness gate and can reject mismatched physical state before route matching starts.

Impact:
- Field workflow can use "Floor" or "Stand" selection in the future app while the Pi remains responsible for the actual expected altitude window and pass/fail verdict.
- Custom measured setups remain possible through explicit expected/tolerance values.
- Ordinary repeated dry-runs avoid unnecessary waiting: the wrapper defaults to a 15 second altitude preflight and 5 second operator cue unless overridden.

Risk:
- Presets are only as good as the physical setup they describe. The UI must show the resolved values and the latest sanity verdict so the operator can catch a misplaced vehicle or stale height origin.

## 2026-06-20 - Add Non-Binary External-Nav Operator Readiness

Decision:
- Keep existing external-nav pass/fail and quality gates unchanged, but add `external_nav_operator_readiness=ready|marginal|blocked` plus `external_nav_operator_reason` to live-route summaries.
- Treat failed tolerant quality gates as `blocked`.
- Treat passed quality gates with soft diagnostics such as route directional progress regressions or strict all-frames diagnostic misses as `marginal`.
- Use a looser route-progress soft threshold for operator status than the core directional gate: up to `15` progress regressions and `1.0` total rollback can still be `ready` when endpoint and quality gates pass.

Why:
- The Pi dry-run evidence showed useful green safety/readiness passes that still had small route-progress regressions. Calling those hard failures is too noisy, but hiding them loses signal.
- Operators need a compact status that separates unsafe/not-ready conditions from "usable, but not perfect" evidence.
- Recent stand runs repeatedly passed endpoint, altitude, telemetry, and 150/150 external-nav readiness while exceeding the inherited core directional threshold of `5` regressions. That threshold remains useful as a diagnostic, but it is too strict for the top-level operator label.

Impact:
- Existing scripts can still require `passed=true` and `external_nav_quality_ready=true`.
- Android can later display `ready`, `marginal`, or `blocked` without reimplementing the gate logic.

Risk:
- `marginal` must not be treated as permission for live MAVLink output. Writer enablement remains governed by the stricter reviewed live-output gates.

## 2026-06-21 - Future Android Readiness UI Explains Pi-Owned Gates

Decision:
- The future Android companion UI should present Pi-owned readiness as a compact operator dashboard, not as raw log text and not as independently computed flight state.
- The top-level screen should emphasize `external_nav_operator_readiness=ready|marginal|blocked` and `external_nav_operator_reason`.
- Android should visually group the underlying evidence into a few operator-facing cards: route, altitude, telemetry, external-nav estimate validity, and safety gate.
- Android may use color and subtle motion to communicate state: calm green for `ready`, amber/glow or soft pulse for `marginal`, and red blocked framing for `blocked`.

Why:
- The dry-run logs now contain enough signal to explain what happened, but raw key-value fields are too easy to misread during field work.
- Operators need to see both the single verdict and the reason behind it: for example, `ready` with route regressions inside the operator threshold is different from `blocked` because altitude is outside the stand/floor window.
- The Pi must remain the source of truth for thresholds, altitude presets, route-progress classification, and safety/readiness decisions.

Impact:
- Android can later show a main status tile plus focused cards such as:
  - Route: progress start/end, endpoint reached, regressions against operator threshold.
  - Altitude: selected preset, expected window, observed min/avg/max on a small scale.
  - Telemetry: health/freshness and dropped bytes.
  - ExternalNav: valid estimates, valid fraction, invalid streak.
  - Safety Gate: expected blocked reason such as `vehicle_not_armed`.
- The UI should reveal details on tap, but the first screen should remain readable under field pressure.

Risk:
- Visual emphasis must not imply live-output permission. `ready` in the dry-run UI still means readiness evidence only until the separate reviewed live-output gates exist and pass.

## 2026-06-22 - Export External-Nav Readiness JSON From Compact Logs

Decision:
- Add a dependency-free shell exporter for `visual_homing.external_nav_readiness.v1` JSON from the final `live_route_match_compact` line.
- Keep the C++ compact key-value log as the primary low-level artifact and derive the first Android/API contract from that existing line.
- Make the standard external-nav dry-run wrapper write the JSON artifact next to the run log.

Why:
- Pi Zero class hardware benefits from avoiding Python, `jq`, Node, SQLite, or an HTTP server while the contract is still stabilizing.
- Android should consume structured Pi-owned readiness data instead of parsing raw logs or reimplementing thresholds.
- Exporting from the compact line keeps old field-debug workflows intact and gives us a cheap bridge toward a future Pi API.

Impact:
- A successful readiness wrapper run produces both the existing `.log` and a sibling `.json`.
- Existing scripts and grep-based checks continue to work unchanged.
- Future Android work can start from `schema=visual_homing.external_nav_readiness.v1`.

Risk:
- The shell JSON exporter must stay simple and limited to fields already present in the compact line. If the contract grows significantly, move JSON emission into a dedicated Pi API layer rather than teaching Android to infer missing state.

## 2026-05-31 - Use Strict Key-Value Camera Profile Files

Decision:
- Add dependency-free camera profile loading from strict `key = value` text files.
- Reject unknown keys, malformed lines, invalid enum values, missing required fields, and invalid numeric ranges.
- Track an initial `config/camera_profiles/imx219-visible-wide.profile` template and expose `--inspect-camera-profile` plus `--match-route-profile`.

Why:
- Pi Zero 2W class hardware and the flight-critical core benefit from a small parser with no JSON/YAML dependency.
- A strict format prevents misspelled profile fields from being silently ignored.
- Profile files provide the stable Pi-owned configuration layer needed before adding Android profile selection APIs.

Impact:
- Operators can validate a camera profile without touching camera hardware.
- Replay route matching can now use profile files directly instead of inline profile arguments.
- Future Pi API endpoints can list and select these files without inventing another profile representation.

Risk:
- The first tracked IMX219 FOV values are placeholders until measured for the exact lens/crop mode. They are calibration templates, not final flight calibration.

## 2026-05-31 - Store Active Camera Profile As Validated Local State

Decision:
- Add a core camera profile registry layer that lists valid `.profile` files from a directory.
- Store the active camera profile as a local state file containing only the selected profile id.
- Allow setting the active id only after the referenced profile exists and passes strict loading/validation.

Why:
- Future Android profile selection needs a simple Pi-owned state model that can be exposed through API endpoints without making Android the source of calibration truth.
- Keeping the active selection separate from committed profile templates avoids local operator choices changing git-tracked calibration files.
- Validating before writing active state prevents stale or misspelled profile ids from becoming the selected runtime configuration.

Impact:
- The CLI now supports profile listing, active-profile selection, and active-profile inspection.
- The Pi script exposes the same operations through environment flags.
- Future Pi API work can wrap these core functions directly.

Risk:
- The active state file is still local filesystem state. API work must preserve the same validation path and avoid introducing a parallel configuration store.

## 2026-05-31 - Use Camera Profiles For Hardware Validation Dimensions

Decision:
- Add profile-backed camera smoke and live route recording CLI forms.
- Allow hardware validation to use either a specific profile file or the active profile state for capture and target dimensions.
- Keep the older manual-dimension commands for compatibility and diagnostics.

Why:
- Once profiles exist, capture size and preprocessing target size should come from the same validated calibration source used by matching and future Android selection.
- Active-profile hardware validation proves that operator selection can affect real Pi capture paths before adding a network API.
- Preserving manual commands keeps low-level camera troubleshooting possible if a profile is wrong.

Impact:
- Pi operators can run smoke/record using `VISUAL_HOMING_USE_CAMERA_PROFILE=1` or `VISUAL_HOMING_USE_ACTIVE_CAMERA_PROFILE=1`.
- The future Android selector can target the active profile state and expect profile-backed hardware capture to use the same selection.

Risk:
- Profile-backed capture still does not imply flight permission. Live MAVLink command output remains blocked, and profile values, especially FOV, still require real calibration.

## 2026-05-31 - Report Sample Frame IDs For Route Distinctiveness Failures

Decision:
- Extend route distinctiveness output with bounded sample frame id lists for low-texture, exact-duplicate, and ambiguous-nearest entries.
- Keep the lists small so Pi logs remain readable.

Why:
- Aggregate counts explain that a route failed quality policy, but not where the issue occurred.
- Sample frame ids help distinguish start/end pauses from repeated visual content in the middle of a route.

Impact:
- Operators can rerun route recording with clearer feedback about which part of the route caused `quality_pass=false`.
- The route-quality policy itself is unchanged.

Risk:
- The sample ids are diagnostic hints, not a full visualization. Deeper analysis may still require exporting or visualizing route frames later.

## 2026-05-31 - Allow Explicit Edge Trim For Route Quality Diagnostics

Decision:
- Add an optional `edge_trim_entries` value to route distinctiveness analysis and the `--route-distinctiveness <route.vhrs> [edge_trim_entries]` CLI.
- Expose the same setting in Pi validation through `VISUAL_HOMING_ROUTE_EDGE_TRIM`.
- Report how many entries were ignored at the start and end, plus the evaluated frame/time span and sample times in `frame_id@route_time_ms` format.

Why:
- A 240-frame hand-carried `jtzero` route showed that the only route-quality failures were startup frames `0,1,2`.
- Operators need a way to evaluate the useful part of a route without hiding the original full-artifact diagnostics or rewriting the `VHRS` file.

Impact:
- Route quality can now distinguish global visual ambiguity from short operator-controlled start/end pauses.
- The default no-trim path remains unchanged.

Risk:
- Trimming can make a weak artifact look better if overused. It must remain explicit in logs and should be limited to known start/end handling, not used to mask mid-route ambiguity.

## 2026-06-01 - Persist Pi Run Logs With Wall-Clock Time

Decision:
- Make `scripts/test-core-pi.sh` tee every run to `artifacts/logs/test-core-pi-<UTC>.log` by default.
- Print `pi_test_run_start` and `pi_test_run_done` lines with UTC wall-clock time, elapsed seconds, exit code, route path, and log path.
- Add `wall_time_utc` to the core boot line for every CLI invocation.

Why:
- Route-relative timestamps explain frame chronology inside a `VHRS` artifact, but they do not show when an operator ran a command.
- Pi storage is expected to be 32-64 GB, so preserving text logs is cheap and useful for comparing field/bench attempts.

Impact:
- Pi validation runs now leave a persistent operator log without changing command usage.
- Wall-clock logs and route-relative diagnostics can be correlated when investigating route quality or camera behavior.

Risk:
- Logs can accumulate over time. They live under ignored `artifacts/` by default and can be deleted locally when no longer needed.

## 2026-06-01 - Drop Live Route Warmup Frames Before Recording

Decision:
- Add `warmup_frames` to live route recording configuration and optional CLI arguments.
- Default `scripts/test-core-pi.sh` to `VISUAL_HOMING_ROUTE_WARMUP_FRAMES=3` for live route recording.
- Log each dropped warmup frame and report `warmup_frames_dropped` in the recording summary.

Why:
- Pi validation showed that the first route frames can be low-texture/exact-duplicate startup samples even when the useful route body is distinctive.
- Dropping a few initial camera frames keeps new `VHRS` artifacts cleaner without relying on diagnostic edge trim afterward.

Impact:
- New Pi-recorded routes should no longer include the known startup frames by default.
- Operators can set `VISUAL_HOMING_ROUTE_WARMUP_FRAMES=0` to reproduce raw startup behavior for diagnostics.

Risk:
- A fixed default may not be optimal for every camera or exposure condition. It is logged, configurable, and should be revisited after more field captures.

## 2026-06-01 - Add One-Flag Offline Route Validation

Decision:
- Add `VISUAL_HOMING_VALIDATE_ROUTE=1` to the Pi test script.
- Run inspection, self-match, perturbation, and distinctiveness checks on the selected route artifact.

Why:
- Operators should not need to remember four separate environment flags to validate an existing `VHRS` route after recording or copying artifacts.
- The convenience mode keeps route validation offline and avoids opening camera hardware.

Impact:
- Bench and field iteration can validate the current route artifact with one command.
- Individual check flags remain available for targeted diagnostics.

Risk:
- The convenience flag still uses heuristic route-quality thresholds. Passing it remains an artifact-quality signal, not flight authorization.

## 2026-06-01 - Add Machine-Readable Camera Profile Registry Commands

Decision:
- Add JSON-output core commands for listing camera profiles, getting the active profile, and setting the active profile.
- Expose those commands through `scripts/test-core-pi.sh` with `VISUAL_HOMING_API_LIST_CAMERA_PROFILES`, `VISUAL_HOMING_API_GET_ACTIVE_CAMERA_PROFILE`, and `VISUAL_HOMING_API_SET_CAMERA_PROFILE_ID`.

Why:
- Android profile selection needs a stable machine-readable contract before UI work should depend on it.
- The deterministic core already owns profile validation and active-state writes, so a later Pi HTTP service should wrap this behavior rather than reimplement calibration rules.

Impact:
- The future endpoints `GET /api/camera-profiles`, `GET /api/camera-profiles/current`, and `POST /api/camera-profiles/current` now have a concrete payload source.
- Operators can validate the API-shaped profile payload on the Pi without camera hardware.

Risk:
- This is not yet an HTTP server. It is the core-side contract and JSON payload layer that a later Pi service can expose over the network.

## 2026-06-01 - Start Read-Only MAVLink Telemetry With Offline Byte Inspection

Decision:
- Add a dependency-free MAVLink v1/v2 byte-stream inspector for captured telemetry files.
- Decode `HEARTBEAT`, `ATTITUDE`, and `GLOBAL_POSITION_INT` payloads into heartbeat presence, armed state, coarse ArduPilot mode, roll/pitch/yaw, and relative altitude.
- Expose the inspector through `--inspect-mavlink-telemetry` and `VISUAL_HOMING_INSPECT_MAVLINK_TELEMETRY=1`.

Why:
- Milestone 6.5 needs real flight-controller telemetry before long-range visual return can be evaluated seriously.
- Starting with captured bytes keeps the step read-only and testable before opening serial devices on the Pi.

Impact:
- The core can now parse the telemetry fields needed for future camera-frame/attitude/altitude correlation from MAVLink byte streams.
- Live command output remains blocked; this path is inspection-only.

Risk:
- This first inspector does not validate MAVLink CRC and does not read from a live serial port yet. It is a parser/diagnostic foundation, not the final telemetry transport.

## 2026-06-01 - Add Read-Only MAVLink Serial Byte Capture

Decision:
- Add a POSIX serial capture path that opens a configured MAVLink device read-only, records raw bytes for a bounded duration, and writes them to an ignored artifact file.
- Run the existing MAVLink byte-stream inspector after capture.

Why:
- The Pi needs a safe way to prove telemetry wiring, baud rate, and frame parsing before continuous live telemetry is integrated with camera frames.
- Capturing bytes first preserves field evidence in `artifacts/` and keeps the step auditable.

Impact:
- Operators can run a short read-only serial capture against `/dev/ttyAMA0`, `/dev/ttyS0`, `/dev/serial0`, or a USB telemetry adapter and inspect heartbeat/attitude/altitude fields.
- The captured `.bin` can be replayed through `--inspect-mavlink-telemetry` later.

Risk:
- Serial device setup still depends on Pi/ArduPilot wiring, baud rate, and OS permissions. This path does not yet provide continuous telemetry freshness into the live navigation loop.

## 2026-06-01 - Add Read-Only MAVLink Telemetry Smoke Validation

Decision:
- Add `--validate-mavlink-telemetry` and `VISUAL_HOMING_VALIDATE_MAVLINK_TELEMETRY=1`.
- Gate captured telemetry on minimum heartbeat, attitude, and global-position message counts plus a maximum malformed-frame count.

Why:
- A serial capture that only receives bytes is not enough to prove the Pi is receiving useful flight-controller telemetry.
- The field workflow needs a simple pass/fail signal for device, baud, wiring, and stream-rate configuration before continuous telemetry integration.

Impact:
- Operators can combine capture and validation in one Pi test run, and the script will fail closed if the capture lacks the required telemetry classes.
- Thresholds remain configurable for slower stream rates or targeted diagnostics.

Risk:
- This is still a bounded capture validator, not a continuous freshness monitor. CRC validation and live telemetry buffering remain future work.

## 2026-06-01 - Attach Read-Only MAVLink Snapshot To Live Route Recording

Decision:
- Allow live route recording commands to accept a validated MAVLink telemetry capture file.
- When enabled, use the latest decoded relative altitude as route altitude metadata and latest yaw as the route heading hint for recorded entries.

Why:
- Route artifacts need flight-controller context before altitude-aware matching and scale policies can be designed.
- A run-level snapshot is a low-risk bridge from offline MAVLink validation to future continuous telemetry buffering.

Impact:
- Pi route captures can now carry real FC-derived altitude/heading baseline data without changing the `VHRS` format.
- `VISUAL_HOMING_ROUTE_USE_MAVLINK_TELEMETRY=1` makes `scripts/test-core-pi.sh` pass the current captured telemetry artifact into live route recording.

Risk:
- The snapshot is not per-frame telemetry and may become stale during a moving capture. It is logged explicitly and should be replaced by continuous frame-correlated telemetry before flight use.

## 2026-06-01 - Start Live Read-Only MAVLink Telemetry Buffer For Route Recording

Decision:
- Add a read-only MAVLink telemetry stream that opens the configured serial device in a background thread during active route recording.
- Let live route recording use the latest validated telemetry summary for altitude band and heading hint once heartbeat, attitude, and global-position messages have been observed.
- Require a bounded telemetry warmup before recording camera frames when live telemetry route metadata is explicitly enabled.

Why:
- A pre-captured telemetry snapshot proves wiring and metadata plumbing, but it cannot track changing altitude or yaw during a moving route capture.
- The next safe step is a read-only live buffer that can feed metadata while preserving the command-output block.

Impact:
- `VISUAL_HOMING_ROUTE_USE_LIVE_MAVLINK_TELEMETRY=1` records route metadata from live serial telemetry during camera capture.
- The recorder logs stream byte/frame/message counts and falls back to configured route metadata until the live telemetry summary validates.
- The Pi script defaults `VISUAL_HOMING_ROUTE_TELEMETRY_WARMUP_MS=1500`; if telemetry does not validate before the timeout, the run fails closed without writing a mixed-metadata route.
- After warmup, the recorder keeps the last valid telemetry snapshot so transient partial serial frames do not cause individual route entries to fall back to zero/default metadata.

Risk:
- The current buffer reparses accumulated bytes for snapshots and is intended for short bench captures, not long-running flight loops. CRC validation, bounded buffer pruning, and timestamp-aligned per-frame telemetry remain future work.

## 2026-06-01 - Add Read-Only Live Route Matching

Decision:
- Add a live camera route-matching path that reads an existing `VHRS` route artifact and matches current camera frames without writing or replacing the route.
- Expose it through `--match-live-route-active-profile` and `VISUAL_HOMING_MATCH_LIVE_ROUTE=1`.
- Support explicit expected progress modes: `any` for recognition-only bench passes, `forward` for repeating the recorded route direction, and `reverse` for return-direction checks.

Why:
- After a route passes distinctiveness diagnostics, the next bench step is proving that a repeat pass can recover route index/progress from live frames.
- The operator needs a command that cannot accidentally overwrite the current good route artifact.

Impact:
- The live matcher logs per-frame route index, progress, confidence, validity, FOV-derived direction error, and final pass/fail metrics.
- The Pi workflow can now separate capture validation from follow/match validation.
- A repeat pass with high confidence but hand-carried backtracking can pass recognition mode while still exposing forward/reverse progress regressions for diagnosis.

Risk:
- This is still a read-only matching diagnostic. It does not command the flight controller, does not use live MAVLink freshness in the match gate, and does not yet implement route reacquisition or long-running bounded matcher state.

## 2026-06-03 - Add Live Route Dry-Run Navigation Commands

Decision:
- Let opt-in live route matching feed `BoundedNavigator` and `DryRunCommandSink`.
- Expose the mode through `VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_COMMANDS=1` and navigator tuning env vars in `scripts/test-core-pi.sh`.
- Keep live MAVLink command output blocked.

Why:
- Manual route matching is validated enough to move from recognition metrics to command-shape inspection.
- The next safe integration step is seeing bounded yaw/velocity commands in the live match log before any real flight-controller writer is enabled.

Impact:
- Live match logs can include `dry_run_command` lines and per-frame `command_*` fields.
- The final `live_route_match_done` line reports generated and valid dry-run command counts.
- The final summary also reports command-quality metrics and can optionally include the quality gate in `passed`.

Risk:
- This is still a bench diagnostic. It uses a synthetic health link for the no-output dry-run path and must not be treated as permission to send live MAVLink commands.

## 2026-06-03 - Gate Live Dry-Run Commands With Read-Only MAVLink Health

Decision:
- Add an opt-in read-only live MAVLink telemetry stream to active-profile live route matching.
- When enabled, use fresh validated heartbeat telemetry as the `mavlink_ok` health input for `BoundedNavigator`.
- Keep live MAVLink command output blocked; dry-run output still goes only to `DryRunCommandSink`.

Why:
- The synthetic dry-run health link was useful for command-shape inspection, but the next safety step needs proof that command generation fails closed when real FC telemetry is absent or stale.
- This keeps the integration read-only while exercising the same health boundary future live command output will depend on.

Impact:
- `VISUAL_HOMING_MATCH_LIVE_ROUTE_USE_LIVE_MAVLINK_TELEMETRY=1` opens the configured serial device read-only during live matching.
- The match run logs telemetry warmup, byte/frame/message counts, per-frame `telemetry_mavlink_ok`, and final telemetry-health frame counts.
- `VISUAL_HOMING_MATCH_LIVE_ROUTE_REQUIRE_LIVE_TELEMETRY_HEALTH=1` makes the final pass/fail result include telemetry warmup and per-frame freshness health.

Risk:
- This is a freshness/heartbeat health gate for bench dry-run matching, not live command permission. Armed/GUIDED permission and a real MAVLink writer remain blocked for a later explicit safety step.

## 2026-06-03 - Harden Bounded Navigator Fail-Closed Inputs

Decision:
- Reject non-finite `BoundedNavigator` config values and negative `yaw_gain`.
- Keep invalid health, invalid match, low confidence, stale match age, and future match timestamps as invalid zero-command outputs that reset the slew limiter.

Why:
- The navigator sits directly before any future command writer, so malformed tuning must fail at construction instead of producing NaN, infinite, or inverted yaw commands.
- Resetting command history on any invalid input prevents stale valid commands from shaping the next accepted command after a fail-closed interval.

Impact:
- Unit tests now cover per-link health blocking, future timestamps, invalid-match reset, and invalid config rejection.
- Live MAVLink output remains blocked; this only strengthens the dry-run/navigation boundary.

Risk:
- Negative yaw gain is no longer allowed. If an airframe ever needs inverted correction, that should be represented explicitly in camera/profile orientation or route geometry rather than by silently flipping navigator gain.

## 2026-06-03 - Harden VHRS Route Artifact Parsing

Decision:
- Add v1 safety limits for route entry count and per-entry payload size.
- Require every route entry payload length to match `width * height * bytes_per_pixel(format)` on both read and write.
- Reject malformed route files before large vector reservation or payload allocation.

Why:
- `VHRS` files are operator-provided artifacts used by live route matching. A malformed or corrupted file should fail closed with a controlled error, not pressure memory or defer structural validation to later matcher code.

Impact:
- Reader rejects excessive entry counts, oversized payload declarations, zero dimensions, unsupported formats, and payload-size mismatches.
- Writer no longer emits structurally malformed route entries.
- Regression tests cover excessive entry counts, payload-size mismatch, and malformed writes.

Risk:
- The safety limits are intentionally generous for current Pi route artifacts. Future high-resolution or long-duration route formats may need an explicit v2 policy rather than relaxing v1 silently.

## 2026-06-03 - Bound Live MAVLink Telemetry Stream Buffer

Decision:
- Keep the read-only live MAVLink telemetry stream as a bounded retained byte buffer instead of an unbounded accumulated string.
- Preserve total captured byte accounting while reporting retained and dropped byte counts in snapshots and live route logs.
- Return telemetry stream errors by value under lock instead of exposing an unlocked string reference.

Why:
- Short bench runs worked with accumulated bytes, but future longer dry-run and tethered tests should not let serial telemetry memory grow without limit.
- The inspector only needs recent MAVLink bytes for freshness and latest-message health in live loops; total captured bytes remain available as an audit counter.

Impact:
- Default retained buffer size is 64 KiB.
- `live_route_*_telemetry_warmup`, stream-done logs, and final summaries can expose retained/dropped byte metrics.
- A new unit test covers pruning, captured/retained/dropped accounting, clear behavior, and zero-size rejection.
- Stream error reads no longer race the worker thread's error writes.

Risk:
- Very long historical telemetry inspection should use the existing capture-to-file path, not the live stream snapshot buffer.

## 2026-06-04 - Bound Dry-Run Command History

Decision:
- Keep `DryRunCommandSink` and `DryRunMavlinkBridge` as bounded retained command histories instead of unbounded vectors.
- Preserve total sent command accounting and expose dropped-history counters.
- Keep the command-output path dry-run only; no live MAVLink writer is present.

Why:
- Dry-run command logging can be used in longer bench and tethered runs. Retaining every command in memory is unnecessary when logs are streamed and summary counters are available.
- This mirrors the bounded live telemetry buffer policy before any future live writer is considered.

Impact:
- Default retained command history is 10,000 commands.
- `commands()` remains available for tests and short runs, but represents retained history rather than an unlimited archive.
- Unit tests cover pruning and zero-size history rejection for both dry-run output classes.

Risk:
- Long-run historical command analysis should use the emitted log stream, not the retained in-memory `commands()` vector.

## 2026-06-04 - Add Explicit Live MAVLink Output Blocker

Decision:
- Add `LiveMavlinkBridge` as an explicit `MavlinkBridge` implementation that is unavailable by default.
- Make `start()` fail closed and make `send()` throw a controlled blocked-output error.
- Require any future live command output to modify this boundary intentionally instead of appearing through dry-run paths.

Why:
- The safest pre-writer state is not just absence of a writer, but an explicit, tested live-output boundary that rejects commands.
- This gives future integration work a clear place to add compile-time/runtime safety gates.

Impact:
- Unit coverage verifies that live output is unavailable, does not enter running state, and rejects valid commands.
- Existing dry-run paths are unchanged.

Risk:
- This is still not a live writer. The next step toward real output must define arming/mode/freshness/operator-enable gates before changing this class.

## 2026-06-04 - Add Compile-Time Live MAVLink Output Gate

Decision:
- Add `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT` as an explicit CMake option that currently fails configuration when enabled.
- Define `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_BLOCKED=1` for the core library and test the blocked build contract.
- Keep `LiveMavlinkBridge::command_output_compiled_out()` true until a reviewed live writer replaces the fail-closed boundary.

Why:
- Live command output should not become available by accidentally wiring a runtime flag, environment variable, or alternate bridge path.
- A future writer must deliberately change both the compile-time gate and the runtime safety boundary after arming/mode/freshness/operator-enable gates are specified.

Impact:
- Default builds continue to compile and test normally.
- Attempts to configure with live MAVLink output enabled fail early with a clear safety error.
- The current Pi and desktop paths remain dry-run/read-only for command output.

Risk:
- This intentionally blocks experimentation with real command output until the next safety review step removes or replaces the gate.

## 2026-06-04 - Harden Live Route CLI And Pi Env Parsing

Decision:
- Parse live-route active-profile CLI numeric arguments with full-string validation for integers, unsigned integers, and floating-point values.
- Reject negative values before unsigned parsing.
- Require Pi script boolean environment flags to be exactly `0` or `1`, and require live-route expected progress to be `any`, `forward`, or `reverse`.

Why:
- Future safety-sensitive modes will be controlled through these CLI/env surfaces. Typos and partial numeric strings should fail closed instead of being silently truncated or treated as false.

Impact:
- Invalid values such as `15bad` for CLI numeric fields now produce a controlled parse error.
- Invalid values such as `VISUAL_HOMING_MATCH_LIVE_ROUTE=true` now fail before build/test actions begin.
- Existing documented Pi commands using `0`/`1` and valid numeric values are unchanged.

Risk:
- Operators must use `0`/`1` in Pi env flags rather than shell-style `true`/`false`.

## 2026-06-04 - Define Live MAVLink Output Safety Gate Contract

Decision:
- Add `LiveMavlinkOutputSafetyGate` as a tested, standalone permission gate for any future live MAVLink writer.
- Require explicit runtime enable, operator confirmation, single-writer ownership, audit logging, prior dry-run quality, fresh heartbeat telemetry, armed `Guided` mode, fresh high-confidence route match, valid command, finite command fields, and command bounds.
- Return a stable blocked reason for the first failed gate.

Why:
- The project needs a concrete contract before a real command writer exists. Keeping the gate independent lets tests cover the safety policy without enabling live output.

Impact:
- Unit tests cover every currently required failure reason plus the all-clear path.
- No live command output is added; the compile-time and runtime live-output blockers remain in place.

Risk:
- This gate is necessary but not sufficient for flight. A future writer still needs integration tests, operator workflow, and hardware-level kill/stop behavior before sending commands.

## 2026-06-04 - Log Live Output Gate Diagnostics During Dry-Run Matching

Decision:
- Evaluate `LiveMavlinkOutputSafetyGate` for each live-route dry-run command and append `live_output_gate_allowed` plus `live_output_gate_reason` to frame logs.
- Re-evaluate the final frame after aggregate dry-run command quality is known and append final gate counters/reason to `live_route_match_done`.
- Keep this diagnostic independent from any real command writer.

Why:
- The next hardware dry-run should show whether the future writer would be blocked by telemetry, mode, route match, command bounds, or dry-run quality without sending MAVLink commands.

Impact:
- Live-route dry-run logs now include per-frame and final live-output gate diagnostics.
- Final summaries include compact per-reason blocked-frame counts such as `vehicle_not_armed:150`.
- No live MAVLink output is enabled.

Risk:
- Per-frame diagnostics assume runtime/operator/audit prerequisites are intentionally satisfied so that frame-level blockers are visible; the final summary includes aggregate dry-run quality.

## 2026-06-04 - Add Compact Live Route Match Summary

Decision:
- Emit a short `live_route_match_compact` summary after the full `live_route_match_done` line.
- Include only the operator-critical fields: pass/fail, frame and valid-match counts, progress range, endpoint/progress gates, confidence, telemetry health/drops, dry-run command quality, and live-output gate block reasons.

Why:
- Field runs produce hundreds of per-frame lines. A compact final line makes it easier to confirm whether a run failed due to route endpoint/progress, telemetry, dry-run command quality, or live-output safety gate state.

Impact:
- Existing detailed logs remain unchanged.
- Operators can inspect the final compact line first, then fall back to detailed frame logs only when needed.

Risk:
- The compact line is a summary only; detailed diagnosis still depends on the full `live_route_match_done` and frame logs.

## 2026-06-05 - Insert Pre-Live MAVLink Output Safety Readiness

Decision:
- Add Milestone 6.7 before the flight-test ladder for pre-live MAVLink output safety readiness.
- Keep live output blocked until a safety plan, operator checklist, audit log policy, single-writer ownership, stop/kill behavior, and failure handling are documented and reviewed.
- Define the first future live-output boundary as bench-only with propellers removed.
- Limit the first future command scope to yaw-rate only with zero forward velocity.
- Require at least three clean Pi dry-runs before changing any compile-time or runtime live-output blocker.

Why:
- The `jtzero` compact dry-run proved the current live camera, matcher, read-only telemetry, dry-run command quality, and safety-gate diagnostics are healthy enough to plan the next safety layer.
- It does not prove that a live MAVLink writer is safe to enable.

Impact:
- Milestone 7 is deferred until Milestone 6.7 is complete.
- The current compile-time CMake gate and fail-closed `LiveMavlinkBridge` remain unchanged.
- The next implementation work should be safety readiness, not live command output.

Risk:
- This delays live-output experimentation, but keeps the project aligned with the dry-run evidence actually collected.

## 2026-06-05 - Create Live Output Safety Plan

Decision:
- Add `docs/LIVE_OUTPUT_SAFETY_PLAN.md` as the controlling Milestone 6.7 safety artifact.
- Require the plan to define the current blocked boundary, first future bench props-off test boundary, readiness evidence, operator checklist, gate conditions, stop/failsafe policy, implementation readiness checklist, and completion criteria.
- Keep the first future writer scope limited to yaw-rate-only commands with zero forward speed.

Why:
- The roadmap needs an actionable safety artifact before any implementation can touch the live-output blockers.
- Keeping the plan separate from the roadmap avoids burying operator-critical constraints in milestone status text.

Impact:
- Future live-output implementation work has a required checklist and acceptance criteria.
- No code, CMake option, CLI, or Pi script behavior changes.
- Live MAVLink output remains blocked.

Risk:
- The plan is necessary but not sufficient; implementation still needs a separate reviewed change before any command output exists.

## 2026-06-05 - Add Readiness Log Checker

Decision:
- Add `scripts/check-live-readiness-log.sh` to validate `live_route_match_compact` lines against Milestone 6.7 readiness criteria.
- Require the current clean-run defaults: 150/150 frames, 150 valid matches, endpoint/progress gates passed, healthy read-only telemetry with zero dropped bytes, dry-run quality passed, zero live-output allowed frames, 150 blocked frames, and `vehicle_not_armed:150` gate reasons.
- Allow the expected gate reason to be overridden with `VISUAL_HOMING_EXPECTED_LIVE_OUTPUT_GATE_BLOCK_REASONS` for later reviewed stages.

Why:
- Milestone 6.7 requires three clean Pi dry-runs. Manual inspection of long logs is error-prone and easy to summarize inconsistently.

Impact:
- Operators can check one or more Pi logs with a single command.
- This is offline log validation only; it does not change live route matching, telemetry, command generation, CMake gates, or live-output blockers.

Risk:
- The checker only validates the compact summary line. Detailed diagnosis still requires the full run log when a check fails.

## 2026-06-05 - Track Readiness Evidence Without Rebuilding Old Route

Decision:
- Add `docs/LIVE_OUTPUT_READINESS_RECORD.md` to track the three clean Pi dry-run logs required by Milestone 6.7.
- Count `artifacts/logs/test-core-pi-20260604T205416Z.log` as 1/3 accepted readiness evidence because it passes `scripts/check-live-readiness-log.sh`.
- Do not recreate the dismantled physical route just to collect the remaining 2/3 logs.
- Collect the remaining logs only on a future stable route or repeatable no-yaw bench stand.

Why:
- The old physical route was intentionally treated as no longer critical after backup and validation.
- Readiness evidence should be honest about what was already proven without forcing fragile reconstruction of a dismantled setup.

Impact:
- Milestone 6.7 now has an explicit evidence ledger.
- Live output remains blocked.

Risk:
- The readiness count stays at 1/3 until a new comparable route or bench stand is available.

## 2026-06-05 - Enforce Zero Forward Speed In Live Output Safety Gate

Decision:
- Add `require_zero_forward_speed` to `LiveMavlinkOutputSafetyConfig`, defaulting to `true`.
- Block any nonzero `NavigationCommand::vx_mps` with `command_forward_speed_not_zero` before the general forward-speed bound is evaluated.
- Keep a tested opt-out path for later reviewed stages by setting `require_zero_forward_speed=false`, while still enforcing `max_abs_forward_speed_mps`.

Why:
- The first future live-output scope is yaw-rate-only with zero forward velocity. This must be a tested gate policy, not only a roadmap statement.

Impact:
- Current dry-runs with `VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_FORWARD_SPEED_MPS=0.0` remain unchanged.
- Any accidental nonzero forward command is blocked by the safety gate even if it is below the configured speed bound.
- Live MAVLink output remains blocked.

Risk:
- Later stages that intentionally test forward velocity must explicitly change this policy in a reviewed commit.

## 2026-06-05 - Require Ready Audit Log Before Live Output Gate Allows Commands

Decision:
- Add `audit_log_ready` to `LiveMavlinkOutputSafetyConfig`, defaulting to `false`.
- Keep `audit_log_enabled` as the policy/configuration flag and use `audit_log_ready` as the runtime readiness flag.
- Block with `audit_log_disabled` when audit logging is not enabled.
- Block with `audit_log_not_ready` when audit logging is enabled but not ready/open.

Why:
- A future live writer must not accept a command merely because audit logging is intended. The log must be ready before any command can pass the gate.

Impact:
- Dry-run diagnostics explicitly mark audit logging as ready so existing gate diagnostics still expose vehicle/match/command blockers.
- Unit tests cover both disabled and not-ready audit log states.
- Live MAVLink output remains blocked.

Risk:
- A future real audit logger still needs implementation; this only makes readiness a required safety-gate input.

## 2026-06-05 - Add Non-Live Live Output Audit Log Boundary

Decision:
- Add `LiveMavlinkOutputAuditLog` as a small append-capable file audit boundary for future live-output writer integration.
- Make the audit log fail-closed: empty log paths do not start, readiness becomes true only after the start line is opened and flushed, command records before readiness throw, and `stop()` writes a final stop reason before clearing readiness.
- Include safety decision reason and command fields in command audit records.
- Keep this independent from `LiveMavlinkBridge`; this change does not enable MAVLink command output.

Why:
- The safety gate now has an `audit_log_ready` precondition, so the project needs a testable boundary that can later drive that readiness bit from real file state instead of a placeholder.

Impact:
- The CTest suite gains a focused audit-log boundary test.
- Future writer work can require `audit.ready()` before constructing an allowing gate snapshot.
- Live MAVLink output remains blocked.

Risk:
- The audit format is intentionally minimal key-value text for now; future bench tests may add run metadata fields before live-output use.

## 2026-06-05 - Add Non-Live Live Output Session Scaffold

Decision:
- Add `LiveMavlinkOutputSession` as a writer-shaped coordinator that starts audit logging first, starts the bridge only after audit readiness, evaluates `LiveMavlinkOutputSafetyGate` for each snapshot, records every safety decision to audit, and calls `bridge.send()` only after an allowed safety result.
- Introduce `LiveMavlinkOutputAuditSink` so the session can depend on the audit boundary contract while tests use a fake sink and the file audit log remains the concrete implementation.
- Keep `LiveMavlinkBridge` compiled out and unavailable; session startup with the real live bridge fails and records `bridge_start_failed`.

Why:
- The next Milestone 6.7 step needs the exact future writer ordering without enabling real MAVLink output: audit readiness first, safety gate second, bridge send last.

Impact:
- Tests now prove stopped sessions reject processing, failed/not-ready audit prevents bridge startup, blocked safety decisions are audited without sending, allowed decisions can reach a dry-run bridge, and the real live bridge remains unable to start.
- Live MAVLink output remains blocked.

Risk:
- This is still an orchestration scaffold, not the reviewed live writer. Later work must add runtime CLI controls, final audit metadata, stop/failsafe integration, and bench-only procedures before any live-output blocker changes.

## 2026-06-05 - Add Real-File Session Audit Smoke Test

Decision:
- Add a `LiveMavlinkOutputSession` smoke test that uses the real `LiveMavlinkOutputAuditLog` with `DryRunCommandSink`.
- The test starts a session, processes one blocked snapshot and one allowed snapshot, verifies the dry-run bridge only receives the allowed command, stops the session, and checks the real audit file for start, blocked command, allowed command, and stop records.

Why:
- The fake-audit session tests prove ordering and edge cases, but Milestone 6.7 also needs proof that the real file audit boundary works through the session path before any live-output writer is considered.

Impact:
- The CTest suite now validates real audit-file contents through the non-live session coordinator.
- Live MAVLink output remains blocked.

Risk:
- The test uses a temporary local file only; future bench tests still need a configured artifact path and richer run metadata.

## 2026-06-05 - Add Optional Pi Live-Route Session Audit Artifact

Decision:
- Add opt-in live-route dry-run session audit support behind `VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT=1`.
- `test-core-pi.sh` writes the audit file to `VISUAL_HOMING_LIVE_ROUTE_SESSION_AUDIT_PATH` or a timestamped path under `artifacts/logs/`.
- Require the full existing safety dry-run shape before enabling the artifact path: dry-run commands, live read-only MAVLink telemetry, and explicit camera target dimensions.
- Route matching still computes its existing live-output gate diagnostics directly and additionally runs the same snapshot through `LiveMavlinkOutputSession` with a real file audit log and dry-run bridge. If the two safety results diverge, the run fails.

Why:
- CTest validates the session and audit file behavior with synthetic snapshots. The Pi readiness path also needs an optional real artifact generated during live camera matching without enabling live MAVLink output.

Impact:
- Operators can collect a live-route session audit file from the same Pi dry-run that produces `live_route_match_compact`.
- Default runs are unchanged unless the new env flag is set.
- Live MAVLink output remains blocked.

Risk:
- The positional active-profile CLI only accepts this audit pair in the full target-override plus live-telemetry form used by the current readiness command; broader CLI cleanup should happen before adding more optional groups.

## 2026-06-06 - Extend Live Output Audit Command Contract

Decision:
- Change `LiveMavlinkOutputAuditSink::record_command` to accept the full `LiveMavlinkOutputSafetySnapshot` instead of only `NavigationCommand`.
- Keep the existing command audit fields for backward readability and add `decision=allowed|blocked`, telemetry heartbeat/armed/mode/age fields, and route-match validity/confidence/progress/age fields.
- Extend `scripts/check-live-session-audit-log.sh` with expected allowed and blocked command counts while keeping the current default of 150 blocked `vehicle_not_armed` records for existing readiness evidence.

Why:
- The first bench props-off live-output run must be auditable as a safety decision, not just as a command value. A reviewer needs to see what telemetry and route-match state existed at the moment a command was blocked or, in the future, allowed.

Impact:
- Existing dry-run-blocked Pi audit logs remain checkable with the default script settings.
- Future bench props-off runs can require explicit allowed/blocked command counts and can use `VISUAL_HOMING_EXPECTED_LIVE_SESSION_AUDIT_REASON='*'` when a mixed allowed/blocking reason set is intentional.
- Live MAVLink output remains blocked.

Risk:
- The audit format is still key-value text. Future tooling should parse it conservatively and avoid treating the order of fields as stable.

## 2026-06-06 - Add Fail-Closed Bench Props-Off Pi Wrapper

Decision:
- Add `scripts/run-live-output-bench-props-off-pi.sh` as the dedicated Phase 6 Pi command wrapper for the bench props-off live-output boundary.
- Require the exact `VISUAL_HOMING_LIVE_OUTPUT_BENCH_PROPS_OFF_CONFIRM=I_UNDERSTAND_PROPS_ARE_REMOVED` string before running.
- Route the run through the runtime live-output gate path and automatically check the main readiness log plus session audit log after completion.
- Default the expected result to `allowed=0 blocked=150 reason=live_output_unavailable` because no concrete live MAVLink writer exists.

Why:
- The bench command must be visually and operationally separate from the old readiness dry-run command. A wrapper avoids copy/paste drift while preserving the current fail-closed boundary.

Impact:
- Operators have one explicit command for the reviewed bench props-off boundary.
- The wrapper will fail if the run unexpectedly allows writer decisions before the writer phase is reviewed.
- Live MAVLink output remains blocked.

Risk:
- The wrapper still depends on a stable current-room route and operator movement matching the recorded route. A route/progress failure remains a validation failure, not a writer failure.

## 2026-06-06 - Start Live-Output Session Timing At Capture Phase

Decision:
- Start the live-output session audit after camera warmup, operator cue, and pending-frame drain, immediately before the live route matching capture loop.
- Keep audit readiness as a pre-command requirement, but do not count warmup/countdown time against `VISUAL_HOMING_LIVE_OUTPUT_MAX_SECONDS`.

Why:
- The bench props-off wrapper uses a 10 second command window and also has a 10 second operator cue. Starting the session before the cue made the first command frame hit `max_duration_reached`, while the direct gate diagnostics correctly reported `live_output_unavailable`.

Impact:
- The session max-duration limit now measures the live command-attempt phase.
- Session audit and direct gate diagnostics stay aligned for the fail-closed bench wrapper.
- Live MAVLink output remains blocked.

Risk:
- Audit files start closer to the first command attempt, so pre-capture warmup/cue context remains in the main run log rather than the session audit log.

## 2026-06-06 - Require Directional Progress For Runtime Live Output

Decision:
- When live-output runtime controls are provided, require `directional_progress_passed=true` in addition to the selected progress gate.
- Keep legacy readiness dry-runs unchanged when runtime live-output controls are not provided.

Why:
- Endpoint-only progress can pass when the route reaches an endpoint but includes too many local forward regressions. That is acceptable as diagnostic evidence for some manual dry-runs, but too weak for any future writer-enabled live-output boundary.

Impact:
- The bench props-off wrapper now fails if forward/reverse directional progress exceeds the configured regression or rollback limits, even when endpoint progress passes.
- Existing non-runtime readiness audit evidence remains interpretable under the old policy.
- Live MAVLink output remains blocked.

Risk:
- Manual bench passes may need steadier movement or explicit directional thresholds before the wrapper passes again.

## 2026-06-06 - Accept Fail-Closed Bench Props-Off Runtime Evidence

Decision:
- Accept the `jtzero` bench props-off runtime-controlled fail-closed run at commit `d355bf1` as Phase 6 evidence.
- Record both the main run log and session audit log in `docs/LIVE_OUTPUT_READINESS_RECORD.md`.

Why:
- The run used the dedicated props-off wrapper after directional progress became mandatory for runtime live-output controls.
- The wrapper proved the current boundary is operationally fail-closed: route matching, telemetry health, dry-run command quality, readiness checking, and session audit checking all passed while all 150 live-output decisions remained blocked with `live_output_unavailable`.

Impact:
- The fail-closed runtime path is now documented evidence, not just terminal output.
- The next implementation step can focus on the reviewed concrete writer boundary while keeping sends disabled until the writer phase explicitly changes availability and is revalidated.
- Live MAVLink output remains blocked.

Risk:
- This evidence proves the unavailable-writer boundary only. It does not prove a concrete writer implementation, serial send behavior, or any flight behavior.

## 2026-06-07 - Add Bench-Only Serial MAVLink Writer Boundary

Decision:
- Add `LiveMavlinkSerialCommandWriter` behind the existing `LiveMavlinkCommandWriter` interface.
- Add an injectable `LiveMavlinkByteTransport` plus a POSIX serial transport backend.
- Encode unsigned MAVLink2 `SET_POSITION_TARGET_LOCAL_NED` messages in `MAV_FRAME_BODY_NED` with position, velocity, acceleration, and yaw ignored by the type mask, zero-filled velocity fields, and yaw-rate as the only active command authority.
- Keep `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0` and do not attach the writer to the runtime live-route session yet.

Why:
- The concrete writer should be reviewable and testable before it can be selected by the live route runtime path.
- Memory-transport tests can validate frame shape, sequence increments, start/stop behavior, and command rejection without opening a serial device or sending bytes to hardware.

Impact:
- The project now has a concrete serial writer library boundary for the bench props-off phase.
- Runtime Pi wrapper behavior is intentionally unchanged: it should still fail closed with `allowed=0 blocked=150 reason=live_output_unavailable` until the explicit attach/availability phase.
- Live MAVLink output remains blocked.

Risk:
- The frame encoder and POSIX byte writer are unit-tested, but no flight-controller acceptance test has been run. The next phase must attach this only through the existing audit/session/safety gate and re-run the props-off wrapper before any send-enabled bench attempt.
