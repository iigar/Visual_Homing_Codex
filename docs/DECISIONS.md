# Decisions

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
