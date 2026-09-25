# Поточний Стан Проєкту

Оновлено: `2026-09-26`.

Гілка спрощення коду: `refactor/optimization-tech-debt`. Незмінена база `main`: `62c772b961917347471996a157c8a6e50d9c5449`; точка повернення — опублікований tag `baseline/pre-optimization-2026-09-17`.

Локальні Graphify/GitNexus індекси — допоміжна навігація, не доказ повноти залежностей. GitNexus FTS/BM25 недоступний; impact у цій сесії повертав неповний `UNKNOWN`, тому виклики додатково перевірено в коді.

Це короткий канонічний snapshot для відповіді на питання «де ми зараз і що робимо далі». Детальні стабільні факти зберігаються в `PROJECT_MEMORY.md`, хронологія — у `SESSION_LOG.md`, рішення — у `DECISIONS.md`, повна черга — у `ROADMAP.md`, а hardware facts — у `HARDWARE_ACCESS_BASELINE_UA.md`.

## Поточна Межа

Visual Homing залишається GPS-denied, replay-first, fail-closed системою без дозволеної flight authority. Поточний активний development stream — Milestone 6.10: bounded kilometer-scale route package та multiscale verification/reacquisition groundwork.

Реалізовано й перевірено на library level:

- bounded streaming `VHRS` recording із explicit finalize/checkpoint behavior;
- `VHRM v1` manifest, bounded tracking chunks і recovery scanner;
- `VHIX v1` compact coarse descriptors та offline index builder;
- transactional verification selector і immutable package writer;
- synchronized native-frame capture session;
- single-worker bounded publisher з explicit backpressure/failure/drain metrics;
- fail-closed `LiveRouteVerificationProducer`, який приймає тільки same-frame match/progress/health/fresh scalar evidence та optional fully named local pose.
- operational progress-only caller у `match_live_camera_route` та explicit environment contract для indexed source package/output revision. Він подає raw native frame, same-frame match, tracked progress, health, read-only relative altitude, visual scale та image-derived yaw residual; `local_pose` завжди absent.

## Доведена Валідація

