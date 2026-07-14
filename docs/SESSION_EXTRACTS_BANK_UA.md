# Session Extracts Bank

Цей файл є банком вижимок з попередніх Codex/operator сесій, які користувач надає у чаті. Він не зберігає raw transcript. Його задача: витягнути з довгих сесій практично корисні факти, рішення, артефакти, помилки, команди, safety-висновки і питання для продовження.

Цей файл доповнюється щоразу, коли користувач приносить попередню сесію або фрагмент сесії, з якого є сенс зберегти контекст.

## Як Додавати Нові Вижимки

Для кожної нової попередньої сесії додавати окремий entry:

```text
source_date=<дата, коли користувач надав transcript>
covered_dates=<дати польових/код-сесій всередині transcript>
scope=<що було зроблено>
status=<accepted|inconclusive|blocked|future idea>
```

Зберігати:

- прийняті артефакти: route, logs, audits, stop frames, JSON;
- числові evidence поля, які допомагають повторити або оцінити тест;
- safety-рішення: що дозволено, що заборонено, що не є flight evidence;
- команди/env, які часто плутаються;
- field observations від оператора;
- помилки, які вже траплялись, і як їх діагностувати;
- майбутні ідеї лише як research/future, якщо вони не є поточним планом.

Не зберігати:

- повний чат;
- неважливі проміжні спроби команд;
- дублікати того, що вже краще зафіксовано в evidence docs;
- будь-які слова, які можуть виглядати як flight authorization без окремого reviewed plan.

## Entry 2026-07-14: Вижимка З Наданого Попереднього Transcript

```text
source_date=2026-07-14
covered_dates=2026-07-12..2026-07-14
scope=endpoint ambiguity, ambiguous endpoint hold, focus ROI, July 13 route/send evidence, FC/JT_Zero acceptance probe, low-light failure, thermal future direction
status=curated_extract
```

Цей entry витягнутий з довгого transcript, який користувач надав 2026-07-14 як продовження попередньої роботи.

### Головний Висновок

Денний OV9281 Visual Homing шлях уже має повторюване props-off Pi-side external-nav provider send evidence, але ще не має FC/JT_Zero acceptance evidence і не є flight evidence.

Поточний основний напрям:

```text
daylight OV9281 -> FC/JT_Zero acceptance probe -> stationary/restrained/HOVER ladder
```

Thermal/EasyCAP:

```text
future branch after stable daytime flight ladder
not current blocker
not mixed with daylight OV9281 evidence
```

### 2026-07-12 Route And Endpoint Ambiguity

Accepted route:

```text
/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260712T164651Z.vhrs
entries=600
target_size=160x100
duration=about 20.03 s
route_self_match passed=true
route_perturb_check passed=true
route_distinctiveness quality_pass=true warning=false
ambiguous_nearest_entries=0
average_nearest_mean_abs_diff=9.06913
```

Forward attach-only accepted evidence:

```text
log=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260712T170240Z.log
valid_matches=1072/1072
tracked_progress=0..0.998331
endpoint_stop=true
endpoint_dwell_ms=1230.22/1200
external_nav_valid=1072/1072
external_nav_max_invalid_streak=0
confidence_min_avg=0.906222/0.940632
external_nav_session_ready=true
external_nav_strict_session_ready=true
external_nav_operator_readiness=ready
```

Send-enabled run on the same route:

```text
log=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-send-20260712T170735Z.log
valid_matches=490/490
external_nav_valid=490/490
external_nav_output_sent=490
external_nav_output_blocked=0
final_external_nav_output_reason=allowed
operator_observation=physical stop about 1 m before real endpoint
classification=provider-send evidence only, not accepted physical endpoint evidence
```

Endpoint diagnostic no-stop run:

```text
log=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260712T171244Z.log
operator_walked_full_route=true
operator_stood_at_real_finish=true
tracked_progress_final=about 0.994992
top_match_gap_avg=0.000480759
edge_top_match_gap_avg=0.000283189
external_nav_valid=485/485
```

