# Roadmap

## Milestone 0 - Repository Baseline

- Import legacy project under `reference/`.
- Create clean C++ core skeleton.
- Document architecture direction.

## Milestone 1 - Replay Pipeline

- Define replay input format. Initial CSV manifest: `id,timestamp_ns,path`.
- Load image/video sequences with timestamps. Initial implementation supports PGM P5 Gray8 image sequences.
- Run preprocessing on small frames.
- Emit per-frame timing and health metrics.

## Milestone 2 - Visual Route Signature

- Store compact grayscale/thermal signatures.
- Add route metadata: frame id, time, altitude band, coarse heading hint.
- Keep format versioned and stream-friendly.

## Milestone 3 - Coarse Route Matching

- Match current frame against a route window.
- Estimate route progress, confidence, and direction error.
- Add offline tests with synthetic route perturbations.

## Milestone 4 - Navigation Command Model

- Convert route match into bounded course correction.
- Add command age, confidence gates, acceleration/yaw-rate limits.
- Define failsafe behavior for stale or low-confidence matches.

## Milestone 5 - MAVLink Integration

- Read ArduPilot heartbeat, attitude, altitude, and mode.
- Send commands from a single writer.
- Add dry-run and guided-command modes.

## Milestone 6 - Hardware Capture

- Add Pi camera source.
- Add USB/thermal source.
- Validate CPU, memory, frame drops, latency on Pi Zero 2W.

## Milestone 7 - Flight Test Ladder

- Bench replay.
- Tethered/ground test.
- 50 m return.
- 100-300 m return.
- 1 km+ only after replay logs show stable behavior.
