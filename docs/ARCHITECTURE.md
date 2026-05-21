# Architecture

## Goal

Build a replay-first C++ core for coarse GPS-denied return on constrained hardware.

The system should prefer stable approximate homing over accumulated long-range metric visual odometry. A 400-500 m arrival radius is a different problem than meter-level RTL, so the architecture is optimized for robustness, low latency, and recoverability.

## Core Modules

- `CameraSource`: Pi camera, USB capture, or replay input.
- `FramePreprocessor`: resize, grayscale, contrast normalization, thermal normalization.
- `RouteRecorder`: compact route signatures and metadata.
- `RouteMatcher`: route progress and direction-error estimate.
- `Navigator`: bounded yaw/velocity correction.
- `MavlinkBridge`: single-writer ArduPilot telemetry and command interface.
- `HealthMonitor`: stale data, dropped frames, latency, confidence, failsafe state.
- `Logger/Replay`: deterministic reproduction of flights on a desktop.

## Realtime Rules

- No disk I/O in the camera or command loop.
- No unbounded queues in flight-critical paths.
- Every sensor sample and command carries a monotonic timestamp.
- MAVLink command output is single-writer.
- Web/UI reads immutable snapshots only.
- Route matching must report confidence and sample age.

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
