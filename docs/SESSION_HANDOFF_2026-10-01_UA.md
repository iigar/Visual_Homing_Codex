# Visual_Homing: контекст для нової сесії

Збережено за прямим запитом користувача `2026-10-01`. Це контрольна точка завершеної сесії та порядок продовження. Нову реалізацію або апаратні дії під час збереження контексту не починали.

## Як продовжити

Повідомлення для нової сесії:

> Продовжуй Visual_Homing із docs/SESSION_HANDOFF_2026-10-01_UA.md. Спочатку перевір git status і поточний checkpoint. Наступний bounded software крок — явний контракт джерела телеметрії sysid/compid та коректне mode mapping. Працюй локально, без борту, камери, FC/UART і ввімкнення output; не послаблюй gates. Підтверджуй зміни відтворенням і тестами, оновлюй документацію та публікуй перевірений checkpoint.

Репозиторій: `D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex`.
Батьківський workspace: `D:\LLM\ChatGPT\Codex\Visual-Homing`.
WSL: `/mnt/d/LLM/ChatGPT/Codex/Visual-Homing/Visual_Homing_Codex`.
Гілка: `refactor/optimization-tech-debt`; origin: `https://github.com/iigar/Visual_Homing_Codex.git`.

На початку збереження tracked tree чистий; HEAD — `c73514b8044ad4456034736d16ee6433fa48c758`. Цей handoff буде окремим documentation commit поверх нього. Актуальний HEAD/remote перевіряти через Git, а не припускати, що наступна сесія досі на цьому SHA.

База `main` — `62c772b961917347471996a157c8a6e50d9c5449`; rollback tag — `baseline/pre-optimization-2026-09-17`. Не переписувати main/history і не скидати чужі робочі зміни. Немає незавершеної code patch, яку треба дописувати після переривання: parser fix і системний опис уже закомічено та опубліковано.

## Що читати та що має пріоритет

1. Локальний `AGENTS.md` — правила GitNexus/Graphify перед роботою.
2. Цей handoff і [CURRENT_PROJECT_STATUS_UA.md](CURRENT_PROJECT_STATUS_UA.md) — checkpoint та найближча черга.
3. [SYSTEM_DESCRIPTION_UA.txt](../SYSTEM_DESCRIPTION_UA.txt) — загальна система. На самому початку 14 підрозділів цільового сценарію «якщо → тоді»: запис, масштабування, обидва типи гейтів, передача борту Б, вхід із середини коридору, RTL/HOVER, recovery, reset/Home, ROI, thermal і acceptance ladder. Реалізація відділена від планів.
4. [MAVLINK_PARSER_VALIDATION_2026-09-27_UA.md](MAVLINK_PARSER_VALIDATION_2026-09-27_UA.md) і [TELEMETRY_RECEIPT_FIX_2026-09-27_UA.md](TELEMETRY_RECEIPT_FIX_2026-09-27_UA.md) — остання реалізація, відтворення й обмеження.
5. [SESSION_LOG.md](SESSION_LOG.md), [PROJECT_MEMORY.md](PROJECT_MEMORY.md), [DECISIONS.md](DECISIONS.md) — хронологія та довготривалі рішення.
6. [ROADMAP.md](ROADMAP.md), [VISUAL_HOMING_IDEAS_UA.md](VISUAL_HOMING_IDEAS_UA.md), [SESSION_EXTRACTS_BANK_UA.md](SESSION_EXTRACTS_BANK_UA.md) — повна функціональна черга та збережені обговорення.

Старі документи з польовими командами й старими кількостями тестів — датований evidence. Вони не змінюють поточний software-only scope й не є новою командою підключити борт. За суперечностей щодо фактичного стану пріоритет мають новіший checkpoint і код; цільовий вступ не означає, що вся готова система вже існує.

## Завершені етапи: не починати їх заново

