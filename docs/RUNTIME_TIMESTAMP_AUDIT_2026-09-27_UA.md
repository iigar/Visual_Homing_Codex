# Походження timestamps і перевірка replay caller

Дата: `2026-09-27`. Початковий commit: `8f95fbd71553e88958e9ebc2f5c3fc1455d54958`, гілка `refactor/optimization-tech-debt`.

Виконано програмний аудит походження часу та додано керований годинник до справжнього `match_replay_route`. Новий тест проходить повний offline loop із файлами, resize, matcher, health, scripted telemetry, navigator і журналом dry-run commands. Окремо локально відтворено два дефекти live telemetry caller: повторне надання свіжого часу старому буферу та пропуск нової altitude sample після витіснення даних із буфера. **Ці два дефекти в цьому кроці не виправлено.** Camera runtime і production policy не змінювалися.

## Походження часу за поточним кодом

| Етап | Джерело timestamp / момент оцінки | Межа висновку |
|---|---|---|
| `PiCameraSource::Backend::on_request_completed` | `frame.timestamp = now()` у callback, перед копіюванням і додаванням у чергу | Це час callback на host, не timestamp експозиції сенсора; затримку до callback він не вимірює |
| Camera queue / `poll` | Зберігає timestamp; черга обмежена двома кадрами | Очікування в черзі входить у подальший вік кадру |
| `Gray8ResizePreprocessor` / `Gray8RouteMatcher` | Копіюють timestamp вхідного кадру | Matcher сам не перевіряє свіжість |
| `ReplayFrameSource` | Читає nanoseconds із manifest у clock epoch; видає записи в порядку manifest | Немає pacing або автоматичного перенесення timestamps до поточного host time |
| `match_replay_route` | Початковий health time і start/end кожного кадру; тепер доступний явний clock callback | Старий двоаргументний API як і раніше використовує `now`; caller нового API має надати час у тій самій epoch, що й manifest |
| `match_live_camera_route` | `processing_finished` після match/raw/edge/focus diagnostics; далі telemetry, health snapshot, navigator | Snapshot time фіксується раніше за наступні етапи |
| Live visual scale / verification / endpoint | Scale обчислюється після `processing_finished`; scale observation прив'язана до processed frame, altitude — до окремого cached observation; endpoint отримує раніше знятий `processing_finished` | Звітна processing latency не охоплює ці пізні етапи. Це висновок із порядку коду, не виміряне перевищення live deadline |
| `MavlinkTelemetryStream::snapshot` | Під mutex інспектує поточні retained bytes і повертає counters | Немає часу отримання окремих повідомлень; `relative_altitude_samples` рахує samples у поточному вікні |

Джерела: `core/src/pi_camera_source.cpp`, `gray8_resize_preprocessor.cpp`, `gray8_route_matcher.cpp`, `replay_frame_source.cpp`, `pipeline_harness.cpp`, `camera_smoke.cpp`, `mavlink_telemetry_stream.cpp`, `mavlink_telemetry_inspector.cpp`, `mavlink_telemetry_adapter.cpp`.

## Відтворені невиправлені дефекти caller

Використано точний блок `camera_smoke.cpp:2233–2252`, автоматично виділений зі source, справжні `MavlinkTelemetryByteBuffer`, inspector, validator та adapter. Синтетичні packet helpers узято з наявного inspector test. Не запускалися serial stream, camera loop або hardware/output session. Це виконання конкретного фрагмента caller з реальними програмними залежностями, а не перевірка цілого live loop.

### Старий буфер знову вважається свіжою телеметрією

Перший snapshot містить 102 bytes і прийнятий у `1000 ms`. Потім нові bytes не надходять. У `10000 ms`, за test limit `500 ms`, adapter правильно повертає `mavlink_ok=false` до повторного виконання caller block. Validator retained buffer знову проходить, caller встановлює `latest_gate_telemetry.timestamp = processing_finished` і викликає `observe` з поточним часом. Після цього `mavlink_ok=true`, хоча реальний вік отриманого fixture становить `9000 ms`.

Cached altitude timestamp при цьому залишається `1000 ms`, тож окремий altitude freshness gate може відхилити verification. Результат доводить помилкове оновлення telemetry health, **не** проходження всіх verification/output gates або відправлення команди на борт.

### Нове значення висоти не замінює cached observation

Buffer capacity у fixture дорівнює 102 bytes. Другий 102-byte пакет повністю замінює retained tail і містить нову висоту `1.0 m` замість `42.5 m`. Загальний `bytes_captured` зростає зі 102 до 204, але кількість altitude samples у поточному вікні залишається `1`.

Caller перевіряє `relative_altitude_samples > last_relative_altitude_samples`. Умова хибна; inspector уже бачить `1.0 m`, а cached verification altitude лишається `42.5 m` із timestamp `1000 ms`. Лічильник елементів обмеженого вікна не є послідовним ідентифікатором нових samples; кеш може перестати оновлюватися після заповнення буфера.

Точний stdout локального відтворення:

```text
unchanged_buffer bytes=102 before_refresh_mavlink_ok=false after_refresh_mavlink_ok=true receive_age_ms=9000 limit_ms=500 altitude_age_ms=9000
replaced_tail bytes=204 window_samples=1 latest_altitude_m=1 cached_altitude_m=42.5 cached_timestamp_ms=1000
```

SHA-256 виділеного блоку: `d56aa63582bf70e57ae5599726915b758454a2ac9c78f37425e17518e4284ab4`. Нормалізований source hash, команда компіляції та stdout збережені в `artifacts/runtime-timestamps-20260927/telemetry-reproduction-evidence.json`.

