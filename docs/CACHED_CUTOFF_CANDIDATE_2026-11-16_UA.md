# Cached-cutoff candidate: exact результати без достатнього API gain

Дата: `2026-11-16`. Відновлено remote-verified
`2495b45a71a9c712fa1d88391c84b156c3457c3d` після
[exact cutoff characterization](EXACT_CUTOFF_CHARACTERIZATION_2026-11-15_UA.md).
**Єдиний local cached-cutoff candidate відхилено. Production/tests/runner/core/
config незмінні.** Software-only, сім camera/output flags OFF, без hardware/output
і subagents. Local source/tests/binaries залишені в ignored artifacts.

## Що перевірено

Frozen `plan.json`: один candidate, block **1024**, чотири balanced rounds усіх
44 API scenarios. Baseline — current canonical production, а не попередній
відхилений prototype. Baseline core archive звірено з кожним current Git blob;
headers/loaders/benchmark однакові, обидві збірки напряму лінкують три frozen
sources, без змішування бібліотек revisions.

Candidate додає stack-local cutoff state на один `match`. Gate: configured
bias=0, positive best distance, full nonempty top-k, checked uint64 numerator
domain, IEEE double і FE_TONEAREST. За unsupported domain/rounding використовується
повний scorer; нових input rejection/throws немає. Structural validation кожного
eligible entry лишається перед scoring.

Cutoff lazy обчислюється на першому non-final block після зміни best/worst tuple.
Binary search використовує original rounded predicate:

```text
lower = double(numerator) / (double(full_pixel_count) * 255.0)
lower >= best && clamp(1.0 - lower, 0.0, 1.0) < worst
```

Є explicit impossible flag і overflow-safe midpoint, без inverse/epsilon.
Subsequent blocks порівнюють integer sum із cutoff. Partial normalized score
повертається лише для proven loser; normal insert/pop зберігає capacity та
allocation-failure prefix/state. Surviving top-k/winner мають full exact score.
Full raw scorer/diagnostics, zero-skip, scale/bias/window/initial policy не змінені.

Baseline characterization виконано до створення candidate. Успішні baseline і
candidate кожен пройшли **739 bit-exact RouteMatch / 154 capacity comparisons,
28 paired allocation-fault cases із 13 paired throws**. Це попередні 675/90
controls плюс **64 comparisons на чотирьох rounding modes**: FE_TONEAREST,
FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO; mode після match збережено.
Є confidence-boundary ties, wide uint64 sum, zero-confidence ties, malformed
later entry/repeated error, rejected/recovery/window/initial/direction/scale controls.
Assertions ввімкнені через `-UNDEBUG`; noinline test allocation scaffold усуває
попередні GCC allocator warnings. Усі чотири successful local builds warning-free.
Equivalence обмежена перевіреними outputs/state/C++ errors; floating-status flags
і enabled FP traps цим experiment не приймалися. Неможливі за доступною пам'яттю
oversized payloads не створювали: domain arithmetic bound має prior contract
evidence і source review, а не public-API allocation test.

Initial source-generator preflight зупинився після успішного baseline control:
обраний loop marker повторювався в diagnostics. Candidate source тоді не створено.
У `failed-01/` збережено script/plan/archive/binaries/log; marker прив'язано до
унікального zero-bound блоку `match`, потім preparation виконано успішно.
Це виправлення generator, не друга реалізація candidate або зміна oracle.

## Fixed paired API comparison

Ті самі 19 hash/session-bound files: шість VHRS routes / 2583 Gray8 frames,
13 stop-frame PGM; original/native bytes/mtime та file set незмінні. Нових
captures/independent annotated data немає. GCC 13.3.0, C++20,
`-O3 -DNDEBUG -Wall -Wextra -Wpedantic -pthread`, без profiler. Native input/binary
copies у WSL temporary directory; обидва benchmark self-tests пройшли.

**88 excluded warmup processes + 352 timed processes = 440 exact processes**.
**176 balanced timed pairs**: case/round parity дає два before-first і два
after-first на case. Незмінений benchmark має один internal warmup та три
emitted fresh-matcher passes/process. На variant — **93300 timed queries**
(92988 self-match + 312 PGM). Усі full normalized JSONL historical oracles exact;
вилучено лише init_us/match_us. Schema, timestamps, winner, candidates/order,
coverage та explicit session mapping збережені.

