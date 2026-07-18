# Visual Homing Ideas / Research Backlog

Цей документ є живим backlog для ідей, гіпотез, майбутніх алгоритмів і польових спостережень. Він не є дозволом на політ і не замінює `LIVE_OUTPUT_SAFETY_PLAN`, `VISUAL_HOMING_RTL_HOVER_PLAN_UA` або окремий reviewed test plan.

Формат кожної ідеї:

- Що це.
- Для чого потрібно.
- Чому з'явилась ідея.
- Як можна реалізувати.
- Що треба логувати.
- Як перевіряти.
- Чи реально.
- Ризики і межі безпеки.
- Поточний статус.

## Поточний Контекст 2026-07-12

Маршрут:

```text
/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260712T164651Z.vhrs
entries=600
duration=about 20.03 s
target=160x100
quality_pass=true
warning=false
ambiguous_nearest_entries=0
average_nearest_mean_abs_diff=9.06913
```

Важливі спостереження:

- Forward attach-only пройшов добре: `external-nav-output-attach-20260712T170240Z.log`, `external_nav_valid=1072/1072`, invalid streak `0`, endpoint dwell пройшов.
- Forward send-enabled довів, що provider messages можуть bounded/audited sent, але фізично зупинився приблизно на `1 m` раніше endpoint. Це не accepted endpoint evidence.
- Diagnostic no-stop run на реальному фініші показав, що matcher бачить фінішну зону, але top/edge gaps дуже малі.
- Endpoint texture не ідеальна, але є локальні орієнтири: петля дрота вздовж майже всього маршруту, плями, дрібні контрастні елементи.
- Висновок: система не може вимагати від оператора "ідеальної" текстури. Вона має мати fail-closed або fallback behavior для неоднозначного endpoint.

## Ідея 1: Endpoint Ambiguity Gate

### Що Це

Опціональний gate, який не дозволяє оголошувати endpoint complete тільки за `tracked_progress >= endpoint_threshold` і dwell, якщо поточний endpoint кадр візуально неоднозначний.

Поточна реалізація:

```text
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_REQUIRE_UNAMBIGUOUS=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_MIN_TOP_MATCH_GAP=<double>
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_MIN_EDGE_TOP_MATCH_GAP=<double>
```

Логові поля:

```text
endpoint_confirmation_required=true
endpoint_confirmation_passed=false
endpoint_confirmation_reason=top_match_gap_low|edge_top_match_gap_low|...
endpoint_top_match_gap=<value>
endpoint_edge_top_match_gap=<value>
```

### Для Чого

Щоб не повторити ситуацію, коли send-enabled run зупинився приблизно на `1 m` до реального фінішу, хоча мав:

```text
endpoint_stop=true
endpoint_stop_route_index=599
endpoint_stop_progress=1
external_nav_output_sent=490
```

### Чому З'явилась Ідея

Diagnostic run на реальному фініші показав:

```text
tracked_progress=0..0.994992
top_match_gap_avg=0.000480759
edge_top_match_gap_avg=0.000283189
external_nav_valid=485/485
```

Тобто маршрут бачиться, але endpoint-кандидати дуже близькі між собою.

### Як Реалізовано Зараз

У `match_live_camera_route` endpoint dwell рахується тільки якщо:

```text
endpoint progress reached
AND endpoint confirmation passed
```

Якщо confirmation не проходить, dwell скидається до `0`, `endpoint_stop=false`, run завершується по frame limit або timeout.

### Що Треба Логувати

Обов'язково:

```text
endpoint_confirmation_required
endpoint_confirmation_passed
endpoint_confirmation_reason
endpoint_top_match_gap
endpoint_edge_top_match_gap
endpoint_dwell_ms
endpoint_stop
stop_reason
```

Бажано для діагностики:

```text
top_match_diagnostics=true
edge_match_diagnostics=true
zone_probe_diagnostics=true
```

### Як Перевіряти

Attach-only first:

```text
external_nav_output_runtime_enabled=0
endpoint_require_unambiguous=1
```

Accepted fail-closed behavior на поточному маршруті:

```text
valid_matches=all/all
external_nav_valid=all/all
tracked_progress reaches endpoint
endpoint_confirmation_passed=false
endpoint_stop=false
stop_reason=capture_timeout|frame_limit_reached
```