| Commit | Зміна / висновок |
|---|---|
| `de1f5bb`, `192bb4c` | Спільний scale-distance kernel; окремий testable progress helper. |
| `5793361` | CMake helper замінив 45 однакових registration blocks; log-field parser, endpoint і readiness helpers. Еквівалентність перевіряли generated CTest/Ninja/compile artifacts і поведінкою. |
| `7761d3a` | Numeric config fix: мінус після пробілів, uint32 narrowing, overflow ms→ns, finite checks; software runner і CLI/API regressions. Це не повний аудит усіх config fields. |
| `3cae825`, `a33217a` | Інвентаризація борту й відтворюваний offline matcher benchmark. |
| `6da72f4` | Lazy edge cache зі `std::call_once`, shared immutable cache для копій; збережена рання перевірка route. |
| `1345ab9` | Точне early exit scale refinement без epsilon, зміни ties або directional ranking. |
| `0936cf1` | Повторне використання scaled coordinates у bounded блоках, без heap/cache. |
| `a5513da` | Bounded uint32 partial sums із uint64 total для звичайної Gray8 distance; точна поведінка збережена. |
| `6966e38`, `8f95fbd` | Offline matcher→progress→endpoint/readiness integration та downstream freshness navigator/verification. |
| `6b7060b` | Clock seam у справжньому replay caller; timestamp audit і відтворення двох telemetry caller defects. |
| `1a6ddbe` | Кореневий текстовий опис структури, алгоритмів, мов, сильних і слабких сторін. |
| `db699fb` | Receipt timestamps/absolute packet offsets; old snapshots більше не освіжають telemetry, rollover не приховує нову altitude. |
| `6107675` | CRC семи message IDs, правильні optical-flow offsets, zero-padding MAVLink 2, reject signed/unknown incompatibility flags, новий protocol test. |
| `c73514b` | На початку системного опису додано повний цільовий сценарій і плани з наших зафіксованих обговорень; code unchanged. |

Останні три SHA перед handoff:

```text
c73514b8044ad4456034736d16ee6433fa48c758  system workflow documentation
6107675636b4945377586bacb7224ebaa0b48eda  parser CRC/payload fix
db699fbcca4c5f697ba960f7972ace33f43593ed  telemetry receipt fix
```

## Виміряна оптимізація та її межі

- Lazy edge cache: приблизно 9.2 MiB менше RSS після створення matcher для 600 кадрів 160×100. Перший edge probe переносить на себе побудову кешу: приблизно 20–22 ms проти 2.8–3.0 ms; цей cold cost не приховувати. ThreadSanitizer для відповідної зміни пройшов. Звичайний stateful `match` не став thread-safe.
- Scale-coordinate kernel: на 13 незалежно збережених stop-кадрах із ненульовою помилкою median match 14.197 → 3.474 ms; 11019 scalar-oracle comparisons, 44 before/after scenarios і targeted ASan/UBSan.
- Raw distance: median без refinement 2.727 → 0.579 ms, з refinement 3.504 → 1.381 ms; 65622 scalar-oracle API cases й еквівалентні результати 44 сценаріїв.
- Після цих змін профіль match орієнтовно raw distance 43%, scale kernel 53%, candidates 1.24%. Окрема оптимізація вставки кандидатів на цих даних не виправдана.
- Ці числа належать конкретним WSL x86_64 measurements. Це не Pi FPS, не latency всього runtime і не доказ точності маршрутизації. Self-match і синтетичні сценарії доводять свої властивості, а не загальну стійкість розпізнавання.

Звіти: [EDGE_CACHE_OPTIMIZATION_2026-09-22_UA.md](EDGE_CACHE_OPTIMIZATION_2026-09-22_UA.md), [SCALE_REFINEMENT_OPTIMIZATION_2026-09-24_UA.md](SCALE_REFINEMENT_OPTIMIZATION_2026-09-24_UA.md), [SCALE_KERNEL_OPTIMIZATION_2026-09-25_UA.md](SCALE_KERNEL_OPTIMIZATION_2026-09-25_UA.md), [RAW_DISTANCE_OPTIMIZATION_2026-09-26_UA.md](RAW_DISTANCE_OPTIMIZATION_2026-09-26_UA.md).

## Поточна валідація

Остання виконана code suite: WSL/GCC Debug **62/62**, Release **62/62**, Python **15/15** та **5/5**. `BUILD_TESTING=OFF` успішно зібрано з 0 tests; усі сім camera/output flags OFF. Release tests мають активні assertions (`-UNDEBUG` після `-DNDEBUG`).

Protocol test: 35 pymavlink-generated golden packets, 5384 одиночні bit corruptions, 3584 payload-length combinations, 1089 incomplete prefixes; перевірено signed/flags/extensions/unknown IDs/values/offsets/recovery. Старий parser очікувано провалив regression. Targeted inspector ASan/UBSan із leak detection/halt-on-error пройшов. Це не повний sanitized build runtime.

Receipt integration зберігає 126 splits і перевіряє пошкоджені пакети через реальні buffer/caller/adapter. Навіть явний дозвіл кількох malformed packets не робить старий receipt time свіжим. CLI відхиляє пошкоджений heartbeat: exit 2, checksum_errors=1, heartbeat_messages=0.