Steady-clock scope — `match`, без load/construction/serialization/candidate copy.
Для self sample — p50 queries після першого query кожного pass; для PGM — median
трьох first-query times. Спершу median чотирьох after/before ratios на case,
потім median case medians на mode. Ranges — case medians, не confidence intervals.

| Mode | Cases | Median paired ratio | Case range |
|---|---:|---:|---:|
| self-global | 6 | 1.0036 | 0.9308–1.1086 |
| self-window30 | 6 | 1.0029 | 0.9882–1.0147 |
| self-window30-scale | 6 | 1.0059 | 1.0029–1.0181 |
| pgm | 13 | **0.9739** | 0.8039–1.1494 |
| pgm-scale | 13 | **1.0223** | 0.8106–1.1928 |

Ratio <1 означає швидший candidate. PGM median нижчий приблизно на 2.6%, але
PGM-scale median вищий приблизно на 2.2%; case spread широкий. Clear consistent
API gain не встановлено, **candidate відхилено**. Не заявляємо доведену regression
кожного case чи tight timing bound. Self controls мають top-k=0 і не активують
новий scorer; їхні ratios не є його speedup. Чотири pairs, warm host/cache і
code layout обмежують висновки. Старі samples не pooled; cross-date порівняння
з першим prototype або threshold-only replay не встановлює причин чи API gain.
Нових tuning variants, block sweep або додаткових runs для пошуку gain немає.

## Валідація та checkpoint

Successful preparation/characterizations/paired probe/independent finalizer —
**exit 0**. Finalizer перечитав усі raw JSONL/sidecars, schedule, coverage,
nearest-rank percentiles/ratios, archive/source/compiler/binary/input bindings.
Native Windows readback: **1359 bound evidence files / 244 unchanged source
hashes**. `artifacts/cached-cutoff-20261116/` містить frozen plan/tar/helper/tests,
commands/build logs, baseline/candidate binaries, усі paired outputs, validation
і SHA manifest, failed preflight. Extracted `baseline-source` duplicates та
finalizer stdout log виключено з evidence manifest.

Existing CMake Release binary SHA незмінний:
`836f380af6fdaa65a5387909200bc589d20a1759a4856302835e747480c7c7b8`.
Новий full runner не запускався: code/tests/runner/config незмінні. Prior
same-source/Python suite log звірено за hash/counts/success: **357 Python tests,
Debug/Release CTest 65/65 кожна**, без skips/failures. Це historical validation;
739/154 controls і 440 paired processes — нові. Нових sanitizer/NoTests/MSVC/
hardware acceptance немає. WSL kernel 6.6.87.2/i5-10300H evidence не є cold-cache,
Pi/Windows-filesystem/live latency, accuracy або calibrated CPU coverage.
Graph metadata stale at `6d8f547`; headers/implementation/callers/tests і actual
candidate diff переглянуто напряму, graph не є impact verdict.

| Binding | SHA-256 |
|---|---|
| validation.json | `52a4f6d4ede837ee14f11e14f318322a36ad33b5f94aef72187552cfc3fa6ca5` |
| evidence-sha256.json | `2c977094df5e45d803e7247f6edc4b88f7f27c7fdc464c85eaada22a3751448f` |
| Local candidate source | `b0a5792c4b4118e05eedeef763a8cc2a3ce2771de2886b7487a574ad72b62a19` |
| Before benchmark | `0afe8f9883eb485c85c383000e056d09d63f96e6e51cef0864f15df77d1b5297` |
| After benchmark | `cfd88b38820ab988b4f0ca141b5720a87aa9b483dfc454770dd12e2fa49ed2ff` |
| Current source manifest | `c6f89be0bcdd08606c0dc36c0086b8f7b3c361fd89278cbfe84b07ea75306724` |
| Prior full software log | `133df6a2dbf050f50f6048612701adca62c6dcc0528366f3f55721069d78d650` |

Наступний bounded slice — **read-only generated-loop/vectorization review**
frozen baseline і відхиленого cached prototype: ELF/disassembly та compiler
optimization reports із тими самими sources/compiler/flags. Встановити, що
code generation справді підтверджує про full/bounded loops; не видавати
instruction count або compiler report за runtime attribution. Нового candidate,
block tuning чи production kernel/cache rewrite без concrete evidence не робити.
Завершені API/profile runs для цього не повторювати. Hardware/output поза scope.