Це не "успішний endpoint", але це успішне блокування false finish.

### Чи Реально

Так. Уже реалізовано як safety diagnostic gate.

### Ризики

- Як фінальна поведінка для RTL/HOVER занадто жорстка: система може вічно не оголошувати endpoint complete, навіть якщо фізично знаходиться на фініші.
- Пороги `top/edge gap` не мають бути flight authority без окремої валідації.
- Gate не вирішує endpoint arrival, він тільки не дає помилково оголосити точний finish.

### Поточний Статус

Implemented and pushed:

```text
162954f Add endpoint ambiguity confirmation gate
5d4bdeb Preserve top match candidates for endpoint confirmation
```

Наступний крок: додати `ambiguous_endpoint_hold`.

## Ідея 2: Ambiguous Endpoint Hold

### Що Це

Окремий стан системи, коли маршрут, ймовірно, пройдений до endpoint zone, але endpoint не можна підтвердити як точний останній кадр через ambiguity.

Можливий стан:

```text
stop_reason=ambiguous_endpoint_hold
endpoint_arrival_candidate=true
route_complete=false
```

### Для Чого

Щоб система не поводилась у двох крайностях:

- не оголошувала false `route_complete` на `1 m` раніше;
- не висіла нескінченно в `route_session_not_passed`, коли operator/vehicle вже стоїть на реальному фініші.

### Чому З'явилась Ідея

Поточний ambiguity gate правильно fail-closed, але user справедливо помітив: якщо стояти на фініші 1-5 хвилин і gate ніколи не проходить, це не робоча поведінка для RTL/HOVER.

### Умови Входу

Приклад conservative condition:

```text
tracked_progress >= endpoint_threshold
endpoint_dwell_elapsed >= 1200 ms або ambiguous_endpoint_dwell_ms >= 1500-3000 ms
external_nav_valid_for_fc=true
external_nav_max_invalid_streak=0 або <= small threshold
telemetry_health=true
confidence >= minimum_confidence
endpoint_confirmation_passed=false
endpoint_confirmation_reason=top_match_gap_low|edge_top_match_gap_low
```

Важливо: це не `route_complete`. Це `arrival_candidate`.

### Поведінка

Dry-run/log-only first:

```text
ambiguous_endpoint_hold=true
route_following_should_stop=true
live_output_should_continue=false або hold-only
handoff.decision=ambiguous_endpoint_hold
```

Для майбутнього HOVER:

```text
зупинити RTL path-following
не йти далі по маршруту blindly
передати стан у HOVER/JT_Zero/операторський UI
```

### Що Треба Логувати

```text
ambiguous_endpoint_hold_triggered
ambiguous_endpoint_hold_dwell_ms
ambiguous_endpoint_hold_reason
endpoint_confirmation_reason
tracked_progress
route_index
confidence
external_nav_valid_fraction
external_nav_max_invalid_streak
telemetry_health
```

У JSON readiness:

```json
{
  "handoff": {
    "candidate": true,
    "decision": "ambiguous_endpoint_hold",
    "reason": "endpoint_match_ambiguous"
  }
}
```

### Як Перевіряти

1. Attach-only run на 2026-07-12 route.
2. Operator проходить маршрут і стоїть на endpoint 5-10 секунд.
3. Очікуємо:

```text
endpoint_stop=false
ambiguous_endpoint_hold=true
external_nav_valid=all/all
operator_readiness=marginal або ready_ambiguous_endpoint
```

4. Жодного send-enabled до повторюваних attach-only доказів.

### Чи Реально

Так. Це найреалістичніший наступний крок.

### Ризики

- Якщо назвати це `route_complete`, можна знову отримати false endpoint.
- Якщо одразу дозволити live movement, ризик невідомий.
- Стан має бути clearly distinct: `ambiguous hold`, не `complete`.

### Поточний Статус

Implemented as an opt-in attach-only/log-only state:

```text
VISUAL_HOMING_LIVE_ROUTE_MATCH_ALLOW_AMBIGUOUS_ENDPOINT_HOLD=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_AMBIGUOUS_ENDPOINT_DWELL_MS=3000
```

Current behavior:

