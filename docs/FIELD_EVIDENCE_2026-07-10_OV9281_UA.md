# Польовий Evidence 2026-07-10 OV9281

Цей документ фіксує факти польової сесії 2026-07-10 без екстраполяцій. Мета сесії: перевірити endpoint dwell confirmation, записати новий OV9281 route без захисного ковпачка, прийняти forward endpoint preset і зберегти доказовий stop-frame.

Це hand-carried dry-run / attach-only evidence. Воно не є allowed-send, armed, tethered або flight evidence. External-nav output writer був attach-capable, але runtime send залишався disabled, а audit блокував кожну оцінку з `reason=runtime_disabled`.

## Артефакти

Accepted route на Pi:

```text
/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260710T155821Z.vhrs
```

Route keyframes на Pi:

```text
/home/pi/Visual_Homing_Codex/keyframes/field-route-20260710T155821Z-keyframes
```

Route keyframes на ноуті:

```text
D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex\keyframes\field-route-20260710T155821Z-keyframes
D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex\keyframes\field-route-20260710T155821Z-keyframes-png
```

Accepted forward dwell log:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260710T163641Z.log
```

Accepted audit log:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-audit-20260710T163641Z.log
```

Stop-frame:

```text
/home/pi/Visual_Homing_Codex/artifacts/stop_frames/endpoint-stop-frame-20260710T163725Z-id-685-route-599.pgm
D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex\artifacts\stop_frames\endpoint-stop-frame-20260710T163725Z-id-685-route-599.pgm
D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex\artifacts\stop_frames\endpoint-stop-frame-20260710T163725Z-id-685-route-599.png
```

`artifacts/` і `keyframes/` є локальними/польовими артефактами і не є tracked source files.

## Route Record

Route record summary:

```text
route=/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260710T155821Z.vhrs
entries=600
size=160x100
elapsed_ms=20015.1
effective_fps=29.9774
relative_altitude_m=1.229
```

Route inspect:

```text
first_frame_id=153
last_frame_id=752
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
low_texture_entries=0
exact_duplicate_entries=0
ambiguous_nearest_entries=4
ambiguous_nearest_fraction=0.00666667
average_nearest_mean_abs_diff=15.3908
```

Висновок із цих полів: route usable для field readiness. Warning обмежений `4/600` ambiguous nearest entries near start і не заблокував accepted forward dwell run.

## Важлива Операторська Корекція

Ранні 2026-07-10 runs проти старого route не матчились, тому що камера була фізично закрита захисним ковпачком. Після зняття ковпачка live route matching знову давав валідні матчі.

Це пояснює логи з `valid_matches=0`, низьким confidence і `tracked_progress=0`. Вони не є evidence проти dwell logic або route quality.

## Accepted Forward Preset

Accepted параметри для нового route:

```bash
VISUAL_HOMING_ROUTE_OUTPUT=/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260710T155821Z.vhrs
VISUAL_HOMING_CAMERA_TARGET_WIDTH=160
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=100
VISUAL_HOMING_CAMERA_FRAMES=1200
VISUAL_HOMING_LIVE_ROUTE_MATCH_WINDOW_RADIUS=30
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=forward
VISUAL_HOMING_LIVE_ROUTE_MATCH_INITIAL_PROGRESS_MIN=0
VISUAL_HOMING_LIVE_ROUTE_MATCH_INITIAL_PROGRESS_MAX=0.08
VISUAL_HOMING_LIVE_ROUTE_MATCH_DIRECTIONAL_SEARCH=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_DIRECTIONAL_BIAS=0
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.985
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_DWELL_MS=1200
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPORT_STOP_FRAME=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_FRAME_DIR=/home/pi/Visual_Homing_Codex/artifacts/stop_frames
VISUAL_HOMING_MATCH_LIVE_ROUTE_TELEMETRY_MAX_AGE_MS=3000
VISUAL_HOMING_MAVLINK_MAX_MALFORMED_FRAMES=2
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=1
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=3
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=5
```

## Accepted Forward Dwell Run

Log:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260710T163641Z.log
```

Final summary:

```text
passed=true
frames=533/1200
valid_matches=533
progress=0.00166945..1
tracked_progress=0.00166945..1
tracked_directional_progress=true
endpoint_passed=true
progress_gate_passed=true
endpoint_stop=true
endpoint_dwell_ms=1202.81
endpoint_dwell_required_ms=1200
endpoint_dwell_passed=true
endpoint_stop_frame_written=true
endpoint_stop_frame_id=685
endpoint_stop_frame_size=160x100
endpoint_stop_route_index=599
endpoint_stop_progress=1
endpoint_stop_tracked_progress=1
endpoint_stop_confidence=0.872061
stop_reason=endpoint_progress_reached
telemetry_health=true
dry_run_quality=true
dry_run_valid=533/533
external_nav_valid=533/533
external_nav_valid_fraction=1
external_nav_invalid_reasons=none
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_quality_ready=true
external_nav_operator_readiness=ready
external_nav_operator_reason=valid
confidence_min_avg=0.872061/0.89565
external_nav_output_block_reasons=runtime_disabled:533
```

Stop-frame event:

```text
live_route_match_endpoint_stop_frame path=/home/pi/Visual_Homing_Codex/artifacts/stop_frames/endpoint-stop-frame-20260710T163725Z-id-685-route-599.pgm frame_id=685 width=160 height=100 route_index=599 progress=1 tracked_progress=1 confidence=0.872061
```

Audit checker:

```text
external_nav_output_audit_log_check path=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-audit-20260710T163641Z.log passed=true estimates=533 allowed=0 sent=0 blocked=533 reason=runtime_disabled stop_reason=endpoint_progress_reached
external_nav_output_attach_check passed=true
```

## What This Proves

- OV9281 `160x100` forward route matching on this 20-second route can reach `progress=1` coherently.
- Endpoint stop no longer triggers on the first threshold crossing; it waited for `endpoint_dwell_ms=1202.81` against a required `1200 ms`.
- The exact processed frame that satisfied dwell was exported as Gray8 PGM.
- External-nav attach-only output stayed fail-closed: allowed `0`, sent `0`, blocked `533`, reason `runtime_disabled`.
- Operator readiness reached `ready`.

## What This Does Not Prove

- No armed, tethered, allowed-send, or flight behavior was tested.
- No external-nav MAVLink provider messages were sent.
- Stop-frame export currently captures the processed matcher frame (`160x100`), not the pre-resize OV9281 capture (`640x400`).
- Reverse readiness on the 2026-07-10 route still needs its own accepted run.
- High-resolution stop-frame export requires a separate pre-resize frame tap.

## Next Plan

1. Add high-resolution stop-frame capture before resize if ground-station visual evidence needs real `640x400`.
2. Run reverse attach-only readiness on `/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260710T155821Z.vhrs`.
3. Keep external-nav provider send disabled until a separate props-off send plan is reviewed and accepted.
4. If moving toward send-enabled bench evidence, require explicit runtime confirmations, positive message/time limits, audit enabled, props removed, single-provider ownership, and a fresh accepted readiness run.
