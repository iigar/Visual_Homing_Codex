# Freshness у navigator та verification — 2026-09-26

Продовження `6966e38`: route-only перевірка може пройти на старих кадрах,
тому окремо перевірено справжні downstream consumers. Новий
`match_freshness_integration_test` проходить **21 сценарій** зі справжнім
Gray8 matcher, progress adapter, `BoundedNavigator` і `LiveRouteVerificationProducer`.
Production-код та policy не змінені. Debug/Release — **59/59 CTest**,
Python suites — **15/15** та **5/5**.

## Що саме виконується

До цього `bounded_navigator_test` уже перевіряв stale/future/invalid matches,
health flags та скидання slew state на вручну створених RouteMatch.
`live_route_verification_test` перевіряв rejection reasons, publisher admission,
backpressure і failure на вручну підготовленій observation. Новий тест передає
результати справжнього matcher обом consumers і перевіряє їхню різну реакцію
на той самий вхід, включно з межами віку та відновленням.

Маршрут містить три синтетичні Gray8 кадри 16×12. Matcher обчислює confidence,
timestamp і direction validity; progress helper — tracked progress; HealthMonitor
обчислює timing/confidence і snapshot. Потім чинний
`make_progress_only_live_route_verification_observation` прив'язує дані того самого
кадру. Link flags, altitude/scale та їхні timestamps задаються тестом; це не
отримані з апаратури вимірювання. У тесті є справжній publisher worker, але його
storage processor замінено лічильником у пам'яті. Файли route revisions не пишуться.

Navigator повертає лише об'єкт команди в пам'яті: command sink/live session
не запускається. Це порівняння окремих API, не поєднання несумісних operational
command та progress-only verification режимів. Operational validation вимагає
для verification MAVLink health; у тесті `require_mavlink_health=true`.

## Результати

Для зіставного тесту обом consumers задано максимальний вік frame/match
**200 ms**, scalar age теж **200 ms**. Це тестова конфігурація, не нові defaults.
Перевірка «на один такт більше» використовує `Clock::duration{1}`, у WSL — 1 ns.

| Вхід | Navigator | Verification |
|---|---|---|
| Свіжий match; вік 0 або рівно 200 ms | Валідна команда | Accepted |
| Вік 200 ms + один такт | Невалідна нульова команда | `health_frame_context_stale` |
| Frame timestamp на один такт у майбутньому | Невалідна нульова команда | `health_frame_context_stale` |
| Старий ненульовий timestamp: вік 2699 ms | Невалідна нульова команда | `health_frame_context_stale` |
| Пікселі не проходять matcher threshold | Невалідна нульова команда | `route_match_invalid` |
| Свіжий збіг після відмов | Знову валідна команда | Accepted; передано tracked progress, не raw endpoint |
| Втрата camera, MAVLink або navigation flag | Невалідна нульова команда | `health_not_ready` |
| Match confidence=1, health confidence=0.999 | Проходить threshold 0.95 | `health_match_confidence_mismatch` |
| Native frame timestamp відрізняється від match на один такт | Match/health залишаються прийнятними | `route_match_frame_mismatch` |
| Altitude/scale віком рівно 200 ms | Не використовує ці scalars | Accepted |
| Altitude/scale старші за межу на такт або з майбутнього | Команда валідна | Відповідно `altitude_observation_invalid` / `scale_observation_invalid` |
| Frame timestamp=0, health timestamp=100 ms | Проходить перевірку відносного віку | `health_frame_context_stale`: нульовий timestamp не приймається |
| Валідний match із вимкненим direction estimation | Валідна команда | `yaw_observation_invalid` |

У кожному rejected verification case перевірено відсутність context/submission,
незмінність лічильника звернень до publisher та числа виконаних jobs.
Прийнятий case проходить справжню чергу, завершується worker-ом і не залишає
outstanding jobs. Після stale/invalid sequence прийнято свіжий кадр, причому
context зберігає саме optional tracked progress та не містить local pose.

## Як тлумачити відмінності

Route-only `passed`, валідність NavigationCommand та допуск verification —
три різні результати. Попередній route-only pass на старих кадрах не переноситься
на navigator/verification: за їхніми власними часовими межами старий match
відхиляється. Обидва спираються на `health.timestamp` як evaluation time;
незалежного читання `now()` у цих API немає. Походження цього timestamp у live
caller новим тестом не перевірене.

Verification має додаткові вимоги до зв'язності evidence: ненульовий timestamp,
рівність frame/match timestamp, узгоджена confidence та валідний свіжий yaw/scalars.
Navigator не має окремого zero-timestamp guard і не вимагає
`direction_observation_valid` у своєму admission condition. Тест підтверджує
цю відмінність контрактів; він не оголошує її новою помилкою чи дозволом
на використання таких даних у реальній навігації. Policy tightening, якщо потрібен,
має бути окремим кроком зі звіренням callers.

## Відтворення й межі

```sh
bash scripts/test-software.sh artifacts/config-validation-20260921/clean-build
ctest --test-dir artifacts/config-validation-20260921/clean-build/Debug \
  -R '^match_freshness_integration$' --output-on-failure
```

Сценарні clocks детерміновані; очікування worker використовує condition-variable
drain із timeout 1 s, без sleep. Тест не вимірює latency або throughput.
Target спочатку пройшов на поточній production library; після явної ініціалізації
порожнього local-frame contract повна збірка пройшла без нових compiler warnings.
Assertions активні в Release. `BUILD_TESTING=OFF` збирається з 0 тестів,
усі сім camera/output flags OFF у трьох конфігураціях.

Логи й graph evidence: ignored `artifacts/downstream-freshness-20260926/`.
Production `core/src` та `core/include` незмінені відносно `6966e38`.
Перевірка не охоплює native capture, runtime caller wiring, storage durability,
запис справжніх verification revisions або hardware/output acceptance.
Нового sanitizer/Pi/MSVC прогону в цьому test-only кроці немає.

Graphify оновлено до 5859 nodes / 9029 edges, GitNexus CLI — до
14082 nodes / 29370 edges. Impact повертав неповний `UNKNOWN`; FTS недоступний.
Staged change analysis охопив п'ять очікуваних файлів, але приписав усі 206
процесів (`critical`) локальному тестовому `at_ms` з порожнім ID, включно з JSX.
Прямий CMake/diff review підтвердив, що helper існує лише в test executable.
Branch comparison із `main` охоплює 47 файлів та 209 процесів: переважно ту саму
empty-ID аномалію endpoint helper, а також parser flows попереднього numeric fix.
Parser source у поточному кроці не змінений; його тести входять до повної suite.
Це обмеження графа, не clean impact verdict. Staged whitespace check пройшов;
сирі відповіді збережено у `graph-impact.json` і `graph-final-review.json`.

## Наступний крок

Звірити походження frame/health/scalar timestamps і порядок викликів у чинних
програмних callers із перевіреною композицією API. Встановити конкретну
прогалину для мінімального спільного replay/runtime seam, який дозволить
перевіряти фактичний caller wiring без копіювання camera loop. До зміни policy
або апаратних запусків у цьому software slice не переходити.
