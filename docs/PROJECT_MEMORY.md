# Project Memory

## Stable Context

- Project goal: build an independent StabX-like coarse GPS-denied visual return system.
- Target hardware: Raspberry Pi Zero 2W class companion computer, Pi Camera or thermal camera, Matek H743 Slim V3 / ArduPilot flight controller.
- Repository strategy: keep the imported baseline under `reference/` and build the new flight-critical system under `core/`.
- Core implementation direction: C++ replay-first architecture with deterministic timing, bounded queues, and explicit health/failsafe state.
- Current validated core baseline: replay pipeline plus route signature v1, coarse matching, initial navigation command model, dry-run MAVLink boundary, and initial hardware capture boundary has `ReplayFrameSource`, `Gray8ResizePreprocessor`, `HealthMonitor`, `PipelineHarness`, binary `VHRS` route signature writer/reader, route inspection summary, stateless route self-match artifact check, stateless route perturbation artifact check, offline route distinctiveness diagnostics, `RouteSignatureRecorder`, `Gray8RouteMatcher`, `BoundedNavigator` with yaw-rate slew limiting, `DryRunCommandSink`, `DryRunMavlinkBridge`, `MavlinkTelemetryAdapter` with heartbeat freshness plus armed/Guided permission gates, `PiCameraSource` with initial libcamera backend behind compile-time `VISUAL_HOMING_ENABLE_LIBCAMERA` and runtime `enable_live_capture`, live camera pipeline smoke, live camera route recording with configurable warmup frame dropping and optional read-only MAVLink telemetry snapshot metadata, route artifact inspection/self-match/perturbation/distinctiveness validation including one-flag Pi validation, read-only MAVLink telemetry capture/inspection/validation, replay-to-route matching CLI with progress/confidence/direction error/health gate/command output metrics, UTC wall-clock operator logging for Pi validation runs, MSVC/CMake build support, Raspberry Pi bootstrap/test scripts validated on Pi Zero 2W class hardware, and 16 passing CTest tests.
- Near-term priority: continue Milestone 6.5 before flight-test ladder work. Milestone 6.5 now has an initial in-core camera profile model with FOV-derived angular scale, strict profile file loading, an initial tracked IMX219 profile template, profile inspection/list/get-active/set-active CLI, machine-readable JSON profile registry commands for future Pi API/Android selection, profile-backed camera smoke/live route recording commands, replay route matching can use inline/file profile FOV-derived direction scaling, an initial read-only MAVLink v1/v2 telemetry byte-stream inspector/capture/validation path validated on `jtzero` at 115200 baud with heartbeat, attitude, and relative altitude, route recording can attach the latest validated telemetry snapshot as altitude/heading metadata, and the future Android profile selector is defined as a thin UI over Pi-owned profile files/API state rather than phone-owned flight state. Remaining work includes continuous live MAVLink telemetry integration, an HTTP wrapper over the profile registry, altitude-aware route metadata, and thermal normalization/profile support. Milestone 6.6 must review the completed baseline for weak points, safety risks, and security/operational hardening before Milestone 7. Static table captures correctly report poor route distinctiveness, while hand-carried bench routes prove movement/texture sensitivity: one 180-frame route improved to 8/180 ambiguous nearest entries with average nearest mean absolute byte difference ~7.44 and `quality_pass=true`; a later 240-frame profile-backed route had only frames `0,1,2` reported as low-texture/duplicate/ambiguous, so route distinctiveness now supports explicit diagnostic edge trimming. Live MAVLink output remains explicitly blocked and desktop builds must keep replay-first tests working and fail closed without camera backend.
- StabX-like target remains coarse GPS-denied visual return, not metric SLAM or precision hover: long-distance behavior depends on camera/FOV profiles, attitude/altitude telemetry, altitude/scale-aware route metadata, thermal/visible camera profiles, route segmentation, reacquire behavior, and conservative confidence gates.
- Python/reference code is useful for documentation, tools, diagnostics, UI concepts, and comparison, but should not become the new flight-critical scheduler.
- Commit messages should be detailed and explain what changed, why, impact, validation, and risk.

## Working Rules

- Technical decisions go to `docs/DECISIONS.md`.
- Current progress goes to `docs/SESSION_LOG.md`.
- Stable project context goes to this file.
- Ideas and research notes go to `notes/`.
- Code changes are captured in detailed git commits.
- Generated build artifacts should stay out of git; prefer reproducible scripts/docs for local validation.
- Every completed task must be pushed to GitHub after the local commit and validation are complete.

## Session Startup Prompt

Use this when starting a new Codex session:

```text
Продовжуємо Visual_Homing_Codex. Перед роботою прочитай docs/PROJECT_MEMORY.md, docs/SESSION_LOG.md, docs/DECISIONS.md, docs/ROADMAP.md і git log -3.
```
