# Project Memory

## Stable Context

- Project goal: build an independent StabX-like coarse GPS-denied visual return system.
- Target hardware: Raspberry Pi Zero 2W class companion computer, Pi Camera or thermal camera, Matek H743 Slim V3 / ArduPilot flight controller.
- Repository strategy: keep the imported baseline under `reference/` and build the new flight-critical system under `core/`.
- Core implementation direction: C++ replay-first architecture with deterministic timing, bounded queues, and explicit health/failsafe state.
- Current validated core baseline: replay pipeline plus route signature v1, coarse matching, initial navigation command model, dry-run MAVLink boundary, and hardware capture boundary has `ReplayFrameSource`, `Gray8ResizePreprocessor`, `HealthMonitor`, `PipelineHarness`, binary `VHRS` route signature writer/reader, route inspection summary, stateless route self-match artifact check, stateless route perturbation artifact check, offline route distinctiveness diagnostics, `RouteSignatureRecorder`, `Gray8RouteMatcher`, `BoundedNavigator` with yaw-rate slew limiting, `DryRunCommandSink`, `DryRunMavlinkBridge`, `MavlinkTelemetryAdapter` with heartbeat freshness plus armed/Guided permission gates, `PiCameraSource` with initial libcamera backend behind compile-time `VISUAL_HOMING_ENABLE_LIBCAMERA` and runtime `enable_live_capture`, active camera profiles, live camera route recording/matching, read-only MAVLink telemetry capture/inspection/validation/streaming, live dry-run command quality metrics, live-output safety-gate diagnostics, default zero-forward-speed live-output gate policy, non-live live-output audit-log boundary, non-live live-output session scaffold, real-file session audit smoke coverage, opt-in Pi live-route session audit artifact support, route-quality/readiness log checker support, audit-log checker support, UTC wall-clock operator logging, MSVC/CMake build support, Raspberry Pi bootstrap/test scripts validated on Pi Zero 2W class hardware, and 22 passing CTest tests. The latest `jtzero` compact live-route dry-run validated 150/150 live matches, strict monotonic forward progress, endpoint/progress gates, read-only telemetry health with zero dropped bytes, 150/150 valid dry-run commands, expected live-output blocking with `vehicle_not_armed:150`, and a session audit artifact with 150 blocked command records.
- Near-term priority: keep live MAVLink output blocked while implementing only the reviewed plan for the first bench props-off live-output boundary. Phase 1 split the compile-time boundary: default builds keep output disabled, `VISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=ON` is invalid without `VISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=ON`, and even the paired bench-scope build keeps `VISUAL_HOMING_LIVE_MAVLINK_OUTPUT_AVAILABLE=0` until a writer phase changes it. Phase 2 added an injectable `LiveMavlinkCommandWriter` interface behind `LiveMavlinkBridge` with fake-writer tests, but no concrete serial MAVLink writer exists. `docs/LIVE_OUTPUT_SAFETY_PLAN.md` is the controlling safety artifact and `docs/LIVE_OUTPUT_BENCH_PROPS_OFF_PLAN.md` defines the next implementation scope: bench-only, propellers removed, yaw-rate-only, zero forward velocity, explicit compile-time/runtime/operator gates, audit readiness, single-writer ownership, telemetry freshness, route-match freshness/confidence, command quality, and hard max command/duration limits. `docs/LIVE_OUTPUT_READINESS_RECORD.md` records 3/3 clean Pi dry-runs. Remaining non-live work includes timestamp-aligned continuous MAVLink telemetry integration, an HTTP wrapper over the profile registry, altitude-aware route metadata, thermal normalization/profile support, and then implementing the reviewed bench props-off writer boundary without enabling flight. Milestone 7 flight ladder is deferred until the bench props-off live-output boundary is separately implemented and reviewed.
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
