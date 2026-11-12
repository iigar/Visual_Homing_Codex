# CPU attribution для non-exact stop-frames

Дата: `2026-11-12`. Відновлено remote-verified
`e7457ae02257d5041cd8c3ccb0de22b778e8d318` після
[coarse-zero optimization](COARSE_ZERO_SCORING_2026-11-10_UA.md).
**Production/tests/runner/core/config не змінено.** Це read-only profile slice;
нового candidate або speedup claim немає. Scope software-only, сім camera/output
flags OFF, без Pi/camera/FC/UART/output чи subagents.

## Зафіксований workload і точність результатів

Ті самі **19 bound input files**: шість VHRS routes, 2583 Gray8 160×100 frames
і 13 stop-frame PGM. Hashes, file set, explicit session start/done mappings та
original/native bytes/mtime звірено. Кадри та їхні повтори не є новими captures
або незалежними annotated даними.

До запусків зафіксовано `plan.json`: **8192 повтори/image для pgm**, **4096 для
pgm-scale**, по 13 images/mode; мінімум **1000 recorded samples/mode**. За
shortfall додаткові samples за результатом не передбачені. Окремо — один
conformance process усіх 44 historical scenarios і три unprofiled timing rounds
усіх 26 PGM cases. Старі paired studies/finalizers не перезапускали.

Поточний CMake Release `gray8_matcher_benchmark` має той самий binary hash, що
в coarse-zero checkpoint; `ninja -n` підтвердив no work. **244 source hashes**,
compiler executable/version/hash, compile/link commands, cache та binary
identities bound. Окремий local diagnostic прямо лінкує current matcher,
route-signature та replay sources із `-std=c++20 -O3 -DNDEBUG -pg -g -pthread`
і warning flags; production-бібліотеки різних revisions не змішуються.

Калібровка: по одному PGM/PGM-scale batch із 32 iterations. Raw gmon arcs
підтвердили **рівно N match calls**, N×route_entries raw/validation/top-k calls,
очікувані scaled calls та **0 constructor/route-read calls у gate**. Потім:

- **122 unprofiled Release processes**: 44 conformance + 78 timed PGM;
- **26 profiled processes**, **159744 exact repeated queries**;
- **2 calibration processes**, 64 додаткові repeated queries.

Усі **150 processes** зберегли full historical match/candidate oracles. У
conformance/timing JSONL вилучено лише init_us/match_us. Batch перевіряє кожен
повтор bit-exact за всіма RouteMatch fields і candidate index/progress/confidence
та order; перший і останній результати незалежно звірені з historical oracle.
Top-k=5, confidence <1 у всіх PGM fixtures; coarse-zero gate тут не застосовується.

## Що саме профілюється

Fresh process завантажує route/PGM, створює matcher і виконує один warmup query
з profiling OFF. Потім glibc `moncontrol` вмикає profiling для **одного continuous
batch**: retained matcher, той самий query, `match` плюс exact-result checks.
I/O, construction та JSON serialization поза gate. Після batch profiling OFF.
Короткі per-query gates не використовувалися: query коротший за 100 Hz tick.
Невелика pre-main activity від `-pg` можлива; її не трактуємо як match cost.

Retained state — свідома межа цього diagnostic: global window=0, initial bounds
disabled, bias/direction/yaw disabled. Повторні outputs підтвердили, що state не
змінює результати, але cache/allocation/tracking conditions відрізняються від
fresh-matcher benchmark і live pipeline. Oracle-check/loop overhead включено в
recorded profile та класифіковано окремо.

Finalizer незалежно декодує glibc gmon v1 histogram/arcs і зіставляє **всі nonzero
PC bins та callee PCs із повними `addr2line -i` chains**. Для call counts береться
outermost callee, для source attribution — фактичний inlined function chain.
Compiler clones та innermost vector::size() не підміняють matcher function.
Flat gprof labels не використовуються. Raw gmon, symbols та inline mappings
збережено. Rate **100 Hz**, bins **4 bytes**, saturation не знайдено.

## Recorded executable-text samples

Ці percentages мають знаменник **recorded executable-text samples**, а не всю
CPU latency. Shared-library code може не потрапити в histogram. Це instrumented
diagnostic з batch/checks overhead, без formal confidence intervals чи tail claim.

| Mode | Queries | Recorded samples | Raw kernel | Scaled kernel | Top-k insertion | Validation |
|---|---:|---:|---:|---:|---:|---:|
| pgm | 106496 | **6492** | **6369 / 98.11%** | 0 | 30 / 0.46% | 15 / 0.23% |
| pgm-scale | 53248 | **7264** | **3664 / 50.44%** | **3507 / 48.28%** | 25 / 0.34% | 14 / 0.19% |

Budget перевищено в обох groups без додаткових rounds. Raw accumulation/loop
lines **88/86** домінують у pgm. У pgm-scale до них додаються scaled accumulation
та indexed loads **59/57**. Raw/scaled shares у другому mode близькі; різниця
двох percentage points не доводить строгий порядок їхньої вартості. Малим
categories із десятками samples не приписуємо високої точності.