## Зміна replay API і перевірка повного offline loop

Додано overload `match_replay_route(config, metrics, read_clock)`. Порожній callback відхиляється до роботи з файлами або виводу. Старий двоаргументний overload делегує в новий із `now`; CLI, default thresholds, timestamp кадрів та scripted telemetry не змінені. Механічне source comparison підтвердило: старе тіло функції збережено дослівно, крім трьох замін `now()` на `read_clock()`. Решта `pipeline_harness.cpp` до цієї функції незмінна.

`replay_timing_integration_test.cpp` створює справжні PGM 4×4, VHRS із двома signatures 2×2 та manifest. Кожен піксель target відповідає сталому блоку 2×2 native image, тому resize має заздалегідь відомий результат. Navigator age limit — `200 ms`; healthy scripted telemetry ізолює перевірку match freshness.

| Кадр | Умова на момент завершення processing | `command_valid` |
|---|---|---|
| 10 | Свіжий точний збіг, вік 0 | true |
| 11 | Вік рівно 200 ms, processing latency 110 ms | true |
| 12 | Вік 200 ms + 1 ns | false |
| 13 | Timestamp на 1 ns у майбутньому | false |
| 14 | Невалідні pixels при свіжому timestamp | false |
| 15 | Свіжий збіг після відмов, вік 3 ms | true |
| 16 | Старий кадр поза часовим порядком, вік 1901 ms | false |
| 17 | Відновлення свіжим кадром, вік 7 ms | true |

Перевіряються IDs, route indices/confidence, match validity, health flags, рішення та швидкість у кожному dry-run command, кількість кадрів і точна кількість clock reads. Два проходи тієї самої послідовності дають побайтово однаковий повний журнал. `last_frame_age_ms=0` — вік останнього кадру на початку processing за поточним контрактом HealthMonitor; `last_processing_latency_ms=7`. Навігаційна перевірка використовує вік на момент health snapshot після processing.

Цей тест виконує справжній offline caller, але не містить live verification, endpoint або telemetry stream. Попередні [24 offline API scenarios](OFFLINE_INTEGRATION_VALIDATION_2026-09-26_UA.md) та [21 downstream freshness scenario](DOWNSTREAM_FRESHNESS_VALIDATION_2026-09-26_UA.md) мають окремі межі покриття.

## Валідація та відтворення

- WSL/GCC, інкрементальні Debug і Release на попередніх clean builds: по **60/60** CTest; Python suites **15/15** і **5/5**.
- `BUILD_TESTING=OFF` успішно зібрано, `ctest -N` показує 0 тестів.
- У всіх трьох caches сім camera/output flags OFF; новий Release test має `-UNDEBUG` після `-DNDEBUG`.
- Source hashes чотирьох змінених code/build files відповідають протестованій версії; `git diff --check` пройшов.
- Sanitizers для нового тесту, Pi/MSVC та hardware acceptance у цьому кроці не запускалися. Повного незалежного повторного проходу маршруту для оцінки точності досі немає.

Повторення committed tests із кореня checkout у WSL:

```sh
bash scripts/test-software.sh artifacts/config-validation-20260921/clean-build
```

Локальні, не tracked artifacts: `artifacts/runtime-timestamps-20260927/`. Там містяться `software-tests.log`, `no-tests-build.log`, `verify-source.py`, `source-hashes.json`, `reproduce-telemetry.py`, extracted include, reproduction source/binary/log/JSON. За наявності цих локальних artifacts відтворення запускається `python3 artifacts/runtime-timestamps-20260927/reproduce-telemetry.py`; потрібна вже зібрана Debug library. У свіжому clone доступний committed replay test, але не ці допоміжні artifacts.

GitNexus pre-edit impact для replay caller повернув `UNKNOWN` із неповними callers; direct source review знайшов два CLI call sites і шість наявних test calls. Graphify оновлено після зміни коду. Графи слугують навігацією; приймальні докази тут — source comparison, реальне виконання caller, точний telemetry reproduction та regression suites.

Фінальний GitNexus CLI refresh завершився: 14086 nodes / 31268 edges / 202 flows, FTS недоступний. Обов'язкові staged та compare-main перевірки повернули `CRITICAL`, відповідно 200 і 202 affected processes. Переглянуто всі changed steps: staged приписує всі 648 кроків одному `match_replay_route` із порожнім `id`, зокрема JSX і MAVLink encoding; compare-main так само приписує всі 739 кроків `live_route_match_update_endpoint` із порожнім `id`. Context нового overload також повертає порожні UID та stale metadata попри CLI refresh. Це некоректна прив'язка графа, а не перевірений call graph; висновку «graph clean» немає. Фактичний staged diff містить тільки сім очікуваних replay/build/test/docs файлів, а source comparison підтверджує незмінність старого replay body крім clock calls. Сирі graph responses збережено поряд з іншими artifacts; camera/telemetry/output implementation у staged diff відсутні.

## Наступний обмежений крок

Пріоритет — коректні metadata походження read-only telemetry: час отримання релевантних повідомлень і монотонна ідентичність altitude sample в узгодженому snapshot. Caller має використовувати ці дані замість часу повторного читання та розміру retained sample window. Простий контроль `bytes_captured` недостатній: сторонні bytes не мають освіжати старий heartbeat або altitude. Перед зміною потрібні regression cases для незмінного буфера, rollover, часткових повідомлень і відновлення після паузи. Пороги свіжості й output authority не послаблювати; board/output acceptance лишається окремою роботою.
