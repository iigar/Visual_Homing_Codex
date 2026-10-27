# Побудова path objects перед fresh resolve

Початок: `2026-10-25`; завершення перевірки й документації: `2026-10-27`. Початковий checkpoint
`5acd2886e6fb5dec5734ffc90b6cbdf87e056307` звірено з remote; tracked tree
початково чистий. Продовження [catalog baseline](DATASET_COUNT_SCALING_2026-10-24_UA.md).

## Межі експерименту

Перевірено лише вираз побудови шляху в `Dataset.path()`:

```python
path = resolved(self.root / value)                  # operator: чинний код
path = resolved(Path(self.root, value))              # constructor
path = resolved(Path(os.path.join(self.root, value))) # native_join
```

Локальні кандидати отримано однією перевіреною текстовою заміною trusted
validator source. Production-файл не редагувався. Portable character/component
checks, strict resolve, native containment, regular-file stat, consumed-byte
hashes, повторні/expected hashes та mutation guards збережено. Немає кешування
resolved paths або content і зміни правил symlink admission.

## Ізольоване вимірювання

Python 3.12.3, native WSL temporary directory, прогріті файли. Три глибини root,
по три relative paths — дев'ять існуючих regular files; включено Unicode,
пробіли й глибоку вкладеність. Сім раундів із циклічним порядком трьох варіантів.
Construction містить `str(path)`, щоб матеріалізувати lazy parsing; 90 000
викликів на sample. Повний `Dataset.path()` — 13 500 викликів на sample.
`timeit` вимикає GC. Окремий контроль resolve/stat використовує вже створені
Path objects; це описова контрольна величина, не підстава віднімати два
незалежні timings для оцінки точної вартості syscall.

| Варіант | Construction, медіана [min–max], µs | Повний lookup, медіана [min–max], µs |
|---|---:|---:|
| `root / value` | 7.49 [6.58–8.15] | 67.05 [58.72–70.57] |
| `Path(root, value)` | 6.79 [6.13–7.61] | 65.42 [58.61–69.99] |
| `Path(os.path.join(root, value))` | 6.52 [5.77–7.50] | 67.98 [57.74–70.15] |

Resolve/stat control: **43.67 [39.84–47.52] µs**. Constructor скорочує
construction median приблизно на 9%, але повний lookup — лише на 2.4%; native
join має менший construction median і водночас більший lookup median.
Розкид суттєво перевищує різницю повного lookup. Цього недостатньо для
твердження про покращення campaign/audit API.

Для кожного варіанта пройшли ті самі **12 path/mutation tests без skips**:
783 character placements, native Unicode/case, POSIX/Windows drive/UNC
containment, internal contained symlinks, escapes/cycles/missing paths,
root/file/link replacement, same-size/mtime content changes, consumed bytes,
in-read mutation та nonregular rejection. Додаткові **40 lexical cases**
порівнюють constructors для PurePosixPath/PureWindowsPath, зокрема root `/`,
drive root і UNC. Це не native Windows filesystem acceptance.
Profile дев'яти lookup дає однакові **9 resolve / 96 lstat / 9 stat**.

## Повний read-only audit

Використано `dataset_count-12-repeat-00` із hash-bound tar попереднього
baseline: **12 datasets / 12 runs / 8 frames / 32×32 / 4 references**,
96 frame-run pairs, 2401 files. Tar перевірено й розпаковано в native WSL
temporary directory. Виконано лише trusted checkout checker, з локальною
підстановкою candidate `Dataset.path`; archived sources/binary не виконувалися.
Runner і subprocess execution блокувалися test mocks.

Кожний audit — у fresh Python worker: один warmup на варіант, сім раундів із
циклічним порядком, потім окремий profile. Timer охоплює тільки `check()`;
imports, snapshots, oracle checks і persistence поза timer. Перед кожним
вимірюванням inventory snapshot читає файли, тому це **warm-cache** experiment.
Порівнюється один і той самий relocated archive, а не незалежні captures.