```text
ambiguous_endpoint_hold=true
stop_reason=ambiguous_endpoint_hold
passed=false
external_nav_operator_readiness=marginal
external_nav_operator_reason=ambiguous_endpoint_hold
handoff.decision=ambiguous_endpoint_hold
```

Runtime output-enabled runs are rejected while this state is in the first safety phase.

## Ідея 3: Endpoint Reacquire / Micro-Search

### Що Це

Окрема поведінка після `ambiguous_endpoint_hold`, коли система виконує дуже малий контрольований пошук навколо endpoint, щоб покращити endpoint confirmation або знайти більш однозначний кадр.

Це не продовження RTL route-following. Це окремий endpoint refinement mode.

### Для Чого

Щоб у випадку слабко унікального endpoint не зупинятись назавжди у fail-closed hold, а спробувати знайти локальний ракурс/позицію, де:

```text
endpoint_confirmation_passed=true
або endpoint candidate стабілізувався краще
```

### Чому З'явилась Ідея

User запропонував: дрон/система може зробити невелике коло, квадрат або інший search pattern, щоб знайти endpoint/waypoint match. Якщо знаходить - продовжує/завершує маршрут; якщо ні - переходить у hold.

### Рекомендований Перший Варіант

Не коло. Коло занадто складне для першого live behavior:

- yaw/roll/pitch змінюють картинку;
- wide lens дає перспективні зміни;
- route matcher може отримати більше aliasing.

Краще перший варіант:

```text
micro-search box / cross pattern
```

Приклад:

```text
1. hover на місці 1-2 s
2. маленький зсув вперед 0.3-0.5 m
3. назад через центр
4. вліво 0.3-0.5 m
5. вправо через центр
6. якщо endpoint_confirmation_passed або endpoint candidate стабілізувався - route_complete_candidate
7. якщо ні - ambiguous_endpoint_hold / operator handoff
```

### Обмеження

Жорсткі limits:

```text
max_search_radius_m=0.5..1.0
max_search_time_s=5..10
max_speed_mps дуже мала
altitude stable
abort on confidence drop
abort on telemetry stale
abort on altitude drift
abort on route match invalid streak
abort on large progress jump away from endpoint
```

### Як Реалізовувати

Етапи:

1. Log-only state machine:
   - detect when micro-search would start;
   - emit intended pattern step;
   - no command output.

2. Dry-run command generation:
   - produce hypothetical small velocity/yaw commands;
   - keep external output disabled.

3. Props-off send evidence:
   - only after dry-run logs are stable;
   - bounded/audited messages;
   - vehicle remains disarmed.

4. Tethered hover:
   - only after separate reviewed test plan;
   - very small radius and strict abort.

### Що Треба Логувати

```text
endpoint_search_started
endpoint_search_pattern=cross|box
endpoint_search_step=center|forward|back|left|right
endpoint_search_radius_m
endpoint_search_elapsed_ms
endpoint_search_abort_reason
endpoint_confirmation_before_after
best_endpoint_gap_before_after
match_confidence_min_avg
telemetry_health
altitude_min_avg_max
```

### Чи Реально

Так, але не зараз як live movement. Реально після `ambiguous_endpoint_hold` і dry-run-only pattern evidence.

### Ризики

- Active search near ground/endpoint can move the drone into obstacles.
- If route texture is repetitive, search may strengthen a wrong alias.
- Without reliable velocity/position authority, even 0.5 m command can overshoot.
- Requires HOVER-quality stabilization from FC/JT_Zero side, not only Visual Homing.

### Поточний Статус

Good research direction. Not immediate live feature.

## Ідея 4: Endpoint Visual Landmark Use

### Що Це

Використовувати локальні ознаки endpoint з raw image або route artifact: петля дрота, плями, контрастні елементи, локальний shape/edge pattern.

### Для Чого

Щоб система могла працювати на реальному полі, де текстура не ідеальна, але є weak landmarks.

### Чому З'явилась Ідея

User помітив, що на фініші все ж є унікальність:

```text
петля дрота майже по всьому маршруту
деякі плями
локальні візуальні відмінності
```

### Можливі Реалізації

