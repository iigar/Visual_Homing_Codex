# Decisions

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
