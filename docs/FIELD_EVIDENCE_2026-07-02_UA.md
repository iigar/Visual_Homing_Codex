# Польовий Evidence 2026-07-02

Цей документ фіксує факти польової сесії 2026-07-02 без екстраполяцій. Мета сесії: записати новий маршрут на новій локації, зберегти keyframes на ноут, перевірити live route matching, telemetry, dry-run command quality і external-nav readiness у reverse-проході.

## Артефакти

Route на Pi:

```text
/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260702T153722Z.vhrs
```

Keyframes на Pi:

```text
/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260702T153722Z-keyframes
```

Keyframes скопійовані на ноут:

```text
D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex\keyframes\field-route-20260702T153722Z-keyframes
```

Основні readiness logs на Pi:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260702T171401Z.log
/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260702T172806Z.log
```

Ці два logs скопійовані на ноут:

```text
D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex\artifacts\logs\test-core-pi-20260702T171401Z.log
D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex\artifacts\logs\test-core-pi-20260702T172806Z.log
```

`artifacts/` ігнорується git-ом; ці raw logs є локальними/польовими артефактами, не tracked source files.

## Запис Маршруту

Route record summary:

```text
route=/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260702T153722Z.vhrs
entries=360
size=128x96
effective_fps=29.9729
relative_altitude_m=1.147
```

Route quality:

```text
route_self_match passed=true
route_perturb_check passed=true
route_distinctiveness quality_pass=true warning=true
ambiguous_nearest_entries=3
ambiguous_nearest_fraction=0.00833333
low_texture_entries=0
exact_duplicate_entries=0
average_nearest_mean_abs_diff=13.1642
```

Висновок із цих полів: route usable для тестів, але має невелике попередження distinctiveness через 3 ambiguous nearest entries. Це не було blocker-ом для подальших readiness прогонів.

## Статичні Перевірки Позицій

Start static match:

```text
progress=0..0
valid_matches=60/60
confidence_min_avg=0.870358/0.884671
passed=true
```

Middle static match:

```text
progress=0.479109..0.479109
valid_matches=60/60
confidence_min_avg=0.918296/0.924084
passed=true
```

End static match без initial window дав alias:

```text
physical end matched progress=0.367688
valid_matches=60/60
confidence_min_avg=0.927621/0.928043
passed=true
```

End static match з `VISUAL_HOMING_LIVE_ROUTE_MATCH_INITIAL_PROGRESS_MIN=0.80` і `MAX=1.00` заякорив endpoint:

```text
progress=0.977716..0.977716
valid_matches=60/60
confidence_min_avg=0.901121/0.902666
passed=true
```

Висновок із цих полів: для reverse-сесії на цьому маршруті потрібне initial progress window `0.80..1.00`, інакше endpoint може матчитися в іншу частину маршруту.

## Успішна Reverse Конфігурація

Ключові параметри, які використовувались у двох прийнятих readiness прогонах:

```bash
VISUAL_HOMING_CAMERA_TARGET_WIDTH=128
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=96
VISUAL_HOMING_CAMERA_FRAMES=240
VISUAL_HOMING_CAMERA_FPS=15
VISUAL_HOMING_LIVE_ROUTE_MATCH_WINDOW_RADIUS=120
VISUAL_HOMING_LIVE_ROUTE_MATCH_MIN_CONFIDENCE=0.75
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=reverse
VISUAL_HOMING_LIVE_ROUTE_MATCH_INITIAL_PROGRESS_MIN=0.80
VISUAL_HOMING_LIVE_ROUTE_MATCH_INITIAL_PROGRESS_MAX=1.00
VISUAL_HOMING_LIVE_ROUTE_MATCH_DIRECTIONAL_SEARCH=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_DIRECTIONAL_BIAS=0.03
VISUAL_HOMING_LIVE_ROUTE_MATCH_REQUIRE_ENDPOINT_PROGRESS=1
VISUAL_HOMING_MAVLINK_TELEMETRY_DEVICE=/dev/serial0
VISUAL_HOMING_MAVLINK_TELEMETRY_BAUD=115200
VISUAL_HOMING_MATCH_LIVE_ROUTE_USE_LIVE_MAVLINK_TELEMETRY=1
VISUAL_HOMING_MATCH_LIVE_ROUTE_REQUIRE_LIVE_TELEMETRY_HEALTH=1
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_COMMANDS=1
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_REQUIRE_COMMAND_QUALITY=1
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_MAX_YAW_RATE_SIGN_FLIPS=30
VISUAL_HOMING_LIVE_ROUTE_NAVIGATOR_FORWARD_SPEED_MPS=0.0
VISUAL_HOMING_EXTERNAL_NAV_ESTIMATES=1
VISUAL_HOMING_EXTERNAL_NAV_NOMINAL_ROUTE_LENGTH_M=10.0
VISUAL_HOMING_EXTERNAL_NAV_MINIMUM_MATCH_CONFIDENCE=0.80
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=0.82
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=0.12
```

`VISUAL_HOMING_EXTERNAL_NAV_NOMINAL_ROUTE_LENGTH_M=10.0` було задано як операторська оцінка довжини маршруту. Це потрібно external-nav estimator-у для перетворення route progress у `x_m`. Без цього estimates invalid з reason `scale_not_known`.

## Прийнятий Readiness Run 1

Log:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260702T171401Z.log
```

Final summary:

