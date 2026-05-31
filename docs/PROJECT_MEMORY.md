# Project Memory

## Stable Context

- Project goal: build an independent StabX-like coarse GPS-denied visual return system.
- Target hardware: Raspberry Pi Zero 2W class companion computer, Pi Camera or thermal camera, Matek H743 Slim V3 / ArduPilot flight controller.
- Repository strategy: keep the imported baseline under `reference/` and build the new flight-critical system under `core/`.
- Core implementation direction: C++ replay-first architecture with deterministic timing, bounded queues, and explicit health/failsafe state.
- Current validated core baseline: replay pipeline plus route signature v1, coarse matching, initial navigation command model, dry-run MAVLink boundary, and initial hardware capture boundary has `ReplayFrameSource`, `Gray8ResizePreprocessor`, `HealthMonitor`, `PipelineHarness`, binary `VHRS` route signature writer/reader, route inspection summary, stateless route self-match artifact check, stateless route perturbation artifact check, offline route distinctiveness diagnostics, `RouteSignatureRecorder`, `Gray8RouteMatcher`, `BoundedNavigator` with yaw-rate slew limiting, `DryRunCommandSink`, `DryRunMavlinkBridge`, `MavlinkTelemetryAdapter` with heartbeat freshness plus armed/Guided permission gates, `PiCameraSource` with initial libcamera backend behind compile-time `VISUAL_HOMING_ENABLE_LIBCAMERA` and runtime `enable_live_capture`, live camera pipeline smoke, live camera route recording, route artifact inspection, route self-match, route perturbation checks, and route distinctiveness diagnostics validated on `jtzero`, replay-to-route matching CLI with progress/confidence/direction error/health gate/command output metrics, MSVC/CMake build support, Raspberry Pi bootstrap/test scripts validated on Pi Zero 2W class hardware, and 14 passing CTest tests.
- Near-term priority: continue Milestone 6.5 before flight-test ladder work. Milestone 6.5 now has an initial in-core camera profile model with FOV-derived angular scale; remaining work includes profile file loading, read-only real MAVLink telemetry, altitude-aware route metadata, and thermal normalization/profile support. Milestone 6.6 must review the completed baseline for weak points, safety risks, and security/operational hardening before Milestone 7. The latest static table captures correctly report poor route distinctiveness, while a hand-carried 180-frame bench route improved to 8/180 ambiguous nearest entries with average nearest mean absolute byte difference ~7.44, passed structure/self-match/perturbation checks, and satisfies the initial route-quality policy with `quality_pass=true`. Live MAVLink output remains explicitly blocked and desktop builds must keep replay-first tests working and fail closed without camera backend.
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
