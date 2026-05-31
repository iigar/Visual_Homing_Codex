# Camera Profiles

Camera profiles describe how a specific camera should be interpreted by the deterministic core. They are required before moving from bench captures toward StabX-like longer-route behavior because direction error, route quality, and future altitude/scale logic depend on camera FOV and preprocessing assumptions.

## Current Fields

The initial in-core `CameraProfile` contains:

- `id`: stable profile identifier.
- `sensor_type`: `Visible`, `Thermal`, or `Other`.
- `pixel_format`: current capture/signature format such as `Gray8` or future `Thermal16`.
- `capture_width`, `capture_height`: camera frame dimensions.
- `target_width`, `target_height`: deterministic preprocessed signature size.
- `horizontal_fov_rad`, `vertical_fov_rad`: operator-supplied camera FOV.
- route-quality thresholds:
  - `low_texture_range_threshold`;
  - `ambiguous_mean_abs_diff_threshold`;
  - `maximum_low_texture_fraction`;
  - `maximum_ambiguous_nearest_fraction`;
  - `minimum_average_nearest_mean_abs_diff`.

The core derives:

- radians per capture pixel on X/Y axes;
- radians per target pixel on X/Y axes.

## File Format

Camera profile files use a strict dependency-free `key = value` text format. Blank lines and `#` comments are ignored. Unknown keys, malformed lines, invalid enum values, missing required fields, and invalid numeric ranges are rejected.

Required keys:

```text
id = imx219-visible-wide
capture_width = 320
capture_height = 240
target_width = 32
target_height = 24
horizontal_fov_rad = 1.08
vertical_fov_rad = 0.83
```

Optional keys:

```text
sensor_type = Visible
pixel_format = Gray8
low_texture_range_threshold = 4.0
ambiguous_mean_abs_diff_threshold = 2.0
maximum_low_texture_fraction = 0.05
maximum_ambiguous_nearest_fraction = 0.10
minimum_average_nearest_mean_abs_diff = 5.0
```

Supported `sensor_type` values are `Visible`, `Thermal`, and `Other`. Supported `pixel_format` values are `Gray8`, `Bgr8`, and `Thermal16`.

The first tracked profile template is:

```text
config/camera_profiles/imx219-visible-wide.profile
```

Inspect a profile without touching camera hardware:

```bash
visual_homing_core --inspect-camera-profile <camera.profile>
```

List all valid profiles in a directory:

```bash
visual_homing_core --list-camera-profiles <profile_dir>
```

The active profile is stored as a small operator-state file containing the selected profile id. The setter validates that the requested profile exists and can be loaded before writing the active state:

```bash
visual_homing_core --set-active-camera-profile <profile_dir> <active_profile_state> <profile_id>
visual_homing_core --get-active-camera-profile <profile_dir> <active_profile_state>
```

The active profile state should be untracked local state, for example `artifacts/active_camera_profile.txt`, not a committed file.

`--match-route` can use a camera profile inline for replay matching:

```bash
visual_homing_core --match-route <route.vhrs> <manifest.csv> <width> <height> <window_radius> <minimum_confidence> <max_direction_shift_px> <profile_id> <capture_width> <capture_height> <horizontal_fov_rad> <vertical_fov_rad>
```

In that form, the matcher uses `horizontal_fov_rad / target_width` for horizontal direction-error scaling. The older `radians_per_pixel` CLI remains available for compatibility and focused tests.

Replay matching can also load a profile file directly:

```bash
visual_homing_core --match-route-profile <route.vhrs> <manifest.csv> <width> <height> <window_radius> <minimum_confidence> <max_direction_shift_px> <camera.profile>
```

The profile `target_width` and `target_height` must match the CLI target dimensions.

## Example Visible Camera Profile

```text
id: imx219-visible-wide
sensor_type: Visible
pixel_format: Gray8
capture_width: 320
capture_height: 240
target_width: 32
target_height: 24
horizontal_fov_rad: <measure-or-configure>
vertical_fov_rad: <measure-or-configure>
maximum_low_texture_fraction: 0.05
maximum_ambiguous_nearest_fraction: 0.10
minimum_average_nearest_mean_abs_diff: 5.0
```

## Example Thermal Placeholder

```text
id: thermal-placeholder
sensor_type: Thermal
pixel_format: Thermal16
capture_width: 256
capture_height: 192
target_width: 32
target_height: 24
horizontal_fov_rad: <measure-or-configure>
vertical_fov_rad: <measure-or-configure>
normalization: pending deterministic Thermal16 policy
```

Thermal profiles are not enough by themselves. Thermal capture needs deterministic normalization before matching thresholds are meaningful.

## Policy

- FOV must be explicit per supported camera.
- FOV-derived radians-per-pixel should replace ad hoc direction scaling.
- Camera profiles are configuration/calibration data, not live flight permission.
- A route artifact can pass camera/profile validation and still be rejected by route-quality or confidence gates.

## Companion App Selection

The Android companion app should treat camera profiles as Pi-owned configuration, not as phone-owned flight state.

Planned flow:

- The Pi stores profile files under a deterministic config directory, for example `config/camera_profiles/`.
- The core validates profile contents before using them for capture, route matching, or route-quality policy.
- The Pi stores the active profile id in an untracked state file after validating the selected profile.
- A small Pi API exposes available profiles and the active profile:
  - `GET /api/camera-profiles`;
  - `GET /api/camera-profiles/current`;
  - `POST /api/camera-profiles/current`.
- The Android app shows the available profiles in Settings and sends only the selected profile id to the Pi.

The reference Android codebase under `reference/` is a useful starting point because it already has Kotlin/Compose screens, Retrofit API plumbing, Pi URL preferences, and a Settings screen. The old APK artifact should not be treated as a maintainable base. The production path should reuse source patterns from the reference app only after the Pi profile-file and API contract exists.
