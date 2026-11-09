# Пропуск зайвого coarse scoring після exact zero

Розпочато `2026-11-09`, завершено `2026-11-10`. Baseline — remote-verified
`403e96e186ea03c8b4e252d03d05432eba11cad6` після
[актуального matcher baseline/profile](MATCHER_BASELINE_PROFILE_2026-11-07_UA.md).
Scope software-only: без Pi/camera/FC/UART/output, subagents, threshold changes
чи перебудови pixel kernels/cache.

## Зміна й доказ збереження поведінки

`Gray8RouteMatcher::match` пропускає подальше raw pixel scoring після
`best_distance == 0.0`, лише коли `top_candidate_count == 0` і
`directional_search_bias == 0.0`. Production diff — сім доданих рядків в одній
функції. Сама перевірка `validate_frame_against_entry` виконується **перед skip**
для кожного eligible entry; цикл не переривається. Existing initial/window
selection, scale refinement, confidence, yaw, timestamps і state update збережені.

Raw absolute differences невід'ємні. За нульового bias ranked score дорівнює raw
score, тому після exact zero пізніша оцінка не може виконати strict `<`.
Перший winner/tie залишається тим самим; raw winner score вже відомий і потрібний
для confidence. Немає epsilon, наближеного distance або зміни acceptance policy.

Top-k потребує точних confidence та порядку пізніших candidates, тому там skip
вимкнено. Ненульовий bias може дати ranked score нижче нуля, тому цей path також
повністю зберігає scoring. Навіть за неактивного direction ненульовий configured
bias консервативно вимикає нову оптимізацію. Positive-score queries не досягають
zero gate; очікуваного виграшу для них не заявлено.

## Characterization до зміни production

Новий `gray8_coarse_zero_test` спочатку зібрано проти архівованого baseline
source/header, з `-UNDEBUG` після `-DNDEBUG`, і успішно виконано **до** редагування
matcher. Той самий test потім пройшов на candidate. Він фіксує:

- earliest coarse winner, duplicate ties, повний top-k та reverse insertion order;
- zero bound, який скидається на наступному query, positive scoring після exact;
- window state після invalid match, повторний invalid exact zero й initial window;
- forward bias, де пізніші negative scores перемагають ранній raw zero;
- нейтральний bias у трьох directions, scale-refinement on/off, yaw observation;
- помилку в пізнішому entry після exact zero, її точний текст і partial candidates;
- malformed query, clearing candidates та empty initial window exception;
- **3456 bit-exact RouteMatch comparisons** із path, який збирає всі top
  candidates і тому виконує повний coarse scoring. Deterministic cases поєднують
  duplicates, exact/random queries, windows, rejected matches, bias, direction,
  initial bounds і refinement; progress-zone diagnostics також звірено.

Baseline archive отримано з Git із `core.autocrlf=false`; кожен extracted file
перевірено за Git blob identity. Перші preflight перевірки зупинилися на різниці
Windows/Git line endings до компіляції test. Їхні logs/tar збережено; canonical
archive використано без послаблення source bindings і без зміни решти checkout.

## Paired workload та межі виміру

Bound inputs — ті самі **19 файлів / шість VHRS routes / 2583 Gray8 160×100
frames / 13 stop-frame PGM**, з explicit session start/done bindings. Baseline і
candidate використовують один benchmark source, однакові compiler/flags та native
input paths. Кожен binary прямо включає відповідні `gray8_route_matcher.cpp`,
`route_signature.cpp`, `replay_frame_source.cpp`; production-бібліотеки revision
не змішуються. Flags: `-std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -pthread`.
Compiler executable/hash/version, commands, source/binary/cache identities bound.

Schedule визначено до вимірів: один excluded warmup process кожного варіанта
на case, потім **чотири paired rounds усіх 44 scenarios**. Parity case/round
чергує before/after order; кожен case має два before-first і два after-first
pairs. Кожен fresh process додатково виконує один discarded internal pass та
три emitted passes із новим matcher. Отже, **176 timed pairs / 352 timed
processes**, 88 excluded warmup processes, **92988 timed self-matches + 312 PGM
на кожен варіант**. Samples не відкидали й не додавали за результатом.

Окремо звірено 44 cases поточного CMake Release target та 50 `-pg -g`
diagnostic processes: два variants × (6 global + 6 window-scale + 13 pgm-scale).
Усі **534 processes** мають exact full historical config/match/candidate oracles.
Schema/behavior drift відсутній. Нормалізовано лише `init_us` та `match_us`;
pass/query coverage і repeated-pass stability перевіряються окремо.

