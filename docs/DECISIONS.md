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
