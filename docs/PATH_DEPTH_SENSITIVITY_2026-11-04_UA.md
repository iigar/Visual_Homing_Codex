# Absolute path depth: парний read-only study

Дата: `2026-11-04`. Відновлено remote-verified
`68c8f9bbe3aed63c526c90901c6a1b2c1bff49b7` з чистим tracked tree.
Це наступний bounded крок після
[filesystem attribution](FILESYSTEM_ATTRIBUTION_2026-11-03_UA.md).

## Методика

Той самий hash-bound `frames_per_dataset-64-repeat-00` archive скопійовано
через `copy2` у два native WSL розміщення. Кожна копія має **11137 files,
12 catalog datasets, 12 saved runs, 64 frames/dataset, 32×32 pixels,
4 references, 768 unique frame-run pairs**; усі 11 peers/run збережено.
Copies/repeats не додають незалежних captures чи нових replay executions.

| Умова | Absolute root | Depth без `/` | Довжина root |
|---|---|---:|---:|
| Shallow | `/tmp/path-depth-jeuxlpe5/sssssssssssssssssssssssssssssss/archive` | 4 | 64 |
| Deep | `/tmp/path-depth-jeuxlpe5/d/a000/b000/c000/d000/e000/f000/archive` | 10 | 64 |

Різниця — **шість components**, довжина absolute path однакова. Обидва roots
мають той самий `st_dev=2096` і `stat -f` filesystem family `ext2/ext3`.
Це family label, не визначення конкретної версії filesystem.
Trusted source root спільний: `/tmp/path-depth-jeuxlpe5/trusted-source`, depth 3.
Inputs та worker cwd теж спільні. Symlinks у fixture roots відсутні.
Тимчасові copies після завершення прибрані; jobs/snapshots/results залишилися
в локальному evidence. Production/cwd/cache policy не змінювалися.

Runtime: Python `3.12.3 (main, Aug 31 2026, 10:18:26) [GCC 13.3.0]`,
`/usr/bin/python3.12`, SHA-256
`e50d468e8b0adfb05733f5b87b3cff34829c4a8c1aea50c865aa8bdfe4bb150f`;
WSL kernel `6.6.87.2-microsoft-standard-WSL2`. Identity збігається з PGM та
filesystem studies. Current trusted sources скопійовано й звірено за hashes;
архівні tools/binary не виконуються. Subprocess run/Popen і runner resume
під час audit заблоковані. Default GC увімкнений.

До запуску зафіксовано **8 pairs / 16 timed audits**, по одному excluded warmup
і окремому cProfile на layout: **20 fresh workers** загалом. Непарні pairs
виконують shallow→deep, парні — deep→shallow. Samples не відкидаються, кількість
не збільшується за результатом. Для кожного audit bytes/mtime snapshots inputs
і відповідної archive copy перевірені до/після; snapshots прогрівають cache.
Наприкінці обидві copies повторно звірені з однаковим snapshot.

API timer охоплює лише чинний `check`; startup/imports, source checks, snapshots,
oracle checks і persistence поза timer. VmHWM current worker записаний після
audit, включає imports; memory-improvement claim не робимо. cProfile runs
відокремлені від timed pairs. Counts нижче є Python/C API calls, не kernel
syscalls; buffered reads і внутрішня робота C methods мають інші OS boundaries.

Є одна physical copy на layout, порядок створення copies фіксований; filesystem
allocation не рандомізовано. Чергування порядку audit зменшує часовий перекіс,
але не ізолює всі host/cache effects. Paired differences описують цей warm
workload, а число API calls окремо перевіряє механізм впливу depth. Це не
cold-cache, Windows/Pi/live latency, accuracy або p90/p99 acceptance.

Прямий source/caller review підтвердив fresh resolve, regular-file stat,
consumed-byte hashes, before/after fstat, initial/final inventories та фінальні
verify_frozen. Validator contained-symlink contract і closed-inventory rejection
збережені. Graph metadata `6d8f547` лишається advisory; чинний доказ — source,
raw profiles та exact reports.

## Результати та рішення

**Shallow layout був швидшим у 8/8 pairs.** Median paired delta
`deep − shallow` = **1.410 s**, median paired reduction `1 − shallow/deep` =
**11.99%**. Це ефект двох розміщень одного warm archive в цьому experiment;
production candidate не реалізовано, загального production speedup не заявляємо.

| Pair | Порядок | Shallow, s | Deep, s | Deep − shallow, s |
|---:|---|---:|---:|---:|
| 1 | shallow→deep | 11.073 | 11.097 | 0.025 |
| 2 | deep→shallow | 9.320 | 12.708 | 3.388 |
| 3 | shallow→deep | 10.309 | 11.068 | 0.758 |
| 4 | deep→shallow | 9.698 | 12.004 | 2.307 |
| 5 | shallow→deep | 10.199 | 11.788 | 1.589 |
| 6 | deep→shallow | 10.022 | 11.197 | 1.175 |
| 7 | shallow→deep | 9.358 | 12.512 | 3.154 |
| 8 | deep→shallow | 10.814 | 12.046 | 1.232 |

