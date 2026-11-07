# Offline matcher baseline та CPU profile

Дата завершення: `2026-11-07`. Відновлено remote-verified
`4131371e30c3d776842f4d22628ec8d6a04d8b1d` із чистим tracked tree.
Production/tests/runner/core/config не змінено; candidate у цьому slice немає.

## Inputs, build та межі виміру

Звірено closed set **19 файлів** із
`artifacts/board-inventory-20260922/hash-verification.json`: шість VHRS routes,
**2583 кадри Gray8 160×100**, 13 stop-frame PGM. Прив'язки PGM до routes
перевірено через explicit start/done поля `session-summaries.log`, без
інтерпретації filename як ground truth. Native copies та originals зберегли
bytes/mtime. Нових captures немає.

Поточний CMake Release target `gray8_matcher_benchmark` перевірено rebuild
(`ninja: no work to do`), його self-test пройшов. Окрема свіжа CMake Release
збірка має додаткові `-pg -g`, linker `-pg`, `BUILD_TESTING=OFF`; її self-test
також пройшов. **Сім camera/output flags OFF в обох caches.** Записано source,
compiler executable/version/hash, compile/link commands, options, caches і
binary hashes. Основні flags: `-O3 -DNDEBUG -std=c++20`, без fast-math.

Runtime: WSL2 x86_64, Intel Core i5-10300H, kernel
`6.6.87.2-microsoft-standard-WSL2`, GCC `13.3.0-6ubuntu2~24.04.1`;
Python `3.12.3 (main, Aug 31 2026, 10:18:26) [GCC 13.3.0]`.
Python executable SHA-256 збігається з попереднім full-suite evidence.

Усі виміри виконано на native WSL filesystem після копіювання й hash checks:
це **warm offline matcher evidence**. Benchmark читає route/PGM до вимірів;
`steady_clock` охоплює лише `matcher.match`, без I/O, frame/candidate copying,
JSON output, camera pipeline або timestamp pacing. Initialization вимірюється
окремо й охоплює route copy та matcher construction. CPU profile охоплює весь
діагностичний process, включно з initialization, internal warmup та output;
його час не підміняє latency звичайного Release binary.

## Точна відповідність історичним результатам

Відновлено **44 scenarios** із `artifacts/scale-refinement-20260924/comparison/`:
18 route cases (шість routes × `self-global`, `self-window30`,
`self-window30-scale`) та 26 PGM cases (13 × `pgm`, `pgm-scale`).
Для кожного звірено три historical after-process JSONL із bound summary.
**Schema/behavior drift не знайдено.** Порівняння охоплює весь config,
RouteMatch і candidates: index, timestamp, progress, direction/error/validity,
confidence, candidate order. Вилучено лише `init_us` і `match_us`.
Pass/query order, coverage та repeated-pass stability перевірено окремо.

Новий schedule: один excluded warmup batch і три timed rounds, кожен по
44 fresh processes: **176 Release processes**, із них **132 timed**.
Кожен process додатково виконує один internal discarded pass та три emitted
passes із новим matcher. Timed rounds містять **69741 self-match + 234 PGM**.
Окремі profiles: 6 global + 6 window-scale + 65 stop-scale processes
(13 PGM × 5 repeats). **Усі 253 процеси** зберегли exact historical oracles.
Усі self-matches valid, confidence `1`, route index дорівнює query index.

## Поточні timings

Нижче — microseconds, median відповідного process percentile із трьох timed
processes. Percentiles — nearest rank за трьома emitted passes без першої query
кожного pass. Першу query та init збережено окремо у validation JSON.

| Route timestamp | Frames | global p50 / p95, µs | window30 p50 / p95, µs | window30-scale p50 / p95, µs |
|---|---:|---:|---:|---:|
| 20260708T174000Z | 300 | 159.35 / 303.34 | 31.12 / 47.87 | 31.47 / 37.37 |
| 20260709T151301Z | 300 | 165.80 / 316.66 | 31.37 / 50.90 | 31.37 / 40.16 |
| 20260709T161017Z | 600 | 539.16 / 753.29 | 31.23 / 43.67 | 30.78 / 43.67 |
| 20260710T154326Z | 183 | 92.23 / 150.28 | 30.78 / 47.02 | 30.78 / 37.27 |
| 20260710T155821Z | 600 | 531.27 / 709.58 | 30.85 / 41.11 | 31.18 / 41.70 |
| 20260712T164651Z | 600 | 527.25 / 711.42 | 31.16 / 35.86 | 31.46 / 42.05 |

PGM має одну query/pass, тому steady percentile для нього відсутній. Для кожного
PGM взято median дев'яти emitted query times (3 processes × 3 passes), потім
median цих 13 image medians: **596.358 µs `pgm`**, **1337.181 µs `pgm-scale`**.
Ranges image medians: **553.763–669.996 / 1140.898–1434.865 µs** відповідно.
Різні modes мають різний workload. Немає paired candidate comparison або
cross-date speedup claim; попередні raw/scale kernel changes уже входять у source.
Три processes на case не є p99/tail або real-time acceptance.

## CPU attribution та обмеження інструмента