1. Higher-detail endpoint patch refinement:
   - coarse matcher знаходить endpoint candidate set;
   - тільки для top candidates запускається дорожчий local refinement.

2. Edge/shape consistency:
   - не повний ORB/SLAM;
   - прості edge maps або gradient patches для endpoint zone.

3. Temporal endpoint evidence:
   - не один кадр;
   - перевірка послідовності останніх N endpoint-zone кадрів.

4. Manual landmark annotation later:
   - оператор або GS може позначити endpoint landmark на keyframe;
   - поки це research, не flight authority.

### Чи Реально

Так, але треба уникати heavy full-route OpenCV/ORB на Pi Zero 2W. Пріоритет:

```text
coarse Gray8 full-route
small candidate set
optional local refinement only near endpoint
```

### Ризики

- Wire/spot landmarks можуть змінюватись або зникати.
- Wide FOV і altitude changes змінюють scale.
- Landmark може бути видимий уздовж майже всього маршруту, тому він не завжди endpoint-specific.

### Поточний Статус

Research. Може стати частиною endpoint confirmation v2.

## Ідея 5: Stop Frame / High-Resolution PNG / Ground Station Evidence

### Що Це

Зберігати кадр, на якому система прийняла endpoint/hold/search decision, і передавати або показувати його в Ground Station.

### Для Чого

Оператор має бачити не тільки числа, а й картинку:

```text
що саме бачила камера
який route_index був обраний
чому endpoint прийнятий або заблокований
```

### Що Уже Є

Stop-frame PGM export:

```text
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPORT_STOP_FRAME=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_FRAME_DIR=/home/pi/Visual_Homing_Codex/artifacts/stop_frames
```

Логи містять:

```text
endpoint_stop_frame_path
endpoint_stop_frame_id
endpoint_stop_route_index
endpoint_stop_progress
endpoint_stop_confidence
```

### Що Варто Додати

- PNG export або post-conversion helper.
- Frame export not only for confirmed endpoint, but also for:
  - `ambiguous_endpoint_hold`;
  - `endpoint_search_started`;
  - `endpoint_search_failed`;
  - `endpoint_confirmation_passed`.
- Optional higher-resolution frame snapshot, якщо capture path дозволяє.
- GS/API endpoint для отримання останнього decision frame.

### Чи Реально

Так. Це low-risk, дуже корисно для оператора і дебагу.

### Ризики

- High-resolution frame може бути heavy для Pi Zero 2W.
- Не можна блокувати realtime loop на PNG encoding.
- PNG краще робити async або offline post-run.

### Поточний Статус

PGM stop-frame implemented. PNG/GS integration later.

## Ідея 6: Route Time Awareness

### Що Це

Враховувати очікуваний час проходження маршруту як diagnostic prior, але не як основний gate.

### Для Чого

Route 2026-07-12 записаний приблизно за `20.03 s`. Якщо matching доходить до endpoint за 5 s або за 80 s, це може бути корисним diagnostic сигналом.

### Чому Не Можна Робити Це Головним Gate

Реальний RTL може йти швидше/повільніше через:

- вітер;
- altitude;
- FC mode;
- operator/vehicle speed;
- HOVER/search pauses;
- obstacle avoidance later.

### Можлива Реалізація

Лог-only first:

```text
route_elapsed_expected_ms
route_elapsed_actual_ms
route_elapsed_ratio
endpoint_time_plausible=true|false
```

Soft use:

```text
якщо endpoint зараховується дуже рано, збільшити підозру ambiguity
якщо endpoint ambiguous, але час/trajectory plausible, дозволити ambiguous_endpoint_hold
```

### Чи Реально

Так як diagnostic/soft prior. Ні як hard gate.

### Поточний Статус

Idea only.

## Ідея 7: Visual Homing RTL/HOVER State Machine

### Що Це

Явна state machine замість одного `passed/failed` endpoint рішення.

Можливі стани:

```text
route_acquire
route_follow
endpoint_confirmed
ambiguous_endpoint_hold
endpoint_micro_search
handoff_to_hover
operator_handoff
abort_fail_closed
```

### Для Чого

Реальна система не повинна залежати від одного route matcher boolean. Вона має розрізняти:

