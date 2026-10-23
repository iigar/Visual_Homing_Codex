# Вимірювання масштабування за кількістю кадрів

Дата: `2026-10-23`. Початковий checkpoint: `48f551e63cbf32428bf66906beb9574f8cdcd107`,
remote SHA перевірено, початковий tracked tree чистий. Продовження
[inventory slice](INVENTORY_OPTIMIZATION_2026-10-23_UA.md).

## Що змінено

Розширено лише manual probe `scripts/tests/benchmark_second_pass_campaign.py`
та його тести. Production C++/Python/config і всі guards залишаються незмінними.
Використано пряме читання fixture/probe/validator/scorer і actual diffs;
graph metadata лишається `6d8f547` і не є актуальним impact verdict.
Новий `--frames 8 64 256` змінює query frame count при сталих 12 runs,
трьох datasets/splits, двох configs, двох policies, чотирьох reference frames
і native/target resolution 2×2. `--frames` і `--runs` взаємовиключні;
default лишається 12/24/48-run scaling з вісьмома кадрами.

Frame counts — унікальні кратні восьми в [8, 512]. Вісім початкових кадрів
і labels повторюються окремими файлами з новими sequence/ID/timestamp,
metadata counts/bounds та input hashes. 8-frame inputs byte-identical до
початкового fixture. Source timestamps ростуть на 100 ns без gaps. Pixels,
reference routes, configs і policies сталі; повторення не додає незалежних
captures, нової візуальної складності або accuracy evidence.

Кожний audit перевіряє 12 runs, всі peers, coverage кожного dataset/split,
source identity, очікуваний reference index, match/navigation validity,
відсутні unsupported gates, TP/FP/FN, positive/negative/unscorable denominators.
Наприклад, для 16 кадрів near-inclusive має TP=6/FP=4/FN=6, near-clear —
4/4/6, exact-inclusive — 0/0/12, exact-clear — 0/0/10 на кожний dataset.
Усі expected frame-run pairs дорівнюють 12 × frame count.

Нові `test_second_pass_scaling.py` містять шість regression tests з actual
local replay. Вони перевіряють byte identity 8-frame inputs, 64-frame source
contracts, invalid counts до створення inputs, hand-calculated 16-frame
oracles, read-only relocation без execution і відхилення пошкоджених
reports/predictions. Набір зареєстровано для Debug і Release.

## Метод і уточнення RSS

Python 3.12.3 / native WSL `/tmp`, той самий Release binary. Кожна матриця:
окремий 8-frame warmup, три rounds × 8/64/256 кадрів, fresh worker для
campaign та audit, окремий 256-frame cProfile. API time виключає imports,
fixture setup, oracles і persistence. Під час timed runs не запускаються
full suite або readback audits. Host load/CPU clocks не фіксовані, cache warm;
це не randomized controlled experiment або performance guarantee.

Перший повний pass виявив спотворення RSS: після великого fixture наступні
малі workers мали однаковий підвищений `ru_maxrss`. Окремий локальний
`rss-probe.py` відтворив це: parent allocation 64 MiB дала child
`ru_maxrss=77824 KiB`, але власний VmHWM child був 12288 → 14336 KiB після
allocation 2 MiB. При parent allocation 128 MiB child ru_maxrss став 143460 KiB,
а VmHWM знову 12288 → 14336 KiB. Це evidence конкретного WSL subprocess/exec
workflow, а не припущення про додаткову пам'ять у validator.

Probe тепер зберігає обидві величини:

- `worker_peak_rss_kib`: попередній getrusage ru_maxrss без зміни значення;
  може включати пам'ять parent до exec;
- `worker_vm_hwm_kib`: VmHWM поточного executable image з `/proc/self/status`;
  плюс initial VmHWM/ru_maxrss перед timed action для видимості початкової межі.