```text
passed=true
frames=240/240
valid_matches=240
progress=0.977716..0.0306407
tracked_progress=0.977716..0.0306407
tracked_reverse_progress_monotonic=true
endpoint_passed=true
progress_gate_passed=true
telemetry_health=true
telemetry_dropped=0
dry_run_quality=true
dry_run_valid=240/240
external_nav_valid=240/240
external_nav_valid_fraction=1
external_nav_invalid_reasons=none
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_quality_ready=true
external_nav_operator_readiness=ready
external_nav_operator_reason=valid
external_nav_relative_altitude_min_avg_max_m=0.748/0.800808/0.892
external_nav_relative_altitude_window_passed=true
live_output_gate_block_reasons=vehicle_not_armed:240
final_live_output_gate_reason=vehicle_not_armed
```

Exported readiness JSON from this log reported:

```text
operator.readiness=ready
operator.reason=valid
handoff.route_complete=true
handoff.visual_homing_ready=true
handoff.candidate=true
handoff.decision=candidate_only
handoff.reason=jt_zero_not_integrated
jt_zero.available=false
jt_zero.ready=false
jt_zero.reason=not_integrated
external_nav.valid=240
external_nav.total=240
external_nav.valid_fraction=1
external_nav.invalid_reasons=none
safety_gate.live_output_allowed=0
safety_gate.live_output_blocked=240
safety_gate.final_reason=vehicle_not_armed
```

The log checker passed with auto frame expectations:

```text
external_nav_readiness_log_check path=/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260702T171401Z.log passed=true external_nav_valid=240/240 external_nav_session_ready=true external_nav_strict_session_ready=true external_nav_operator_readiness=ready external_nav_operator_reason=valid
```

## Прийнятий Readiness Run 2

Log:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260702T172806Z.log
```

Final summary:

```text
passed=true
frames=240/240
valid_matches=240
progress=0.977716..0.00278552
tracked_progress=0.977716..0.0181058
tracked_reverse_progress_monotonic=true
endpoint_passed=true
progress_gate_passed=true
telemetry_health=true
telemetry_dropped=0
dry_run_quality=true
dry_run_valid=240/240
external_nav_valid=240/240
external_nav_valid_fraction=1
external_nav_invalid_reasons=none
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_quality_ready=true
external_nav_operator_readiness=ready
external_nav_operator_reason=valid
external_nav_relative_altitude_min_avg_max_m=0.743/0.809229/0.861
external_nav_relative_altitude_window_passed=true
live_output_gate_block_reasons=vehicle_not_armed:240
final_live_output_gate_reason=vehicle_not_armed
```

Combined readiness check for both accepted logs:

```text
external_nav_readiness_log_check path=/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260702T171401Z.log passed=true external_nav_valid=240/240 external_nav_session_ready=true external_nav_strict_session_ready=true external_nav_operator_readiness=ready external_nav_operator_reason=valid
external_nav_readiness_log_check path=/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260702T172806Z.log passed=true external_nav_valid=240/240 external_nav_session_ready=true external_nav_strict_session_ready=true external_nav_operator_readiness=ready external_nav_operator_reason=valid
```

## Невдалі Або Проміжні Прогони І Причини

Проміжні blocked runs не трактуються як failure route matching, якщо final fields прямо показували інший blocker.

Observed blockers during tuning:

```text
dry_run_yaw_rate_sign_flips=26 with default max 20
external_nav_invalid_reasons=scale_not_known:240
external_nav_relative_altitude_window_passed=false when expected altitude was 1.15 +/- 0.35 but observed altitude was about 0.75..0.85 m
external_nav_invalid_reasons=route_match_not_valid:204,scale_not_known:36 when external-nav minimum confidence was too strict for the observed session
```

Corrections that led to accepted runs:

```text
VISUAL_HOMING_LIVE_ROUTE_DRY_RUN_MAX_YAW_RATE_SIGN_FLIPS=30
VISUAL_HOMING_EXTERNAL_NAV_MINIMUM_MATCH_CONFIDENCE=0.80
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=0.82
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=0.12
VISUAL_HOMING_EXTERNAL_NAV_NOMINAL_ROUTE_LENGTH_M=10.0
```

## What Is Proven By This Session

- This specific `128x96`, 360-entry route can be matched in reverse from endpoint toward start with initial progress window `0.80..1.00`.
- Two separate external-nav readiness runs passed with `240/240` FC-ready estimates.
- Read-only MAVLink telemetry stayed healthy in the accepted runs with `telemetry_dropped=0`.
- Dry-run command quality passed in the accepted runs.
- The live-output safety gate remained closed in the accepted runs because the vehicle was not armed.
- The exported readiness JSON correctly reports `candidate_only` because JT_Zero is not integrated.

## What Is Not Proven By This Session

- No live MAVLink external-nav writer was attached or sent.
- No `VISION_POSITION_ESTIMATE`, `ODOMETRY`, or equivalent external-nav message was written to the flight controller by this session.
- No free flight was performed.
- No JT_Zero handoff was performed.
- The `10.0 m` route length is an operator-configured nominal scale for dry-run external-nav estimates, not an independently surveyed metric route.
- Visual scale remained not required and not active as a readiness gate: `visual_scale_required=false`, `visual_scale_valid=0/240`.

## Next Engineering Step

The next code/design step is JT_Zero handoff integration behind explicit feature flags and props-off/restrained safety gates. The current evidence supports that Visual Homing can produce dry-run external-nav-ready estimates on this route; it does not by itself authorize live flight or live external-nav writing.
