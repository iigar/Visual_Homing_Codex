# Польовий Evidence 2026-07-12 OV9281

Цей документ фіксує польову сесію 2026-07-12. Мета: записати новий маршрут, перевірити forward attach/send behavior і зафіксувати endpoint aliasing, який не можна вирішувати вимогою "ідеальної" текстури.

Це hand-carried / props-off bench evidence. Воно не є armed, tethered або flight evidence.

## Route

Accepted route на Pi:

```text
/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260712T164651Z.vhrs
```

Route keyframes:

```text
/home/pi/Visual_Homing_Codex/keyframes/field-route-20260712T164651Z-keyframes
D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex\keyframes\field-route-20260712T164651Z-keyframes
D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex\keyframes\field-route-20260712T164651Z-keyframes-png
```

Route quality:

```text
entries=600
size=160x100
elapsed_ms=20026.5
effective_fps=29.9603
relative_altitude_m=1.22
route_self_match passed=true
route_perturb_check passed=true
route_distinctiveness quality_pass=true warning=false
low_texture_entries=0
exact_duplicate_entries=0
ambiguous_nearest_entries=0
average_nearest_mean_abs_diff=9.06913
```

Serial/MAVLink readiness was restored before recording:

```text
/dev/serial0 -> ttyS0
/dev/ttyS0 root:dialout crw-rw----
mavlink_telemetry_capture opened=true
heartbeat_messages=3
attitude_messages=15
global_position_int_messages=3
altitude_messages=3
malformed_frames=0
mode=AltHold
armed=false
```

## Forward Attach-Only Pass

Log:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260712T170240Z.log
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-audit-20260712T170240Z.log
```

Stop frame:

```text
/home/pi/Visual_Homing_Codex/artifacts/stop_frames/endpoint-stop-frame-20260712T170341Z-id-1224-route-598.pgm
```

Summary:

```text
passed=true
frames=1072/1200
valid_matches=1072/1072
progress=0..0.998331
tracked_progress=0..0.998331
endpoint_stop=true
endpoint_dwell_ms=1230.22
endpoint_dwell_required_ms=1200
endpoint_stop_route_index=598
endpoint_stop_progress=0.998331
endpoint_stop_confidence=0.949282
confidence_min_avg=0.906222/0.940632
external_nav_valid=1072/1072
external_nav_max_invalid_streak=0
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_operator_readiness=ready
external_nav_output_sent=0
final_external_nav_output_reason=runtime_disabled
```

Operator note: the full route was walked and then the operator stood at the end for about 20 seconds. This is strong attach-only route/readiness evidence.

## Forward Send-Enabled Pass With Physical Early Stop

Log:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-send-20260712T170735Z.log
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-send-audit-20260712T170735Z.log
```

Summary:

```text
passed=true
frames=490/1200
valid_matches=490/490
progress=0..1
tracked_progress=0..1
endpoint_stop=true
endpoint_dwell_ms=1231.05
endpoint_stop_route_index=599
endpoint_stop_progress=1
endpoint_stop_confidence=0.881953
external_nav_valid=490/490
external_nav_max_invalid_streak=0
external_nav_output_allowed=490
external_nav_output_sent=490
external_nav_output_blocked=0
final_external_nav_output_reason=allowed
```

Operator note: the physical system stopped about 1 m before the route end.

Conclusion: this run proves bounded/audited provider send still works, but it is not acceptable as physical endpoint evidence. A single terminal route-index match plus dwell can fire early on repeated/weak endpoint texture.

## Endpoint Diagnostic Run

Log:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260712T171244Z.log
```

This run disabled endpoint stop and enabled match diagnostics. The operator walked the full route in about 20 seconds and then stood at the real finish for almost a minute.

Summary:

```text
passed=false
stop_reason=capture_timeout
frames=485/1200
valid_matches=485/485
progress=0..0.994992
tracked_progress=0..0.994992
endpoint_passed=true
progress_gate_passed=true
endpoint_stop=false
confidence_min_avg=0.917922/0.933631
top_match_diagnostics=true
top_match_frames=485
top_match_gap_min_avg=0/0.000480759
zone_probe_diagnostics=true
zone_probe_frames=485
end_zone_gap_min_avg=-0.0187373/-0.000853042
edge_match_diagnostics=true
edge_match_frames=485
edge_top_match_gap_min_avg=0/0.000283189
edge_end_zone_gap_min_avg=0/0.00114975
external_nav_valid=485/485
external_nav_max_invalid_streak=0
```

Conclusion:

- Real physical finish stabilized near `tracked_progress=0.994992`, not necessarily exactly `1.0`.
- The top and edge match gaps are extremely small at/near the finish, so endpoint candidates are visually close.
- The system must handle this by fail-closed endpoint confirmation, not by asking the operator to choose a perfect texture.

## Code Response

After this session, endpoint-stop logic gained an opt-in ambiguity gate:

```text
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_REQUIRE_UNAMBIGUOUS=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_MIN_TOP_MATCH_GAP=<double>
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_MIN_EDGE_TOP_MATCH_GAP=<double>
```

When enabled, endpoint dwell only counts while the current endpoint match has enough full-frame and edge top-candidate separation. If the endpoint is visually ambiguous, logs report:

```text
endpoint_confirmation_required=true
endpoint_confirmation_passed=false
endpoint_confirmation_reason=top_match_gap_low|edge_top_match_gap_low|...
endpoint_top_match_gap=<last evaluated gap>
endpoint_edge_top_match_gap=<last evaluated gap>
```

The first validation after this change should be attach-only, not send-enabled. Expected result on the current ambiguous finish is likely fail-closed `endpoint_stop=false` with `endpoint_confirmation_reason=..._low`; that is safer than an early physical stop.