Interpretation:

- pure `progress=1 + dwell` is too weak for endpoint completion on this route;
- the endpoint texture is ambiguous, not necessarily wrong or badly selected;
- the system must fail closed or switch to a separate ambiguous-arrival state, not demand a perfect final texture from the operator.

### Endpoint Ambiguity Gate

Code response from this transcript:

```text
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_REQUIRE_UNAMBIGUOUS=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_MIN_TOP_MATCH_GAP=<double>
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_MIN_EDGE_TOP_MATCH_GAP=<double>
```

Logged fields:

```text
endpoint_confirmation_required
endpoint_confirmation_passed
endpoint_confirmation_reason
endpoint_top_match_gap
endpoint_edge_top_match_gap
```

Safety meaning:

- If the endpoint is visually ambiguous, endpoint dwell does not accumulate.
- This prevents the earlier false finish about `1 m` before the endpoint.
- It is a safety gate, not a final RTL/HOVER behavior.

Important diagnostic fix:

```text
commit=5d4bdeb Preserve top match candidates for endpoint confirmation
reason=copy full-frame top candidates before edge diagnostics can overwrite matcher-owned buffer
effect=reason should become *_low instead of misleading *_unavailable when gap exists but is too small
```

### Ambiguous Endpoint Hold

Reason:

The strict ambiguity gate is too hard as final behavior. If the vehicle/operator stands at the real endpoint and matching remains valid but endpoint gaps stay too low, the system should not declare exact `route_complete`, but it also should not remain an uninformative generic route failure forever.

Implemented direction:

```text
VISUAL_HOMING_LIVE_ROUTE_MATCH_ALLOW_AMBIGUOUS_ENDPOINT_HOLD=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_AMBIGUOUS_ENDPOINT_DWELL_MS=3000
```

Important constraints:

- Requires endpoint unambiguous confirmation to be enabled.
- Refuses runtime output-enabled runs in the first version.
- Attach-only/log-only behavior first.
- Keeps `passed=false`.
- Stops with `stop_reason=ambiguous_endpoint_hold`.
- Marks external-nav operator readiness as `marginal`, not `ready`.
- Exports JSON handoff decision:

```json
{
  "handoff": {
    "candidate": true,
    "decision": "ambiguous_endpoint_hold",
    "reason": "endpoint_match_ambiguous"
  }
}
```

Meaning:

```text
route_complete=false
arrival_candidate=true
do_not_continue_RTL_path_blindly=true
```

### Endpoint Micro-Search Idea

User suggested a behavior where the drone/system searches around the endpoint if endpoint match is ambiguous.

Assessment:

- Technically real and useful.
- Higher risk than hold because it is active vehicle behavior.
- Should not be mixed into normal route-following.
- It should be a separate state after ambiguous endpoint hold.

Recommended first pattern:

```text
micro-search cross/box pattern
not circle first
```

Reason not to start with circle:

- yaw/roll/pitch and wide lens perspective changes can make aliasing worse;
- route matcher may become less stable;
- circle is harder to constrain and audit.

Possible first dry-run pattern:

```text
1. hover/hold center for 1-2 s
2. small forward shift 0.3-0.5 m
3. return through center
4. small left shift 0.3-0.5 m
5. return through center / right if needed
6. if endpoint confirmation improves -> endpoint candidate
7. else remain ambiguous_endpoint_hold / operator handoff
```

Hard limits before any live movement:

```text
max_search_radius_m=0.5..1.0
max_search_time_s=5..10
max_speed_mps=very_low
abort_on_confidence_drop=true
abort_on_telemetry_stale=true
abort_on_altitude_drift=true
abort_on_match_invalid_streak=true
abort_on_large_progress_jump=true
```

Validation ladder:

```text
log-only state -> dry-run commands -> props-off send evidence -> separate tethered hover plan
```

### Ideas Backlog Created

Created:

```text
docs/VISUAL_HOMING_IDEAS_UA.md
commit=8a67e39 Add Visual Homing ideas backlog
```

Purpose:

Living research backlog for ideas that are not immediate flight authorization.

Main ideas captured there:

- Endpoint Ambiguity Gate.
- Ambiguous Endpoint Hold.
- Endpoint Reacquire / Micro-Search.
- Endpoint visual landmarks: wire, spots, local features.
- Stop frame / PNG / Ground Station evidence.
- Route time awareness.
- Visual Homing RTL/HOVER state machine.
- Reverse route handling.
- Android/Ground Station operator UI.
- Full precision HOVER vs coarse Visual RTL.
- Visual Focus Window / Focus ROI Mode.
- Night / Thermal Camera Route Mode.

Rule:

Ideas in that file are not armed/tethered/free-flight authorization.

### Visual Focus Window / Focus ROI

Motivation:

The operator liked the idea of a “focus mode” where only a central/useful part of the image is used or inspected, with transparent ignored areas in the UI. The concept is useful especially for future operator UI and possible fallback matching, but was kept diagnostic-only.

Overlay concept:

```text
green border=full 160x100 frame
yellow rectangle=active focus ROI
red-tinted periphery=ignored/low-weight area
```

Generated overlay artifact:

```text
keyframes/field-route-20260712T164651Z-focus-roi-overlay/
frames=start,025,050,075,end
```

Diagnostic envs:

```text
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_DIAGNOSTICS=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_LEFT=<fraction>
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_RIGHT=<fraction>
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_TOP=<fraction>
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_BOTTOM=<fraction>
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_TOP_K=<count>
```

Important design decision:

Focus ROI diagnostic does not affect:

- endpoint stop;
- `passed`;
- readiness;
- external-nav estimates;
- output send/block decisions.

It logs separate diagnostics only:

```text
focus_roi_valid
focus_roi_confidence_min_avg
focus_roi_progress
focus_roi_route_index_agreement
focus_roi_endpoint_agreement
focus_roi_top_match_gap_min_avg
```

ROI tuning observations:

Aggressive ROI:

```text
left=0.12 right=0.12 top=0.08 bottom=0.22
result=too aggressive
forward_delta_avg=18.709
forward_delta_max=115
within_30=69.5%
```

Better baseline:

```text
left=0.08 right=0.08 top=0.05 bottom=0.15
```

Forward attach with better ROI:

```text
log=external-nav-output-attach-20260713T170437Z.log
passed=true
focus_roi_valid=679/679
focus_roi_confidence_min_avg=0.853788/0.929617
focus_roi_route_index_agreement=272/679
focus_roi_route_index_agreement_fraction=0.400589
focus_roi_endpoint_agreement=496/679
focus_roi_endpoint_agreement_fraction=0.730486
focus_delta_avg=11.7791
focus_delta_max=77
within_30=617/679
within_30_fraction=0.909
```

Reverse clean attach with better ROI and `END_PROGRESS=0.97`:

```text
log=external-nav-output-attach-20260713T171523Z.log
passed=true
focus_roi_valid=664/664
focus_roi_confidence_min_avg=0.882229/0.903935
focus_roi_route_index_agreement=279/664
focus_roi_route_index_agreement_fraction=0.420181
focus_roi_endpoint_agreement=663/664
focus_roi_endpoint_agreement_fraction=0.998494
```

Conclusion:

```text
Focus ROI is accepted as diagnostic/fallback evidence only.
Full-frame matcher remains authority.
```

### 2026-07-13 Route Update And Evidence

Important artifact note:

The active route path still used the filename:

```text
/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260712T164651Z.vhrs
```

But the route content had been updated with a fresh 2026-07-13 recording. Do not infer route content only from filename.

Route record:

```text
record_log=/home/pi/Visual_Homing_Codex/artifacts/logs/field-route-record-20260713T164943Z.log
keyframes=/home/pi/Visual_Homing_Codex/keyframes/field-route-20260713T164943Z-keyframes
entries=600
target_size=160x100
duration=about 20.025 s
effective_fps=29.9621
relative_altitude_m=1.48
route_self_match passed=true
route_perturb_check passed=true
route_distinctiveness quality_pass=true warning=true
ambiguous_nearest_entries=9
average_nearest_mean_abs_diff=8.68194
```

Forward attach accepted:

```text
log=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260713T170437Z.log
passed=true
frames=679
endpoint_route_index=599
external_nav_valid=679/679
telemetry_health=true
external_nav_output_block_reason=runtime_disabled
```

Reverse attach accepted:

```text
log=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-attach-20260713T171523Z.log
passed=true
endpoint_route_index=0
external_nav_valid=664/664
telemetry_health=true
external_nav_output_block_reason=runtime_disabled
note=reverse needed endpoint_end_progress=0.97
```

Forward props-off send accepted:

```text
log=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-send-20260713T173231Z.log
passed=true
frames=628
external_nav_valid=628/628
endpoint_route_index=599
external_nav_output_allowed=628
external_nav_output_sent=628
external_nav_output_blocked=0
final_external_nav_output_reason=allowed
vehicle_armed=false
```

Forward props-off send repeat accepted:

```text
log=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-send-20260713T175911Z.log
audit=/home/pi/Visual_Homing_Codex/artifacts/logs/external-nav-output-send-audit-20260713T175911Z.log
passed=true
frames=277/1200
progress=0.00834725..1
tracked_progress=0.00834725..0.999994
endpoint_stop=true
endpoint_route_index=599
endpoint_dwell_ms=1217.09/1200
external_nav_valid=277/277
external_nav_max_invalid_streak=0
external_nav_relative_altitude_min_avg_max_m=0.726/0.989917/1.186
external_nav_output_allowed=277
external_nav_output_sent=277
external_nav_output_blocked=0
final_external_nav_output_reason=allowed
vehicle_armed=false
live_output_gate_block_reasons=vehicle_not_armed:277
```

Evidence conclusion:

```text
Pi-side external-nav provider send is repeatable in props-off bench state.
This is still not FC/JT_Zero acceptance.
This is still not armed, tethered, hover, or flight evidence.
```

### FC/JT_Zero Acceptance Probe Wrapper

Added:

```text
script=./scripts/run-external-nav-output-acceptance-probe-pi.sh
commit=36cd2cb Add external nav acceptance probe wrapper
```

Wrapper shape:

```text
1. pre-send read-only telemetry sanity capture
2. reviewed props-off external-nav provider send wrapper
3. post-send read-only telemetry sanity capture
4. manifest log
```

Main manifest:

```text
artifacts/logs/external-nav-acceptance-probe-<stamp>.log
```

Underlying logs:

```text
external-nav-acceptance-pre-<stamp>.log
external-nav-acceptance-send-<stamp>.log
external-nav-acceptance-send-audit-<stamp>.log
external-nav-acceptance-post-<stamp>.log
```

Automatic result is deliberately conservative:

```text
acceptance_probe_result=probe_complete_requires_fc_status_review
```

Meaning:

```text
Pi-side sent=true is necessary but not sufficient.
FC/JT_Zero acceptance must come from reviewed telemetry/status evidence.
```

### Low-Light Acceptance Probe Failure

Evening/low-light run showed:

```text
confidence=about 0.54
focus_roi_confidence=about 0.55
valid=false
route_match_not_valid
progress stuck near 0.0417362
```

Interpretation:

```text
classification=acceptance_probe_inconclusive
likely_reason=low_light_domain_shift
not_fc_acceptance_evidence=true
```

This does not weaken earlier clean daytime send evidence. It only says the daylight OV9281 route should not be expected to work after dusk/night without a separate route/profile.

### Stopped External Nav Output Session Fix

The low-light probe also exposed a lifecycle edge case:

```text
match_live_route_active_profile_error=LiveExternalNavOutputSession process called while stopped
```

