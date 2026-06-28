# Звіт Польових Тестів І Наступні Кроки

Цей документ підсумовує вже виконані тести Visual Homing, висновки, які з них можна безпечно зробити, і рекомендовані команди для наступної польової сесії.

Англомовний оригінал: [`FIELD_TEST_REPORT_AND_NEXT_STEPS.md`](FIELD_TEST_REPORT_AND_NEXT_STEPS.md).

## Поточний Стан

Поточний підтверджений baseline: ручний dry-run повернення по Visual Homing з такими умовами:

- Raspberry Pi Zero 2W;
- активний профіль Pi camera;
- route signatures у розмірі `96x72`;
- live MAVLink telemetry тільки для читання;
- тільки dry-run навігаційні команди;
- без live MAVLink command output;
- endpoint-stop увімкнений для вимірювання часу до endpoint;
- visual scale diagnostics увімкнені, але не використовуються як readiness gate.

Останній прийнятий route artifact:

```text
/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260624T210459Z.vhrs
```

Цей маршрут корисний як локальний baseline, але для наступної outdoor/field сесії треба записати свіжий маршрут саме на місці тестування.

## Що Вже Доведено

- `96x72` route matching працює на Pi приблизно на `15 FPS`.
- Route matcher може розпізнавати reverse route і доходити до endpoint readiness.
- Tracked route-progress краще переносить ручний jitter, ніж raw progress.
- Endpoint-stop працює: dry-run зупиняється при досягненні endpoint progress, а не завжди добиває всі `150` кадрів.
- External-nav readiness JSON і log checks працюють і для повних `150` кадрів, і для endpoint-stop сесій.
- Operator summaries вже доступні як короткий текстовий вивід; це прототип майбутніх Android UI cards.
- Live output у поточних тестах залишається заблокованим, зазвичай з причиною `vehicle_not_armed:<frames>`.

## Що Ще Не Доведено

- Реальне повернення в польоті.
- Реальний MAVLink command output у flight controller.
- Прийняття flight controller-ом ExternalNav / position-provider даних.
- Runtime handoff до JT_Zero.
- Visual scale як safety або handoff gate.
- Поведінка на великих дистанціях і висотах, наприклад `20 m` проти `200 m`.

## Важливі Висновки

### Route Readiness

Останні route-completion тести показали стабільну readiness на двох ручних діапазонах висоти:

```text
Вищий hand-carried baseline:
  altitude avg: приблизно 1.75..1.85 m
  endpoint time: 7.053..7.584 s
  readiness: READY

Нижчий hand-carried baseline:
  altitude avg: приблизно 0.84..0.90 m
  endpoint time: 8.318..8.582 s
  readiness: READY
```

Один blocked-прогін у серії мав нормальні FPS, confidence, telemetry, dry-run commands і altitude, але не досяг endpoint. Його tracked route delta була значно меншою, ніж у успішних прогонів. Це інтерпретується як неповне фізичне проходження reverse-маршруту, а не як збій matcher або продуктивності Pi.

### Scale Diagnostics

Visual scale diagnostics рухаються в очікуваному загальному напрямку:

- нижча ручна висота дала вищі average scale ratios;
- вища ручна висота дала нижчі average scale ratios.

Але histogram досі широкий і часто впирається в межі кандидатів, наприклад `0.3` або `1.5`. Тому:

- тримаємо `VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1` для збору evidence;
- тримаємо `VISUAL_HOMING_SCALE_REFINEMENT=0` для нормальних польових тестів;
- поки не використовуємо visual scale як readiness gate.

### Optical Flow

Поточний Visual Homing не використовує класичний optical flow для навігації.

У control path зараз немає Lucas-Kanade, Farneback, KLT, ORB-flow або dense optical-flow. Поточна візуальна логіка базується на `Gray8RouteMatcher`:

- normalized mean absolute difference по Gray8 route entries;
- horizontal shift search для direction/yaw error;
- temporal tracked progress;
- optional visual-scale diagnostics.

MAVLink optical-flow повідомлення можна інспектувати, якщо вони присутні, але Visual Homing їх зараз не використовує.

## Команди, Які Вже Використовувались

Усі команди запускати з кореня репозиторію на Pi:

```bash
cd ~/Visual_Homing_Codex
```

### Оновити Код На Pi

```bash
git pull
```

Забирає останній закомічений код, скрипти і документацію з GitHub.

### Записати Польовий Маршрут

Рекомендована команда запису маршруту для наступної польової сесії:

```bash
VISUAL_HOMING_CAMERA_TARGET_WIDTH=96 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=72 \
VISUAL_HOMING_CAMERA_FRAMES=150 \
VISUAL_HOMING_FIELD_ROUTE_USE_LIVE_TELEMETRY=1 \
./scripts/run-field-route-record-pi.sh
```

Що вона робить:

- записує новий `.vhrs` route artifact;
- експортує keyframes;
- інспектує формат і розмір маршруту;
- запускає self-match;
- запускає perturbation checks;
- запускає route distinctiveness diagnostics;
- пише route-quality log.

Маршрут приймаємо тільки якщо wrapper друкує `route_quality_log_check passed=true`. Якщо quality fails, треба подивитись keyframes і одразу записати маршрут ще раз.

### External-Nav Reverse Dry-Run З Endpoint Stop

Використовувати після запису нового прийнятого маршруту. Замінити `<route.vhrs>` і поля висоти.

```bash
VISUAL_HOMING_ROUTE_OUTPUT=<route.vhrs> \
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=<expected-height-m> \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=<tolerance-m> \
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=0 \
VISUAL_HOMING_CAMERA_TARGET_WIDTH=96 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=72 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=reverse \
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.75 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1 \
VISUAL_HOMING_SCALE_REFINEMENT=0 \
VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M=<record-reference-height-m> \
VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M=3 \
VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M=<expected-height-m> \
./scripts/run-external-nav-dry-run-pi.sh
```

Що вона робить:

- робить read-only MAVLink telemetry preflight;
- перевіряє relative altitude sanity;
- запускає live camera route matching;
- генерує тільки dry-run navigation commands;
- генерує external-nav readiness estimates;
- рано зупиняється, якщо endpoint досягнуто;
- експортує readiness JSON;
- перевіряє readiness log.

Очікуваний безпечний результат для поточного dry-run етапу:

```text
external_nav_readiness_log_check passed=true
external_nav_operator_readiness=ready
endpoint_stop=true
stop_reason=endpoint_progress_reached
live_output_gate_block_reasons=vehicle_not_armed:<frames>
```

### Команда Для Відомого Baseline Route

Ця команда використовує вже прийнятий `96x72` route:

```bash
VISUAL_HOMING_ROUTE_OUTPUT=/home/pi/Visual_Homing_Codex/artifacts/field_routes/field-route-20260624T210459Z.vhrs \
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=1.8 \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=0.45 \
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=0 \
VISUAL_HOMING_CAMERA_TARGET_WIDTH=96 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=72 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=reverse \
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.75 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1 \
VISUAL_HOMING_SCALE_REFINEMENT=0 \
VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M=0.75 \
VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M=3 \
VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M=1.8 \
./scripts/run-external-nav-dry-run-pi.sh
```

Використовувати тільки для локальних repeatability checks. Для наступної outdoor-сесії спочатку треба записати свіжий маршрут.

### Інженерна Summary Таблиця

```bash
./scripts/summarize-external-nav-runs.sh \
  <external-nav-dry-run-1.log> \
  <external-nav-dry-run-2.log>
```

Що вона робить:

- друкує TSV table;
- показує readiness, FPS, elapsed time, endpoint time, confidence, altitude, route progress, tracked progress, dry-run validity і scale diagnostics.

Використовувати для порівняння кількох прогонів.

### Operator Summary

```bash
./scripts/operator-readiness-summary.sh \
  <external-nav-dry-run-1.log> \
  <external-nav-dry-run-2.log>
```

Що вона робить:

- друкує короткий human-readable summary;
- є текстовим прототипом майбутнього Android readiness UI.

Приклад:

```text
READY | route complete | endpoint 7.05s | alt 1.84525m | FPS 15.03 | conf 0.880073/0.913902 | reason valid
detail | frames 106/150 | tracked_delta 0.62 | endpoint_stop true | stop endpoint_progress_reached | log ...
```

## План Наступної Польової Сесії

### Мета

Записати свіжий outdoor route і довести reverse Visual Homing readiness на цьому маршруті без live command output.

### Фізична Підготовка

- Використати безпечну відкриту локацію.
- Зняти пропелери, якщо дрон живиться так, що потенційно може бути armed.
- На цьому етапі не армити.
- Камера має бути закріплена так, як у реальній конфігурації.
- Нести стабільно, без різких yaw-поворотів.

### Крок 1 - Записати Новий Маршрут

```bash
cd ~/Visual_Homing_Codex
git pull

VISUAL_HOMING_CAMERA_TARGET_WIDTH=96 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=72 \
VISUAL_HOMING_CAMERA_FRAMES=150 \
VISUAL_HOMING_FIELD_ROUTE_USE_LIVE_TELEMETRY=1 \
./scripts/run-field-route-record-pi.sh
```

Цільовий маршрут:

- для початку короткий: приблизно `10..15 m`;
- повторювана пряма або м'яко вигнута траєкторія;
- по можливості уникати дуже однотонної поверхні;
- тримати висоту приблизно стабільною.

Якщо route quality fails, одразу записати ще раз.

### Крок 2 - Перевірити Keyframes

Перевірити директорію keyframes, яку надрукує команда запису. Потрібно підтвердити:

- start і end достатньо різні;
- випадкова пауза не домінує над маршрутом;
- немає сильного blur;
- напрямок маршруту зрозумілий.

### Крок 3 - Перший Reverse Endpoint-Stop Dry-Run

Використати новий route path, який надрукувала команда запису:

```bash
VISUAL_HOMING_ROUTE_OUTPUT=<new-route.vhrs> \
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=<measured-height-m> \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=0.45 \
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=0 \
VISUAL_HOMING_CAMERA_TARGET_WIDTH=96 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=72 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=reverse \
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.75 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1 \
VISUAL_HOMING_SCALE_REFINEMENT=0 \
VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M=<record-height-m> \
VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M=3 \
VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M=<measured-height-m> \
./scripts/run-external-nav-dry-run-pi.sh
```

Ціль прийняття:

- `passed=true`;
- `external_nav_operator_readiness=ready`;
- `endpoint_stop=true`;
- `stop_reason=endpoint_progress_reached`;
- `effective_fps` близько `15`;
- `dry_run_valid=<frames>/<frames>`;
- live output досі blocked через `vehicle_not_armed:<frames>`.

### Крок 4 - Повторити

Повторити той самий reverse dry-run мінімум два рази.

Потім зробити summary:

```bash
./scripts/operator-readiness-summary.sh <log1> <log2> <log3>

./scripts/summarize-external-nav-runs.sh <log1> <log2> <log3>
```

### Крок 5 - Якщо Run Blocked

Використати summary, щоб розділити причини:

- `fps` низький або `capture_timeout`: проблема продуктивності Pi.
- confidence низький: проблема visual route / matcher.
- altitude window false: неправильна expected altitude або tolerance.
- `tracked_delta` значно менший, ніж в успішних прогонів: імовірно неповне фізичне проходження.
- `endpoint=false` при інших здорових метриках: імовірно фізично не дійшли до start/end route.
- `live_output_gate` не blocked, коли дрон не armed: safety issue, тестування зупинити.

## Props-Off Audit Rehearsal

Це ще не real live-output test. Це той самий endpoint-stop dry-run зі знятими пропелерами і не armed vehicle, щоб підтвердити: Visual Homing може бути ready, але live output лишається blocked.

Використовувати тільки після запису й прийняття свіжого route на місці тестування.

Очікуваний результат:

```text
route complete: yes
operator readiness: READY
endpoint_stop: true
live output: blocked
block reason: vehicle_not_armed:<frames>
```

Форма команди:

```bash
VISUAL_HOMING_ROUTE_OUTPUT=<new-route.vhrs> \
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_PRESET=custom \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_M=<measured-height-m> \
VISUAL_HOMING_EXTERNAL_NAV_EXPECTED_RELATIVE_ALTITUDE_TOLERANCE_M=0.45 \
VISUAL_HOMING_EXTERNAL_NAV_MAX_RELATIVE_ALTITUDE_SPAN_M=0 \
VISUAL_HOMING_CAMERA_TARGET_WIDTH=96 \
VISUAL_HOMING_CAMERA_TARGET_HEIGHT=72 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_EXPECTED_PROGRESS=reverse \
VISUAL_HOMING_LIVE_ROUTE_MATCH_ENDPOINT_END_PROGRESS=0.75 \
VISUAL_HOMING_LIVE_ROUTE_MATCH_STOP_AT_ENDPOINT_PROGRESS=1 \
VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1 \
VISUAL_HOMING_SCALE_REFINEMENT=0 \
VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M=<record-height-m> \
VISUAL_HOMING_HANDOFF_REQUESTED_DISTANCE_M=3 \
VISUAL_HOMING_HANDOFF_REQUESTED_ALTITUDE_M=<measured-height-m> \
./scripts/run-external-nav-dry-run-pi.sh
```

Для цього rehearsal не армити. Не вмикати attached live MAVLink writer. Мета - підтвердити, що route доходить до `READY`, а output gate все ще блокує реальну command authority.

## Поки Не Робити

- Не армити для цих dry-run route tests.
- Не вмикати attached live MAVLink writer у полі.
- Не використовувати visual scale як gate.
- Не використовувати старий локальний route як доказ для нової outdoor-локації.
- Не переходити до real flight return, поки ExternalNav writer, safety audit і props-off live-output audit не будуть окремо доведені.

## Рекомендований Наступний Development Після Field Route Baseline

Коли свіжий outdoor route матиме мінімум три `READY` endpoint-stop reverse dry-runs:

1. Додати props-off audit checklist і набір команд.
2. Дати readiness JSON ті самі спрощені operator fields, які зараз друкує `operator-readiness-summary.sh`.
3. Підготувати Pi-owned Android/API contract, який читає readiness JSON, а не raw logs.
4. Тримати JT_Zero handoff як майбутній same-Pi module після стабілізації Visual Homing route return.
