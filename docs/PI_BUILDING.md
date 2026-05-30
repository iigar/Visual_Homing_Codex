# Raspberry Pi Build

This document describes the intended Raspberry Pi build path for the C++ core.

The default desktop build remains replay-first and fail-closed. Pi camera capture is enabled only when the hardware backend is explicitly requested.

## Supported Target

- Raspberry Pi OS 64-bit or Debian-based Raspberry Pi OS.
- Raspberry Pi Zero 2W class hardware is the target performance class.
- Pi camera support is expected to use the Raspberry Pi libcamera stack.

## One-Time Bootstrap

Run this on the Pi from the repository root:

```bash
./scripts/bootstrap-pi.sh
```

The bootstrap script installs the expected system packages and then runs the Pi test script.

## Build And Test

Run:

```bash
./scripts/test-core-pi.sh
```

The script configures `core/build-pi` with:

```bash
-DCMAKE_BUILD_TYPE=Release
-DVISUAL_HOMING_ENABLE_LIBCAMERA=ON
```

Then it builds the core and runs CTest.

## Camera Smoke Test

After a real libcamera backend is implemented and camera hardware is attached, run:

```bash
VISUAL_HOMING_RUN_CAMERA_SMOKE=1 ./scripts/test-core-pi.sh
```

Optional camera smoke settings:

```bash
VISUAL_HOMING_CAMERA_WIDTH=320
VISUAL_HOMING_CAMERA_HEIGHT=240
VISUAL_HOMING_CAMERA_FPS=15
VISUAL_HOMING_CAMERA_FRAMES=30
```

The underlying CLI is:

```bash
./core/build-pi/visual_homing_core --pi-camera-smoke <width> <height> <fps> <frames>
```

Until the real libcamera backend exists, this command fails closed and reports that camera capture is unavailable.

## Hardware Backend Policy

- `VISUAL_HOMING_ENABLE_LIBCAMERA` defaults to `OFF`.
- Desktop and CI-style builds should leave it `OFF`.
- Pi builds may set it `ON`.
- Until the libcamera backend is implemented, `PiCameraSource` remains fail-closed and reports that capture is unavailable.
- Live loops must not do disk I/O.
- Every live frame must carry a monotonic timestamp from the core clock at capture receipt.
- Live ArduPilot command output remains blocked; camera validation is separate from flight command validation.

## Expected Validation Metrics

When the real backend is added, Pi validation should record:

- configured frame size and frame rate;
- frames captured;
- frames dropped;
- preprocessing latency;
- end-to-end frame age;
- CPU and memory use;
- whether `PiCameraSource` ever fails to deliver frames while running.