Fix:

```text
commit=75d91a5 Handle stopped external nav output session
```

Behavior after fix:

- `process()` before `start()` still throws, preserving misuse detection.
- `process()` after the session stopped no longer crashes the whole route/probe.
- It returns blocked:

```text
reason=external_nav_output_session_stopped
allowed=false
sent=false
```

### Thermal / EasyCAP Future Direction

User noted that night tests and future night flights should use a thermal USB camera through EasyCAP.

Decision from transcript:

```text
thermal_easycap=deferred
when=after daytime system flies/stabilizes
current_blocker=false
```

Rules:

- Do not use daylight OV9281 route as night/thermal readiness evidence.
- Do not mix daylight and thermal artifacts in one acceptance bucket.
- Thermal/EasyCAP needs separate capture profile, route artifacts, keyframes, attach-only evidence, and provider-send evidence.
- EasyCAP may introduce different latency/format path and needs its own validation.

### Altitude / Telemetry Observations

Relative altitude can be noisy or origin-shifted during hand-carried tests.

Bad examples:

```text
external_nav_altitude_blocker=relative_altitude_non_positive
relative_altitude_min_avg_max_m=-0.175/-0.0903458/-0.011
external_nav_valid=0/1200
```

Sanity can sometimes pass while min/avg still look suspicious because tolerance is wide:

```text
relative_altitude_min_avg_max_m=-0.254/-0.00538211/1.048
latest=0.731
sanity_passed=true
```

Clean send-repeat altitude:

```text
relative_altitude_min_avg_max_m=0.726/0.989917/1.186
```

Operational lesson:

For send/repeatability evidence, prefer stable positive min/avg/latest, not only formal sanity pass.

### Commands And Pitfalls Preserved From Transcript

Correct route env:

```text
VISUAL_HOMING_ROUTE_OUTPUT
```

Incorrect/confusing env that was called out:

```text
VISUAL_HOMING_ROUTE_PATH
```

Frequent script names:

```text
./scripts/check-external-nav-telemetry-sanity-pi.sh
./scripts/run-external-nav-output-bench-props-off-attach-pi.sh
./scripts/run-external-nav-output-bench-props-off-send-pi.sh
./scripts/run-external-nav-output-acceptance-probe-pi.sh
```

Long confirmation value can be split safely:

```text
A=I_UNDERSTAND_THIS_WILL_SEND_EXTERNAL_NAV_PROVIDER
B=_MESSAGES_WITH_PROPS_REMOVED
export VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BENCH_SEND_CONFIRM="${A}${B}"
export VISUAL_HOMING_EXTERNAL_NAV_OUTPUT_BENCH_FC_STATE_CONFIRM=I_HAVE_VERIFIED_REVIEWED_BENCH_FC_STATE
```

Serial permission fix that appeared in the transcript:

```text
sudo chgrp dialout /dev/ttyS0
sudo chmod 660 /dev/ttyS0
```

Known Pi/local operational issues:

- `/dev/serial0 -> /dev/ttyS0` permissions can block MAVLink.
- Running wrappers with `sudo -E` can leave root-owned `/tmp/visual_homing_*` files.
- Long heredoc/env command blocks in chat are fragile; prefer short `export` lines.
- User prefers build and test commands separated when practical.
- If Pi `git` reports empty object / bad HEAD, repair checkout before trusting another build/probe.

### Current Carry-Forward Plan From This Transcript

Near-term:

```text
daylight only
props-off
normal lighting matching conditions
repeat FC/JT_Zero acceptance probe
analyze FC/JT_Zero acceptance/status evidence
```

Do not do yet:

```text
armed flight
tethered hover without separate reviewed plan
night/thermal route tests as current blocker
more blind send-only repeats without a new question
```

After FC/JT_Zero acceptance evidence:

```text
stationary/restrained/HOVER planning
then controlled RTL/HOVER ladder
thermal/EasyCAP only later as separate branch
```