- маршрут не знайдений;
- маршрут знайдений і слідування нормальне;
- endpoint точно підтверджений;
- endpoint ймовірний, але ambiguous;
- треба маленький search;
- треба HOVER/оператор;
- треба abort.

### Чи Реально

Так. Це бажаний архітектурний напрямок до RTL/HOVER.

### Ризики

- State machine легко ускладнити.
- Кожен стан має мати timeout, abort і audit.
- Не можна вбудовувати live movement без поетапних dry-run доказів.

### Поточний Статус

Architectural next step after `ambiguous_endpoint_hold`.

## Ідея 8: Reverse Route Handling

### Що Це

Окрема acceptance policy для reverse runs.

### Для Чого

Reverse endpoint фізично працював на 2026-07-10 route, але strict provider readiness ще не був clean:

```text
external_nav_valid=534/547
external_nav_max_invalid_streak=9
external_nav_session_reason=external_nav_invalid_streak_high
```

### Можлива Реалізація

- Repeat reverse attach-only with current route.
- Tune initial progress window.
- Track invalid streak locations.
- Do not run reverse send-enabled until:

```text
external_nav_max_invalid_streak <= 3
external_nav_valid_fraction close to 1
external_nav_strict_session_ready=true
```

### Чи Реально

Так, але lower priority than forward endpoint ambiguity behavior.

### Поточний Статус

Pending.

## Ідея 9: Ground Station / Android Operator UI

### Що Це

Операторський UI, який показує Pi-owned readiness/decision state, route, camera profile, endpoint status, ambiguity state, stop frames and logs.

### Для Чого

Щоб оператор не читав raw logs у полі.

### Що Має Показувати

```text
current camera profile
selected route
route quality
live progress
confidence
telemetry health
external_nav readiness
endpoint confirmation status
ambiguous_endpoint_hold status
last decision frame
send/runtime status
armed/mode from telemetry
```

### Що Не Має Робити UI

- Не вирішувати safety gates самостійно.
- Не дублювати thresholds як authority.
- Не дозволяти live output без Pi-side confirmations and audit.

### Чи Реально

Так, але після стабілізації Pi JSON/API contract.

### Поточний Статус

Planned later. GS postponed until flight-core behavior is clearer.

## Ідея 10: Full Precision Hover vs Coarse Visual Return

### Що Це

Чітке розділення:

- Visual Homing RTL: грубе повернення по візуальному маршруту.
- HOVER / station keeping: окремий режим локального утримання.

### Для Чого

Не змішувати route progress matching з precision hover. Route matcher може сказати "ми біля endpoint", але не повинен сам бути повним hover controller.

### Реалістичний Підхід

1. Visual Homing доводить до endpoint zone.
2. Якщо endpoint confirmed - handoff.
3. Якщо endpoint ambiguous - hold/search candidate.
4. HOVER/JT_Zero/FC бере локальне утримання з власними gates.

### Чи Реально

Так, але це багатофазна робота. Не треба намагатись зробити повний hover тільки з route matcher.

### Поточний Статус

Architectural principle.

## Ідея 11: Visual Focus Window / Focus ROI Mode

### Що Це

Конфігурована "робоча зона" всередині кадру, схожа на focus mode у цифровій відеосистемі. Система може аналізувати не весь `160x100` processed frame однаково, а окреме внутрішнє вікно, де очікується стабільна текстура землі/маршруту.

Поточна поведінка:

```text
camera capture: 640x400
processed route frame: 160x100 Gray8
matcher input: full 160x100 frame
```

Ідея focus mode:

```text
green frame = full processed frame
yellow frame = focus ROI
dark/masked outside = ignored or diagnostic-only area
```

### Для Чого

Щоб зменшити вплив об'єктів на краях кадру:

- посадочні ніжки;
- частини дрона, які можуть з'явитись у полі зору;
- ноги оператора під час bench/hand-carried тестів;
- крайові wide-FOV distortion zones;
- випадкові тіні/рух на периферії.

Це має підвищити стійкість matching на реальному полі, де система не може вимагати ідеальної текстури або ідеального кадру.

### Чому З'явилась Ідея

Після overlay на keyframe стало видно, що core matcher зараз використовує весь кадр. User запропонував оцінювати центральнішу область, зміщену від країв до центру, щоб прибрати з аналізу ніжки/ноги/майбутні частини дрона.