Під час цього збереження `2026-10-01` повторно прочитано журнали та звірено SHA-256 усіх code/build files із `artifacts/mavlink-parser-20260927/tested-source-hashes.json`: усі збігаються. Між `6107675` і `c73514b` немає змін `core/`, `scripts/`, `config/`. Нову full suite для documentation-only handoff не запускали.

## Наступний bounded software slice — source/mode contract

Три факти відтворено справжнім parser/validator із правильними CRC:

1. Heartbeat 42:17 + attitude 43:18 + position 44:19 разом проходять validation.
2. Пакети із source 0:0 також проходять validation.
3. Heartbeat type=1, autopilot=12, custom_mode=4 отримує ArduCopter `Guided` mapping без перевірки типу автопілота/апарата.

Reproducer: `artifacts/mavlink-parser-20260927/source-audit.cpp`, результат — `source-audit.log`. Докладний контракт: parser report. Реалізацію виправлення source/mode ще НЕ починали; новий config/API ще не обраний.

Порядок наступного кроку:

1. Прочитати inspector/stream/validation/caller interfaces і спосіб передачі runtime config; виконати обов'язковий impact analysis, підтвердити direct calls.
2. Визначити явний контракт очікуваного джерела та узгодженості heartbeat/attitude/altitude. Не припускати sysid/compid=1:1 і не вважати перший побачений пакет довіреним. Політику для компонентів/режимів потрібно сформулювати до патчу.
3. Звірити heartbeat autopilot/type/mode semantics із первинними визначеннями; mapping ArduCopter застосовувати тільки для підтриманого контракту. Непідтверджений режим не повинен давати Guided authority.
4. Спочатку regressions для змішаних, нульових, невірних і відсутніх джерел та невідповідного heartbeat; потім мінімальна реалізація й tests recovery/receipt freshness.
5. Зберегти CRC/payload behavior, bounded tail, single snapshot consistency, output gates і діагностичну прозорість. Розрізняти structural inspection, source selection і permission.
6. Виконати релевантні тести та full software runner після code change; оновити report/status; staged/main review, commit/push/remote verification.

Код для читання: `core/include/visual_homing/mavlink_telemetry_inspector.hpp`, `core/src/mavlink_telemetry_inspector.cpp`, `mavlink_telemetry_stream.hpp/.cpp`, `core/src/camera_smoke.cpp` (спільний `live_route_match_telemetry_observation` та два його consumers), `mavlink_telemetry_adapter.cpp`, CLI/config у `main.cpp`; тести `mavlink_telemetry_protocol_test.cpp`, `mavlink_telemetry_inspector_test.cpp`, `telemetry_receipt_integration_test.cpp`.

Важливі вже прийняті межі:

- CRC перевіряється лише для семи декодованих IDs; unknown IDs рахуються, не підтверджують health і не проходять CRC verification.
- Signed packets відхиляються; keys/signature/anti-replay implementation немає. CRC не є автентифікацією.
- MAVLink 2 нульовий хвіст відновлюється; майбутні extension bytes дозволені, CRC охоплює весь переданий payload.
- Cached inspection оновлюється на append. Latest offsets переносяться в absolute stream positions; час — host read, що завершив пакет. Повторний snapshot не освіжає дані.
- Aggregate telemetry time — найстаріший із heartbeat/attitude/altitude receipt times; altitude scalar має власний час. Це не FC measurement time і не оцінка всієї transport delay.

## Подальша оптимізація: порядок і критерії

1. **Source/mode correctness** — крок вище, перед наступними speed changes у цьому контурі.
2. **Повний runtime timing** — audit уже показав callback timestamp замість sensor exposure і виключення пізніх scale/verification етапів із processing latency. Визначити чесні межі latency, stage measurements і evaluation timestamps; replay має залишатися детермінованим. Спершу локальний software seam, hardware measurements — окремо.
3. **Дані для оцінки якості** — використати поточні route/PGM для еквівалентності; зібрати або отримати незалежний повний повторний прохід із timestamps та еталонною розміткою для false matches/misses/reacquisition. До цього не підлаштовувати пороги за self-match.
4. **Наступний performance slice лише за профілем** — порівнювати до/після на тих самих 44 сценаріях, реальних stop-кадрах та потрібному oracle. Вимірювати cold/warm cost, RSS, latency distribution і стан matcher. Coarse scan, scale kernel, cache/reparse/queue costs — кандидати для вимірювання, не вже схвалені переписування.
5. **Bounded long-route search** — розділити compact global index, локальне tracking window та top-N native verification; додати multi-chunk consumer без необмеженої RAM. Наявність VHIX builder не означає наявності search consumer.
6. **Подальше спрощення** — лише конкретні testable seams/дублювання після characterization. Не переписувати весь camera loop, не додавати універсальний component manager і не послаблювати freshness/queues/gates заради LOC. Інші копії shell log parser можна брати окремим slice тільки зі своїми fixtures; hardware wrappers не зачіпати автоматично.

