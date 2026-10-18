# Baseline масштабування offline campaign та archive audit

Дата: `2026-10-18`. Початковий checkpoint: `d7fdfd1`.

Виміряно незмінені production tools на 12, 24 і 48 явно заданих runs.
Кампанія росте швидше за кількість runs; головний виміряний hotspot —
повторні перевірки вже зафіксованих файлів у `guard()`, особливо обробка
шляхів. Цей checkpoint додає вимірювач і baseline, без оптимізації runtime,
зміни thresholds, перевірок цілісності або output gates.

## Метод та межі

Новий ручний Linux probe: `scripts/tests/benchmark_second_pass_campaign.py`.
Він повторює [синтетичну матрицю](SECOND_PASS_CAMPAIGN_2026-10-13_UA.md)
3 datasets/splits × 2 configurations × 2 policies із унікальними run IDs.
Кожен dataset залишається 8 кадрів, 4 reference entries, 2×2 Gray8; збільшується
лише кількість runs. Отже 12/24/48 runs мають 96/192/384 frame-run pairs.
Це не масштабування resolution, довжини маршруту або кількості datasets і
не оцінка фізичної accuracy чи незалежності captures.

- WSL2 x86_64, kernel `6.6.87.2-microsoft-standard-WSL2`, Python `3.12.3`;
  Intel Core i5-10300H, 8 доступних logical CPUs. CPU affinity не звужувалася.
- Використано наявний локальний Release `second_pass_replay` з
  `artifacts/telemetry-source-20261002/build/Release/`. SHA-256:
  `a6f8c9f3ead1c5b1384bfae9f60ace667cc14195d2cab06f7633fccbf8ac2bd6`.
- Сім trusted tools, чотири fixture modules, probe і binary скопійовано та
  звірено за SHA-256 у native WSL `/tmp`. Inputs, frozen tools/binary й усі
  archives під час вимірювання там само; `stat -f` повернув `ext2/ext3`.
  Копіювання й збереження evidence на Windows-диск не входять до часу.
- Один виключений 12-run warmup, потім три цикли `12 → 24 → 48`.
  Для кожної фази новий Python process; calls послідовні. Warm cache, без
  cache eviction; перед audit snapshot читає весь archive. OS page cache
  спільний між процесами. Host load, частота CPU й дискові caches не фіксувалися.
- Wall time — `perf_counter()` навколо `r.run()` або `a.check()`. Runner
  включає свої snapshots, replay children, перевірки, fsync і final report.
  Audit включає читання/перевірки та побудову поверненого report. Imports,
  fixture creation, зовнішні oracles, серіалізація audit report і tar — поза
  інтервалом. Це API time, не час CLI startup + stdout serialization.
- Peak RSS — Linux `RUSAGE_SELF.ru_maxrss` у KiB, знятий одразу після API.
  Це high-water mark самого Python worker від старту процесу, включно з
  imports; не приріст пам'яті API. Дочірній replay не входить, сумарний
  одночасний peak дерева процесів не виміряно. MiB нижче = KiB / 1024.
- Окремий 48-run cProfile pass не входить до медіан. Його wall time має
  profiler overhead, тому відсотки нижче характеризують саме цей profile.

Відтворення з новим output directory:

```sh
python3 scripts/tests/benchmark_second_pass_campaign.py \
  --binary artifacts/telemetry-source-20261002/build/Release/second_pass_replay \
  --output artifacts/second-pass-scaling-repeat --runs 12 24 48 --repeats 3 \
  --work-dir /tmp
```

## Виміряні результати

Медіана трьох unprofiled samples; у дужках мінімум–максимум часу.

| Runs | Campaign, с | Audit, с | Campaign peak RSS, MiB | Audit peak RSS, MiB |
| ---: | ---: | ---: | ---: | ---: |
| 12 | 4.476 (4.384–4.948) | 1.156 (1.118–1.303) | 31.36 | 32.25 |
| 24 | 11.881 (11.760–13.561) | 2.623 (2.198–4.192) | 32.11 | 35.82 |
| 48 | 37.617 (37.185–38.865) | 4.383 (4.336–4.519) | 34.59 | 43.25 |