Clock охоплює лише `matcher.match`. Route/PGM I/O, frame/candidate copying та
JSON output поза API timer, initialization збережено окремо. Timings звичайних
unprofiled binaries; diagnostics не входять у ці distributions. Native input
copies та originals зберегли bytes/mtime, sources перевірено до/після запусків.
Це warm WSL evidence, fresh process не означає cold cache. Self-match, sparse
PGM та повторні passes не є незалежними captures/accuracy acceptance; timestamps
не відтворюються в реальному часі.

## Paired результати

Для route case process measure — nearest-rank steady p50 за трьома emitted
passes, без першої query кожного pass. Для PGM із однією query/pass — median
трьох emitted query times. Для кожного case обчислено чотири paired after/before
ratios, потім їх median. Таблиця показує median цих case medians; range також
між case medians, а не між усіма individual samples.

| Mode | Cases | Median case paired ratio | Median paired reduction | Range case ratios |
|---|---:|---:|---:|---:|
| self-global | 6 | 0.411 | **58.93%** | 0.353–0.541 |
| self-window30 | 6 | 0.578 | **42.22%** | 0.509–0.608 |
| self-window30-scale | 6 | 0.628 | **37.18%** | 0.442–0.678 |
| pgm | 13 | 0.967 | gain не заявлено | 0.838–1.136 |
| pgm-scale | 13 | 0.994 | gain не заявлено | 0.906–1.226 |

Global timings нижче — median чотирьох process p50, microseconds. Paired
reduction отримано з чотирьох ratios, а не відношення двох наведених medians.

| Route timestamp | Frames | Before p50, µs | After p50, µs | Median paired reduction |
|---|---:|---:|---:|---:|
| 20260708T174000Z | 300 | 233.54 | 125.60 | 46.94% |
| 20260709T151301Z | 300 | 275.25 | 120.10 | 56.39% |
| 20260709T161017Z | 600 | 776.19 | 297.28 | 63.39% |
| 20260710T154326Z | 183 | 124.87 | 67.50 | 45.88% |
| 20260710T155821Z | 600 | 733.68 | 261.45 | 64.68% |
| 20260712T164651Z | 600 | 722.82 | 285.05 | 61.48% |

Це виміряний API gain для **exact self-match цього workload**. У звичайного
камерного кадру з додатною похибкою zero gate не спрацює. PGM controls мають
top-k=5 і змішані timing ratios; range до 1.226 не є доказом відсутності
performance regressions у всіх cases. Чотири pairs не дають tight overhead/tail
bound; PGM speedup не заявляємо. CPU affinity/frequency не фіксували. Host/cache
effects не відокремлювали hardware counters, а cross-date timings зі старими
reports не є before/after порівнянням.

### Окрема перевірка PGM slowdown signal

Фінальний review чотирьох pairs виявив PGM case medians на 10–23% повільніші.
До publication виконано **новий окремий fixed control experiment** на тих самих
before/after binaries: усі 26 PGM cases, один excluded warmup кожного варіанта
і **20 balanced pairs/case**. Case order обертається між rounds; кожен case має
10 before-first і 10 after-first pairs. Початкові samples не змінювали й не
об'єднували з новими. Це confirmation після виявленого signal, а не частина
початкового four-pair schedule.

Усі **1092 processes** (52 warmups + 1040 timed / 520 pairs) зберегли exact
oracles, top-k=5 і confidence <1. Probe та independent finalizer — exit 0;
244 source hashes, ті самі binary hashes та input bytes/mtime звірено.

| Mode | Median case paired ratio, 20 pairs | Range case medians |
|---|---:|---:|
| pgm | 0.991 | 0.949–1.066 |
| pgm-scale | 1.003 | 0.972–1.020 |

У case-37 початковий median ratio 1.226 став 1.011 у новому experiment;
великий initial slowdown тут не відтворився. Це узгоджується з host variation,
але причину варіації не встановлено. Немає PGM speedup, formal timing-regression
bound чи tail acceptance. Усі 20 ratios/IQR кожного case збережено.
Додатковий read-only run не змінював production; повний suite не повторювали.
Local `artifacts/coarse-zero-controls-20261110/` має **2189 bound files**;
summary SHA-256
`598361ba2594e68edce9e5c90213cb3991f73cac3fb1597bc967e81efa9f0089`.

## Call diagnostics: scoring менше, validation стільки ж

Raw gmon arcs декодовано окремо від timing. Callee addresses звірено з повними
`addr2line -i` inline chains; використано outermost frame фактичного callee.
Початковий reader помилково обирав innermost `vector::size()` замість raw-score
function. Це виявив independent readback; failure log та inspection збережено.
Reader виправлено, усі очікувані call-count assertions залишилися незмінними.
Flat gprof labels із попереднього study для compiler clones не використовувалися.

