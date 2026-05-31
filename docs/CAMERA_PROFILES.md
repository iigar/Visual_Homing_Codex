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

`--match-route` can use a camera profile inline for replay matching:

```bash
visual_homing_core --match-route <route.vhrs> <manifest.csv> <width> <height> <window_radius> <minimum_confidence> <max_direction_shift_px> <profile_id> <capture_width> <capture_height> <horizontal_fov_rad> <vertical_fov_rad>
```

In that form, the matcher uses `horizontal_fov_rad / target_width` for horizontal direction-error scaling. The older `radians_per_pixel` CLI remains available for compatibility and focused tests.

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
- A small Pi API exposes available profiles and the active profile:
  - `GET /api/camera-profiles`;
  - `GET /api/camera-profiles/current`;
  - `POST /api/camera-profiles/current`.
- The Android app shows the available profiles in Settings and sends only the selected profile id to the Pi.

The reference Android codebase under `reference/` is a useful starting point because it already has Kotlin/Compose screens, Retrofit API plumbing, Pi URL preferences, and a Settings screen. The old APK artifact should not be treated as a maintainable base. The production path should reuse source patterns from the reference app only after the Pi profile-file and API contract exists.
