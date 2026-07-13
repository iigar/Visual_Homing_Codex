# Польовий Evidence 2026-07-13 OV9281

Цей документ фіксує польову сесію 2026-07-13 на тій самій локації. Мета: перезаписати маршрут для чистоти тестів, перевірити clean forward/reverse attach-only behavior і оцінити `Visual Focus Window / Focus ROI` як diagnostic/fallback evidence.

Це hand-carried / props-off bench evidence. Воно не є armed, tethered або free-flight evidence.

## Важлива Примітка Про Шлях Route

Новий маршрут 2026-07-13 був записаний у старий шлях:

```text
/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260712T164651Z.vhrs
```

Тобто filename має дату `20260712`, але фактичний вміст маршруту після цієї сесії є новим записом від 2026-07-13.

## Route Record

Log:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/field-route-record-20260713T164943Z.log
```

Keyframes:

```text
/home/pi/Visual_Homing_Codex/keyframes/field-route-20260713T164943Z-keyframes
```

Route summary:

```text
entries=600
size=160x100
elapsed_ms=20025.3
effective_fps=29.9621
telemetry_warmup_passed=true
relative_altitude_m=1.48
route_self_match passed=true
route_perturb_check passed=true
route_distinctiveness quality_pass=true warning=true
low_texture_entries=0
exact_duplicate_entries=0
ambiguous_nearest_entries=9
ambiguous_nearest_fraction=0.015
average_nearest_mean_abs_diff=8.68194
```

Warning reason:

```text
ambiguous_nearest_samples=153@0.000ms,154@33.345ms,155@66.538ms,156@99.807ms,
746@19765.968ms,747@19799.190ms,748@19832.477ms,749@19865.934ms
```

Interpretation: route is accepted (`quality_pass=true`), but there are short ambiguous regions at the beginning and end, likely caused by start/end slow movement or short pauses.

## Focus ROI Diagnostic Baseline

Current diagnostic ROI:

```text
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_LEFT=0.08
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_RIGHT=0.08
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_TOP=0.05
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_BOTTOM=0.15
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_TOP_K=5
```

Purpose: remove some edge/peripheral clutter while keeping enough route context. This ROI is less aggressive than the first diagnostic `0.12/0.12/0.08/0.22`, and produced better route-index agreement.

## Forward Attach-Only Pass

Logs:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260713T170437Z.log
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-audit-20260713T170437Z.log
```

Stop frame:

```text
artifacts/stop_frames/endpoint-stop-frame-20260713T170526Z-id-831-route-599.pgm
```

Summary:

```text
passed=true
frames=679/1200
valid_matches=679/679
progress=0.00834725..1
tracked_progress=0.00834725..1
endpoint_stop=true
endpoint_stop_route_index=599
endpoint_stop_progress=1
endpoint_stop_confidence=0.937696
telemetry_health=true
dry_run_quality=true
external_nav_valid=679/679
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_operator_readiness=ready
external_nav_output_allowed=0
external_nav_output_sent=0
external_nav_output_blocked=679
external_nav_output_reason=runtime_disabled
audit_check passed=true
```

Focus ROI summary:

```text
focus_roi_valid=679/679
focus_roi_valid_fraction=1
focus_roi_confidence_min_avg=0.853788/0.929617
focus_roi_progress=0.00834725..1
focus_roi_route_index_agreement=272/679
focus_roi_route_index_agreement_fraction=0.400589
focus_roi_endpoint_agreement=496/679
focus_roi_endpoint_agreement_fraction=0.730486
focus_roi_top_match_gap_min_avg=2.19491e-06/0.00114209
```

Route-index delta check:

```text
frames=679
avg_abs_route_index_delta=11.7791
max_abs_route_index_delta=77
within_3=354/679
within_10=424/679
within_30=617/679
```

Interpretation: forward full-frame authority is clean. Focus ROI is valid on all frames and useful as diagnostic/fallback evidence, but not primary-ready because exact route-index agreement is still modest.

## Reverse Attach-Only First Pass

Logs:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260713T171054Z.log
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-audit-20260713T171054Z.log
```

Summary:

```text
passed=false
endpoint_stop=true
endpoint_stop_route_index=0
endpoint_stop_progress=0
external_nav_valid=703/703
telemetry_health=true
dry_run_quality=true
```

Reason for failure:

```text
endpoint_passed=false
progress_gate_passed=false
first_progress=0.978297
max_progress_seen=0.978297
```

The run physically reached route index `0`, but the reverse session gate was too strict because `VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.985` required seeing the route end at or above `0.985` at the beginning of the reverse run.

Focus ROI reverse delta:

```text
frames=703
avg_abs_route_index_delta=4.2091
max_abs_route_index_delta=39
within_3=491/703
within_10=549/703
within_30=701/703
focus_roi_endpoint_agreement=703/703
```

Interpretation: reverse Focus ROI evidence was strong, especially endpoint agreement.

## Reverse Attach-Only Clean Pass

Adjustment:

```text
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.97
```

Logs:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260713T171523Z.log
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-audit-20260713T171523Z.log
```