Не відкриті повторно: lazy edge-cache, precise scale early exits, coordinate blocks, raw partial sums, CMake registration, progress/endpoint/readiness extraction — уже завершені. Новий профіль/регресія можуть виправдати повернення до них, але сама команда «продовжуй» не починає оптимізацію з нуля.

Known behavior, яке не міняти випадково: invalid gap може зберігати endpoint dwell history; route-only quality не має самостійного age gate; navigator і verification мають різні вимоги до timestamp/confidence/yaw/scalars. Це описано у звітах integration/freshness; зміна policy потребує окремого обґрунтованого кроку.

## Дані, артефакти та локальні команди

`artifacts/board-inventory-20260922/data/` зараз містить **6 VHRS + 13 PGM**. Копії звірялися SHA-256 з перевіреним бортом. Повного незалежного другого проходу в перевірених місцях не знайдено. Один маршрут не проходить distinctiveness policy; self-frames і sparse stop-images не замінюють sequence ground truth.

Локальні каталоги evidence збережені:

```text
artifacts/config-validation-20260921/
artifacts/matcher-benchmark-20260922/
artifacts/edge-cache-review-20260922/
artifacts/scale-refinement-20260924/
artifacts/scale-kernel-20260925/
artifacts/raw-distance-20260925/
artifacts/offline-integration-20260926/
artifacts/downstream-freshness-20260926/
artifacts/runtime-timestamps-20260927/
artifacts/telemetry-receipts-20260927/
artifacts/mavlink-parser-20260927/
```

Вони переважно ignored/local, не завантажені цим handoff у GitHub. Нова сесія в цьому workspace їх має; новий clone не має їх автоматично. Source/tests/golden fixtures та текстові звіти — у Git. Не видаляти artifacts як «сміття», не завантажувати великі route/build/tooling directories під виглядом збереження контексту.

З PowerShell у repo:

```powershell
git status --short
git log -5 --oneline
wsl.exe -e bash -lc 'cd /mnt/d/LLM/ChatGPT/Codex/Visual-Homing/Visual_Homing_Codex && bash scripts/test-software.sh artifacts/config-validation-20260921/clean-build'
```

Runner використовує Debug/Release під зазначеним parent і явно вимикає сім camera/output flags. No-tests build: `artifacts/config-validation-20260921/no-tests`. Після docs-only змін не перезапускати всі code tests без причини. Перед code changes зібрати потрібний baseline, після них — релевантну перевірку; не повторювати вже зелені suites без нових змін/сумнівів.

`pymavlink 2.4.49` уже був установлений у WSL. `core/tests/generate_mavlink_telemetry_golden.py` потрібен лише для відтворення committed fixtures; CTest не залежить від Python MAVLink package. Не оновлювати pinned generator просто заради «новішої версії».

## Функціональний план готової системи, який треба зберегти

Повний сценарій із умовами — початок `SYSTEM_DESCRIPTION_UA.txt`; він спирається на 15 названих документів. Основні незмінні рішення:

- coarse GPS-denied visual return, не повний metric SLAM; FC стабілізує, Pi впізнає route, JT_Zero може бути окремою local hold опорою;
- перший recording pass → finalize/index → другий прохід або достовірний offline enrichment; frame index/time не видавати за виміряні метри;
- low-resolution tracking + compact descriptors + sparse native verification; адаптація cadence до руху/altitude/scale/yaw/novelty;
- safety gates і gate-keyframes — різні поняття; coordinate gate лише звужує район пошуку, visual high-resolution/multi-frame confirmation створює route lock;
- route transfer переносить corridor evidence, не Home/authority/actuator commands; борт Б перевіряє camera/extrinsics/frames/envelopes/package revision і власні safety gates;
- старт борту Б із середини маршруту — після reacquisition; до невидимого corridor потрібен окремий operator/shared-frame/optional geographic/search prior; route imagery не вигадує bearing/range;
- scale не створює небаченої місцевості; high-altitude coverage, thermal і cross-camera acceptance потрібні окремо;
- втрата match → відхилити непідтверджену pose → лише перевірений local hold/abort → bounded reacquisition → explicit reset_reference/frame alignment/reset_counter;
- endpoint ambiguity не стає точним COMPLETE; HOVER/handoff, micro-search, ROI authority та точне позиціювання потребують своїх contracts;
- LocalEstimatorReset і FC Home change мають різні permits/executors/ACK/readback; старий RC mapping не переноситься автоматично на інший борт;
- Android/GS відображає Pi-owned readiness; thermal/EasyCAP, temporal scale tracker, active search і fleet sync залишаються планами, не вже прийнятими можливостями.

