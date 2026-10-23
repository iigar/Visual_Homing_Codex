# Масштабування resolution та окрема пам'ять replay

Дата: `2026-10-24`. Початковий checkpoint:
`9ac0172c0d91f87f08296047db1cec182b8b95e8`, remote SHA перевірено,
початковий tracked tree чистий. Продовження
[frame-count baseline](FRAME_COUNT_SCALING_2026-10-23_UA.md).

## Зміни та контроль geometry

Manual probe має третю взаємовиключну вісь `--resolution`; `--runs` і
`--frames` збережені. Новий helper `scripts/tests/second_pass_resolution.py`
будує квадратні PGM/VHRS fixtures і виконує окремі RSS diagnostics.
Production C++/Python/config та integrity guards не змінені. Graph metadata
досі `6d8f547`; використано пряме читання source/callers/diff/tests.

Матриця **2×2 / 32×32 / 128×128**, незмінні **64 query frames / dataset,
12 runs, 3 datasets/splits, 4 reference frames, 2 configs × 2 policies**.
Кожний із чотирьох початкових pixels розгортається у сталий quadrant.
Query/native, reference і target dimensions зростають разом; metadata/hashes
переприв'язані. Labels, source IDs/timestamps, matcher thresholds і policy
сталі. Варіант 2×2 не змінює початкові inputs. Допустимі sides — унікальні
парні integers [2, 256]; цей CLI limit не означає acceptance всіх розмірів.

Основна матриця використовує identity preprocessing. Окремий actual replay
тест порівнює 32×32 identity з 64×64 → 32×32 area downsampling: quadrant
alignment зберігає pixels, predictions та metrics. Для 16 кадрів незалежні
TP/FP/FN очікування на dataset: near-inclusive 6/4/6, near-clear 4/4/6,
exact-inclusive 0/0/12, exact-clear 0/0/10. Не змінювали thresholds заради
fixtures. Довільні текстури, noise, noninteger scaling, camera geometry і
реальна visual accuracy цими repeated quadrants не характеризуються.

## Метод і пам'ять

Python 3.12.3, WSL2 native `/tmp`, той самий Release binary. Окремий warmup
на 2×2/64 frames, три round-robin rounds і окремий profile на 128×128.
Кожна runner/audit фаза має fresh Python worker. API wall time виключає
imports, fixture creation, oracle checks, diagnostics та persistence.
Full suite і readback не запускаються одночасно з вимірюваною матрицею.
Cache warm, eviction немає, host load/CPU clocks не фіксовані.

Worker VmHWM — peak поточного executable image, включно з imports, без
C++ children. Legacy ru_maxrss та початкові значення збережено; успадкований
pre-exec parent floor обмежує їх інтерпретацію, як описано у frame report.

`--replay-rss` копіює/hash-binds локальний GNU Time (`time` package
`1.9-0.2build1`). Після кожної завершеної runner/audit пари виконує 12
**додаткових** replay diagnostics через `%M`. Binary береться з trusted
зовнішньої копії; archived executable не запускається. Перед запуском
перевіряються plan digest, frozen file map і binary hash. CSV stdout має
збігтися з bound `raw-predictions.csv`; bytes/mtime архіву після diagnostics
мають залишитися незмінними. Timeout/interruption завершує всю process group
і reap-ить wrapper. Diagnostics не входять до API-часу, coverage чи accuracy.

Локальний `time-probe.py/json` перевірив RSS accounting саме цього workflow:
при parent allocation 128 MiB прямий child успадкував ru_maxrss 144256 KiB,
хоча його VmHWM був 11392 KiB; через GNU Time `%M` став 11520 KiB.
Без parent allocation `%M` був 11648 KiB. Таким чином, у цій конфігурації
великий Python parent не задає штучний floor додатковому C++ measurement.
Це приблизний kernel peak окремого child, а не simultaneous process-tree
peak чи isolated matcher allocation; не можна додавати незалежні maxima
і видавати суму за виміряну пікову пам'ять системи.

## Результати

Медіани трьох unprofiled samples; у дужках повний min–max діапазон, секунди:

| Pixels | Campaign API | Read-only audit | Worker VmHWM campaign / audit, MiB | Окремий C++ peak RSS, MiB |
|---|---:|---:|---:|---:|
| 2×2 | 12.530 (12.516–13.017) | 2.696 (2.689–2.731) | 34.95 / 41.55 | 4.000 |
| 32×32 | 13.064 (13.031–14.059) | 2.864 (2.758–2.969) | 34.82 / 41.67 | 4.000 |
| 128×128 | 17.272 (17.032–17.959) | 3.732 (3.620–3.899) | 35.07 / 41.68 | 4.125 |

C++ колонка — медіана **36 individual process peaks на розмір** із трьох
samples × 12 runs; це descriptive aggregation різних configs/policies/splits,
а не 36 незалежних datasets. Range для 2×2 і 32×32 — рівно 4.000 MiB,
для 128×128 — 4.000–4.125 MiB. Raw per-run records збережені. Різниця
між розмірами мала й близька до гранулярності kernel accounting; висновку
про memory improvement або upper bound для реального route немає.

128×128 містить у 4096 разів більше pixels, ніж 2×2, але API medians
зросли приблизно на 38% у цій fixed-count матриці. Це scaling baseline,
не оптимізація production або гарантія latency. Worker memory приблизно
стала: збільшується розмір payload, а кількість files/report rows незмінна.
На реальних текстурах, великій кількості reference entries, Pi або Windows
filesystem інші витрати та memory envelopes потребують власних вимірювань.

