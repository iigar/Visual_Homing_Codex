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