При 4× runs медіана campaign time стала приблизно 8.40×, audit — 3.79×.
Це локальний baseline із трьома повторами, не статистична гарантія швидкодії.
Повільний 24-run audit sample 4.192 с залишено в наборі, його не відкинуто.
RSS у цих межах помірний; це не перевірка memory leak або верхньої межі
4096 runs, дозволеної request schema. Performance acceptance thresholds
і автоматичний CI benchmark не додаються.

## Виміряні hotspots та наступний крок

48-run profile: runner 81.312 с, audit 11.387 с з profiler overhead.

- Runner `guard()` — 147 calls, 60.143 с cumulative, близько 74% profiled
  runner time. Кожен guard повторно читає й хешує весь поточний immutable
  inventory; після admission додаються файли чергового bundle. При сталому
  розмірі bundle це дає квадратичний компонент обсягу повторних перевірок.
  Такий висновок спирається і на пряме читання runner, не лише на три timings.
- `Dataset.path()` — 269582 calls, 54.944 с cumulative у runner; вкладені
  `resolve/realpath` і containment через `path.parents` займають значну частку.
  Зафіксовано 2628372 `lstat` calls. Самі hash `update` — 3.443 с self time.
  Cumulative значення вкладених функцій перекриваються, їх не можна додавати.
- Audit `inventory()` — 100 calls, 4.098 с cumulative; `Dataset.path()` —
  22583 calls, 4.469 с cumulative. Читання/containment/inventory є також
  важливими для read-only перевірки; replay audit не запускає.

Наступний bounded software slice: дослідити й скоротити Python overhead
перевірки portable paths/containment у `Dataset.path()`. Зберегти свіжі
resolve/stat/read/hash перевірки на кожній чинній межі, усі mutation guards,
відхилення escape/symlink/nonregular/path aliases і повторне читання вже
прийнятих bundles. Не кешувати content hashes між guards і не прибирати
guards заради красивого benchmark. До прийняття зміни потрібні path/mutation
regressions, повна software suite та before/after на цій самій матриці.
Якщо еквівалентність не підтвердиться, залишити production behavior незмінним.

## Валідація й збережені докази

Кожен sample має verified state, точну повну coverage, request/run/source
identity, очікувані predictions та TP/FP/FN; policy-dependent denominators
збережені. Повний archive audit перевіряє frozen inputs, tool bytes, peers і
saved report. Inputs та archive bytes/mtime до/після audit однакові.
Невдалий sample перериває probe; жодних прихованих retries чи вилучень.

Evidence: `artifacts/second-pass-scaling-20261018/`:

- `baseline/summary.json`: усі 9 unprofiled samples, медіани/min/max,
  environment, 12 source hashes, binary hash, per-run metrics/prediction hashes;
- `runner.profile.*`, `audit.profile.*` у `baseline/`: raw pstats,
  JSON function counts/times і текстові top-30 cumulative/self summaries;
- `baseline/native-evidence.tar.gz`: усі 11 archives (warmup, 9 samples,
  profile), inputs, copied sources/binary, reports, measurement jobs та
  SHA-256/mtime inventory кожного archive. Разом 312 replay runs / 2496 pairs;
- `verify.py`, `verification.log/json`: незалежний перерахунок агрегатів,
  hash-bound readback tar, повторний audit усіх 11 relocated archives з
  plan/state anchors, порівняння predictions/metrics і read-only snapshots.
  Контрольне видалення run або зміна TP відхиляються вимірювальним oracle.

Tar SHA-256:
`ac12ae1909622685b978de65f0463a3ea286c4ce002ee4ecd2173c3e8d821852`.
Historical paths у job/state/report посилаються на прибраний temporary root;
audit використовує переданий archive root. Plan/state hashes зафіксовано
перед audit, це не незалежні historical receipts чи binary-source attestation.

Повний WSL software runner: Debug/Release CTest **64/64**, telemetry CLI
**7/7**, exporter **21/21**, collection runner **23/23**, campaign **4/4**
кожна; readiness **15/15**, recorded-route benchmark **5/5**, validator
**31/31**, scorer **27/27**, bundle checker **24/24**, collection checker
**27/27**, outer archive audit **30/30**. Log: `software.log`.

Збережено **234** hashes core/scripts/config. З попередніх 233 змінено лише
tests README, додано manual probe. Production C++/Python/config/build і
software runner незмінні; сім camera/output flags OFF. Нові native sanitizer,
NoTests, MSVC та hardware runs не виконувалися. Graph metadata лишається
`6d8f547`; висновки спираються на direct source/diff/profile/tests.
