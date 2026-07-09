# Польовий Evidence 2026-07-09 OV9281

Цей документ фіксує факти польової сесії 2026-07-09 без екстраполяцій. Мета сесії: перевірити Arducam/OV9281 wide mono camera після відновлення Pi, записати чистіший daylight route, перевірити forward/reverse live route matching, external-nav readiness, attach-only audit і параметри, потрібні для стабільного проходу.

Це hand-carried dry-run / attach-only evidence. Воно не є allowed-send, armed, tethered або flight evidence. External-nav output writer був attach-capable, але runtime send залишався disabled, а audit блокував кожну оцінку з `reason=runtime_disabled`.

## Артефакти

Route на Pi:

```text
/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260709T151301Z.vhrs
```

Keyframes на Pi:

```text
/home/pi/Visual_Homing_Codex/keyframes/field-route-20260709T151301Z-keyframes
```

Keyframes скопійовані на ноут:

```text
D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex\keyframes\field-route-20260709T151301Z-keyframes-png
```

Accepted readiness logs на Pi:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260709T155154Z.log
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260709T155405Z.log
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260709T155702Z.log
```

Accepted audit logs на Pi:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-audit-20260709T155154Z.log
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-audit-20260709T155405Z.log
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-audit-20260709T155702Z.log
```

Endpoint diagnostic log:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260709T154517Z.log
```

`artifacts/` і `keyframes/` є локальними/польовими артефактами і не є tracked source files.

## Камера Та Профіль

Активний профіль:

```text
profile_id=ov9281-160-wide
profile_path=/home/pi/Visual_Homing_Codex/config/camera_profiles/ov9281-160-wide.profile
capture=640x400
target=160x100
horizontal_fov_rad=2.79
vertical_fov_rad=2.18
```

Pi camera stack бачив OV9281 як mono camera:

```text
ov9281 [1280x800 10-bit MONO]
R8 modes include 640x400, 1280x720, 1280x800
```

FOV values у профілі залишаються nominal placeholders для 160-degree lens. Вони достатні для цього evidence як direction-scale input, але не є виміряною калібровкою.

## Запис Маршруту

Route record summary:

```text
route=/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260709T151301Z.vhrs
entries=300
size=160x100
effective_fps=30.0173
relative_altitude_m=1.133
```

Route inspect:

```text
first_frame_id=152
last_frame_id=451
size=160x100
min_payload_bytes=16000
max_payload_bytes=16000
min_altitude_band_m=1
max_altitude_band_m=1
timestamps_monotonic=true
uniform_dimensions=true
uniform_payload_size=true
uniform_altitude_band=true
uniform_heading_hint=false
all_gray8=true
```

Route quality:

```text
route_self_match passed=true
route_perturb_check passed=true
route_distinctiveness quality_pass=true warning=true
ambiguous_nearest_entries=9
ambiguous_nearest_fraction=0.03
low_texture_entries=0
exact_duplicate_entries=0
average_nearest_mean_abs_diff=10.9387
```

Висновок із цих полів: route usable для field readiness. Warning обмежений стартовими ambiguous nearest entries і не заблокував forward/reverse readiness при правильному tracking window.

## Важлива Корекція Endpoint

Під час тестів був операторський landmark mix-up: спочатку фізичним фінішем помилково вважалась інша цеглина/точка. Standing diagnostic у тому місці показав, що це середина маршруту:

```text
progress=0.458194..0.464883
external_nav_valid=90/90
operator_readiness=ready
confidence_min_avg=0.90438/0.904782
```

Standing diagnostic на справжньому endpoint із `end.png` підтвердив фініш маршруту:

```text
progress=0.949833..0.949833
tracked_progress=0.949833..0.949833
end_zone_gap_min_avg=0/0
external_nav_valid=90/90
external_nav_operator_readiness=ready
confidence_min_avg=0.894134/0.895114
```

Висновок: route endpoint у файлі коректний. Попередні підозри на "ранній endpoint" частково пояснювались помилковим фізичним орієнтиром, а частково дефолтним tracking window.

## Прийнята Конфігурація

Ключові параметри, які дали accepted forward/reverse evidence:

```bash
VISUAL_HOMING_ROUTE_OUTPUT=/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260709T151301Z.vhrs
VISUAL_HOMING_CAMERA_TARGET_WIDTH=160
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=100
VISUAL_HOMING_CAMERA_FRAMES=450
VISUAL_HOMING_LIVE_ROUTE_MATCH_WINDOW_RADIUS=120
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.94
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_START_PROGRESS=0.15
VISUAL_HOMING_MATCH_LIVE_ROUTE_TELEMETRY_MAX_AGE_MS=3000
VISUAL_HOMING_MAVLINK_MAX_MALFORMED_FRAMES=2
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=1
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=3
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=5
```

Critical finding: default `VISUAL_HOMING_LIVE_ROUTE_MATCH_WINDOW_RADIUS=30` can strand tracking around `0.65..0.67` on this route even while matches remain visually valid. `WINDOW_RADIUS=120` is the accepted setting for this OV9281 route.

## Прийнятий Forward Run 1

Log:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260709T155154Z.log
```

Final summary:

