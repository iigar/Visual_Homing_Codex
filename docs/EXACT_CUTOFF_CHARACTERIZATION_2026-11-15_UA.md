# Exact integer cutoff і threshold epochs

Дата: `2026-11-15`. Відновлено remote-verified
`9bd2c5e61f60da2fa24474cce8999afbd7d7f86f` після
[відхиленого block-1024 candidate](PARTIAL_SCORE_CHARACTERIZATION_2026-11-14_UA.md).
**Read-only characterization завершено; нового matcher candidate немає.**
Production/tests/runner/core/config незмінні. Software-only, сім camera/output
flags OFF, без hardware/output чи subagents.

## Точний predicate та domain

Досліджено той самий predicate для partial integer numerator `q`:

```text
den = double(full_pixel_count) * 255.0
lower = double(q) / den
reject = lower >= best && clamp(1.0 - lower, 0.0, 1.0) < worst
```

За сталих positive finite `den` і thresholds це monotone predicate по `q`:
rounded distance не спадає, rounded confidence не зростає. Якщо predicate true
на maximum numerator, binary search знаходить перший true numerator `cutoff`;
інакше cutoff явно impossible. Далі рівнозначна перевірка `possible && q>=cutoff`.
Search викликає оригінальний double/clamp predicate, без inverse/epsilon.
Confidence equality не дозволяє skip; strict winner і rounded top-k ties збережені.

Diagnostic domain: `1 <= pixel_count <= UINT64_MAX/255`, product перевіряється
до multiplication. Midpoint `lo+(hi-lo)/2` не переповнюється; impossible має
окремий bool, не `max+1` sentinel. Є guards для zero count і двох oversized counts.
Це domain local algorithm, не нове production input restriction. У майбутньому
unsupported domain потребує full-score fallback, не нового throw/rejection.
Перевірено IEEE double/FE_TONEAREST на GCC 13.3.0 x86_64 без fast-math; інших
compiler/rounding-mode acceptance цим slice не встановлено.

C++ contract: **9685 cases / 7168176 numerator comparisons**. **3120 cases**
exhaustively перевіряють усі numerators для 1–16 pixels. Large domains: 16000,
4097×4113, 2^32 та два counts біля UINT64_MAX/255; tests охоплюють uint32 sum,
2^53 rounding, maximum uint64, nextafter thresholds, zero-confidence ties,
infinite best та 5000 deterministic random cases. **869 impossible cases**,
три domain guards. Independent Python readback перевірив **6857183 numerator
comparisons**, усі cutoff values/search counts, exhaustive small domains і
large-domain boundaries. Probe sets мов відрізняються; counts не є unique samples.

## Bound traces та epoch lifetime

Ті самі шість VHRS routes / 2583 Gray8 frames і 13 stop-frame PGM, 19 bound files,
explicit session mappings. Один fresh diagnostic process/image. Current trusted
loaders читають native copies; scalar full sums і block-1024 prefix sums
пораховані заново. Усі **7800 entries** точно збігаються з попередніми witnesses,
усі final RouteMatch fields/top-k/order/capacity — з historical oracles.
Немає schema/behavior normalization або нових independent captures.

Cutoff обчислюється lazy на першому non-final block після зміни exact best/worst
tuple, reset для кожного image. Під час одного epoch він перевикористовується;
full scoring, validation чи insert/pop тут не оптимізуються. Python незалежно
відтворив кожен epoch і winner/top-k prefix, every direct/cached decision,
search-evaluation count та nonnegative prefix bounds.

| Work на 13 images | Count |
|---|---:|
| Eligible non-final block checks | **103465** |
| Threshold epochs / cutoff updates | **809** (2–129/image) |
| Original predicate evaluations у cutoff searches | **18586** |
| Proven losing entries | **5668** |
| Full / examined logical pixels | **124800000 / 108311040** |

Cached path все ще виконує 103465 integer comparisons і epoch branches,
додатково до searches. Це не скорочення всіх operations на 82%.
Logical pixels ті самі, що в попередньому candidate; нового kernel немає.

## Вартість threshold-only replay

Frozen plan: **1024 trace replays/batch**, два excluded warmup batches/variant/image,
**8 balanced pairs/image**, разом **104 pairs / 208 timed batches**. Чотири
direct-first і чотири cached-first на image; order залежить від image/round parity.
Кожен batch містить усі captured checks одного image; checksum decisions exact.

