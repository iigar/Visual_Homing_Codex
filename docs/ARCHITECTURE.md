# Architecture

## Goal

Build a replay-first C++ core for coarse GPS-denied return on constrained hardware.

The system should prefer stable approximate homing over accumulated long-range metric visual odometry. A 400-500 m arrival radius is a different problem than meter-level RTL, so the architecture is optimized for robustness, low latency, and recoverability.

This core is an operator-in-the-loop command-assist system during the current safety stages, not an autonomous vehicle controller. The companion may compute route evidence and bounded command proposals, but ArduPilot remains responsible for primary stabilization, failsafe behavior, mode management, and motor mixing.

## Core Modules

- `CameraSource`: Pi camera, USB capture, or replay input.
- `FramePreprocessor`: resize, grayscale, contrast normalization, thermal normalization.
- `RouteRecorder`: compact route signatures and metadata.
- `RouteMatcher`: route progress and direction-error estimate.
- `Navigator`: bounded yaw/velocity correction.
- `MavlinkBridge`: single-writer ArduPilot telemetry and command interface.
- `HealthMonitor`: stale data, dropped frames, latency, confidence, failsafe state.
- `Logger/Replay`: deterministic reproduction of flights on a desktop.

Module boundaries should stay explicit and replaceable. Capture, preprocessing, route artifact I/O, matching, telemetry, navigation, command output, audit, and safety gates should be testable independently so future sensors or algorithms can be added without rewriting the realtime scheduler.

## Extension Points

- Additional cameras and capture paths: visible Pi camera, USB, thermal, or replay.
- Additional scale sources: barometer/rangefinder, optical-flow-like visual scale, VIO, UWB, or other reviewed sources.
- Alternative matchers: normalized MAD baseline, normalized/gradient/census-like descriptors, multi-scale matching, or later feature diagnostics behind explicit config and tests.
- Route-quality policies: terrain/profile-specific thresholds and diagnostics before readiness evidence.

The current Gray8/MAD matcher and `64x48` target are a deterministic baseline, not a claim of outdoor robustness. Higher altitude or range increases ground meters per pixel, can erase texture, and can make a route recorded at one altitude mismatch a route matched at another altitude.

## Realtime Rules

- No disk I/O in the camera or command loop.
- No unbounded queues in flight-critical paths.
- Every sensor sample and command carries a monotonic timestamp.
- MAVLink command output is single-writer.
- Web/UI reads immutable snapshots only.
- Route matching must report confidence and sample age.

## Trust Boundaries

- Route artifacts are inputs, not trusted authority. They must be version-checked, structurally validated, and eventually integrity-checked across record, validation, and match phases.
- MAVLink serial bytes are untrusted telemetry until parsed and gated. Malformed frames, stale heartbeat, wrong sysid/compid, wrong mode, or missing freshness must fail closed.
- MAVLink command output remains single-writer. No UI, script, or secondary module may write command bytes concurrently with the reviewed writer boundary.

## Reference Baseline

The imported baseline remains in `reference/Visual_Homing_System_Claude-conflict_250226_1330`.

Useful parts:

- ArduPilot parameter notes.
- Existing route recorder/follower ideas.
- Python MAVLink reference.
- UI and diagnostics concepts.

Prototype-only parts:

- Python camera callback as scheduler.
- Long-range VO by accumulated frame-to-frame displacement.
- C++ MAVLink stub.
- Web reads of live mutable flight state.
