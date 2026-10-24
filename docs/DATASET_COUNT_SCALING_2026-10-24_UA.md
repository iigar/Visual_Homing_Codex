# Масштабування catalog при сталих replay workloads

Дата: `2026-10-24`. Початковий checkpoint
`87ee902e0058842722a2634f560911c756f5681b` звірено з remote; tracked tree
початково чистий. Наступний крок після
[resolution baseline](RESOLUTION_SCALING_2026-10-24_UA.md).

## Контракт і матриця

Production runner/checker вимагають expected run для кожного catalog dataset.
Порожні peer-only entries не допускаються. Водночас не вимагається повна
config/policy matrix для кожного entry. Validator допускає однакові captures
та content усередині split; leakage checks порівнюють різні splits.
Усі ці production правила залишаються незмінними.

Новий `--datasets 3 6 12` manual probe фіксує **12 runs, 8 query frames,
32×32 geometry, 4 reference frames, 3 splits, 2 configs × 2 policies**.
На кожний split створюються 1/2/4 catalog entries. Копії мають ті самі
query/reference/labels/config/policy bytes, source IDs/timestamps і capture/
session identities; змінюються лише dataset ID у metadata та його binding.
Це явно пов'язані synthetic copies, без твердження про незалежні captures.
Початкові три entries не змінюються.

Чотири початкові runs кожного split призначаються entries round-robin.
Run order/ID, конфігурація, policy, timeout, replay pixels/references і
очікувані результати кожного run сталі. Змінюється dataset ownership,
catalog storage та кількість peer inputs, які проходять fresh validation.

| Catalog datasets | Entries / split | Runs / entry | Pairs / entry | Peers / run | Peer inclusions / archive |
|---|---:|---:|---:|---:|---:|
| 3 | 1 | 4 | 32 | 2 | 24 |
| 6 | 2 | 2 | 16 | 5 | 60 |
| 12 | 4 | 1 | 8 | 11 | 132 |

Завжди 4 runs / 32 pairs на split і **12 runs / 96 pairs** загалом.
Stored catalog frames — 24/48/96, але це копії тих самих 24 початкових
frames. Кожний run включає **весь** інший catalog як peers. Same-split
references можуть збігатися; `disjoint_references=true` зберігає чинний
cross-split contract, а не стверджує унікальність references кожної копії.
Per-entry config/policy coverage змінюється; повна matrix зберігається
на рівні split, тому pooled accuracy або per-entry full-policy comparison
із цих вимірів не робиться.

Manual probe тепер перевіряє coverage keys і counts за фактичним catalog,
окремо by_dataset/by_split, та повний ordered peer list. Старі три-entry
режими збережені. `--datasets`, `--runs`, `--frames`, `--resolution`
взаємовиключні; dataset mode допускає лише унікальні 3/6/12. Production
C++/Python/config, thresholds і integrity guards не змінені. Graph metadata
досі `6d8f547`; використано пряме читання source/callers/diff/tests.

## Метод

Python 3.12.3, native WSL `/tmp`, той самий trusted Release binary. Окремий
3-entry warmup, три round-robin rounds і окремий 12-entry cProfile pass.
API time вимірюється у fresh Python workers без imports, fixture creation,
oracle checks, serialization evidence або tar persistence. Під час main
matrix full suite і readback не виконуються. Cache warm, без eviction;
host load/CPU clocks не зафіксовані. Три samples не дають latency guarantee.

Worker VmHWM — current-image peak із imports, без C++ children; legacy
ru_maxrss і initial values збережено з відомою pre-exec parent caveat.
Сумарний simultaneous process-tree peak не вимірюється. Окремих child RSS
diagnostics у цій матриці немає: C++ workloads сталі, а попередній resolution
slice вже визначив цей окремий memory scope. Synthetic repeated quadrants
не характеризують real-camera accuracy, довгі reference routes або Pi RAM.

## Результати

Медіани трьох unprofiled samples; у дужках повний min–max, секунди:

| Datasets | Campaign API | Read-only audit | Worker VmHWM campaign / audit, MiB |
|---|---:|---:|---:|
| 3 | 5.362 (5.250–5.470) | 1.146 (1.075–1.331) | 31.03 / 32.66 |
| 6 | 8.885 (8.852–9.485) | 1.804 (1.669–2.181) | 31.78 / 34.03 |
| 12 | 15.185 (15.047–15.808) | 3.323 (3.262–3.627) | 33.53 / 36.78 |

Зростання catalog 3 → 12 при сталих replay inputs дає приблизно 2.83×
campaign і 2.90× audit API time у цій матриці. VmHWM зріс приблизно на
2.50 MiB та 4.12 MiB відповідно. Це витрати catalog/peer processing і
більших reports/inventories, а не збільшення роботи C++ matcher. Усі
samples збережені, включно з 6-entry audit 2.181 с. Це synthetic warm WSL
baseline, не production speedup, latency/RAM guarantee або real-data accuracy.

Окремий 12-entry profile: campaign 31.062 с, audit 7.337 с. Campaign
`Dataset.read` — 89291 calls / 25.522 с cumulative, `Dataset.path` —
89627 / 18.924 с, `guard` — 39 / 14.411 с, `verify_frozen` — 138 / 18.156 с.
Audit `Dataset.read` — 18671 / 5.760 с, `Dataset.path` — 18839 / 4.247 с.
Runner має 1044 `Dataset.validate` calls, audit — 300: кожна перевірка
catalog/bundle охоплює його peers. Cumulative times вкладених функцій
перекриваються; їх не додавати. Profile не дозволяє кешувати hashes між
guards, прибирати fresh resolve/stat/read або замінювати content SHA на mtime/size.

