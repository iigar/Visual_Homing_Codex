# Поточний Стан Проєкту

Оновлено: `2026-08-13`.

Repository baseline at reconciliation: `main` commit `2d90dac826e186fa422ebc566073b42a2f42eb84`, synchronized with `origin/main` before these documentation changes.

Local intelligence after reconciliation: Graphify `5527` nodes / `8301` edges / `460` communities; GitNexus `9864` nodes / `20928` edges / `212` clusters / `205` flows. GitNexus FTS/BM25 is unavailable in the current Windows LadybugDB environment, але symbol/context/impact/detect navigation працює.

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

## Доведена Валідація

- Desktop WSL/GCC all-output-off CTest: `46/46`.
- Clean Pi Zero 2W/OV9281 all-output-off CTest: `46/46` на commit `135942f`, `1400 s`, `get_throttled=0x0`.
- Pi build log: `/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260726T225735Z.log`.
- Log SHA-256: `88d6cacaa5e3c24f4a5333b2b93e191d26056affcefad3893adaa75e6dbacd99`.
- Accepted async native `1280x800` benchmark: `60/60` verified publications, `8439/8439` accepted/completed, `1514` explicit backpressure, max outstanding `2`, zero worker/publication/abandoned/discarded failures, camera loop `16.5967 fps`, RSS max `25580 KiB`, temperature max `61.224 °C`, throttle `0x0`.
- MSVC 19.44/Ninja affected tests passed; the new composition test also passed `100` repeated desktop runs.

Це software/camera/storage evidence. Воно не є real-FC acceptance, allowed-send evidence або flight evidence.

## Що Ще Не Підключено

- operational caller у live route matcher/CLI або окремому route-enrichment runtime;
- CLI/environment contract для source VHRM/VHIX, output revision base та scalar sources;
- trusted metric local pose з uncertainty/approach evidence;
- restart/resume immutable verification revisions і physical SD power-loss durability;
- bounded VHIX search consumer, top-N native-resolution content verification і multi-frame lock;
- global route reacquisition state machine та concrete `reset_reference`;
- reset execution, ODOMETRY runtime/UART attachment, JT_Zero handoff і будь-яка FC command/flight authority.

Перший route-recording pass не може чесно знати normalized route progress. Новий маршрут спочатку треба finalize/index, а verification layer створювати під час другого проходу corridor або контрольованого offline enrichment із достовірною прив’язкою кадрів до progress.

## Наступний Робочий Slice

Наступний етап знову програмний, без керування дроном:

1. Під’єднати `LiveRouteVerificationProducer` до існуючого live matcher/CLI в progress-only mode.
2. Додати explicit config для indexed source package, output revision directory і fresh altitude/scale/yaw inputs.
3. Зберегти `has_local_pose=false`: перший caller може писати sparse verification keyframes, але не gate records.
4. Додати deterministic replay/fake-source tests для same-frame provenance, stale inputs, backpressure, terminal worker failure, clean drain і first-pass-recording rejection.
5. Повторити desktop all-output-off suite, affected MSVC tests і clean Pi all-output-off suite.

Лише після цього потрібен фізичний hand-carried second pass по вже записаному короткому маршруту: forward і reverse, приблизно `5–10 m`, без arm/send. Його мета — довести, що live matcher створює коректно прив’язані sparse verification revisions. Вуличний тест не потрібен для поточного програмного slice; достатнє освітлене приміщення з корисною текстурою і безпечним прямим коридором.

## Подальші Milestones До Перших Польотів

1. Operational progress-only verification wiring і software/Pi acceptance.
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
