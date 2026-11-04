# Filesystem operations: read-only attribution

Вимірювання: `2026-11-03`; документацію завершено `2026-11-04`. Відновлено remote-verified
`c927c933ae71eac6c5110591194770330e800b5e` з чистим tracked tree за
[handoff](SESSION_HANDOFF_2026-11-03_UA.md).

**Рішення: production залишено без змін.** Основна кількість filesystem calls
припадає на fresh path resolution у validator, а не на closed inventory.
Повторні читання належать окремим integrity boundaries; цей study не встановив
безпечної підстави прибрати жодну з них. Candidate не реалізовано, speedup не
заявляється. Наступний обмежений study — чутливість audit до глибини абсолютного
шляху на тому самому native WSL filesystem, без зміни validator чи guards.

## Метод і межі вимірювання

- Python `3.12.3 (main, Aug 31 2026, 10:18:26) [GCC 13.3.0]`, resolved executable
  `/usr/bin/python3.12`, SHA-256
  `e50d468e8b0adfb05733f5b87b3cff34829c4a8c1aea50c865aa8bdfe4bb150f`;
  WSL kernel `6.6.87.2-microsoft-standard-WSL2`, native `ext2/ext3` storage
  за `stat -f` (назва family, не твердження про конкретну версію filesystem).
- Той самий hash-bound `frames_per_dataset-64-repeat-00` archive:
  **11137 files / 12 datasets / 12 saved runs / 64 frames / 32×32 pixels /
  4 references / 768 унікальних frame-run pairs**, усі 11 peers/run.
  Trusted current sources копіюються на native storage з перевіркою hashes;
  bundled tools і binary є даними. Під час audit subprocess run/Popen та
  runner resume заблоковані. **Нових replay executions — 0.**
- Шість fresh workers у порядку warmup, timed-1, attribution, timed-2,
  cProfile, timed-3. Default GC увімкнений. Bytes/mtime snapshots archive та
  inputs до/після кожного audit прогрівають cache; imports, snapshots, oracle
  checks та persistence поза API timer. Це warm read-only evidence.
- Окремий `sys.setprofile` observer рахує входи у вибрані Python functions і
  C methods. Через стек чинних frames він фіксує найближчий owner
  (`Dataset.path`, `Dataset.read`, constructor, inventory, other) і causal
  chain archive/collection/bundle/score/validate/verify_frozen/binding.
  Файли не proxy-уються, filesystem/parser methods не замінюються.
- **Це Python/C API call counts, не kernel syscall counts.** Buffered read
  може обслуговуватися з buffer; open/DirEntry можуть виконувати внутрішні OS
  операції, яких observer не бачить. Час профілю не є syscall time або
  uninstrumented breakdown. Вкладені cumulative times не додаються.
- Окрема маленька calibration на справжніх Dataset/inventory перевіряє
  coverage read/open/stat/fstat/scandir/DirEntry та cProfile agreement.
  Початкова calibration виявила пропуск `_io.open`; виправлено **до** study,
  повтор пройшов. Початковий diagnostic log збережений. Усі **19** totals
  повного attribution audit незалежно збігаються з окремим raw cProfile audit.

Прямо прочитано implementations/callers у validator, scorer, exporter,
bundle/collection/runner checkers. Graph metadata `6d8f547` застаріла; source,
raw profile та exact reports є evidence цього етапу.

## Кому належать операції

| Owner | Resolve | `os.lstat` | `os.stat` | Open | Buffered read | `os.fstat` |
|---|---:|---:|---:|---:|---:|---:|
| `Dataset.path` | 87383 | 1023002 | 87383 | 0 | 0 | 0 |
| `Dataset.__init__` | 496 | 4078 | 496 | 0 | 0 | 0 |
| `Dataset.read` | 0 | 0 | 0 | 87215 | 425399 | 174430 |
| Closed inventory | 0 | 0 | 0 | 0 | 0 | 0 |
| Other: scorer source hashes | 0 | 0 | 0 | 24 | 24 | 0 |
| **Разом** | **87879** | **1027080** | **87879** | **87239** | **425423** | **174430** |

Closed inventory окремо виконав **28 traversals / 3954 `os.scandir` /
68922 `DirEntry.stat(follow_symlinks=False)`**. Це 2 outer archive inventories,
2 collection inventories і 2 inventories на кожен із 12 bundles. Zero у
таблиці для `os.stat` не означає відсутності metadata work: `DirEntry.stat`
обліковано окремо. Entry stat зберігає symlink/non-regular rejection.

`Dataset.path` викликано 87383 рази, `Dataset.read` — 87215. Різниця **168**:
144 membership checks для embedded dataset roots і 24 configuration path
checks у bundle/outer audit. Read має рівно **два fstat** — до/після споживання.
24 додаткові open/read — source hashes scorer + validator у 12 rescores;
це не читання dataset поза Dataset guards.

Resolve-depth histogram (кількість абсолютних components без `/`):

| Depth | Resolve calls |
|---:|---:|
| 4 | 108 |
| 5 | 110 |
| 6 | 29 |
| 7 | 92 |
| 8 | 72 |
| 9 | 1224 |
| 10 | 5748 |
| 11 | 9744 |
| 12 | 70752 |

