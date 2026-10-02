# Runtime timing: межі вимірювання й актуальний час оцінки

Дата: `2026-10-02`. Продовження source/mode checkpoint `a5f2b38` у гілці
`refactor/optimization-tech-debt`. Локальний software slice без Pi, камери,
FC/UART або ввімкнення output. Попередні gates і thresholds збережено.

## Відтворена проблема

`match_live_camera_route` фіксував `processing_finished` після matcher і
diagnostics, до navigation, scale, external navigation, verification та
reporting/endpoint/export. `latency_ms` не вимірював ці пізні етапи, а
verification отримував health із тим самим раннім evaluation timestamp.

До патчу справжній progress-only observation builder, producer та in-memory
publisher приймали кадр із source timestamp `1000 ms` за cached health time
`1100 ms`, хоча змодельований пізній етап завершився на `1400 ms`.
При ліміті `200 ms` оцінка на `1400 ms` правильно давала
`health_frame_context_stale`. Це локальне відтворення композиції caller data,
а не запуск camera loop. Код і результат: `late-before.cpp`, `late-before.log`
у `artifacts/runtime-timing-20261002/`.

## Контракт метрик

Новий `RouteFrameTiming` має фіксовані дев'ять часових меж для восьми
послідовних інтервалів. Heap/history/cache для таймінгу не додаються.
Live та actual replay caller пишуть окремий `route_frame_timing` на кадр:

| Поле | Що охоплює |
|---|---|
| `preprocess_ms` | Від початку роботи після `poll()` до завершення resize |
| `match_ms` | Matcher |
| `diagnostics_ms` | Top/zone/edge diagnostics до колишнього `processing_finished` |
| `navigation_ms` | Health/telemetry, navigator, dry-run reporting і наявний output-session виклик |
| `scale_ms` | Visual scale diagnostic |
| `external_nav_ms` | External-nav estimate, statistics, logging і наявний session виклик |
| `verification_ms` | Progress/accounting та синхронний producer submission |
| `reporting_endpoint_ms` | Звичайний frame log, endpoint decision, optional PGM export і stop log |
| `frame_work_ms` | Сума всіх восьми інтервалів, від початку до останньої межі |
| `source_age_at_start_ms`, `source_age_at_finish_ms` | Signed вік від незміненого `Frame.timestamp` на відповідній межі |

Значення відсутніх replay stages — нуль. У live навіть вимкнений stage може
мати невелику вартість перевірки config та читання clock. `timing_valid=false`
позначає незавершену, переплутану, дубльовану або немонотонну послідовність;
від'ємні значення не ховаються clamp. `source_timestamp_present=false`
позначає нульовий source timestamp; ages без нього не є валідним віком кадру.

Старі `latency_ms` і `last_processing_latency_ms` зберігають межі:
`latency_scope=through_match` для replay, `through_diagnostics` для live.
Старий `age_ms` залишається clamp-to-zero віком на початку processing.
Результати matching API мають `timed_frames` і `last_frame_timing`;
live summary додає last full-work/source-age values. Recording і базовий
replay pipeline цим кроком не інструментовано.

`frame_work_ms` не містить очікування/роботи `poll()`, свого нового timing-log
рядка, асинхронної publisher/storage роботи, drain, timeout checks після
кадру або cleanup. Звичайні frame/command/diagnostic logs усередині інтервалів
включені. Normal, endpoint-stop та terminal verification-failure гілки
фіналізують trace; exception до цієї межі не гарантує завершеного запису.
Це synchronous frame-work metric, не вся тривалість сесії або sensor-to-output
latency. Camera timestamp досі host callback до копіювання, не sensor exposure.

## Час прийняття рішень

`refresh_route_frame_health` змінює evaluation timestamp без нового
`observe_processed_frame`, повторного підрахунку кадру або зміни source times.
Він повторно перевіряє receipt freshness наявного telemetry adapter, зберігає
попередні failures і fail-closed відхиляє нульовий/зворотний evaluation time.
Чинні camera/navigation flags та phase metrics не перетворюються на нові дані.

- Navigator отримує час безпосередньо перед своєю оцінкою.
- Verification отримує новий health snapshot після scale/external-nav роботи:
  frame, altitude, scale та yaw перевіряються на актуальній межі submission.
