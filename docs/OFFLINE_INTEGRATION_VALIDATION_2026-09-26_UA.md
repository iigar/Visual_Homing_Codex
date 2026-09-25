# Offline-перевірка matcher → progress → endpoint/readiness — 2026-09-26

Після `a5513da` додано `live_route_offline_integration_test`: **24 детерміновані
сценарії** викликають справжній Gray8 matcher, progress tracking, endpoint decision
і фінальні route-quality/readiness functions. Production-код і policy не змінені.
Debug та Release проходять **58/58 CTest**, Python suites — **15/15** та **5/5**.

## Яку прогалину закрито

До цього кроку `live_route_progress_test` перевіряв progress на заданих числах;
`live_route_endpoint_test` — endpoint і частково його композицію з progress;
`live_route_readiness_test` — readiness на вручну підготовлених counters/results.
Жоден із них не отримував ці дані від справжнього matcher в одному сценарії.
Наявний `match_replay_route` перевіряє іншу гілку: resize, matcher, health і
dry-run navigator; він не викликає ці progress/endpoint/readiness helpers.

Новий test-only fixture виконує порядок із `match_live_camera_route`:

1. `Gray8RouteMatcher::match`, `recent_top_candidates`, `probe_edge_diagnostics`.
2. Вимірювання віку кадру/затримки через `HealthMonitor` із заданими timestamps.
3. `live_route_match_record_progress` передає optional tracked progress того самого кадру.
4. `live_route_match_update_endpoint` отримує справжні raw/edge gaps і processing time.
5. Після послідовності — `live_route_match_evaluate_route_quality`, потім
   `live_route_match_evaluate_session_readiness`.

Алгоритми підрахунку distance, smoothing, dwell і readiness не скопійовані
в fixture. Тестова обв'язка лише створює вхід, передає результати та перевіряє
очікувані рішення. Порядок і формули двох candidate gaps звірені з camera loop.
Сам `match_live_camera_route`, джерело кадрів, його cleanup і runtime wiring
цим тестом не виконуються: це перевірка композиції API, не повного live runtime.

## Сценарії й результати

Кожен сценарій виконується вперед і назад. Маршрут — 17 Gray8 кадрів 16×12:
детерміновані різні візерунки, дубль endpoint або кадри різної сталої яскравості
з однаковими edge maps. Незбіг створюється зміною пікселів, не підміною `valid`.
Frame time та processing time задаються незалежно; sleep/wall-clock немає.

| Сценарій | Кількість | Перевірений результат |
|---|---:|---|
| Повний прохід: global, window=3, window=3 + scale | 6 | Endpoint не спрацьовує на 99 ms dwell, спрацьовує на 100 ms; 19 валідних кадрів достатньо замість запитаних 64 завдяки підтвердженому early stop; route session проходить |
| Стрибок від початку до raw endpoint | 2 | Raw match уже на кінці, tracked progress ще ні; dwell не починається |
| Невалідний кадр під час dwell, потім відновлення | 2 | Optional tracked progress відсутній, попереднє tracked значення збережене; timer не скинутий; endpoint згодом спрацьовує, але route session не проходить |
| Дубль endpoint, ambiguous hold вимкнено | 2 | Gap=0, причина `top_match_gap_low`; підтвердженого stop немає навіть після тривалої паузи |
| Дубль endpoint, ambiguous hold увімкнено | 2 | Hold не спрацьовує на 199 ms, спрацьовує на 200 ms; не прирівнюється до підтвердженого early stop |
| Унікальний raw match, однакові edge maps | 2 | Raw gap достатній, edge gap=0; причина `edge_top_match_gap_low`, stop заблоковано |
| Порожня/часткова послідовність або лише endpoint | 6 | Сесія не проходить; знімки лише кінця не підтверджують проходження від початку |
| Старі однакові frame timestamps | 2 | Вік кадру виміряно, dwell залежить від processing time; route-only session проходить попри старі timestamps |