Важливий висновок: обрізати `30%` з усіх сторін одразу занадто агресивно, бо залишиться лише `40% x 40%` кадру і можна втратити корисні орієнтири: петлю дрота, плями, бокову перспективу, endpoint texture.

### Рекомендований Перший ROI

Почати не з симетричного 30% crop, а з асиметричного focus window:

```text
left=12%
right=12%
top=8%
bottom=22%
```

Причина: нижня частина кадру найімовірніше містить посадочні ніжки, ноги оператора або частини дрона. Верх/боки теж трохи обрізаються, але не настільки, щоб знищити маршрутну текстуру.

### Режими

Потрібно мати мінімум три режими:

```text
full_frame
focus_roi_diagnostic
focus_roi_primary
```

`full_frame`:

```text
Поточна поведінка. Весь 160x100 кадр бере участь у route matching.
```

`focus_roi_diagnostic`:

```text
Паралельно рахуємо focus ROI match, але не впливаємо на stop, output або readiness.
```

`focus_roi_primary`:

```text
Route matching працює по focus ROI, а full-frame лишається sanity-check/fallback.
```

Починати тільки з `focus_roi_diagnostic`.

### Як Реалізовувати

Етап 1: Visualization only.

```text
Зробити keyframe overlays:
- зелена рамка = весь processed frame;
- жовта рамка = focus ROI;
- затемнена периферія = ignored/masked zone.
```

Етап 2: Diagnostic metrics.

На кожному live кадрі рахувати паралельно:

```text
full_progress
full_confidence
full_top_match_gap
full_route_index
focus_progress
focus_confidence
focus_top_match_gap
focus_route_index
focus_agrees_with_full
focus_endpoint_agrees_with_full
```

Етап 3: No authority comparison.

```text
focus_roi_diagnostic не змінює:
- endpoint_stop;
- ambiguous_endpoint_hold;
- external_nav output;
- route_session_ready;
- operator_readiness.
```

Етап 4: Acceptance evidence.

Після 2-3 forward/reverse attach-only прогонів порівняти:

```text
чи focus ROI має менше false endpoint;
чи focus ROI має кращий endpoint gap;
чи focus ROI не втрачає progress monotonicity;
чи confidence не падає нижче full-frame;
чи route_index agreement стабільний;
чи invalid streak не росте.
```

Тільки після цього розглядати `focus_roi_primary`.

### Що Треба Логувати

Бажані поля:

```text
focus_roi_enabled
focus_roi_mode=diagnostic|primary
focus_roi_left_fraction
focus_roi_right_fraction
focus_roi_top_fraction
focus_roi_bottom_fraction
focus_valid_matches
focus_confidence_min_avg
focus_progress_first_last
focus_tracked_progress_first_last
focus_top_match_gap_min_avg
focus_route_index_agreement_fraction
focus_endpoint_agreement
focus_invalid_reasons
```

Для endpoint:

```text
endpoint_full_confirmation_passed
endpoint_focus_confirmation_passed
endpoint_full_top_match_gap
endpoint_focus_top_match_gap
endpoint_focus_reason
```

### Як Перевіряти

Перший тестовий цикл:

```text
route=/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260712T164651Z.vhrs
expected_progress=forward
output_runtime_enabled=0
focus_roi_mode=diagnostic
```

Accepted diagnostic behavior:

```text
full-frame behavior unchanged
focus metrics present
no send
no stop-policy changes
no readiness changes caused by focus ROI
```

Після цього:

```text
1-2 forward attach-only
1-2 reverse attach-only
one endpoint stand-still diagnostic
compare full vs focus logs
```

### Чи Реально

Так. Це реалістично і low-risk, якщо почати як diagnostic-only. CPU overhead має бути помірним, бо ROI менший за full frame, а matching уже працює на `160x100`.

### Ризики

- Занадто малий ROI може втратити унікальні орієнтири і погіршити ambiguity.
- Якщо route записаний full-frame, а live match робиться ROI-only без однакового preprocessing, можна порівнювати різні представлення.
- Якщо focus ROI стане primary занадто рано, система може стати сліпою до корисних бокових ознак.
- ROI не вирішує endpoint ambiguity сам по собі; це додатковий evidence channel.

