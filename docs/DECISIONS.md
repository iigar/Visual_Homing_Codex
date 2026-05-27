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