Незавершений local run `matcher-profile-20261105/` збережено без перезапису.
Його `gprof -l` падає з `somebody miscounted: ltab.len=6122 instead of 5920`.
Flat `gprof` також приписує raw-score compiler-clone samples/calls сусідньому
`gray8_edge_payload`, якого benchmark не викликає. Цей label непридатний для
вибору hotspot; причину всередині binutils окремо не встановлювали.

Новий probe зберіг raw `.gmon` і flat output, але читає glibc gmon v1 records
безпосередньо й зіставляє цілі histogram bins з ELF `nm -S` ranges, включно
з `.isra.0` clones. Boundary bins лишаються явно unassigned. Формат звірено з
локальним `sys/gmon_out.h`; synthetic calibration перевіряє records, arcs,
clones, boundary handling і відхилення unsupported tags. C++ destructor aliases
з однаковими name/range дедупліковано.

Independent finalizer іншим способом декодує raw histogram integers, зіставляє
**кожен nonzero PC bin через `addr2line` із source lines**, звіряє total samples
та окремо обчислює очікувані coarse-call totals із workload/window rules.
Sample rate **100 Hz**, bins **4 bytes**. Усі values нижче включають internal
warmup; profile overhead відділено від Release timing.

| Group | Raw-distance calls | Scaled-distance calls | Total CPU samples | Samples in raw-score lines 73–95 | Samples in scaled-score lines 14–67 |
|---|---:|---:|---:|---:|---:|
| global | 5173956 | 0 | 449 | 446 (99.33%) | 0 |
| window-scale | 617520 | 0 | 34 | 32 (94.12%) | 0 |
| stop-scale | 156000 | 17280 | 33 | 16 (48.48%) | 17 (51.52%) |

ELF range attribution відносить 447/449 global samples до raw-score clone;
source-line subset містить 446. Це різні межі attribution, не втрата sample.
`gray8_edge_payload` має **0 calls** в усіх трьох groups. Global count дорівнює
`4 × sum(route_frames²)`; window count враховує initial global query й подальші
clipped windows. Self-match не запускає scaled kernel після exact coarse zero.
Global має достатньо samples для вибору dominant coarse-scoring hotspot;
**34/33 samples** у двох менших groups — лише груба діагностика, без точності
percentage/tail claims. `perf` локально відсутній; Release CPU sampling без
instrumentation не проводили. Це не kernel syscall або hardware-counter study.

## Рішення й наступний bounded slice

Вибрано **coarse scoring у `Gray8RouteMatcher::match`**. Наступний крок —
characterization можливості пропускати подальше raw pixel scoring після
досягнення zero lower bound, коли **top_candidate_count=0 і directional bias
нейтральний**. Повний structural validation traversal, strict winner/tie order,
state/exception semantics і всі diagnostics contracts мають зберегтися.
Top-k та directional-bias paths потребують явних negative controls; ширший
skip без доказу неприпустимий. Спочатку bounded characterization проти baseline,
потім лише виправданий candidate, alternating paired full-workload equivalence
і вимір API gain. Code change потребуватиме нового full software runner.

Завершені raw/scale pixel kernels і lazy edge cache у цьому slice не змінювалися.
Немає підстав tuning policy/thresholds за self-match. Sparse stop-frames та
повторні passes не є незалежними annotated даними або accuracy acceptance.
Cold-cache, Windows filesystem, Pi/live latency та hardware/output acceptance
лишаються окремими gates. Уся робота software-only, без subagents.

## Валідація й bindings

Probe, calibration та independent finalizer — **exit 0**. Звірено **243 source
hashes**, compiler/build/binary identities, input/session bindings, raw results,
oracles, query coverage, percentiles, histogram totals та coarse-call formulas.
Нової full-suite перевірки немає: production/tests/runner/core/config незмінні;
попередній log hash/success/counts **357 Python tests, Debug/Release CTest 64/64**
перечитано на тому самому Python executable/build. Це historical validation.
Нових native sanitizer/NoTests/MSVC/hardware acceptance runs немає.

Прямо прочитано matcher/header/benchmark, конфігурацію та actual caller paths;
stale graph metadata `6d8f547` залишається advisory, graph verdict не заявляється.
Local ignored `artifacts/matcher-profile-20261106/` (каталог розпочато 6 листопада)
містить **658 bound evidence files** поза build directory: scripts/calibration,
44 oracles, 253 JSONL/result pairs, 77 gmon files, profiles/source mappings,
summary/validation і SHA-256 manifest. Build binaries збережені локально та
bound окремими hashes. Fresh clone отримує docs/source, не generated artifacts.

| Binding | SHA-256 |
|---|---|
| Release benchmark | `626d288673d2669708d231086e82913b79c8466b114a4213c445b5d8a02dad8b` |
| Profile benchmark | `d5d2cea7ccffba1c89acd2c72210ee989690cc0c4281fa90b4527478fc793972` |
| Summary | `804cd457b5855cf233181f563e356d4742811b0784c81c84a267a919013113c4` |
| 243-source manifest | `a565452a0cfa785d58d2bc8f2b0856032575180b4e090106fc4732c30e958cdc` |
| Prior full software log | `d939a3e613a67f33cec9f8574084150781c8294d3cd4ccbf2cfe6da2e86aeb54` |