Steady-clock timing охоплює iteration над in-memory Event vector. Direct variant
виконує predicate на кожному prefix; cached — epoch branch, binary search update
та integer comparison. Counter instrumentation search evaluations у timing
вимкнено template parameter. No-inline replay functions і compiler memory barrier
перед кожним повтором запобігають hoisting повторних calls; checksum споживається.
Pixel reads/accumulation, validation, insert/pop, scale refinement, input/output
та matcher state handling поза timing. Це **threshold replay, не `match` API**.

Median image paired ratio **0.7993**, case range **0.6086–0.9874**. Спершу median
8 cached/direct ratios на image, потім median 13 image medians. Median image
batch costs /1024: **15.762 µs direct / 12.576 µs cached**. Ratio medians і ratio
of medians відрізняються; near-one case не доводить стійкий gain. Ranges не є
confidence intervals. Event layout, repeated warm inputs і compiler впливають
на цей microbenchmark; жодного production API speedup або причин попереднього
slowdown не встановлено. Дані не pooled із попередніми paired API runs.

## Evidence і межі перевірки

Successful probe/finalizer **exit 0**. Перша спроба завершилася на emitter schema
assertion: local output помилково мав `frame_id` замість `timestamp_ns`, match/top-k
values збігалися. Її plan/source/binary/raw output/log збережені в `failed-01/`;
виправлений emitter зберігає exact historical schema, assertion не послаблювали.
Final build warning-free. Нових sanitizer/NoTests/MSVC/Pi/hardware runs немає.

Native Windows readback: **50 bound evidence files / 244 source hashes**.
`artifacts/cutoff-20261115/` містить frozen plan, source/compiler commands/hash,
contract JSONL, усі entry/event/timing rows, run/validation manifests і failed
attempt. `evidence-sha256.json` виключає себе й finalizer stdout log.
Compiler — GCC 13.3.0, C++20 `-O3 -DNDEBUG -Wall -Wextra -Wpedantic -pthread`.
WSL kernel 6.6.87.2, i5-10300H; це warm synthetic/local evidence, без cold-cache,
Windows-filesystem, live-latency, accuracy або calibrated process-CPU claims.

Sources, input bytes/mtime, session bindings і сім OFF flags незмінні. Existing
CMake Release benchmark SHA лишився `836f380af6fdaa65a5387909200bc589d20a1759a4856302835e747480c7c7b8`.
Нових full-workload 44 API runs або full runner немає: production незмінний.
Prior same-source/Python suite log звірений за hash/counts/success: **357 Python,
Debug/Release CTest 65/65 кожна**, без skips/failures; це historical validation.
Stale graph at `6d8f547` не є impact verdict; declarations/implementation,
benchmark/callers і local diagnostic переглянуто напряму.

| Binding | SHA-256 |
|---|---|
| validation.json | `243ee82c879c20e447ed4046a42e9e19cb2c7199fe833e789aec235c69b2c8b2` |
| evidence-sha256.json | `1964e605197c9acf36833bcd1adbf1ed46c802e998ed7bd1024adee0a654a8cc` |
| run.json | `d2e689d1d895c0427782d3290f0be1dc310e2c429fbb6a7077d81d02ad2439d1` |
| Current source manifest | `c6f89be0bcdd08606c0dc36c0086b8f7b3c361fd89278cbfe84b07ea75306724` |
| Prior full software log | `133df6a2dbf050f50f6048612701adca62c6dcc0528366f3f55721069d78d650` |

Наступний bounded slice — **один local cached-cutoff candidate** на тому самому
block=1024, до production edits. Cutoff state local per match, neutral bias,
positive best/full nonempty top-k, checked domain/full-score fallback. Baseline
characterization перед candidate; зберегти validation, strict/rounded ties,
insert/pop capacity/allocation failures, diagnostic APIs, zero-skip/scale/window
policy. Нове порівняння всіх 44 API scenarios із current baseline обов'язкове:
не порівнювати лише з відхиленим candidate. Threshold-only gain може бути замалим;
за відсутності API gain production лишити. Code change потребує нового full
runner. Block sweep, epsilon/inverse, broad redesign та hardware/output поза scope.
