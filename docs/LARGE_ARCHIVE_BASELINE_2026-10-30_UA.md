# Large-archive baseline: кадри при сталому каталозі

Початок slice: `2026-10-29`; відновлення й завершення: `2026-10-30`.
Продовження [path-construction experiment](PATH_CONSTRUCTION_2026-10-25_UA.md)
на `90b48b293bc12923636f7ab1544a3530476b4c29`, звіреному з remote.
При відновленні вже були незакомічені benchmark/runner/README, п'ять нових
tests і focused log із результатом 5/5. Завершених вимірювань не було;
збережений slice перевірено й продовжено. Активних WSL test/benchmark процесів
при відновленні не виявлено. Production C++/Python/config не змінювалися.

## Контрольована зміна

Manual probe отримав `--frames 8 64 --large-archive`. Змінюється лише кількість
query frames; **12 catalog datasets / 12 replay runs / 32×32 pixels /
4 reference frames** лишаються сталими. Прапорець вимагає explicit `--frames`;
помилковий режим відхиляється до створення output directory. Старий frame mode
зберігає 3 datasets і 2×2 pixels, інші defaults не змінено.

Кожний із трьох synthetic splits має чотири same-split copies. Вони зберігають
capture/session identities та вміст, мають окремі dataset IDs і по одному
config/policy run. Кожний run містить усі **11 peers**. Configurations,
thresholds, policies та reference bytes однакові між розмірами. Повторені
8-frame blocks мають окремі files, послідовні IDs/timestamps і labels;
copies/повтори не додають незалежних фізичних captures.

Coverage зростає з **96 до 768 frame-run pairs**: 8/64 на catalog entry,
32/256 на split. Exact prediction oracle використовує початковий 8-frame
блок; TP/FP/FN для кожного split при 64 frames: near-inclusive **24/16/24**,
near-clear **16/16/24**, exact-inclusive **0/0/48**, exact-clear **0/0/40**.
Unknown/degraded frames і scoring denominators залишаються явними.

## Метод вимірювання

Linux/WSL, native `/tmp`, copied hash-bound trusted sources і Release replay.
Кожна campaign/audit фаза запускається в fresh Python worker. Timer охоплює
лише API call; imports, fixture creation, snapshots, oracle checks, persistence
і readback поза timer. Один warmup має 12 entries / 8 frames / 32×32;
далі три черговані раунди 8/64 і окремий cProfile pair на 64 frames.
Warmup/profile не входять до aggregates. Невдалі samples не відкидаються й
не повторюються автоматично. Повна software suite та compatibility smoke
виконуються окремо від основних timings.

Це **warm-cache** synthetic API baseline. Перед audit snapshot читає архів;
fresh worker або нова директорія не означає cold cache. `VmHWM` — peak
поточного Python image з imports, без replay children. Legacy `ru_maxrss`
також збережено; він може успадкувати pre-exec peak parent. Окремі child RSS
у цьому frame mode не вимірюються. Немає Pi/Windows-filesystem, process-tree
memory, physical accuracy, live latency або production speedup claim.

## Timings і worker memory

Python 3.12.3, WSL2, Intel Core i5-10300H. Медіани трьох timed samples:

| Frames | Campaign, s [min–max] | Audit, s [min–max] | Campaign VmHWM, MiB | Audit VmHWM, MiB |
|---:|---:|---:|---:|---:|
| 8 | 11.603 [10.850–13.132] | 2.748 [2.452–2.755] | 33.55 | 36.98 |
| 64 | 52.264 [50.078–52.844] | 10.981 [10.822–12.258] | 45.39 | 61.80 |

Усі campaign/audit samples за порядком раундів, секунди:

| Frames | Round 1 | Round 2 | Round 3 |
|---:|---:|---:|---:|
| 8 | 10.850 / 2.748 | 13.132 / 2.755 | 11.603 / 2.452 |
| 64 | 52.264 / 12.258 | 52.844 / 10.981 | 50.078 / 10.822 |

У цьому запуску 8× query frames дають приблизно **4.50× campaign** і
**4.00× audit** median; сталі catalog/run/reference/control costs не зростають
у вісім разів. Це дві точки однієї workload shape, не asymptotic complexity
або performance guarantee. Розкид і host variance збережено; попередні
baseline timings з інших запусків не є contemporaneous before/after.

VmHWM ranges: campaign **33.43–33.55 / 45.34–45.42 MiB**, audit
**36.86–37.02 / 61.50–61.85 MiB** при 8/64 frames. На останньому 8-frame
sample legacy `ru_maxrss` = 42 292 KiB для обох workers, тоді як VmHWM =
34 232/37 748 KiB. Це наочний inherited pre-exec floor; legacy peak не
підміняє current-image memory measurement.

## Profile і межа оптимізації

Окремий 64-frame cProfile: campaign **106.834 s**, audit **25.759 s**;
overhead instrumentation не входить до звичайних timings. Campaign:
`Dataset.read` 415 211 calls / 91.479 s cumulative, `Dataset.path`
415 547 / 68.249 s, strict `resolved` 428 828 / 43.203 s, `guard`
39 / 50.466 s, `verify_frozen` 138 / 63.997 s. Audit: `Dataset.read`
87 215 / 20.815 s, `Dataset.path` 87 383 / 15.219 s, `resolved`
87 879 / 9.479 s, `verify_frozen` 14 / 7.209 s.

Ці cumulative times вкладені й **не додаються**. Fresh filesystem/path work
домінує, але це також integrity boundary: campaign робить 4 465 764 lstat,
audit — 939 417. Саме hash update займає лише 2.264/0.555 s self time;
профіль не дає підстав прибирати hashes або guards. Попередню відхилену
constructor replacement не поновлено. Production optimization у цьому
baseline не виконувалася; нового підтвердженого safe speedup candidate немає.

