# Warm audit latency одного великого архіву

Підготовка: `2026-10-30`; вимірювання й завершення: `2026-10-31`.
Продовження [large-archive baseline](LARGE_ARCHIVE_BASELINE_2026-10-30_UA.md)
на `f704a2a63c0fada78968efd1ea5a33d4f42372ec`, звіреному з remote.
Tracked tree перед початком чистий. Production, tests, runner і config не
редагувалися; measurement scripts/evidence збережено в ignored artifacts.

## Workload та метод

Взято один `frames_per_dataset-64-repeat-00` із hash-bound tar попереднього
baseline: **12 datasets / 12 saved runs / 64 query frames / 32×32 /
4 references**, **768 унікальних frame-run pairs**, **11137 files**,
**26 044 586 bytes** без directory metadata. Це один і той самий архів,
а не 16 незалежних captures або нові replay runs.

Tar SHA-256 звірено перед extraction; обраний sample перенесено в native
WSL temporary directory. Trusted sources копіювалися з поточного checkout,
звірялися з baseline hashes і працювали з native WSL filesystem. Archived
code/binary не імпортувалися й не виконувалися. Кожний audit отримував
збережені external plan/state SHA-256 anchors.

Один excluded warmup, потім **15 послідовних unprofiled audits**. Кожний
в окремому fresh Python worker, default GC enabled, той самий relocation
path і input archive. Перед кожним worker parent читає всі archive files
для bytes/mtime snapshot; після worker snapshot повторюється. Це явно
**warm-cache** workload без cache eviction. Fresh process не означає cold cache.

Timer охоплює тільки `check()`. Source/tar copying, snapshots, imports,
oracle comparison, normalization, JSON serialization і process startup
поза timer. VmHWM знято відразу після API, до oracle/report persistence;
це current-image peak worker з imports, не simultaneous process-tree peak.
Початкові VmHWM та legacy `ru_maxrss` теж збережено. Parent snapshots і
його великі report objects не включаються до worker VmHWM.

Під час audit блокуються `subprocess.run`, `subprocess.Popen` і runner
`run`. Порівнюється **повний normalized report** з persisted baseline:
нормалізуються лише документовані dataset-root display fields через чинний
`portable_report`; hashes, diagnostics, controls, progress і metrics не
вилучаються. Окремо перевіряються exact prediction/metric/coverage oracles,
усі 11 peers кожного run, bytes/mtime та input/source immutability.

## Результати

Probe та незалежний finalizer завершилися з **exit 0**. Всі **16 audits**
(warmup + 15 timed) відтворюють повний normalized baseline report та exact
oracles; archive bytes/mtime, inputs і всі **242 source hashes** незмінні.
Це один архів / 12 saved runs / 768 унікальних pairs, **0 нових replay executions**.

| Показник | Median | Min–max | p90 nearest-rank |
|---|---:|---:|---:|
| Audit API, s | 10.461 | 10.371–11.111 | 11.032 |
| Worker VmHWM, MiB | 60.78 | 60.58–60.90 | 60.90 |

Warmup: **10.245 s / 60.78 MiB**, виключений зі статистики. Усі timed samples
за порядком запуску, секунди:

| Sample | Audit API, s | Sample | Audit API, s | Sample | Audit API, s |
|---:|---:|---:|---:|---:|---:|
| 1 | 11.111 | 6 | 10.371 | 11 | 10.414 |
| 2 | 10.416 | 7 | 10.418 | 12 | 10.384 |
| 3 | 10.708 | 8 | 10.461 | 13 | 10.516 |
| 4 | 10.540 | 9 | 10.444 | 14 | 10.454 |
| 5 | 11.032 | 10 | 10.619 | 15 | 10.826 |

Legacy `ru_maxrss` має **132.11–142.15 MiB**, median **141.66 MiB**: це
успадкований pre-exec parent floor, уже присутній до audit. Current-image
VmHWM на старті worker близько 29 MiB і після API близько 61 MiB. Legacy
поле збережено в raw evidence, але не використовується як пам'ять audit worker.

Host: Intel Core i5-10300H, WSL2 `6.6.87.2-microsoft-standard-WSL2`, native
filesystem `ext2/ext3`, affinity CPUs 0–7. Python **3.12.3**, build metadata
`main, Aug 31 2026, 10:18:26`, GCC 13.3.0. Попередній large-archive baseline
записав ту саму версію, але `main, Mar 23 2026, 19:04:32`. Причину різниці
build metadata не досліджували; це додаткова причина не трактувати timings
двох запусків як before/after. Самі archive results відтворилися точно.

## Статистична межа