- Raw Gray8 distance (`2026-09-26`): профілювання віднесло 76.94% часу `match` до звичайного порівняння, лише 0.58% до вставки кандидатів. Часткові суми в обмежених `uint32_t` блоках із загальною `uint64_t` сумою зменшили медіану на 13 PGM `2.727 → 0.579 ms`, зі scale refinement `3.504 → 1.381 ms`. Усі 44 сценарії та edge-діагностика шести маршрутів еквівалентні; 65622 scalar-oracle API cases, Debug/Release `57/57`, targeted ASan/UBSan пройшов. [Звіт і методика](RAW_DISTANCE_OPTIMIZATION_2026-09-26_UA.md).
- Scale kernel (`2026-09-25`): профілювання виявило близько 80% часу `match` у масштабованому порівнянні. Повторні координати винесено в локальні блоки без heap/cache. На 13 PGM із ненульовою похибкою медіана `14.197 → 3.474 ms` (приблизно 4.1×); усі 44 before/after сценарії еквівалентні. 11019 scalar-oracle порівнянь побітово однакові, Debug/Release `56/56`, ASan/UBSan test пройшов. [Звіт і методика](SCALE_KERNEL_OPTIMIZATION_2026-09-25_UA.md).
- Early exit scale refinement (`2026-09-24`): усі 44 before/after сценарії еквівалентні (69741 self-query та 234 PGM-query на варіант). На exact self-кадрах медіана `11.17–11.31 → 0.235–0.246 ms`; на 13 stop-frame зі refinement помітного виграшу немає (`14.225 → 14.076 ms`). Debug/Release `55/55`, Python `15/15` і `5/5`, `BUILD_TESTING=OFF` успішний із 0 тестів. [Звіт і межі висновку](SCALE_REFINEMENT_OPTIMIZATION_2026-09-24_UA.md).
- Відкладений edge cache (`2026-09-22`): синхронізована одноразова побудова зі збереженням constructor validation та copy/move. Для останнього 600-frame маршруту RSS після конструктора `22396 → 13004 KiB`; перша edge-діагностика `3.016 → 21.608 ms`, після неї пам'ять майже однакова. Сім свіжих процесів на кожен варіант і маршрут, результати edge/match однакові. Debug/Release `54/54`, Python suites `15/15` і `5/5`, окремий ThreadSanitizer test пройшов. [Звіт, методика та компроміс](EDGE_CACHE_OPTIMIZATION_2026-09-22_UA.md).
- Початковий matcher benchmark: усі `2583/2583` власні кадри шести маршрутів точно зіставлено у трьох режимах. Для `11/13` stop-frame знімків глобальний індекс відрізняється від історичного stateful результату; це не error rate без незалежної розмітки. [Початкові виміри](MATCHER_BENCHMARK_2026-09-22_UA.md).
- Поточна software validation (`2026-09-21`): після виправлення чотирьох відтворених numeric configuration defects — окремі чисті WSL/GCC 13.3/Ninja Debug і Release по `52/52`, shell `15/15`. Новий CLI test містить `60` випадків, із яких `35` не проходили на старій програмі; окремий API test захищає endpoint/rollback validation без CLI. Усі сім camera/output flags OFF, assertions активні в усіх `48` стандартних C++ test targets, `BUILD_TESTING=OFF` реєструє `0` тестів. Повторення: `bash scripts/test-software.sh`. [Звіт і межі перевірки](SOFTWARE_VALIDATION_2026-09-21_UA.md).
- Desktop WSL/GCC Debug all-output-off CTest: `47/47` до й після першого рефакторингу спільного Gray8 scale-distance kernel (`2026-09-18`), включно з окремим чистим GCC 13.3/Ninja build.
- Після другого кроку (виділення обліку raw/tracked progress у тому самому модулі) цей WSL/GCC Debug/Ninja build проходить `48/48`, включно з новим `live_route_progress` test. Усі сім `VISUAL_HOMING_*` build flags залишаються `OFF`; Pi/MSVC evidence не оновлювався.
- Після CMake cleanup (`2026-09-20`): WSL/GCC 13.3/Ninja Debug `48/48` до й після зміни; окремий чистий Release build `core/build-wsl-cmake-release-20260920` теж `48/48`. Усі `147` Debug build commands, `96` compile-command records та назви/порядок/команди/властивості `48` CTest сценаріїв збігаються з baseline (службові source backtraces не порівнювалися). У всіх `45` стандартних unit-test targets Release має `-UNDEBUG` після `-DNDEBUG`; окрема конфігурація `BUILD_TESTING=OFF` реєструє `0` тестів. Усі сім output/camera flags OFF; Pi/MSVC acceptance не оновлено.
- Після винесення спільного shell log parser (`2026-09-20`): окремі WSL/Python CLI-тести `15/15` до й після рефакторингу; `68` порівнянь із початковими скриптами зберегли stdout/stderr/exit codes (у usage нормалізовано тільки шлях до копії скрипта). JSON file output також побайтово збігається; `bash -n` пройшов для чотирьох споживачів і бібліотеки. Тести: `python3 scripts/tests/test_readiness_logs.py -v`. Це окрема suite, вона не збільшує CTest `48/48`; C++/Pi/MSVC повторно не перевірялися в цьому shell-only кроці.
- Clean Pi Zero 2W/OV9281 all-output-off CTest: `46/46` на commit `135942f`, `1400 s`, `get_throttled=0x0`.
- Pi build log: `/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260726T225735Z.log`.
- Log SHA-256: `88d6cacaa5e3c24f4a5333b2b93e191d26056affcefad3893adaa75e6dbacd99`.
- Accepted async native `1280x800` benchmark: `60/60` verified publications, `8439/8439` accepted/completed, `1514` explicit backpressure, max outstanding `2`, zero worker/publication/abandoned/discarded failures, camera loop `16.5967 fps`, RSS max `25580 KiB`, temperature max `61.224 °C`, throttle `0x0`.
- Попередній MSVC 19.44/Ninja affected baseline проходив; поточна Windows перевірка не стартувала, бо локальний Visual Studio Build Tools shell не має доступного `cl.exe`. Це toolchain blocker, не test failure.

Це software/camera/storage evidence. Воно не є real-FC acceptance, allowed-send evidence або flight evidence.

Останній endpoint/dwell slice (`2026-09-20`): WSL/GCC Debug baseline `48/48`, потім Debug і Release по `49/49` із новим `live_route_endpoint`. Characterization tests спочатку пройшли на механічно скопійованому початковому decision-блоці без export/I/O, потім на production helper. Source-equivalence перевірка підтвердила збереження decision body, frame export/logging і решти camera runtime. В обох конфігураціях усі сім `VISUAL_HOMING_*` flags OFF; новий Release test має `-UNDEBUG` після `-DNDEBUG`. Pi/MSVC acceptance не оновлено.