## Persistence і read-only relocation

Основний probe, readback, compatibility smoke і finalizer завершилися
послідовно з **exit 0**. Усі **8 archives / 96 replay runs / 3456 frame-run
pairs** перевірено після hash-bound tar extraction у новий native WSL path:
warmup, шість timed samples і profile. Кожний архів має 12 datasets/12 runs;
8/64-frame archives містять **2401/11137 files**. Timed archives займають
**8 864 572 / 26 044 586 bytes** відповідно; це сума file sizes без directory
metadata. Окремі byte counts warmup/profile збережено у `verification.json`.

Перераховані aggregates збігаються зі summary. Exact predictions/metrics/
coverage збігаються для однакового frame count; перші вісім rows і raw CSV,
і predictions однакові між усіма розмірами/повторами. Перевірено сталі
settings/references/capture identities, per-entry/split coverage й усі peers.
Saved report відтворюється checker-ом; bytes/mtime незмінні до/після relocated
audit, hashes файлів збігаються з inventory перед persistence. Archived
execution/resume заблоковано mocks. Це не chronology/authorship attestation.

Старий `--frames 8 --repeats 1` smoke окремо підтвердив 3 datasets / 2×2,
verified campaign/audit і `large_archive=false`; його timings не включено
до основної матриці.

## Software validation

Повний `scripts/test-software.sh` завершився з **exit 0**, без skips/failures:

| Набір | Результат |
|---|---:|
| CTest Debug / Release | 64/64 кожна |
| CLI / exporter / runner | 7/7, 21/21, 23/23 кожна збірка |
| Campaign / frame scaling / resolution / catalog | 4/4, 6/6, 7/7, 7/7 кожна збірка |
| Large archive | 5/5 кожна збірка |
| Readiness / benchmark / validator | 15/15, 5/5, 31/31 |
| Paths / inventory | 12/12 кожна |
| Scorer / bundle / collection / audit | 27/27, 24/24, 27/27, 30/30 |

Усього **343 Python tests**. Software log SHA-256:
`9b7ea0593ef7055d6a33bfda6098e952e5d226bc8e4873dd4479440f7193d900`.
Native sanitizer/NoTests/MSVC/hardware acceptance у цьому slice не запускали.

До suite зафіксовано й після всіх вимірювань звірено **242 source hashes**.
Із попередніх 241 змінилися лише software runner, tests README та manual
probe; додано `test_second_pass_large_archive.py`. Production/core/config
bytes незмінні; усі сім camera/output flags OFF у Debug/Release caches.

SHA-256 evidence:

- Main tar: `bedb5f838258e006b378a7cc30b42b279524c58a81b5f0fb5361fd0bc279453b`.
- Release binary: `a6f8c9f3ead1c5b1384bfae9f60ace667cc14195d2cab06f7633fccbf8ac2bd6`.
- Source manifest: `2e483375f031b6243d16a42a91c3ed5b17ecf216b1a0851a1cbc3567462fcac0`.

## Перевірки й відтворення

П'ять integration tests зареєстровано в Debug і Release runner. Вони перевіряють
composed inputs/references/settings/capture identities, actual hand-calculated
metrics і повну dataset/split coverage, rejection зіпсованого coverage або
відсутнього peer, read-only relocation з блокованим execution і неправильний
CLI mode до створення output. Timing assertions у suite відсутні.

```sh
python3 scripts/tests/benchmark_second_pass_campaign.py \
  --binary artifacts/telemetry-source-20261002/build/Release/second_pass_replay \
  --output artifacts/large-archive-new \
  --frames 8 64 --large-archive --repeats 3 --work-dir /tmp
bash scripts/test-software.sh artifacts/telemetry-source-20261002/build
```

Output directory має бути новим. Локальні ignored evidence:
`artifacts/large-archive-20261029/`, включно з `measurement/summary.json`,
raw samples/profiles і hash-bound `native-evidence.tar.gz` із trusted source
copies, binary, inputs, archives та job reports. `verify.py` перевіряє tar,
source/binary identities, aggregates і relocated archives через trusted checkout
checker; archived source/binary не виконуються. Bytes/mtime порівнюються
до/після audit, bytes також звіряються з inventory перед persistence.
`finalize.py` звіряє source manifest, suite logs і output flags. Новий clone
отримає source/tests/docs, але не локальні tar/log/build files.

GitNexus metadata досі вказує `6d8f547`; використано пряме читання source,
callers/guards, diff і tests. Граф не є актуальним impact verdict.
Hardware/output робота не виконувалася; fail-closed gates збережено.

## Наступний bounded крок

Продовжити saved latency-distribution queue: **15 unprofiled read-only audits
одного 64-frame / 12-entry архіву** в fresh workers після окремого warmup.
Використати hash-bound tar цього baseline, trusted checkout checker, однаковий
relocation path і warm-cache метод; зберегти всі samples, VmHWM, median/range
та описовий p90 із явно названим estimator. Для 15 samples не заявляти p99
або tail-latency guarantee. Кожний audit має відтворити повний normalized
report/exact oracles і не змінити bytes/mtime; archived execution блокувати.

Це дасть кращу оцінку host variance для наступного виміряного performance
slice. Fresh resolve/stat/read/hash, closed inventory, усі peers і mutation
guards лишаються. Cold-cache study потребує окремої контрольованої методики;
cache dropping, hardware/output і thresholds не входять до цього плану.
Незалежні annotated data й ширша функціональна черга збережені в
[handoff](SESSION_HANDOFF_2026-10-20_UA.md).
