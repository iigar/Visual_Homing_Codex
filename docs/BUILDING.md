# Building

## Windows MSVC

The validated local setup is Visual Studio Build Tools 2022 with the C++ workload and CMake tools.

Activate the Visual Studio developer shell from PowerShell:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1"
```

Configure, build, and test the core:

```powershell
cmake -S core -B core/build
cmake --build core/build
ctest --test-dir core/build -C Debug --output-on-failure
```

Or run the repository helper:

```powershell
.\scripts\test-core.ps1
```

Use `-Clean` to remove the previous build directory first:

```powershell
.\scripts\test-core.ps1 -Clean
```

## Raspberry Pi

The Pi build path is documented separately in `docs/PI_BUILDING.md`.

The short version on Raspberry Pi OS is:

```bash
./scripts/bootstrap-pi.sh
```

For repeat builds after bootstrap:

```bash
./scripts/test-core-pi.sh
```

The Pi script uses `core/build-pi` and enables `VISUAL_HOMING_ENABLE_LIBCAMERA=ON`. Live camera access remains runtime opt-in through explicit hardware validation commands.

## Current Validation

Validated on Windows with:

- MSVC 19.44.35227
- CMake 3.31.6-msvc6
- CTest 3.31.6-msvc6

Expected CTest coverage:

- `bounded_navigator`
- `camera_profile`
- `camera_smoke`
- `dry_run_command_sink`
- `dry_run_mavlink_bridge`
- `gray8_resize_preprocessor`
- `replay_frame_source`
- `health_monitor`
- `mavlink_telemetry_adapter`
- `pi_camera_source`
- `pipeline_harness`
- `route_signature`
- `route_signature_recorder`
- `gray8_route_matcher`

Latest validation command:

```powershell
.\scripts\test-core.ps1 -Clean
```

The helper checks exit codes after configure, build, and CTest so stale test binaries cannot hide a failed build.
