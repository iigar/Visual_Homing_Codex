# Методика оптимізації, класи та більші напрями розвитку

Збережено `2026-11-23` за запитом «збережи все». Цей документ фіксує обговорення
після [retained-window checkpoint](RETAINED_WINDOW_BASELINE_2026-11-21_UA.md)
`073a5d11a6384de9c874283f453de58f933d6f3e`, його evidence та запропоновану чергу.
HEAD/remote звірені на вході; publication SHA цього документа перевіряти через Git.
Production/tests/runner/core/config не змінені. Нових experiments, benchmarks,
full-suite, hardware/output runs у цьому documentation-only slice немає.

## Запити користувача та статус рішень

Користувач попросив пояснити методику оптимізації й момент написання нового коду,
оцінити стан класів, порівняти цінність архітектурного рефакторингу з дрібними
matcher оптимізаціями та рекомендувати напрями такого самого або більшого масштабу.
Наприкінці попросив зберегти весь контекст.

Архітектурні напрями нижче обговорені як рекомендації. Їхня реалізація,
перепріоритизація чинного slice й performance gain ще не підтверджені.
Чинний зафіксований next slice — **read-only retained-window scale-work attribution**
за [handoff](SESSION_HANDOFF_2026-11-03_UA.md). Запропонований архітектурний порядок
збережений окремо, щоб не видати рекомендацію за вже виконану або прийняту зміну.
Звичайне «продовжуй» відновлює актуальний bounded checkpoint; нова явна вказівка
користувача має пріоритет над збереженою чергою.

## Як проводимо оптимізацію

Новий код для перевірки пишеться: зазвичай спочатку окремий diagnostic harness,
reference або лічильники. Production candidate готується після конкретної гіпотези
та вихідних вимірювань. Робочий цикл:

1. Зафіксувати revision, source/compiler/options/binary identities, input bytes,
   session bindings, конфігурацію та очікувані результати. Зберегти чинні контракти.
2. Встановити baseline і причину витрат. Profiler/counters/reference runs
   відокремити від звичайної API latency: observer змінює вимірюваний процес.
3. Перевірити гіпотезу diagnostic кодом. Для нового workload спочатку незалежна
   characterization, explicit config/schema та нові oracles; старі не переписувати.
4. Готувати один bounded candidate. Перевіряти results/top-k/ties/state, timestamps,
   capacity/allocation/error semantics і validation order, які зачіпає зміна.
5. Порівнювати baseline/candidate на однакових inputs/runtime/options; budget,
   warmups, порядок і статистику визначити до вимірювань. Для парних campaigns
   чергувати порядок варіантів; для нового baseline фіксувати власний protocol.
   Зберігати raw outputs, розкид і regressions, без favorable-run selection.
6. Прийняти або відхилити candidate за correctness і API evidence. Менша кількість
   операцій або менший LOC не доводять latency/RAM/FPS gain. Завершити actual diff
   review, доречні checks, documentation, commit/push, remote SHA й tracked status.

Для code change потрібен актуальний software runner. Для documentation/read-only
slice можна звірити prior suite лише за доведеної source/runtime identity та прямо
назвати відсутність нового запуску. Не заявляти hardware або production acceptance
з warm WSL/synthetic/repeated-image evidence.

Останній приклад: retained-window slice написав C++ probe, independent scalar
reference та Python witness verifier; production unchanged. 234 PGM queries,
360 synthetic checks і 104 measurement processes пройшли gates. Це characterization
і baseline, а не прийнята нова production optimization. Деталі й hashes у звіті.

## Стан класів на перевіреному source

У [interfaces.hpp](../core/include/visual_homing/interfaces.hpp) є межі
`CameraSource`, `FramePreprocessor`, `RouteRecorder`, `RouteMatcher`, `Navigator`,
`MavlinkBridge`, `MavlinkTelemetrySource`. Конкретні `Gray8RouteMatcher`,
`BoundedNavigator`, `HealthMonitor`, `MavlinkTelemetryAdapter` мають власні ролі
та стан. Verification producer, bounded publisher і safety gates також виділені.
Replay pipeline викладений функціями; назва файла не означає наявність класу
`PipelineHarness` у поточному header.

Найпомітніший борг цього огляду — великий код композиції:

- [camera_smoke.cpp](../core/src/camera_smoke.cpp): **3128 фізичних рядків**;
  `match_live_camera_route` починається на рядку 1479 і займає понад 1600 рядків.
- [main.cpp](../core/src/main.cpp): **1543 фізичні рядки**, `main` починається на 749.
- Progress, endpoint/dwell, route-quality і readiness вже винесені в тестовані
  функції; частина з них залишається в camera-модулі. Це завершені попередні slices.
- Replay має injected processing clock та synchronous observer, але його public
  observation contract не охоплює endpoint/readiness/verification/publication.
  Повну тотожність replay і live обробки одного кадру не заявляємо.

Ці розміри — snapshot при `073a5d1`, не самостійний доказ багу або performance
проблеми. Повного нового dependency/ownership audit усіх класів не проводили.
Клас доречний для стану й правил його зміни; обчислення без власного стану можуть
залишатися функціями. Мета — чіткі responsibilities й testability, без великого
універсального component manager, нової ієрархії або blanket runtime rewrite.

## Оцінка цінності архітектурного напряму

Оцінка асистента: bounded architectural refactoring зараз може бути ціннішим для
керованості проєкту, перевірок та наступних змін, ніж чергові дрібні matcher
variants. Попередні partial-score candidates скорочували logical work без
переконливого стабільного API gain й відхилені; ця серія залишається закритою.