```text
passed=true
frames=222/450
valid_matches=222
progress=0.0200669..0.986622
tracked_progress=0.0200669..0.92286
tracked_directional_progress=true
endpoint_passed=true
progress_gate_passed=true
endpoint_stop=true
stop_reason=endpoint_progress_reached
telemetry_health=true
dry_run_quality=true
dry_run_valid=222/222
external_nav_valid=222/222
external_nav_valid_fraction=1
external_nav_invalid_reasons=none
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_quality_ready=true
external_nav_operator_readiness=ready
external_nav_operator_reason=valid
confidence_min_avg=0.893437/0.916294
external_nav_output_block_reasons=runtime_disabled:222
```

Audit checker:

```text
external_nav_output_audit_log_check path=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-audit-20260709T155154Z.log passed=true estimates=222 allowed=0 sent=0 blocked=222 reason=runtime_disabled stop_reason=endpoint_progress_reached
external_nav_output_attach_check passed=true
```

## Прийнятий Forward Run 2

Log:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260709T155405Z.log
```

Final summary:

```text
passed=true
frames=245/450
valid_matches=245
progress=0.0167224..0.943144
tracked_progress=0.0167224..0.940253
tracked_directional_progress=true
endpoint_passed=true
progress_gate_passed=true
endpoint_stop=true
stop_reason=endpoint_progress_reached
telemetry_health=true
dry_run_quality=true
dry_run_valid=245/245
external_nav_valid=245/245
external_nav_valid_fraction=1
external_nav_invalid_reasons=none
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_quality_ready=true
external_nav_operator_readiness=ready
external_nav_operator_reason=valid
confidence_min_avg=0.900522/0.917264
external_nav_output_block_reasons=runtime_disabled:245
```

Audit checker:

```text
external_nav_output_audit_log_check path=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-audit-20260709T155405Z.log passed=true estimates=245 allowed=0 sent=0 blocked=245 reason=runtime_disabled stop_reason=endpoint_progress_reached
external_nav_output_attach_check passed=true
```

## Прийнятий Reverse Run

Log:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260709T155702Z.log
```

Final summary:

```text
passed=true
frames=222/450
valid_matches=222
progress=0.923077..0.143813
tracked_progress=0.923077..0.153076
tracked_reverse_progress_monotonic=true
tracked_directional_progress=true
endpoint_passed=true
progress_gate_passed=true
endpoint_stop=true
stop_reason=endpoint_progress_reached
telemetry_health=true
dry_run_quality=true
dry_run_valid=222/222
external_nav_valid=222/222
external_nav_valid_fraction=1
external_nav_invalid_reasons=none
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_quality_ready=true
external_nav_operator_readiness=ready
external_nav_operator_reason=valid
confidence_min_avg=0.878196/0.890983
external_nav_output_block_reasons=runtime_disabled:222
```

Audit checker:

```text
external_nav_output_audit_log_check path=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-audit-20260709T155702Z.log passed=true estimates=222 allowed=0 sent=0 blocked=222 reason=runtime_disabled stop_reason=endpoint_progress_reached
external_nav_output_attach_check passed=true
```

## Що Доведено

- OV9281 camera path на Pi працює для `640x400 -> 160x100` field route recording.
- Новий daylight route має достатню якість для hand-carried forward/reverse dry-run readiness.
- External-nav estimates можуть бути FC-ready протягом усього accepted endpoint-stop проходу (`valid == total`) у forward і reverse.
- Attach-only external-nav output audit працює: writer attach-capable, runtime send disabled, `allowed=0`, `sent=0`, `blocked=<frames>`, reason `runtime_disabled`.
- `VISUAL_HOMING_LIVE_ROUTE_MATCH_WINDOW_RADIUS=120` є потрібним польовим параметром для цього OV9281 route; дефолт `30` може утримувати tracker у локальному сегменті.
- Фізичний endpoint був підтверджений standing diagnostic проти `end.png`.

## Що Не Доведено

- Це не allowed-send bench evidence.
- Це не armed, tethered, ground-movement або flight evidence.
- Це не доказ точного метричного масштабу; FOV у профілі nominal, а visual scale diagnostics залишались вимкнені.
- Це не JT_Zero handoff evidence; JT_Zero досі не інтегрований як same-Pi module.
- Це не доказ стабільності в темряві/сутінках; попередній вечірній repeatability test показав confidence drop.
- Це не доказ, що `WINDOW_RADIUS=120` є універсальним для усіх маршрутів; це прийнятий параметр саме для цього OV9281 route.

## Наступні Кроки

1. Зробити окремий operator wrapper або documented command для OV9281 attach-only field readiness, щоб не вводити довгі env-команди вручну.
2. Винести `WINDOW_RADIUS=120`, `endpoint_end=0.94`, `telemetry_max_age=3000`, `malformed_frames=2`, `altitude_tolerance=3`, `max_altitude_span=5` у явний field preset для OV9281 route tests.
3. Перед будь-яким allowed-send bench етапом окремо переглянути safety plan, provider ownership, ArduPilot ExternalNav acceptance, max message/time limits, and operator confirmations.
4. Для майбутньої якості маршруту додати швидший operator feedback: keyframe preview, standing start/end diagnostics, and route endpoint confirmation before readiness runs.