| Mode | Match calls | Raw = validation = top-k calls | Scaled calls | Constructor / route read |
|---|---:|---:|---:|---:|
| pgm | 106496 | **63897600** | 0 | 0 / 0 |
| pgm-scale | 53248 | **31948800** | **3538944** | 0 / 0 |

Ці totals незалежно відновлено з fixed N, route sizes та coarse winner/refinement
window. Для scale всі 24 scales виконуються для кожного eligible neighbor:
жоден fixture не досягає zero. Counts — function calls, не kernel syscalls.
I/O/constructor count=0 свідчить про exclusion із gate, а не про нульову вартість
цих фаз у звичайному process.

## Clocks та окремі unprofiled timings

Спостережена clock anomaly: `std::clock` process-CPU delta перевищує
steady-wall delta у **23/26 workers** (11 pgm, 12 pgm-scale). CPU/wall ratio
ranges **0.9991–1.0353 / 0.9932–1.0641**. Причину в цьому WSL runtime не
встановлено. Sum CPU counters **68.820/78.945 s**, sum steady-wall
**67.505/77.476 s**. Nominal histogram ticks/100 дають **64.92/72.64 s**, але
ratios до process CPU **0.943/0.920** не є каліброваним coverage або оцінкою
частки missing library time. Ці counters не використовуються як API latency.

Timings нижче — звичайний незмінений Release benchmark, поза профілюванням:
три fresh processes/case, internal warmup + три emitted passes, лише match у
steady_clock interval. Для кожного image — median 9 emitted query times;
таблиця — median/min/max цих 13 image medians. Init/I/O/copies/output поза API.

| Mode | Median image median, µs | Range image medians, µs |
|---|---:|---:|
| pgm | **649.4** | 545.9–690.0 |
| pgm-scale | **1354.5** | 1075.0–1931.0 |

Це current warm WSL baseline без candidate comparison. Не робимо cross-date
speedup, bare process latency, cold-cache, Pi/Windows/live latency або physical
accuracy claim. CPU affinity/frequency не фіксували; повтори тих самих frames не
додають незалежності.

## Рішення й наступний bounded slice

**Вибрано coarse raw scoring як спільний hotspot двох modes.** Top-k insertion
та structural validation не дають підстав для rewrite або вилучення guards.
Scaled kernel також матеріальний; його не змінюємо в цьому read-only slice.

Наступний крок — **characterization lossless partial-score rejection для
non-exact coarse top-k path**, спочатку без production edits. За neutral bias
дослідити, чи nonnegative partial sum із повним denominator може довести, що
entry не поліпшить winner і не потрапить у вже заповнений top-k. Preserve strict
winner ties та **rounded confidence ties**: lower_bound віддає перевагу пізнішому
candidate з рівною confidence, отже equality не дозволяє відкидання для top-k.
Перед будь-яким score skip лишається повна structural validation кожного
eligible entry. Чинний full raw scorer, diagnostic APIs, biased/initial/window
paths, state/exception semantics потребують явних controls.

Лише після baseline characterization/feasibility — один bounded candidate,
alternating paired full-workload equivalence та API gain; code change потребує
нового full runner. Finished zero-skip/pixel kernels/cache без evidence не
переробляти; policy/thresholds за self-match не підганяти. Independent annotated
data, cold-cache та hardware gates із ширшої черги збережені окремо.

## Валідація та bindings

Probe/calibration і independent finalizer — **exit 0**. Current 244 source hashes,
input/session/build/compiler/runtime identities й bytes/mtime звірено до/після.
Debug/Release caches мають сім flags OFF. Нового full runner немає, бо production/
tests/runner/core/config незмінні. Попередній software log на ідентичних source
files/Python executable/build перечитано за hash/success/counts:
**357 Python / 65+65 CTest**, без skips/failures.
Це historical full-suite validation, не новий запуск. Нових native sanitizer/
NoTests/MSVC/hardware acceptance runs немає.

Прямо прочитано matcher/header/benchmark/caller config та actual source/diff;
graph metadata `6d8f547` advisory, використано source/tests fallback. Local ignored
`artifacts/stop-profile-20261112/`: **493 bound files**, local harness/plan/probe/
finalizer, 150 raw JSONL/result/stderr sets, 28 raw gmon files, symbols/full inline
maps, source manifest, summary/validation. Native temporary copies прибрано після
probe; local evidence збережено. Fresh clone отримує docs/source, не artifacts.

| Binding | SHA-256 |
|---|---|
| Current Release benchmark | `836f380af6fdaa65a5387909200bc589d20a1759a4856302835e747480c7c7b8` |
| Local batch profiler | `147de9ca815c0e84d9830660a2665b6460f81998ea4e16f4acf8b86145b289df` |
| Summary | `c0a2794db9c66c6037981739a8f5e0ef672c923462eb9b9188ec7219528c21df` |
| 244-source manifest | `c6f89be0bcdd08606c0dc36c0086b8f7b3c361fd89278cbfe84b07ea75306724` |
| Prior full software log | `133df6a2dbf050f50f6048612701adca62c6dcc0528366f3f55721069d78d650` |
