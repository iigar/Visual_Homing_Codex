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

## Current Validation

Validated on Windows with:

- MSVC 19.44.35227
- CMake 3.31.6-msvc6
- CTest 3.31.6-msvc6

Expected CTest coverage:

- `gray8_resize_preprocessor`
- `replay_frame_source`
- `health_monitor`
- `pipeline_harness`
- `route_signature`
- `route_signature_recorder`

Latest validation command:

```powershell
.\scripts\test-core.ps1 -Clean
```
