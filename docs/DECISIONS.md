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