### Поточний Статус

Diagnostic-only implementation added locally:

```text
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_DIAGNOSTICS=1
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_LEFT=0.12
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_RIGHT=0.12
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_TOP=0.08
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_BOTTOM=0.22
VISUAL_HOMING_LIVE_ROUTE_MATCH_FOCUS_ROI_TOP_K=5
```

It logs focus ROI metrics only. It does not change stop, passed, readiness, external-nav estimates, or external-nav output behavior.

Next safe step: run forward/reverse attach-only with `FOCUS_ROI_DIAGNOSTICS=1` and compare full-frame vs focus metrics.

## Ідея 12: Night / Thermal Camera Route Mode

### Проблема

Daylight OV9281 routes не треба вважати валідними після сутінок або вночі. Вечірній probe 2026-07-13 показав типовий domain shift:

```text
confidence ~= 0.54
focus_roi_confidence ~= 0.55
valid=false
route_match_not_valid
progress stuck near route start window
```

Це схоже на low-light / exposure / contrast failure, а не на FC/JT_Zero acceptance result.

### Пропозиція

Для night tests і майбутньої нічної роботи використовувати окрему thermal USB camera через EasyCAP. Це має бути окремий sensor/input mode, а не продовження денного OV9281 route.

### Як Плануємо Реалізовувати

1. Додати окремий capture/input profile для EasyCAP thermal feed.
2. Записувати thermal-specific route artifacts; не змішувати їх з daylight OV9281 route.
3. Експортувати keyframes/overlays для thermal route, щоб оператор бачив, що реально обробляє matcher.
4. Спочатку прогнати attach-only forward/reverse evidence.
5. Лише після повторюваного thermal attach-only evidence переходити до props-off provider-send evidence.
6. Focus ROI diagnostics залишити увімкненими, але ROI retune робити тільки після перегляду реальних thermal кадрів.

### Чи Реально

Так, це реально і, скоріш за все, необхідно для night-capable Visual Homing. Основні ризики:

- thermal image може мати менше spatial detail;
- route distinctiveness і endpoint ambiguity треба переміряти з нуля;
- EasyCAP може додати іншу latency/format path;
- денні confidence thresholds можуть не підходити thermal domain.

### Не Робити

- Не використовувати OV9281 daylight route як доказ thermal/night readiness.
- Не змішувати daylight і thermal evidence в одному acceptance bucket.
- Не йти до RTL/HOVER night tests без окремої thermal repeatability бази.

## Ідея 13: Потоковий Багатомасштабний Маршрут І Розріджені 1280x800 Keyframes

### Що Це

Довгий маршрут не повинен накопичувати всі кадри в RAM. Основний tracking-шар треба записувати потоково на SD-карту, а окремий розріджений high-resolution шар використовувати для глобального reacquisition:

```text
Layer 0: 160x100 tracking signatures, відносно часто
Layer 1: компактні coarse descriptors для глобального індексу
Layer 2: 1280x800 Gray8 keyframes, рідко, для top-N verification
```

JT_Zero за ідеєю працює постійним локальним odometry/hold шаром. Якщо Visual Homing втрачає route match, JT_Zero утримує апарат і компенсує короткочасний знос, а Visual Homing виконує глобальний пошук та пізніше коригує локальний frame через перевірений handoff/reset-counter contract.

### Для Чого

- маршрути у кілька кілометрів не вміщуються у поточну RAM-модель recorder-а;
- висока роздільна здатність потрібна не для brute-force realtime matching, а для розрізнення кількох coarse кандидатів;
- зі зростанням висоти translational image motion приблизно зменшується як `ground_speed / altitude`, тому high-resolution keyframes можуть бути рідшими;
- `1280x800` має удвічі більшу лінійну роздільну здатність за `640x400`, тому приблизно компенсує двократне збільшення висоти для тих самих наземних деталей.

### Як Реалізувати