Останній readiness slice (`2026-09-20`): початкова WSL/GCC Debug база `49/49`; після виділення policy calculations Debug і Release проходять `50/50` із новим `live_route_readiness`, shell log-consumer suite — `15/15`. Нові characterization tests пройшли на початкових скопійованих policy-блоках і на production helpers. Перевірено дослівне збереження умов/причин, незалежність переміщених fraction/altitude-window calculations, незмінність решти runtime та обох log formatters. Усі сім build flags OFF; Release assertions активні. Pi/MSVC acceptance не оновлено.

## Що Ще Не Підключено

- trusted metric local pose з uncertainty/approach evidence;
- restart/resume immutable verification revisions і physical SD power-loss durability;
- bounded VHIX search consumer, top-N native-resolution content verification і multi-frame lock;
- global route reacquisition state machine та concrete `reset_reference`;
- reset execution, ODOMETRY runtime/UART attachment, JT_Zero handoff і будь-яка FC command/flight authority.

Перший route-recording pass не може чесно знати normalized route progress. Новий маршрут спочатку треба finalize/index, а verification layer створювати під час другого проходу corridor або контрольованого offline enrichment із достовірною прив’язкою кадрів до progress.

## Наступний Робочий Slice

Поточний фокус за запитом користувача — поступове спрощення коду в окремій гілці. Перший крок прибрав дубльований Gray8 scale-distance kernel і список масштабів у matcher/camera runtime. Другий виділив облік raw/tracked progress у тестовану функцію `live_route_match_record_progress` у тому самому модулі: `match_live_camera_route` скоротився з `1939` до `1878` фізичних рядків, без зміни формули згладжування, порогів, полів результату чи логів. Невалідний кадр оновлює raw-статистику, але повертає absent current tracked progress; збережене попереднє значення не стає свіжим endpoint/verification evidence. Загальний production-код цього кроку збільшився на `18` рядків через явний інтерфейс/стан; це локалізація відповідальності й закриття прогалини тестів, не заявлена економія пам'яті або FPS.

Після додаткового read-only огляду рекомендований порядок наступних невеликих кроків:

1. Виконано `2026-09-20`: `45` стандартних unit-test блоків замінено на `add_visual_homing_test(name)` у тому самому `core/CMakeLists.txt`; файл скоротився з `833` до `305` фізичних рядків (`-528`, `-63.4%` саме build-файлу). Спеціальний SITL self-test і два RC12 сценарії залишилися явними. Назви, flags, assert overrides і test properties збережені; C++ код та output interlocks не змінені. Це скорочення дублювання build-опису, не заявлена економія runtime/RAM/FPS.
2. Виконано `2026-09-20`: спільний `extract_field` винесено в `scripts/lib/log-fields.sh`; його підключають `check-external-nav-readiness-log.sh`, `check-live-readiness-log.sh`, `export-external-nav-readiness-json.sh` та `operator-readiness-summary.sh`. До редагування production-скриптів зафіксовано синтетичний log/JSON fixture і `15` CLI-тестів. Збережено first-duplicate/last-summary behavior, missing/empty fields, literal-space/extra-equals/CR handling, strict/quality/operator відмінності, auto counters, diagnostics і exit codes. Критерії готовності залишилися в кожному споживачі; бібліотеку потрібно переносити разом зі скриптами. Інші копії parser у hardware/audit wrappers залишаються окремим можливим slice після власних fixtures.
3. Виконано `2026-09-20`: endpoint/dwell decisions винесено в `live_route_match_update_endpoint` у тому самому camera-модулі; `LiveRouteMatchEndpointState` об'єднує два наявні таймери. Функція використовує тільки metadata кадру, match/current tracked progress, gaps і processing timestamp; оновлює ті самі поля результату та повертає рішення про зупинку. Експорт PGM і логування лишилися в camera loop. `match_live_camera_route` скоротився з `1878` до `1795` фізичних рядків; загальний production source/header зріс на `37` рядків через явний інтерфейс/стан, без заяв про FPS/RAM. Тести фіксують forward/reverse, точні threshold/dwell boundaries, missing/low gaps, переходи confirmed/ambiguous, metadata, disabled/zero dwell, повторні/зворотні timestamps та progress-tracker composition. Invalid match або absent current progress не скидає таймер і не завершує маршрут; наступний valid endpoint sample враховує elapsed gap. Цю наявну continuity policy збережено й прямо протестовано; її зміну слід визначати окремо, це не автоматично доведений баг.
4. Виконано `2026-09-20`: у тому самому camera-модулі додано `live_route_match_evaluate_route_quality(config, result)` та `live_route_match_evaluate_session_readiness(config, result)`. Перший обчислює progress/command/telemetry quality перед final output-gate diagnostic; другий — fractions, altitude window/blocker, загальний session pass та strict/quality/operator readiness перед звітом. Це одноразова фіналізація нового accumulated result, не повторна оцінка під час збору кадрів. Критерії, пріоритет причин, прапорці output authority й `live_route_match_done`/`live_route_match_compact` formatters збережено. `match_live_camera_route` скоротився з `1795` до `1626` фізичних рядків; production source/header зріс на `19` рядків через межі функцій. Тести покривають exact thresholds, zero denominators, missing evidence, optional gates, direction/endpoint відмінності, early stop/ambiguity, strict versus quality versus operator та відсутність side effects на output counters/permission. Це testability cleanup без виміряного FPS/RAM ефекту.
5. Виконано обмежений configuration audit/fix (`2026-09-21`): відхилено мінус після пробілів у спільному uint64 parser, перевірено uint32 narrowing і обидва interval conversions, додано finite validation у double parser та для трьох endpoint/rollback runtime-полів. Нові CLI/API тести доводять конкретні виправлення. Це не повний аудит усіх `85` полів або інших прямих `std::sto*` викликів; універсальний configuration framework не додавався.

