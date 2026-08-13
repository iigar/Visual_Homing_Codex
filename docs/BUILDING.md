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

The current registered suite contains `46` CTest tests. It covers replay/camera preprocessing, route signatures and bounded streaming, VHRM/VHIX package artifacts, verification selection/publication/composition, telemetry, route-local ODOMETRY, reset safety, dry-run/live-output boundaries, and matcher/navigation behavior.

Accepted baseline:

- WSL/GCC all-output-off: `46/46`;
- clean Pi Zero 2W/OV9281 all-output-off: `46/46` at commit `135942f`;
- affected MSVC 19.44/Ninja targets: passed.

Exact current evidence and limitations are summarized in `CURRENT_PROJECT_STATUS_UA.md`. Historical lower test counts in timestamped evidence documents describe those earlier commits and must not be rewritten as current failures.

Latest validation command:

```powershell
.\scripts\test-core.ps1 -Clean
```

The helper checks exit codes after configure, build, and CTest so stale test binaries cannot hide a failed build.