## Перевірка архівів

Hash-bound tar readback перевірив **11 relocated archives / 132 runs /
1056 frame-run pairs**. Повні per-run metrics, predictions і raw CSV hashes
збігаються між усіма counts/repeats, warmup та profile. Незалежно звірено
незмінні config/policy bindings і всі replay input bytes, labels/IDs/timestamps
та reference data; dataset metadata відрізняється лише власним ID.
Coverage кожного dataset/split та всі peers перевірено за повним request.

Representative measured archives для 3/6/12 datasets мають **763/1309/2401
files**, **5848061/6853211/8863067 bytes**. Отже, зростає закрита file inventory
і обсяг reports/peer snapshots при сталих дванадцяти C++ workloads.
Warmup/profile metadata трохи відрізняється за розміром від measured samples;
усі фактичні sizes залишені у `verification.json`.

Readback перевіряє source/binary/tar hashes, external plan/state anchors і
відтворення saved reports. Імпортує лише trusted checkout, copied/archived code
не виконується; runner/subprocess execution заблоковано на час audit.
Bytes/mtime архівів до/після незмінні. Median/min/max перераховано незалежно.

Compatibility smokes `--runs 12`, `--frames 8` та `--resolution 2 --replay-rss`
пройшли з окремими warmup/sample/profile. В останньому також перевірено
12 trusted diagnostic replays у кожному archive. Ці smoke timings не входять
до main matrix й не використовуються для performance claims: smokes
виконувалися паралельно з full suite після завершення основних вимірювань.

## Software validation

Повний `scripts/test-software.sh` завершився exit 0, без skips/failures:

| Набір | Результат |
|---|---:|
| CTest Debug / Release | 64/64 кожна |
| CLI / exporter / runner | 7/7, 21/21, 23/23 кожна збірка |
| Campaign / frame scaling / resolution / catalog | 4/4, 6/6, 7/7, 7/7 кожна збірка |
| Readiness / benchmark / validator | 15/15, 5/5, 31/31 |
| Paths / inventory | 12/12 кожна |
| Scorer / bundle / collection / audit | 27/27, 24/24, 27/27, 30/30 |

Усього **333 Python tests**. Нові сім catalog tests охоплюють незмінний
3-entry fixture, content/bindings/ownership/peers, actual replay із незалежними
TP/FP/FN oracles, unused entries/leakage rejection до launch, corrupt coverage/
peer rejection і read-only relocation. Для восьми кадрів per-split очікування
near-inclusive 3/2/3, near-clear 2/2/3, exact-inclusive 0/0/6, exact-clear 0/0/5.

Перед suite зафіксовано, після нього звірено **241 source hash**. Із попередніх
239 змінилися лише software runner, tests README та manual probe; додано catalog
helper і його tests. Production C++/Python/config bytes незмінні, сім camera/
output flags OFF у Debug/Release caches. Native sanitizer/NoTests/MSVC/hardware
acceptance у цьому slice не запускали.

SHA-256:

- Main tar: `1eec88b767c8fc8e08ff03900bf7aae8c71e14300db198655e91e67fea11977f`.
- Release binary: `a6f8c9f3ead1c5b1384bfae9f60ace667cc14195d2cab06f7633fccbf8ac2bd6`.
- Software log: `3ac196f9ccbe83450c042aee1d0b29053fb383ab7a821ab64b86f5aa9a99192d`.

## Відтворення й evidence

```sh
python3 scripts/tests/benchmark_second_pass_campaign.py \
  --binary artifacts/telemetry-source-20261002/build/Release/second_pass_replay \
  --output artifacts/dataset-scaling-new \
  --datasets 3 6 12 --repeats 3 --work-dir /tmp
bash scripts/test-software.sh artifacts/telemetry-source-20261002/build
```

Output directory має бути новим. Локальні ignored artifacts:
`artifacts/dataset-scaling-20261024/`, зокрема `measurement/summary.json`,
profiles і hash-bound `native-evidence.tar.gz` із copied trusted sources/
binary, input catalog, archives, jobs/reports та bytes/mtime inventories.
`verify.py` робить read-only relocation лише trusted checkout tools,
`finalize.py` звіряє source hashes/test logs/flags. Новий clone матиме
source/tests/docs, але не локальні tar/log/build files.

## Наступний bounded крок

Новий 12-entry profile дає підставу окремо перевірити overhead створення
path objects перед fresh resolve у `Dataset.path()`: спочатку ізолювати
construction/normalization cost від syscall cost, потім спробувати локальну
заміну лише за наявності виміряного виграшу. Попередній path slice вже
оптимізував character scan/containment; їхню семантику не переглядати.
Fresh strict resolve/stat, native Windows/POSIX equality, internal contained
symlinks, nonregular rejection, consumed-byte hashes і mutation guards
мають лишитися. Existing path/mutation tests плюс focused characterization,
same-matrix before/after equivalence і full runner — обов'язкова перевірка.
Якщо isolated cost або gain замалі, зафіксувати no-change result.

Більші archives, cold/warm cost, latency distributions і незалежні annotated
data залишаються у [збереженій черзі](SESSION_HANDOFF_2026-10-20_UA.md).
Hardware/output work та native sanitizer/NoTests/MSVC acceptance мають окремі gates.