Окремий 128×128 profile: campaign 33.668 с, audit 7.081 с. Campaign
`Dataset.read` — 109661 calls / 27.562 с cumulative, `Dataset.path` —
109772 / 17.297 с, `guard` — 39 / 14.957 с; hash update self time 4.468 с.
Audit `Dataset.read` — 22955 / 5.696 с, `Dataset.path` — 23015 / 3.478 с,
hash update self time 0.971 с. Nested cumulative times перекриваються;
їх не додавати. Profile вказує на значну вартість повторних filesystem/
content перевірок; він не дозволяє прибирати fresh resolve/stat/hash/guards
або замінювати content hashes на mtime/size.

## Перевірка evidence

Hash-bound tar readback/relocation перевірив **11 archives / 132 runs /
8448 frame-run pairs**. Predictions, повні metrics/coverage і raw prediction
CSV hashes збігаються між усіма resolutions, repeats, warmup та profile.
Сталі labels/IDs/timestamps/config fields/policies перевірено окремо.
Усі median/min/max незалежно перераховано; **132 diagnostic records**
звірено з bound CSV/plan hashes і raw `%M`. Додаткові diagnostics не
збільшують campaign coverage 8448 pairs.

Readback імпортує лише trusted checkout, а copied/archived code — inert
bytes для hash checks. Runner/subprocess execution заблоковано під час audit;
anchors і saved reports перевірені, bytes/mtime після audit незмінні.
Кожний archive містить 2947 файлів; representative measured archive size
зріс із 9084037 до 11799586 і 52544579 bytes. Файловий count сталий,
content hashing обробляє більші payloads. Окремі `--runs 12` та `--frames 8`
compatibility smokes пройшли з warmup/sample/profile; їхні timings не
включені до основної матриці (smokes могли збігатися з full suite).

## Software validation

Повний `scripts/test-software.sh` завершився exit 0:

| Набір | Результат |
|---|---:|
| CTest Debug / Release | 64/64 кожна |
| CLI / exporter / runner | 7/7, 21/21, 23/23 кожна збірка |
| Campaign / frame scaling / resolution | 4/4, 6/6, 7/7 кожна збірка |
| Readiness / benchmark / validator | 15/15, 5/5, 31/31 |
| Paths / inventory | 12/12 кожна |
| Scorer / bundle / collection / audit | 27/27, 24/24, 27/27, 30/30 |

Нові сім tests покривають geometry/VHRS/config bindings, unchanged 2×2,
identity/downsampling actual replay, hand-calculated metrics, trusted RSS
diagnostics із незмінним archive, plan/binary mismatch rejection до launch,
cleanup на timeout/interruption. Усього 319 Python tests, без skips/failures.
Software runner перевіряє наявність `/usr/bin/time` до build.

До suite зафіксовано й після нього звірено **239 source hashes**. Із
попередніх 237 змінилися лише software runner, tests README і manual probe;
додано helper та resolution tests. Production C++/Python/config bytes
незмінні, сім camera/output flags OFF у Debug/Release caches. Native
sanitizer/NoTests/MSVC/hardware acceptance у цьому slice не запускали.

SHA-256:

- Main tar: `09a8f6c9c680e266c2943530b0b9f973b18f3cd6257a2c0b300249c2c734324e`.
- Release binary: `a6f8c9f3ead1c5b1384bfae9f60ace667cc14195d2cab06f7633fccbf8ac2bd6`.
- GNU Time: `3b11dec50514a8473e9f6efa7a34d584d0657538c09988f61b72d38ad4991a10`.
- Software log: `dbae0c1547b7f284930c02e5d23f1cf2a5f60433ea3ade55d6c12a4f423f4e2c`.

## Відтворення

```sh
python3 scripts/tests/benchmark_second_pass_campaign.py \
  --binary artifacts/telemetry-source-20261002/build/Release/second_pass_replay \
  --output artifacts/resolution-scaling-new \
  --resolution 2 32 128 --replay-rss --repeats 3 --work-dir /tmp
bash scripts/test-software.sh artifacts/telemetry-source-20261002/build
```

Output directory має бути новим; Linux Python 3.11+ та `/usr/bin/time`
обов'язкові для нових тестів. Evidence зберігається локально у
`artifacts/resolution-scaling-20261024/`: main `measurement/` містить summary,
raw profile і hash-bound tar з copied sources/binary/GNU Time, inputs,
archives, per-phase reports, raw diagnostic CSV/RSS/stderr та inventories.
Великі artifacts ignored; новий clone матиме source/tests/docs, але не ці
tar/log/build files. Scope лишається software-only, без hardware/output work.

## Наступний bounded крок

Dataset-count scaling із [збереженої черги](SESSION_HANDOFF_2026-10-20_UA.md):
спочатку characterize catalog/peer/coverage contracts і визначити матрицю,
яка відділяє catalog size від requested run count. Кожний catalog dataset
повинен мати expected run; порожній peer-only catalog entry не допускається
чинним collection contract. Frames/resolution/reference
count/config/policy залишити сталими; exact per-run predictions/metrics,
cross-split peer disjointness і denominator accounting — обов'язкові oracles.
Після цього measured API time/VmHWM і profile, без threshold tuning,
hash caching чи ослаблення guards. Completed path/inventory оптимізації
не відкривати знову без нового evidence. Cold/warm cost, великі archives,
latency distributions та independent annotated data залишаються в черзі.
Hardware/MSVC/native sanitizer/NoTests acceptance цим slice не замінюється.