Окрема функціональна черга після локальних fixes: Pi all-output-off acceptance актуального wiring, package-binding/drain/zero-gate evidence, ремонт MSVC toolchain, second-pass wrapper/checker і контрольований no-send прохід; revision resume/SD durability; bounded VHIX → native top-N → multi-frame lock; reacquisition/reset; trusted pose/gates/ownership; real-FC props-off origin/Home/provider acceptance; лише потім окремі restrained/HOVER/RTL/flight plans. Це не завдання підключити hardware у наступному optimization slice.

## Інструменти, переривання та обмеження

- GitNexus impact обов'язковий до зміни symbols, context/direct caller review, detect_changes staged і compare-main перед commit. Graphify query — навігація в незнайомому коді, update після code changes. Не дублювати broad scans без потреби.
- GitNexus incomplete: FTS/BM25 недоступний; empty/misassigned C++ IDs, stale MCP metadata навіть після CLI refresh. `UNKNOWN` не означає «немає впливу». Останні code staged/main reports дали CRITICAL 189/193 flows із empty-ID checksum_errors attribution; попередня документаційна зміна `c73514b` — LOW/0. Під час цього handoff staged check дав CRITICAL/163: один README Section із порожніми id/name приписано всім changed steps, включно з нерелевантними JSX flows. Compare-main — CRITICAL/193 для накопичених змін гілки. Перевірено фактичний staged diff: лише п'ять документів, code unchanged. Сирі відповіді збережено в `artifacts/context-handoff-20261001/`. Source diff, реальні calls, tests і artifacts необхідні.
- Останній CLI index: 14239 nodes / 29075 edges / 209 flows; Graphify: 5971 / 9237. Це датовані числа, не вимога повторно будувати індекси для збереження документації.
- Локальні untracked `.claude/`, `.codex/`, `AGENTS.md`, `CLAUDE.md`, `codex_jtzero_known_hosts`, `graphify-out/` існували до поточного кроку. Не stage/add/delete їх автоматично.
- Був блокер Windows error 112: helper не міг записати setup log, тому термінал зовсім не стартував. Після звільнення місця виконання відновилося. На `2026-10-01` read-only перевірка показала близько 0.49 GiB вільного на C: і 413 GiB на D:; це може змінитися. Перед великим build перевірити місце; не починати самовільне видалення файлів.
- Windows Git має global autocrlf=true; WSL не успадковує цей global config. WSL показував CRLF-only diffs у PROJECT_MEMORY/SESSION_LOG/старому RTL plan. Порівнювати нормалізований зміст і tested hashes; не «виправляти» всі line endings масово.
- `verify-final.py` у parser artifacts — історичний pre-commit checker з очікуванням 14 staged files. Після commit не запускати його як універсальний health check; окремо звіряти hashes/CLI або адаптувати fixture до нового snapshot.
- Pi/camera/FC не потрібні для наступного software slice. Старі SSH host/login/wiring facts не гарантують поточний борт; перед майбутнім окремо дозволеним hardware етапом читати HARDWARE_ACCESS_BASELINE_UA.md і перевіряти identity. Не повторювати старі send-enabled команди з історичних документів.
- Публікація checkpoint завершується `git diff --cached --check`, commit, push, `git ls-remote --heads origin refactor/optimization-tech-debt`, збіг SHA і final status. Стартований процес не є завершеним: забрати final tool output.

## Манера роботи з користувачем

Українською, конкретно, без припущень про готовність системи. «Продовжуй» — відновити цей bounded checkpoint, не новий великий redesign. Коротко повідомляти факти, що лишилося перевірити, та отриманий результат. Не зупиняти вже дозволену локальну роботу заради повторного підтвердження; не змінювати scope на hardware/output. Не створювати subagents без прямого запиту або застосовної явної інструкції. Після user pause/interrupt спочатку перевірити стан, бо останній tool міг виконатися частково.

Цей файл разом із посиланнями зберігає робочий контекст, рішення, докази й плани для продовження; він не є дослівним архівом усіх реплік чату.