| Варіант | Audit median, s | Min–max, s | Median відносно чинного |
|---|---:|---:|---:|
| `root / value` | 5.489 | 5.316–6.374 | baseline |
| `Path(root, value)` | 5.926 | 5.448–6.905 | +7.9% |
| `Path(os.path.join(root, value))` | 6.046 | 5.768–6.676 | +10.1% |

Constructor повільніший за baseline у всіх семи відповідних раундах
(+0.27…13.56%), native join — у шести із семи (−1.03…22.85%). Це результат
цього експерименту з host variance, без статистичного твердження про загальну
регресію Python constructors. Числа не слід порівнювати напряму з попереднім
catalog baseline: інший запуск, relocation path і стан host.

Усі **27 audits одного архіву** (3 warmup + 21 timed + 3 profiled) мають
однаковий повний report і exact prediction/metric/coverage oracles, усі peers,
незмінні bytes/mtime. Profile counts також однакові: 18 839 `path`,
19 335 resolve/stat, 218 856 lstat, 14 788 artifact, 28 inventory,
14 verify_frozen, 22 343 SHA-256 constructor і 73 553 hash update.
Profiles не входять до наведеної статистики.

**Рішення: production replacement не прийнято.** Ізольований construction
виграш не підтвердився в повному audit. `self.root / value` і всі поточні
guards залишаються. Campaign before/after і новий full runner не запускалися:
кандидати відхилено до production edit. Звірено всі **241 source hashes** із
перевіреного `5acd288`, включно з tests/build/config; усі bytes незмінні.
Попередні **333 Python tests і 64/64 CTest на Debug/Release** лишаються
результатом попереднього checkpoint, не новим прогоном. Сім camera/output
flags у кожному cache OFF. Новий tracked diff містить лише документацію.

Це synthetic warm WSL evidence; немає Pi/Windows filesystem, accuracy,
live latency чи memory-improvement acceptance. Hardware/output робота не
виконувалася. Graph metadata досі `6d8f547`; declarations/callers і guards
перевірено прямим source/diff review, без graph impact verdict.

## Наступний bounded крок

Розширити розмір архіву одним контрольованим параметром: **8/64 query frames
при сталих 12 catalog datasets / 12 runs / 32×32 / 4 references**. Це продовжує
чергу large-archive evidence після окремих frame/resolution/catalog baselines.
Same-split copies зберігають capture identities, exact prediction/metric/
coverage oracles та всі peers. Виміряти campaign/audit samples і worker VmHWM,
зберегти read-only relocation й bytes/mtime checks. Спочатку baseline,
production optimization — лише за новим профілем та перевіреним виграшем.

Cold/warm studies і latency distributions лишаються в
[збереженій черзі](SESSION_HANDOFF_2026-10-20_UA.md). Fresh worker або нова
директорія самі по собі не означають cold filesystem cache. Цей slice не
виправдовує cache dropping, hardware/output роботу чи зміни matcher thresholds.

## Evidence і відтворення

Локальні ignored artifacts: `artifacts/path-construction-20261025/`.
`probe.py` зберігає candidate sources, characterization logs, всі raw samples
і `summary.json`; `audit_probe.py` — jobs, повні reports, raw timings та окремі
profiles; `finalize.py` перевіряє source identity й формує SHA-256 manifest.
Для повтору в цьому checkout з наявним локальним evidence:

```sh
python3 artifacts/path-construction-20261025/probe.py
python3 artifacts/path-construction-20261025/audit_probe.py
python3 artifacts/path-construction-20261025/finalize.py
```

Перед повтором зберегти попередній artifact directory: скрипти перезаписують
свої outputs. Новий clone містить цей звіт та незмінні production/tests,
але не локальні experiment scripts, logs або попередній archive tar.

Input tar SHA-256:
`1eec88b767c8fc8e08ff03900bf7aae8c71e14300db198655e91e67fea11977f`.
Production validator SHA-256:
`79e840ef4d683d042c1e67a516aac3be630676c5c4db7f59d7f3433eddbff79d`.