- Live command gate і external-nav session отримують актуальний evaluation time.
  External-nav telemetry freshness оцінюється після scale; timestamp оцінки
  та wire `time_usec` зберігають колишню межу `processing_finished`. Це не
  переносить старі дані на новіший час і не змінює wire timestamp contract.
- Endpoint dwell отримує час endpoint stage. Правило invalid-gap continuity
  і відсутність окремого route-only match-age gate залишаються чинними.

Не змінено policy implementations navigator, producer, telemetry adapter,
output gates/sessions/writers або external-nav estimator. Зокрема, цей крок
не додає source-match-age gate в external-nav estimator/session і не робить
наявну перевірку altitude повним measurement-time контрактом. Async publication
не отримала нового freshness policy на момент фактичного storage write.

## Перевірка

- Новий CTest `route_frame_timing`: точні вісім інтервалів і сума, zero stages,
  absent/future timestamp, regression на кожній межі, неправильний порядок;
  реальні navigator/observation builder/producer/publisher на frame/scalar/
  telemetry age boundaries, включно з `+1 ns`, збереження failures, source
  timestamps, phase metrics та frame counters. Telemetry-required сценарії
  явно вмикають відповідний producer policy; production defaults не змінено.
- `replay_timing_integration`: actual manifest→resize→matcher→health→navigator
  caller; одна initialization clock read та шість reads на кадр. Два повтори
  звичайного й два delayed запуску дають ідентичні повні logs. Додана затримка
  evaluation `+1 ns` блокує match віком понад `200 ms`; зі stage end на `+5 ms`
  старе `latency_ms=110`, нове `frame_work_ms=115`, finish age `205 ms`.
- Full software runner: Debug **64/64**, Release **64/64**, telemetry-source CLI
  **7/7** для кожної збірки, readiness **15/15**, benchmark helpers **5/5**.

Фінальний software runner повторено після останнього code edit з тими самими
результатами. `BUILD_TESTING=OFF` успішно зібрано, CTest бачить 0 tests.
У всіх трьох build caches перевірено сім camera/output flags OFF;
59 стандартних Release test compile records мають `-UNDEBUG` після `-DNDEBUG`.
Targeted ASan/UBSan із leak detection/halt-on-error пройшли для timing test
(інструментовано test і новий timing module) та actual replay test
(також `pipeline_harness.cpp`). Решта library objects — звичайний Debug;
це не full-runtime sanitized acceptance.

GitNexus refresh: 15001 nodes / 33325 edges / 211 flows; FTS/BM25 недоступний,
MCP transport був закритий, використано CLI. Pre-edit live impact — `CRITICAL`,
44 impacted / 42 direct / 10 processes: upstream list включав невірні edges
до matcher/writer/benchmark functions. Replay overload — `UNKNOWN`.
Staged/main checks — `CRITICAL` для 153/170 flows; staged показує, зокрема,
нерелевантні JSX/Android flows із changed symbol `2026-10-02` із документації.
Це не чистий graph verdict. Переглянуто фактичний 14-file staged diff,
прямі caller sites та незмінність policy implementations. Graphify AST update
завершено: 6093 nodes / 9458 edges / 485 communities.

Evidence directory — `artifacts/runtime-timing-20261002/`: `final-software.log`,
`timing-sanitized.log`, `replay-sanitized.log`, `no-tests.log`, executable
`validate-extra.sh`, raw graph responses та `tested-source-hashes.json`
для 217 code/build/script/config files. Збірки повторно використовують
`artifacts/telemetry-source-20261002/build/`. Артефакти local/ignored, код,
tests і цей контракт — у Git.

## Межі й продовження

Це не виміряний приріст FPS і не Pi/MSVC acceptance. Live loop скомпільовано
з усіма сімома camera/output flags OFF; сам camera loop не запускався.
Детерміноване execution evidence належить actual replay caller та composition
seam з реальними downstream consumers. Full live timing distribution,
sensor exposure/transport timing і async completion потребують окремих вимірів.

Наступний bounded software крок — перевірити достатність наявних локальних
route/PGM даних для незалежної оцінки й оформити контракт повного повторного
проходу з timestamps, progress/endpoint ground truth і off-route negatives.
За останньою інвентаризацією є 6 VHRS та 13 sparse stop-PGM; це не labeled
full second pass. Не підлаштовувати пороги за self-match. Новий performance
slice обирати лише за профілем із oracle/equivalence; hardware capture не
входить автоматично в наступне «продовжуй».