[Документація Linux kernel](https://docs.kernel.org/filesystems/proc.html)
визначає VmHWM як peak resident set size та застерігає про асинхронний,
не абсолютно точний RSS accounting. VmHWM включає imports і live returned
report, виключає replay children; це не API allocation delta, не точний
simultaneous process-tree peak і не Pi RAM acceptance.

Після уточнення виконано другу повну матрицю. Перший pass з усіма samples,
profiles і archives збережено; він не відкидається й не видається за
production before/after optimization. Тимчасові raw reports зберігають
історичні paths, але relocation verification не залежить від їх існування.

## Результати

Медіани трьох samples; у дужках minimum–maximum. Час — секунди, VmHWM — MiB.

| Кадрів/dataset | Фаза | Початковий pass, с | Повторний pass, с | VmHWM повторного pass, MiB |
| ---: | --- | ---: | ---: | ---: |
| 8 | Campaign | 3.730 (3.729–3.861) | 3.777 (3.703–3.876) | 30.82 (30.82–30.82) |
| 64 | Campaign | 12.151 (12.079–12.169) | 12.517 (12.354–12.543) | 34.95 (34.82–35.07) |
| 256 | Campaign | 42.086 (41.396–42.398) | 42.170 (41.867–42.576) | 47.40 (47.28–47.42) |
| 8 | Audit | 0.744 (0.736–0.744) | 0.768 (0.764–0.771) | 32.50 (32.47–32.62) |
| 64 | Audit | 2.705 (2.606–2.765) | 2.746 (2.738–2.768) | 41.70 (41.59–41.71) |
| 256 | Audit | 9.339 (9.339–9.412) | 9.663 (9.442–9.801) | 73.72 (73.72–73.73) |

Після збільшення frame count у 32 рази API time зріс приблизно в 11.2 раза
для campaign і 12.6 раза для audit. Сталі setup/control/report costs впливають
на це співвідношення; три точки не доводять asymptotic complexity або поведінки
за межами 256 кадрів. Зміни часу між двома passes не трактуються як
прискорення/регресія production, бо production bytes однакові.

Для 8/64-frame повторних samples legacy ru_maxrss має медіану 42.43 MiB
для обох фаз, тоді як VmHWM розрізняє їхній власний peak. Усі raw samples,
початкові значення RSS і обидві memory series збережено. VmHWM не є оцінкою
додаткової пам'яті тільки алгоритму та не включає C++ replay children.

Окремий 256-frame profile другого pass: campaign **94.453 с**, audit
**21.443 с** із profiler overhead. `Dataset.read`: 389021 / 79.085 с і
81707 / 16.730 с cumulative відповідно; `Dataset.path`: 389132 / 59.556 с
і 81767 / 12.364 с. Campaign `guard`: 39 calls / 43.681 с. Вкладені часи
перекриваються й не додаються. Профіль вказує на повторні path/read/frozen
checks; він не дозволяє прибирати ці перевірки або кешувати content hashes.

## Докази та межі

Full WSL software runner пройшов: Debug/Release CTest **64/64**, telemetry
CLI **7/7**, exporter **21/21**, collection runner **23/23**, campaign **4/4**
і нові scaling tests **6/6** кожна; readiness **15/15**, recorded-route
benchmark **5/5**, validator **31/31**, paths **12/12**, inventory **12/12**,
scorer **27/27**, bundle **24/24**, collection **27/27**, outer audit **30/30**.
Немає пропущених WSL symlink/special-file tests.

Зафіксовано **237** source hashes. Із попередніх 236 змінилися лише software
runner, tests README і manual probe; додано scaling tests. Усі production
C++/Python/config bytes збігаються, сім camera/output flags OFF у двох caches.
Окремий `--runs 12 --repeats 1` compatibility smoke також пройшов із warmup,
sample і profile; його три archives не входять до frame-scaling статистики.

Readback перевірив hash-bound tar і всі **22 relocated archives / 264 runs /
29952 frame-run pairs** у двох passes. На кожному розмірі всі повтори й обидва
passes мають однакові prediction hashes, повні metrics і coverage. Audits
відтворюють saved reports, перевіряють anchors, не запускають archived code
та не змінюють bytes/mtime. Усі median/min/max незалежно перераховано.
Selected production resolve/stat/scandir/read/hash/guard profile counts
між двома passes точно збігаються; змінився лише probe RSS capture.

Evidence: `artifacts/frame-scaling-20261023/`. `measurement/` — початковий
повний pass; `corrected/` — повторний pass з додатковим VmHWM; обидва мають
summary, raw samples/profile та hash-bound native tar. `rss-probe.py/json`
фіксують причину RSS correction. `trusted-initial/` містить перевірені initial
source copies; readback використовує лише trusted tools, не archived code.

SHA-256:

- Initial tar: `17338f711585f5bd946293ee4f99d31a3fb7f76ca9ea1bca5486c050270fa572`.
- VmHWM tar: `6d8e9d940b9751a5d3a3b2d3c6a38f4aed9b2fb775a4532b67c69f90cc055230`.
- Release binary: `a6f8c9f3ead1c5b1384bfae9f60ace667cc14195d2cab06f7633fccbf8ac2bd6`.
- Software log: `4844a116c7a1f9a9d38460763d5fdf59521aecd930b42b6905503286d7a02f63`.

Великі artifacts локальні й ignored. Новий clone матиме source/tests/docs,
але не ці tar/log/build files. Scope — software-only; hardware/output enabling
та native sanitizer/NoTests/MSVC acceptance до цього slice не входять.

Наступний bounded крок — resolution scaling при сталих frame/run/dataset
counts і reference count. Спочатку зафіксувати pixel/preprocessing/matcher
oracles для нової geometry, потім виміряти native WSL API time і VmHWM
з усіма fresh integrity guards. Production оптимізацію обирати лише після
цього profile. Окремо визначити memory scope для C++ child; worker VmHWM
не замінює його peak RSS. Черга й окремі hardware gates лишаються в
[handoff](SESSION_HANDOFF_2026-10-20_UA.md).