Пункти 1–4 і конкретний numeric fix із пункту 5 завершено. Поточний фокус — доказова програмна перевірка: інвентаризація доступних повних replay-записів і обмежений наскрізний offline-сценарій актуальної live-логіки з контрольованим часом та відмовами. Реальну точність оцінювати тільки на незалежному повторному проході з еталонною розміткою. Invalid-match dwell continuity покрита тестами; зміну policy розглядати окремо. Не переписувати весь runtime одразу, не додавати ієрархії класів або універсальний менеджер компонентів і не прибирати freshness, bounded queues чи fail-closed gates заради LOC.

### Точка Відновлення 2026-09-26

- Попередні CMake/shell/endpoint/readiness зміни зафіксовано в `5793361` після `fff1361`. Numeric config fix, два CTest-сценарії, спільна команда `scripts/test-software.sh` і звіт зафіксовані в `7761d3a`; обидва commits опубліковані. Гілка залишається `refactor/optimization-tech-debt`; актуальний commit перевіряти через `git log`/`git status`.
- Остання C++ desktop валідація: після raw distance optimization Debug і Release по `57/57` (інкрементально на попередніх чистих builds); усі сім `VISUAL_HOMING_*` flags OFF; shell `15/15`, benchmark helpers `5/5`; `BUILD_TESTING=OFF` успішний із 0 тестів. Новий raw-distance test пройшов ASan/UBSan; scale-kernel ASan/UBSan evidence — від `2026-09-25`, edge-cache ThreadSanitizer — від `2026-09-22`. `60` CLI cases входять в один CTest-сценарій. `scripts/test-software.sh` запускає обидві Python suites. Pi/MSVC/hardware acceptance цим не підтверджено.
- `2026-09-22` за вказівкою користувача виконано read-only інвентаризацію правильного борту зі строгим SSH host-key match. Знайдено шість маршрутів із timestamps та 13 stop-frame знімків; усі 19 файлів скопійовано в `artifacts/board-inventory-20260922/data/`, SHA-256 збігаються, поточний локальний core читає шість маршрутів. Повної окремої послідовності кадрів повторного проходу в перевірених місцях не знайдено; sparse benchmark captures її не замінюють. Борт лишився на `2d90dac`, без оновлення коду або hardware acceptance.
- Початковий benchmark опубліковано як `a33217a`: `scripts/benchmark-recorded-routes.py`, окремий `gray8_matcher_benchmark`, сирі результати `artifacts/matcher-benchmark-20260922/run1/`. Один маршрут (`20260710T154326Z`) не проходить наявну distinctiveness policy; self-match усіх маршрутів проходить. Борт для подальшої локальної роботи не потрібен.
- Lazy edge-cache завершено й опубліковано як `6da72f4`: одноразова синхронізована побудова; RSS і перший edge-виклик виміряно проти ізольованого `a33217a`. Артефакти `artifacts/edge-cache-review-20260922/`, методика в `EDGE_CACHE_OPTIMIZATION_2026-09-22_UA.md`.
- Early exit scale refinement завершено `2026-09-24` й опубліковано як `1345ab9`: зупинка лише після математично непокращуваної рангової оцінки, без epsilon; тести спочатку пройшли на архіві `6da72f4`. Додано `pgm-scale` та повні match/candidate поля benchmark, before/after runner. Усі 44 сценарії еквівалентні; артефакти `artifacts/scale-refinement-20260924/`, методика в `SCALE_REFINEMENT_OPTIMIZATION_2026-09-24_UA.md`.
- Scale kernel optimization завершено `2026-09-25` й опубліковано як `0936cf1`: блоки до 128 стовпців повторно використовують точні координати, локальні масиви 1 KiB, без heap/cache. На PGM зі refinement медіана `14.197 → 3.474 ms`, контрольні режими практично незмінні. Усі 44 сценарії та 11019 kernel scores еквівалентні; артефакти `artifacts/scale-kernel-20260925/`, методика в `SCALE_KERNEL_OPTIMIZATION_2026-09-25_UA.md`.
- Raw distance optimization завершено `2026-09-26`: часткові суми блоками до 65536 пікселів у `uint32_t`, загальна сума `uint64_t`, без heap/cache чи зміни формули. На PGM зі refinement медіана `3.504 → 1.381 ms`; усі 44 сценарії та edge-діагностика шести маршрутів збігаються. 65622 API cases включають суму понад `UINT32_MAX`; артефакти `artifacts/raw-distance-20260925/`, методика в `RAW_DISTANCE_OPTIMIZATION_2026-09-26_UA.md`.
- Наступний обмежений software крок — встановити покриття наявних offline API й тестів для ланцюжка `match → progress → endpoint/readiness`, потім закрити конкретну прогалину контрольованим часом/відмовами через фактичні функції. Після оптимізації профіль `match`: raw distance близько 43%, scale kernel 53%, candidates 1.24%; окрема оптимізація вставки кандидатів зараз не обґрунтована. Не переносити весь camera runtime і не змінювати policy. Точність і пороги за self-кадрами не оцінювати; незалежна оцінка потребує повного другого проходу з розміткою. До запуску камери або апаратної перевірки як частини software slice не переходити.
- Залишені локальні untracked `.claude/`, `.codex/`, `AGENTS.md`, `CLAUDE.md`, `codex_jtzero_known_hosts`, `graphify-out/` не додавати до коміту без окремої потреби. Graphify використовувався через query/update; обмеження GitNexus FTS та неповного call graph залишаються чинними.