Counts охоплюють internal warmup і три emitted passes кожного diagnostic process:

| Group | Raw calls before → after | Validation calls before = after | Scaled calls before = after |
|---|---:|---:|---:|
| global, 6 routes | 5173956 → **2592144** | **5173956** | 0 |
| window30-scale, 6 routes | 617520 → **318720** | **617520** | 0 |
| pgm-scale, 13 images | 31200 → **31200** | **31200** | **3456** |

Global before/after дорівнюють `4 × sum(n²)` та `4 × sum(n(n+1)/2)`.
Window formulas враховують initial global query, previous winner і clipped
window; finalizer обчислює їх незалежно з `n` та query indices. Отже, structural
validation traversal збережено, а raw scoring у self-workload скоротився
приблизно вдвічі. Це function-call counts, не kernel syscalls чи CPU samples.

## Валідація, рішення та наступний крок

Baseline characterization, candidate experiment, full software runner та
independent finalizer — **exit 0**. Новий test зареєстровано одним CMake рядком.
Debug/Release CTest **65/65 кожна**, **357 Python tests**, без failures/skips.
Release test compile command містить `-UNDEBUG` після `-DNDEBUG`. Сім
camera/output flags OFF в обох build caches. Звірено **244 source hashes**:
241 попередній файл незмінний, змінені matcher/CMake, додано один test.
Inputs/session bindings, усі 534 raw JSONL/oracles, pass/query order, paired
ratios, call formulas і повний software log перечитано незалежно.

WSL x86_64 / Intel Core i5-10300H / GCC 13.3. Python executable
`/usr/bin/python3.12`, SHA-256
`e50d468e8b0adfb05733f5b87b3cff34829c4a8c1aea50c865aa8bdfe4bb150f`;
build `3.12.3 (main, Aug 31 2026, 10:18:26) [GCC 13.3.0]`.
Нових native sanitizer/NoTests/MSVC/hardware acceptance runs немає.
Actual matcher/header/benchmark/callers/CMake/tests і diff перевірено прямо;
graph metadata досі `6d8f547`, використано source/diff/tests fallback.

**Рішення:** candidate залишено: точні результати, state/exception/validation
contracts збережено; exact self-match має виміряний paired gain. Pixel kernels,
lazy edge cache, policy/thresholds, archive hash/containment/mutation guards,
camera/output code не змінювалися.

Наступний bounded slice — **read-only non-exact/top-k/scale CPU attribution**
на тих самих bound stop-frames із достатнім наперед визначеним diagnostic
workload/sample budget. Попередні 33 stop-scale samples недостатні для точних
cost shares. Звірити current 244-source/build identities та exact oracles,
окремо profile й timing, лише потім обирати один наступний hotspot. Завершені
zero-bound skip/kernels/cache без нового evidence не відкривати; self-match не
використовувати для tuning thresholds. Незалежні annotated data, cold-cache та
hardware gates з ширшої черги залишаються окремими.

Local ignored `artifacts/coarse-zero-20261109/` містить **1692 bound evidence
files** поза двома extracted source directories: canonical/preflight archives,
before/after benchmark/characterization binaries, scripts/logs, 534 JSONL/result/
stderr sets, 50 gmon files, full inline call frames, source manifest,
summary/validation та evidence SHA-256 manifest. Extracted canonical sources
окремо звірено з bound tar. Native temporary copies прибрані після experiment;
local evidence збережено. Fresh clone отримує code/tests/docs, не artifacts.

| Binding | SHA-256 |
|---|---|
| Canonical baseline tar | `9be11e8174d94aebff3a6da1ef12ffa642027b7821c3416c7e45433caa01ab1b` |
| Before benchmark | `a46cbe406695071ce771cc29f77e70f27e95d50ad518333479085faf77355323` |
| After benchmark | `0afe8f9883eb485c85c383000e056d09d63f96e6e51cef0864f15df77d1b5297` |
| CMake Release benchmark | `836f380af6fdaa65a5387909200bc589d20a1759a4856302835e747480c7c7b8` |
| Summary | `5688fce8f2e79d4112f4cb74824e0f9df46c48ffb401fd87d56668788954b9be` |
| 244-source manifest | `c6f89be0bcdd08606c0dc36c0086b8f7b3c361fd89278cbfe84b07ea75306724` |
| Full software log | `133df6a2dbf050f50f6048612701adca62c6dcc0528366f3f55721069d78d650` |
