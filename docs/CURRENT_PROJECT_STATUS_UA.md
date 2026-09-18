# Поточний Стан Проєкту

Оновлено: `2026-09-18`.

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
- Clean Pi Zero 2W/OV9281 all-output-off CTest: `46/46` на commit `135942f`, `1400 s`, `get_throttled=0x0`.
- Pi build log: `/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260726T225735Z.log`.
- Log SHA-256: `88d6cacaa5e3c24f4a5333b2b93e191d26056affcefad3893adaa75e6dbacd99`.
- Accepted async native `1280x800` benchmark: `60/60` verified publications, `8439/8439` accepted/completed, `1514` explicit backpressure, max outstanding `2`, zero worker/publication/abandoned/discarded failures, camera loop `16.5967 fps`, RSS max `25580 KiB`, temperature max `61.224 °C`, throttle `0x0`.
- Попередній MSVC 19.44/Ninja affected baseline проходив; поточна Windows перевірка не стартувала, бо локальний Visual Studio Build Tools shell не має доступного `cl.exe`. Це toolchain blocker, не test failure.

Це software/camera/storage evidence. Воно не є real-FC acceptance, allowed-send evidence або flight evidence.

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

1. Прибрати повторення реєстрації тестів у `core/CMakeLists.txt`: на поточному коді це `833` фізичні рядки та `46` майже однакових блоків test executables/compile options. Використати одну невелику helper-функцію, зберегти назви тестів, compiler flags, `-UNDEBUG`/`/UNDEBUG` та спеціальні сценарії; не вводити нову build-систему.
2. Винести спільний розбір полів логів зі скриптів: однаковий `extract_field` є щонайменше в `check-external-nav-readiness-log.sh`, `check-live-readiness-log.sh`, `export-external-nav-readiness-json.sh` та `operator-readiness-summary.sh`. Спочатку зафіксувати чинні результати на fixtures; не об'єднувати різні safety/readiness критерії лише через схожість коду.
3. Відокремити й прямо протестувати endpoint/dwell decision logic від camera loop, експорту кадру та логування, після власного impact review. Важливий відкритий випадок: невалідний match пропускає endpoint-блок і не скидає його таймер. Це спостережена поведінка, не доведений баг; чи має перерва переривати підтвердження, слід визначити окремо. Не змінювати семантику мовчки під виглядом рефакторингу.
4. Відділити обчислення готовності від форматування звітів, зі збереженням відмінності strict/quality/operator readiness та чинного log contract.
5. Перевірити конфігурацію (`85` полів, з них `30` bool у `LiveRouteMatchingConfig`) на дублювання значень, суперечливі комбінації й повторення розбору параметрів. Кількість полів сама по собі не доводить зайвість; не створювати універсальний configuration framework.

Це збережені рекомендації, ще не реалізовані зміни. Не переписувати весь runtime одразу, не додавати ієрархії класів або універсальний менеджер компонентів і не прибирати freshness, bounded queues чи fail-closed gates заради LOC. Оцінювати реальне зменшення повторень/залежностей і місць зміни, а не лише перенесення рядків між функціями.

### Точка Відновлення Після Паузи 2026-09-18

- Користувач попросив зберегти роботу до завтра; нову реалізацію під час збереження не починати.
- Останній code commit: `192bb4c` (progress accounting), попередній: `de1f5bb` (спільний Gray8 scale kernel). Обидва опубліковані в `refactor/optimization-tech-debt`; наступний documentation checkpoint не означає новий test run.
- Останній перевірений desktop результат: WSL/GCC Debug/Ninja `48/48`, build directory `core/build-wsl-tech-debt`, усі сім `VISUAL_HOMING_*` flags OFF. Pi/MSVC/hardware acceptance цим не підтверджено.
- При відновленні прочитати цей snapshot і найновіший запис `SESSION_LOG.md`, перевірити `git status`/branch/log; перший рекомендований software-only slice — CMake cleanup. Не підключати обладнання як частину цього cleanup.
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