Усі 15 timed samples входять до median/range/p90. Warmup не входить.
**p90 — nearest-rank**, без interpolation: `sorted[ceil(0.90 × 15) - 1]`,
тобто **14-й елемент**, другий найбільший sample. Це описова статистика
цього запуску; 15 спостережень не доводять p99 або tail-latency guarantee.
Повільні samples не відкидалися, автоматичних retries немає.

Числа не є contemporaneous before/after відносно попереднього baseline:
інший час запуску й relocation path. Немає production speedup, accuracy,
Pi/Windows-filesystem або cold-cache claim. Нових replay executions немає.

## Валідація та її межі

Finalizer повторно прочитав bound baseline прямо з tar, перевірив усі 16
persisted full reports/oracles, external anchors, незмінний archive path,
окремі worker PIDs, raw result/summary agreement і перерахував median/range/p90.
Hashes archive files збігаються з inventory перед persistence; локальні
snapshots підтверджують незмінність bytes/mtime навколо кожного audit.

Усі 242 source hashes перевірено до/після study і незалежним finalizer;
production/tests/build scripts/config не змінено. Сім camera/output flags
OFF у Debug/Release caches. Новий full software runner не запускався:
**343 Python tests і 64/64 CTest на кожну збірку — попередній checkpoint**,
а не нова full-suite перевірка поточного Python build. Нове evidence тут —
16 actual audits і незалежний readback. Native sanitizer/NoTests/MSVC/hardware
runs відсутні. Tracked зміни лише документаційні.

## Evidence і відтворення

Локальна ignored директорія: `artifacts/warm-audit-latency-20261030/`.
`probe.py` перевіряє source/tar identities, запускає workers, зберігає jobs,
повні reports/oracles, raw measurements і summary. `finalize.py` незалежно
читає baseline із bound tar, порівнює всі збережені reports/oracles, повторно
рахує aggregates, звіряє 242 source hashes і сім output flags у кожній збірці.
`evidence-sha256.json` прив'язує локальні scripts/results.

```sh
python3 artifacts/warm-audit-latency-20261030/probe.py
python3 artifacts/warm-audit-latency-20261030/finalize.py
```

Probe вимагає відсутньої `results/` і не перезаписує попередні samples.
Для повтору скопіювати scripts до нового sibling artifact directory, залишивши
цей evidence незмінним; finalizer очікує declared checkout `f704a2a`.
Потрібні локальний baseline tar і відповідні trusted sources; новий clone
містить документацію, але не ignored scripts/tar/logs.
Якщо source hashes змінилися, старий study не перезапускати зі змішаними tools.

Input tar SHA-256:
`bedb5f838258e006b378a7cc30b42b279524c58a81b5f0fb5361fd0bc279453b`.

Summary SHA-256:
`850f49ac95c702ce1493d8388a5d19ad07de04a2c31147857f1b0f6b18127d1c`.
Canonical normalized-report SHA-256:
`aee128b3105f6825340ce96cb93ef4cf590b1e98506a7f338eb020b569183da1`.

GitNexus metadata досі `6d8f547`; використано прямий source/caller/diff review.
Hardware/output роботи не було; fail-closed gates збережено.

## Наступний bounded experiment

Окремо перевірити overhead **PGM header parsing** у validator. Попередній
large-archive profile показав `pgm` 19 200 calls / 1.673 s cumulative,
`token` 76 800 / 1.003 s і `byte` 249 600 / 0.678 s із 25.759 s profiled
audit. Ці вкладені times не додаються, а весь PGM parser — лише приблизно
6.5% profiled API; header-only potential ще менший. Це підстава для малого
експерименту, не обіцянка помітного speedup або дозвіл на великий rewrite.

Спочатку підтвердити hotspot на поточному runtime, зафіксувавши Python
build/executable identity, і охарактеризувати P5/comments/whitespace,
single-separator/CRLF, 4096-byte header limit, truncations/trailing bytes,
перший binary pixel whitespace/`#`, geometry і 64 MiB payload bound.
Зберегти hashes саме consumed bytes, повне читання, fresh filesystem checks
і mutation guards. Локальну candidate implementation випробовувати лише
після characterization; isolated gain має підтвердитися alternating paired
full audits одного архіву з exact reports/oracles. Якщо користь мала або
еквівалентність непевна — записати no-change result. Production зміна потребує
регресій і нового full runner. Відхилену path-constructor заміну не повторювати.

Cold-cache methodology, незалежні annotated data й решта функціональних
кроків залишаються у [збереженій черзі](SESSION_HANDOFF_2026-10-20_UA.md).
