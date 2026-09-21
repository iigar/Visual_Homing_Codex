# Поточний Стан Проєкту

Оновлено: `2026-09-20`.

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
5. Перевірити конфігурацію (`85` полів, з них `30` bool у `LiveRouteMatchingConfig`) на дублювання значень, суперечливі комбінації й повторення розбору параметрів. Кількість полів сама по собі не доводить зайвість; не створювати універсальний configuration framework.

Пункти 1–4 завершено; наступний software-only slice — пункт 5, read-only audit конфігурації: зіставити defaults, runtime validation, env parsing і script overrides, знайти конкретні дублювання або суперечності та вибрати найменшу доведену зміну з окремими тестами. Не вважати велику кількість полів доказом зайвості. Invalid-match dwell continuity покрита тестами; зміну policy розглядати окремо. Не переписувати весь runtime одразу, не додавати ієрархії класів або універсальний менеджер компонентів і не прибирати freshness, bounded queues чи fail-closed gates заради LOC. Оцінювати реальне зменшення повторень/залежностей і місць зміни, а не лише перенесення рядків між функціями.

### Точка Відновлення 2026-09-20

- Роботу відновлено після документаційної точки `fff1361`; CMake cleanup, спільний parser для чотирьох readiness/reporting scripts, endpoint/dwell і readiness extraction завершено у working tree. На кінець цього кроку commit лишається `fff1361`: раніше staged CMake/docs збережено, нові shell/C++ зміни, CMake registrations для endpoint/readiness tests і доповнення docs не staged. Гілка залишається `refactor/optimization-tech-debt`; перед продовженням перевірити `git log`/`git status`.
- Закомічена C++ база — `192bb4c` (progress accounting) після `de1f5bb` (спільний Gray8 scale kernel); working tree додатково має endpoint helper/state, дві readiness-функції та відповідні нові test files.
- Остання C++ desktop валідація: Debug і Release по `50/50`, усі сім `VISUAL_HOMING_*` flags OFF; shell `15/15` повторно пройшли в readiness slice. `68` before/after shell comparisons належать попередньому parser slice. Pi/MSVC/hardware acceptance цим не підтверджено.
- При відновленні прочитати цей snapshot і найновіший запис `SESSION_LOG.md`, перевірити `git status`/branch/log; наступний рекомендований software-only slice — конфігураційний audit defaults/validation/env/scripts із конкретним обмеженим cleanup за результатами. Не підключати обладнання як частину cleanup.
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