Виділення state/session/frame-processing boundaries має полегшити replay,
verification і майбутній reacquisition. Саме перенесення рядків у класи не
гарантує CPU/RAM/FPS gain; користь для підтримки та runtime benefit оцінюються
окремо. Перший архітектурний slice — characterization поточної обробки кадру,
після неї одна невелика extraction зі збереженням результатів і safety semantics.

## Більші напрями та їхні критерії

| Напрям | Потенційна цінність | Поточний статус |
|---|---|---|
| Спільна обробка кадру live/replay | Тестованість, менше розходжень, простіші зміни | Є окремі helpers/clock/observer; спільний повний frame path не реалізований |
| Ієрархічний пошук | Менше детальних порівнянь на великих routes | Local window та VHIX builder є; bounded search consumer/top-N verification/multi-frame lock pending |
| Route chunks і обмежений pixel working set | RAM та довгі routes | Package/chunk builder є; multi-chunk matcher consumer pending |
| Повторне використання scale-підготовки | Можливе зниження CPU | Cost attribution pending; coordinate-table candidate ще немає |
| Ownership кадрових buffers | Контроль copies/allocations/lifetimes, передбачуваність | Потрібен вимірювальний аудит; масові зайві copies не доведені |

### 1. Спільна обробка кадру live/replay

Виділити session state й обробку одного кадру з camera loop. Джерело кадрів,
файлові операції та log formatting залишаються explicit boundaries. Спочатку
описати inputs, clock domains, state ownership, side effects і call ordering.
Characterization повинна фіксувати invalid/stale/missing telemetry, gaps,
ambiguity, endpoint/dwell continuity, verification/backpressure/failure paths.
За однакових inputs і scripted clock зміна зберігає decisions/timestamps/output
gates; existing log consumers залишаються сумісними. Extraction робити по одному
блоку, не конструювати всю нову систему за один крок.

### 2. Ієрархічний пошук

Компактні descriptors → обмежені кандидати → детальна/native перевірка →
підтвердження кількома кадрами. Розвинути наявний local window й VHIX artifacts
у actual offline search consumer; це може зменшити сам обсяг pixel work.
Найбільший потенціал runtime scaling серед обговорених напрямів, але це також
алгоритмічне розширення зі зміною candidate selection. Потрібні explicit нові
oracles/config, candidate recall, missed matches, false locks/off-route negatives,
forward/reverse і scale coverage. Global reacquisition/reset — наступний окремий
state-machine контракт; не вважати готовим наслідком наявності індексу.

### 3. Route chunks і обмежена пам'ять

Поточний `Gray8RouteMatcher` володіє повним `RouteSignatureFile`. Розглянути
active/successor chunks і кеш із заданою місткістю, з verified immutable package
identity. Великий pixel working set має бути bounded; global index/metadata
можуть і далі рости з довжиною route. Перевіряти peak memory, bytes read, chunk
transitions, I/O latency, eviction/lifetimes, corruption/missing chunks та exact
results у межах незмінної search policy. Content/mutation guards не прибирати.

### 4. Scale-підготовка

Поточний retained baseline має median case latency raw 31.7275/scale 812.5625 µs;
це різні режими одного repeated-image workload, не before/after speedup або
виміряна вартість coordinate helper. Спочатку production counters та independent
readback. За cost evidence перевірити reuse coordinate mappings для однакових
dimensions/ratios, не накопичуючи необмежений кеш. Зберігати lround/rounding,
overlap, scale order, strict ties, early-zero й thread/lifetime semantics.
Лише після цього — один candidate і fixed API comparison.

### 5. Ownership і allocations

Визначити власника й строк життя native/preprocessed frames, передачі в
verification jobs, route payloads і scratch buffers. Виміряти фактичні копії,
обсяг bytes та allocations; `vector` або by-value parameter самі не доводять
зайвого copy. Лише за evidence розглядати move/reuse/bounded pools. Перевіряти
aliasing, asynchronous lifetimes, allocation/error behavior і peak memory.
Bounded publisher/backpressure вже є; їх повторне створення не є новим здобутком.

## Запропонований порядок

Для архітектурної черги асистент рекомендував: **спільна обробка кадру →
вимірювання ownership/scale-work → chunks та індексний пошук окремими етапами**.
Перший етап створює test seams для двох більших змін. Це попередня оцінка,
без обіцяних відсотків gain або строків, не виконана реалізація. Чинний
зафіксований наступний diagnostic slice збережено в handoff.

Спільні межі: software-only, без subagents, Pi/camera/FC/UART/live outputs,
threshold tuning або weakening freshness/fail-closed gates. Не відновлювати
завершені partial-score, PGM lexical, path-constructor чи API campaigns без нового
evidence. Старі graphs advisory/stale; прямий source/diff/tests визначає висновки.

Загальний принцип вимірювань і ресурсів узгоджується з
[C++ Core Guidelines, Performance](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-performance).
Project-specific recommendations вище є інженерною оцінкою за source/roadmap,
а не результатом зовнішнього benchmark або гарантією ефекту.

## Resume checklist

Прочитати AGENTS, current status, November3 handoff і цей документ. Звірити
native HEAD/status/remote, actual source та local artifacts, не припускати старі
SHAs або line counts чинними. Retained-window evidence лишається local ignored
`artifacts/retained-window-20261120/`, 306 bound files/244 sources. Prior suite
357 Python/65+65 CTest у baseline звірена на тих самих sources/runtime; цей save
не запускав suite повторно. Зберегти user untracked `.claude/`, `.codex/`,
`codex_jtzero_known_hosts`, histories й input/session bindings.