1. Додати library-only streaming writer для сумісного `VHRS v1`: `.partial`, bounded buffering, periodic entry-count checkpoint, finalize/rename.
2. Не робити `fsync` на кожний кадр; писати блоками й мати recovery scanner для останнього повного entry/chunk після втрати живлення.
3. Для повної crash recovery та кількох шарів спроєктувати chunked `VHRS v2` або route-directory manifest з окремими layer/chunk files, checksums і spatial/temporal index.
4. Обирати keyframe за пройденою JT_Zero local distance, зміною altitude/scale band, yaw/attitude або scene novelty, а не лише за фіксованим часом.
5. Під час пошуку спочатку порівнювати compact descriptors по всьому маршруту, потім перевіряти тільки top-N через high-resolution keyframes.
6. Не змішувати entries різних dimensions в одному matcher layer без explicit layer selection; поточний matcher очікує сумісний processed frame.

### Обов'язкові Metadata

```text
route/layer/chunk id
frame id and monotonic timestamp
JT_Zero local pose/displacement reference
relative altitude and altitude source
roll/pitch/yaw or reviewed camera attitude
camera profile, crop/FOV, exposure/gain
scale band and parent route segment
payload dimensions/format/compression
checksum/digest and finalized/checkpoint state
```

### Важливе Обмеження

Високий кадр бачить більший ground footprint, а не тільки менші копії низьких об'єктів. Якщо певну ділянку записано лише низько, один low-altitude кадр не можна простим resize перетворити на reference для значно більшої висоти. Recovery має залишатися всередині записаного scale envelope, або маршрут повинен мати spatial coverage потрібного altitude layer; майбутній mosaic з кількох низьких кадрів є окремою складною ідеєю.

### Як Перевіряти

- desktop round-trip та interrupted-write recovery tests;
- доказ bounded RAM на synthetic long route;
- Pi 10-15 minute recording matrix для target sizes і sparse `1280x800` rate;
- логувати effective FPS, latency percentiles, empty/dropped frames, RSS, SD write latency, temperature, CPU frequency і `vcgencmd get_throttled` до/під час/після;
- offline coarse-to-top-N reacquisition з altitude/scale mismatch negatives;
- лише після цього attach-only JT_Zero hold + Visual Homing search state-machine evidence.

### Поточний Статус

Accepted architecture direction on `2026-07-19`. Перший implementation slice завершено: unattached `RouteSignatureStreamWriter` дає byte-compatible `VHRS v1`, `.partial`, configurable checkpoints, explicit finalize і interrupted-tail suppression; WSL/MSVC проходять `37/37`. Camera runtime, recovery scanner/chunked v2, sparse native-resolution capture, multiscale matcher і JT_Zero handoff ще не підключені.

## Поточний Найближчий План

1. Library-only streaming `VHRS v1` writer без camera/runtime caller — виконано (`37/37`).
2. Додати bounded streaming recorder integration для довгих маршрутів, зберігши старий in-memory recorder для deterministic replay tests.
3. Спроєктувати route manifest/chunks/layers та sparse `1280x800` metadata/index contract.
4. Провести Pi thermal/load/storage benchmark перед вибором максимальної sparse-keyframe частоти.
5. Реалізувати offline global coarse search -> top-N high-resolution verification -> multi-frame reacquisition gate.
6. Лише потім композиційно додавати JT_Zero local hold, bounded yaw search і ODOMETRY reset-counter recovery.

Історична endpoint-черга залишається збереженою нижче, але не повинна випереджати bounded long-route storage/reacquisition groundwork:

1. Виправити/підтвердити endpoint confirmation reason після hotfix `5d4bdeb`.
2. Додати `ambiguous_endpoint_hold` як log-only / attach-only behavior.
3. Прогнати forward attach-only на route `20260712T164651Z`:

```text
expected: endpoint_confirmation_passed=false
expected: ambiguous_endpoint_hold=true
expected: external_nav_valid=all/all
expected: no send
```

4. Оновити readiness JSON, щоб UI/оператор бачив `ambiguous_endpoint_hold`, а не generic `route_session_not_passed`.
5. Лише після цього проектувати dry-run endpoint micro-search.

## Правило Безпеки Для Цього Документа

Ідея в цьому файлі не є дозволом на armed/tethered/free-flight test. Будь-яка активна поведінка дрона повинна пройти:

```text
code review
unit tests
replay/offline tests
attach-only dry-run evidence
props-off send evidence if writer involved
separate reviewed tethered test plan
explicit operator approval
```