Функціональна черга operational verification залишається окремою і не закривається цим рефакторингом:

1. Повторити clean Pi all-output-off suite для operational progress-only wiring (вже закомічено у `62c772b`) та наступних змін.
2. Перевірити на Pi exact VHRM/VHIX/VHRS package binding, publisher drain, zero gates і immutable output revisions без відкриття output path.
3. Відновити affected MSVC verification після ремонту локального `cl.exe` toolchain; це не блокує Linux/Pi evidence, але лишається release checklist item.
4. Підготувати окремий hand-carried second-pass wrapper/checker для вже finalized/indexed short route.

Лише після цього потрібен фізичний hand-carried second pass по вже записаному короткому маршруту: forward і reverse, приблизно `5–10 m`, без arm/send. Його мета — довести, що live matcher створює коректно прив’язані sparse verification revisions. Вуличний тест не потрібен для поточного програмного slice; достатнє освітлене приміщення з корисною текстурою і безпечним прямим коридором.

## Подальші Milestones До Перших Польотів

1. Operational progress-only verification wiring: desktop implementation complete; Pi acceptance pending.
2. Hand-carried second-pass verification capture forward/reverse.
3. Immutable revision resume та SD fault/durability acceptance.
4. Offline bounded VHIX coarse search -> top-N native verification -> multi-frame route lock, включно з off-route negatives.
5. Global reacquisition state machine і точний `reset_reference`; replay/SITL reset-counter, timeout і discontinuity acceptance.
6. Trusted local-pose/gate producer та single-owner JT_Zero/Visual Homing handoff design.
7. Real-FC props-off origin/Home/provider acceptance з explicit FC acceptance/rejection evidence.
8. Окремий reviewed restrained/tethered plan з manual abort і failsafe rehearsal.
9. Лише після проходження всіх попередніх gates — окремо погоджений перший bounded flight test.

## Safety Invariants

- Default live MAVLink output залишається OFF і fail closed.
- Жодний documentation або software test не авторизує arm, motor output, flight чи autonomous search.
- Reset не виправляє wind drift і не знаходить невідомий маршрут: він лише очищає stale local tracking; ODOMETRY може поновитися тільки після окремого consistent route reacquisition.
- Географічний bearing не потрібен для route-local traversal уздовж оператором підтвердженого прямого `~10 m` corridor. Geographic/shared-frame prior потрібен лише для майбутнього підведення до ще невидимого transferred route, не для GPS-denied matching усередині знайомого corridor.
- Історичні field/evidence документи залишаються timestamped evidence і не повинні трактуватися як поточна hardware гарантія або найближча команда.