Stop frame:

```text
artifacts/stop_frames/endpoint-stop-frame-20260713T171611Z-id-816-route-0.pgm
```

Summary:

```text
passed=true
frames=664/1200
valid_matches=664/664
progress=0.973289..0
tracked_progress=0.973289..2.84172e-12
endpoint_stop=true
endpoint_stop_route_index=0
endpoint_stop_progress=0
endpoint_stop_confidence=0.891733
telemetry_health=true
dry_run_quality=true
external_nav_valid=664/664
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_operator_readiness=ready
external_nav_output_allowed=0
external_nav_output_sent=0
external_nav_output_blocked=664
external_nav_output_reason=runtime_disabled
audit_check passed=true
```

Focus ROI summary:

```text
focus_roi_valid=664/664
focus_roi_valid_fraction=1
focus_roi_confidence_min_avg=0.882229/0.903935
focus_roi_progress=0.993322..0
focus_roi_route_index_agreement=279/664
focus_roi_route_index_agreement_fraction=0.420181
focus_roi_endpoint_agreement=663/664
focus_roi_endpoint_agreement_fraction=0.998494
focus_roi_top_match_gap_min_avg=7.31636e-07/0.000877914
```

## Forward Props-Off Send-Enabled Pass

This run used the reviewed props-off send wrapper. It is still not armed flight evidence. It confirms the bounded external-nav provider writer path can send messages during a clean forward route-following pass while the vehicle remains not armed.

Logs:

```text
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-send-20260713T173231Z.log
/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-send-audit-20260713T173231Z.log
```

Stop frame:

```text
artifacts/stop_frames/endpoint-stop-frame-20260713T173318Z-id-764-route-599.pgm
```

Summary:

```text
passed=true
frames=628/1200
valid_matches=628/628
progress=0.0100167..1
tracked_progress=0.0100167..1
endpoint_stop=true
endpoint_stop_route_index=599
endpoint_stop_progress=1
endpoint_stop_confidence=0.938656
telemetry_health=true
dry_run_quality=true
external_nav_valid=628/628
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_operator_readiness=ready
external_nav_output_allowed=628
external_nav_output_sent=628
external_nav_output_blocked=0
final_external_nav_output_reason=allowed
external_nav_output_send_check passed=true
audit_check passed=true
```

Safety observations:

```text
external-nav provider messages were sent in the bench props-off boundary
vehicle remained not armed
live_output_gate_block_reasons=vehicle_not_armed:628
external_nav_output_audit stop_reason=endpoint_progress_reached
```

Focus ROI summary:

```text
focus_roi_valid=628/628
focus_roi_valid_fraction=1
focus_roi_confidence_min_avg=0.913671/0.937713
focus_roi_progress=0.0100167..1
focus_roi_route_index_agreement=334/628
focus_roi_route_index_agreement_fraction=0.531847
focus_roi_endpoint_agreement=594/628
focus_roi_endpoint_agreement_fraction=0.94586
focus_roi_top_match_gap_min_avg=0/0.000982946
```

Interpretation: this is the strongest forward Focus ROI diagnostic result from the session, but Focus ROI still remains diagnostic/fallback evidence only. Full-frame matching remains the authority path.

## Conclusions

Accepted evidence:

```text
new route quality_pass=true
forward attach-only passed=true
reverse attach-only passed=true
forward props-off send-enabled passed=true
telemetry_health=true in clean forward/reverse passes
external_nav_valid=all/all in clean forward/reverse passes
external_nav_output remained blocked by runtime_disabled in attach-only wrapper
external_nav_output sent 628/628 messages in reviewed props-off send wrapper
```

Focus ROI conclusion:

```text
full-frame matcher remains primary authority
Focus ROI 8/8/5/15 is useful diagnostic/fallback evidence
Focus ROI primary is not accepted yet
```

Why ROI primary is not accepted yet:

```text
forward route_index_agreement_fraction=0.400589
reverse route_index_agreement_fraction=0.420181
send-forward route_index_agreement_fraction=0.531847
```

Why ROI is still valuable:

```text
focus_roi_valid_fraction=1 in forward and reverse
reverse endpoint agreement ~= 100%
reverse avg_abs_route_index_delta=4.2091 in first reverse pass
```

## Next Recommended Step

Recommended next test: repeat one more props-off send-enabled forward run on the same updated route, with full-frame authority and Focus ROI diagnostics enabled. Goal: prove repeatability before any armed/tethered planning. This is still not an armed flight test.