Додатково перевіряються counters, stop frame ID/index, raw і tracked progress,
confidence, причина stop і збереження timestamp від frame до match. Для кожної
фіналізації external-nav readiness лишається `not_requested`: оцінок estimator
у цьому тесті немає. Output allowance, sends та експорт зображень не виникають.

## Дві підтверджені межі поточної policy

**Невалідний проміжок не перериває dwell.** У сценарії dwell почався на 2600 ms,
кадр на 2650 ms не зіставився, на 2800 ms збіг відновився. Endpoint-stop спрацьовує
з dwell=200 ms. Загальний route result при цьому `passed=false`: 18 валідних
зіставлень із 19. Ця поведінка вже була зафіксована unit-тестом endpoint;
тепер вона підтверджена через справжній matcher. Безперервність лише валідних
спостережень не є чинною умовою цього timer.

**Route-only readiness не перевіряє freshness.** У другому сценарії timestamp
усіх кадрів — 0; останній processing start — 2698 ms. `HealthMonitor` і result
показують `frame_age_ms=2698`, але валідні піксельні збіги, route progress і dwell
дають `passed=true`. Це режим без dry-run commands, telemetry requirements та
verification producer. Age не входить у route-only критерій `passed`.
Висновок не поширюється на downstream navigator/verification/output gates:
їхні freshness checks цей fixture не викликає.

Жодна з цих знахідок сама по собі не є дозволом на навігацію або доказом її
працездатності. Зміна dwell/freshness policy потребує окремого кроку зі звіренням
всіх consumers; цей commit лише робить наявну поведінку відтворюваною.

## Відтворення й перевірки

```sh
bash scripts/test-software.sh artifacts/config-validation-20260921/clean-build
ctest --test-dir artifacts/config-validation-20260921/clean-build/Debug \
  -R '^live_route_offline_integration$' --output-on-failure
```

Сценарії не потребують файлів із борту, мережі, камери чи sleep. Локальні логи:
`artifacts/offline-integration-20260926/target-build.log`, `target-test.log`,
`software-tests.log`, `no-tests-build.log`. Target спочатку пройшов із чинною
production library, потім у повній suite. Debug/Release — інкрементальні builds
на попередніх чистих каталогах; assertions активні також у Release.
`BUILD_TESTING=OFF` збирається з 0 тестів. Усі сім camera/output flags OFF.
Нового sanitizer, Pi або MSVC прогону в цьому test-only кроці немає.

Graphify query використано для навігації, після зміни виконано update.
GitNexus impact чотирьох helpers повернув неповний `UNKNOWN` і не відобразив
фактичних callers; FTS недоступний навіть після CLI refresh. Тому coverage
inventory і порядок викликів перевірено за кодом і виконанням тестів.

Фінальний індекс Graphify — 5822 nodes / 8952 edges, GitNexus CLI —
13525 nodes / 26980 edges. Staged `detect_changes` охопив п'ять очікуваних файлів,
але показав `critical`/209 процесів, усі приписані тестовому `at_ms` з порожнім ID,
включно зі сторонніми JSX. Це helper в anonymous namespace нового test executable;
production library його не містить. Порівняння всієї гілки з `main` охоплює 45
файлів попередніх і поточного кроків та повторює empty-ID attribution anomaly
для endpoint helper (`critical`/206 процесів). Граф не є clean acceptance;
прямий diff підтвердив незмінність `core/src` і `core/include` відносно `a5513da`.
Staged whitespace check пройшов. Сирі результати — `graph-impact.json` та
`graph-final-review.json` у каталозі артефактів.

## Наступний обмежений крок

Перевірити ті самі stale/invalid observations у фактичних downstream consumers
`BoundedNavigator` і progress-only verification: які age/health умови блокують
дані, чи є прогалина між їхніми вимогами й route-only reporting. Спочатку
інвентаризація наявних тестів та локальний сценарій; без зміни policy чи
підключення output. Незалежну точність розпізнавання цей синтетичний тест не
вимірює; для неї все ще потрібен повний другий прохід із розміткою.