Сума `depth × calls` дорівнює **1027080**, точно всьому `os.lstat` count.
На цьому archive без symlinks CPython realpath обходить компоненти абсолютного
шляху на кожному fresh resolve. Це локальний механізм amplification, не доказ
такого самого count на іншій глибині, symlink layout, Python чи Windows.

## Навіщо повторно читаються файли

Розподіл усіх **87215 Dataset.read** за неперекривними causal phases:

| Phase | Reads |
|---|---:|
| Outer archive bindings/controls, поза вкладеними phases | 12008 |
| Collection catalog/control reads | 841 |
| Run admission і збереження перевірених bundle bytes у collection | 10284 |
| Bundle bindings/metadata/outputs, поза вкладеними phases | 10356 |
| Catalog structural validation | 840 |
| Bundle structural validation | 10080 |
| Validation усередині незалежного rescore | 10080 |
| Інші scorer reads | 96 |
| Final bundle `verify_frozen` | 10260 |
| Final collection `verify_frozen` | 11101 |
| Final outer archive `verify_frozen` | 11137 |
| Trusted tool `binding` | 108 |
| Configuration readers | 24 |

25 `validate_datasets` calls = один catalog + 12 bundle validations + 12
scorer validations; це 300 dataset instances, 21000 reads та 19200 PGM parses.
14 `verify_frozen` calls = 12 bundle + collection + outer archive; **32498 reads**
перевіряють hashes після наступних кроків. Однаковий path у різні моменти не
робить такі читання зайвими. Не кешуємо checks/hashes, не замінюємо digest
size/mtime й не прибираємо окремий regular-file check перед open.

Validator дозволяє contained resolved regular-file symlinks, closed inventory
їх відхиляє. Ці різні контракти не дають підстав замінювати validator resolve
результатом попереднього inventory або змішувати guards різних шарів.

## Timing та валідація

| Вимір | Seconds |
|---|---:|
| Excluded warmup | 10.320 |
| Unprofiled timed-1 / timed-2 / timed-3 | 9.832 / 10.335 / 9.942 |
| Median трьох unprofiled samples | **9.942** |
| Attribution observer audit | 166.410 |
| Окремий cProfile audit | 22.641 |
| `Dataset.path` cumulative у cProfile | 13.577 |
| `Path.resolve` cumulative у cProfile | 8.540 |
| Inventory cumulative у cProfile | 0.427 |

Observer wall time / unprofiled median = **16.74×**, cProfile = **2.28×**.
Це observed diagnostic overhead ratios різних runs, не точне віднімання cost.
0.427 s inventory — лише 1.89% profiled API; його не переносимо як точний
uninstrumented share. Немає candidate comparison або cross-date speedup claim.
Три samples не оцінюють p90/p99; VmHWM збережений без memory-improvement claim.

**Probe і незалежний finalizer — exit 0.** Усі **6/6** full normalized reports
і exact prediction/metric/coverage/peer oracles збігаються з bound tar. Archive
та input bytes/mtime незмінні; 243 source hashes і executable identity звірені.
Finalizer перечитав persisted reports/jobs/oracles, tar/input inventories,
raw pstats, перерахував 19 totals/owner/phase aggregates, depth sum і median.

Production, tests, runner, core/config не змінено; нового full runner не було.
Повторно звірено hash та success/counts **попереднього** full run від 3 листопада
на тих самих 243 source files і Python: **357 Python tests, Debug/Release CTest
64/64** кожна. Це reuse ідентичного validation evidence, не новий suite run.
Сім camera/output flags у кожному cache перевірено OFF. Нових native sanitizer,
NoTests, MSVC чи hardware runs немає. Synthetic warm WSL evidence не доводить
cold-cache/Windows/Pi/live latency, фізичну accuracy чи flight acceptance.

## Evidence та продовження

Локальна ignored directory: `artifacts/filesystem-attribution-20261103/`.
Містить probe/calibration/finalizer, шість jobs/reports/oracles/results,
attribution rows, raw pstats/JSON profile, snapshots, summary/validation,
source manifest, logs та `evidence-sha256.json` (**49 bound files**).
Hash-bound helper попереднього warm study імпортується як бібліотека; старі
studies/finalizers не перезапускаються і не переписуються. Fresh clone отримує
docs/source, не ignored evidence. Для повтору потрібна нова artifact directory.

| Binding | SHA-256 |
|---|---|
| Input tar | `bedb5f838258e006b378a7cc30b42b279524c58a81b5f0fb5361fd0bc279453b` |
| Normalized report | `aee128b3105f6825340ce96cb93ef4cf590b1e98506a7f338eb020b569183da1` |
| Summary | `f047e266feeef8a9475c426fa8a25018379727ebe39805f694470356b27373bf` |
| 243-source manifest | `a565452a0cfa785d58d2bc8f2b0856032575180b4e090106fc4732c30e958cdc` |

Наступний bounded slice: **read-only path-depth sensitivity**. Перемістити
ідентичний bound archive між двома контрольованими глибинами на тому самому
native filesystem; явно записати absolute roots/component counts, runtime і
source identities. Alternating paired fresh-worker timings, excluded warmups,
окремий diagnostic profile, exact reports/oracles/peers і bytes/mtime для обох
relocations. Не змінювати production, source trust, guards, workload або storage
між умовами; snapshots однаково прогрівають cache. Не використовувати global
chdir чи кеш resolve як прихований candidate. Це перевірка впливу deployment
layout, не дозвіл послаблювати перевірки. Решта черги збережена в handoff.