| Aggregate | Shallow | Deep |
|---|---:|---:|
| Median 8 timed audits, s | 10.111 | 11.896 |
| Min–max, s | 9.320–11.073 | 11.068–12.708 |
| Окремий cProfile audit, s | 23.143 | 27.390 |
| `os.lstat` API calls у profile | 939417 | 1465395 |

Median парних різниць не дорівнює різниці двох medians; 11.99% — median
восьми pair-wise fractions, не ratio medians. Delta range **0.025–3.388 s**;
median delta для shallow-first pairs **1.174 s**, для deep-first **1.769 s**.
Розкид і різниця між order groups залишають host/time effects видимими.
Samples не відкидалися. Profiles приблизно у 2.3 раза повільніші за timed API;
їхній час не входить у наведені aggregates.

Усі **18 інших tracked call totals** між profiles та попереднім attribution
збігаються: **87879 resolve, 87383 Dataset.path, 87215 Dataset.read,
87239 open, 425423 BufferedReader.read, 174430 fstat, 3954 scandir,
68922 DirEntry.stat, 28 inventory, 14 verify_frozen**, зокрема всі parsers і
validation calls. Fresh checks/content/mutation guards не зменшувалися.

Із 87879 resolves **216** належать спільним trusted-source bindings;
**87663** — archive paths. Різниця profiles:
`1465395 − 939417 = 525978 = 6 × 87663`. Також counts обох layouts точно
узгоджуються з попереднім archive depth 5 / 1027080 lstat. Цей механізм
пояснює додаткові component visits; observer не вимірював kernel syscalls.

**Рішення:** для наступних порівнянь фіксувати absolute depth/path length поряд
із runtime/storage identities. Невелика глибина native working copy є
виміряною можливістю для warm offline audit; не переносити результат на
користувацькі archive locations, Windows, Pi або інший workload без перевірки.
Завершити поточний filesystem slice без production/guard changes.

Наступний bounded крок — **warm end-to-end CLI latency / worker VmHWM** на
тому самому archive з фіксованим shallow layout. Окремо записати parent wall
time від запуску process до завершення JSON output та API-only time, по одному
excluded warmup і щонайменше 15 fresh workers за наперед визначеним планом.
Snapshot/oracle checks — поза timing; worker VmHWM включає startup/imports/
serialization і не підмінюється inherited `ru_maxrss`. Ті самі source/runtime,
exact normalized full reports/oracles/peers і bytes/mtime; no replay/resume.
Cold-cache methodology лишається окремою: fresh process не доводить cold cache,
global cache dropping у цей крок не входить. Matcher і ширша черга збережені.

## Валідація та bindings

Evidence directory: `artifacts/path-depth-20261104/` (local, ignored).
Містить probe/finalizer, 20 jobs/results/full reports/oracles/logs, два raw
pstats/JSON profiles, inventories, source manifest, summary/validation та
`evidence-sha256.json` (**115 bound files**). Probe використовує hash-bound helper попереднього warm
study як бібліотеку; старі studies та їх finalizers не запускаються/не змінюються.
Fresh clone отримує docs/source; ignored evidence зберігається локально.

| Binding | SHA-256 |
|---|---|
| Input tar | `bedb5f838258e006b378a7cc30b42b279524c58a81b5f0fb5361fd0bc279453b` |
| Normalized report | `aee128b3105f6825340ce96cb93ef4cf590b1e98506a7f338eb020b569183da1` |
| Summary | `803850b362962cf33d9927a6c08f91bcaa95b928bb89510dca12a5fa42ce2d9c` |
| 243-source manifest | `a565452a0cfa785d58d2bc8f2b0856032575180b4e090106fc4732c30e958cdc` |

Production/tests/runner/core/config unchanged. Нового full runner для read-only
study не запускали; попередній full log від 3 листопада на ідентичних sources і
runtime перечитано за hash/success/counts: **357 Python tests, Debug/Release
CTest 64/64** кожна. Це historical validation, не новий suite run.
**Probe/finalizer exit 0**; усі **20/20 full normalized reports/exact oracles**
збігаються з bound tar, archive/input bytes/mtime незмінні. Finalizer перечитав
jobs/results/reports/oracles, raw pstats/JSON, перевірив layout controls, schedule,
усі 19 tracked totals і depth prediction, перерахував pairs/aggregates/order groups.
**243 source hashes**, Python identity й сім camera/output flags OFF у кожній
збірці звірено. Hardware/output work і subagents не використовувалися.
