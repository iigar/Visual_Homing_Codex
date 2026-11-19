# First-checkpoint tuple refresh: candidate відхилено

Дата: `2026-11-19`. Відновлено remote-verified
`03ec0a227a052b2fc81ca51f7a30c9375d19ff52` після
[generated-loop review](GENERATED_LOOP_REVIEW_2026-11-19_UA.md).
**Єдиний local candidate відхилено: clear consistent API gain не встановлено.**
Production/tests/runner/core/config незмінні. Software-only, сім camera/output
flags OFF, без hardware/output чи subagents.

## Єдина зміна та characterization

Frozen plan: block **1024**, один candidate, чотири balanced API rounds усіх
44 scenarios. Candidate додає helper-local `first_checkpoint=true`, перевіряє
`valid/best/worst` tuple лише коли цей flag true та скидає flag після першого
non-final checkpoint. Integer cutoff decisions далі виконуються на кожному
checkpoint. Cutoff search лишається після завершення того самого first block;
для sizes ≤1024 checkpoint/search немає. Інших змін до cached helper немає.

Збережено original double/clamp predicate, impossible state, checked-domain/
IEEE/FE_TONEAREST full-score fallback, cache lifetime per match, validation
кожного eligible entry, strict winner/rounded confidence ties, normal loser
insert/pop, capacity/allocation-error prefix/recovery. Full raw scorer/diagnostics,
zero-skip, scale/bias/window/initial policy незмінні.

Baseline — current canonical production, а не відхилений cached prototype.
Archive звірено з current Git blobs; sources/headers/loaders і compiler/options
однакові. Baseline characterization виконано **до створення candidate source**.
Успішні baseline і candidate кожен пройшли **819 bit-exact RouteMatch / 234
capacity comparisons, 28 allocation-fault cases із 13 paired throws**.
До попередніх 739/154 controls додано **80 boundary comparisons** для
1023/1024/1025/2048/2049 pixels, top-k 1/5 і retained window/recovery states.
Чотири rounding modes/64 comparisons, confidence ties, malformed later entry,
wide uint64 sum та інші попередні controls збережені. Assertions ON через
`-UNDEBUG`; усі п'ять local builds warning-free.

Reviewed orchestration скопійовано в нову artifact directory з перевірених
scripts; provenance/hashes і actual diffs збережено. Старі scripts не виконували
повторно й artifacts не переписували. Нового tuning variant чи parameter sweep немає.

## Окремий counter diagnostic

Instrumented copy має callbacks після block accumulation, на checkpoint,
tuple check/update та predicate evaluation. Ordinary benchmark source/binaries
не містять callbacks і лінкують лише звичайні matcher/loader sources.
Diagnostic counters/time не входять до API timing.

**32 boundary/order self-tests**, із них **16 no-checkpoint cases**, підтвердили
відсутність eager updates і match/top-k/capacity equivalence. Після constructor
counters reset; 13 fresh PGM diagnostic processes зберегли historical full oracles.
Порядок callbacks підтверджує search лише після першого completed block.
Independent Python readback звіряє кожен raw output та counts із frozen traces.

| Work на 13 PGM coarse stages | Prior cached trace model | New instrumented copy |
|---|---:|---:|
| Helper calls / eligible entries | 7735 | 7735 |
| Non-final checkpoints | 103465 | 103465 |
| Tuple checks | 103465 | **7735** |
| Threshold updates | 809 | 809 |
| Search predicate evaluations | 18586 | 18586 |
| Bounded blocks | 105532 | 105532 |
| Full fallback calls | 65 | 65 |
| Total examined pixels | 108311040 | 108311040 |

**95730 redundant tuple rechecks усунено** у цій local реалізації. Prior column
є frozen trace/source model, не новим запуском старого prototype. Counters не є
hardware instructions/cycles і не встановлюють API gain.

## Balanced API comparison

Ті самі 19 hash/session-bound files: шість VHRS routes / 2583 Gray8 frames і
13 stop-frame PGM, original/native bytes/mtime та file set незмінні. GCC 13.3.0,
C++20 `-O3 -DNDEBUG -Wall -Wextra -Wpedantic -pthread`; native temporary copies.
Before benchmark hash збігається з попереднім canonical baseline binary.
Обидва benchmark self-tests пройшли.

**440 exact processes = 88 excluded warmups + 352 timed processes**,
**176 timed pairs**, по два before-first/after-first на case. Незмінений benchmark
має internal warmup і три emitted fresh-matcher passes/process. На variant —
**93300 timed queries** (92988 self-match + 312 PGM). Усі full normalized
historical JSONL oracles exact; вилучено лише init_us/match_us, без schema drift.

Steady-clock scope — `match`, без load/init/serialization/candidate copy. Self
sample — p50 queries після першого query кожного pass; PGM — median трьох
first-query times. Median чотирьох after/before ratios на case, потім median
case medians на mode. Ranges — case medians, не confidence intervals.

| Mode | Cases | Median paired ratio | Case range |
|---|---:|---:|---:|
| self-global | 6 | 1.0383 | 0.9159–1.1280 |
| self-window30 | 6 | 1.0030 | 0.9883–1.0120 |
| self-window30-scale | 6 | 1.0015 | 0.9794–1.0177 |
| pgm | 13 | **1.0083** | 0.8285–1.1283 |
| pgm-scale | 13 | **0.9740** | 0.9006–1.1186 |

Ratio <1 означає швидший candidate. Target mode medians змішані: приблизно
+0.8% pgm / −2.6% pgm-scale із широким case spread. **Candidate відхилено**
за відсутності достатнього consistent API gain. Self modes не активують top-k
scorer; їхній розкид не пояснює причин candidate timing. Tight regression/gain
bounds не заявлено. Old cached-prototype samples не pooled; current comparison
проти production не є matched timing ablation лише tuple-check change.
Counter reduction не доводить wall-time causality. Додаткових favorable-run
selection або tuning runs немає.

## Валідація та рішення про чергу

Setup/preparation/characterizations/counter probe/paired probe/independent
finalizer — **exit 0**. Native Windows readback: **1387 bound evidence files /
244 unchanged source hashes**. `artifacts/first-checkpoint-20261119/` містить
frozen plan/archive/helper/tests, script-clone provenance, raw counters/self-tests,
усі paired JSONL/sidecars, compiler/binary bindings і validation/SHA manifests.
Extracted baseline-source duplicates і finalizer stdout log поза SHA manifest.
Current Release binary SHA лишився
`836f380af6fdaa65a5387909200bc589d20a1759a4856302835e747480c7c7b8`.

Новий full runner не запускався: production/tests/runner/config незмінні.
Prior same-source/Python **357 Python tests / 65+65 CTest** log звірено за hash,
counts/success без skips/failures; це historical suite validation. Local controls,
counters і 440 benchmark processes — нові. Floating-status flags/enabled traps,
MSVC/sanitizer/NoTests/Pi/hardware acceptance не встановлювали. Це warm WSL/x86
evidence, не cold-cache, Windows-filesystem/live latency або accuracy claim.
Graph at `6d8f547` stale; source/headers/callers/actual diffs прочитано напряму.

| Binding | SHA-256 |
|---|---|
| validation.json | `a5d76da02278ee545609ae0846aa5527ad331a39016d009c7cd6c1748c98380c` |
| evidence-sha256.json | `ad7b7c583c58aa780a2d1f181dec158c9e60becaf0a2e652cd68dd2f62995b1e` |
| counter.json | `09130887216c0d5f8d21815afaab4e12987d8df26bf590dd309158778c6ae1df` |
| Local candidate source | `ae2f4a20d92780f69ad29ad4b1faea7347cd4ffa0001fa36f700fec1b38077dd` |
| Before benchmark | `0afe8f9883eb485c85c383000e056d09d63f96e6e51cef0864f15df77d1b5297` |
| After benchmark | `8eadc4a0bb64e8f7ea7a523b0e2f45a77e4574362ce7411b06b222e858908cf2` |
| Current source manifest | `c6f89be0bcdd08606c0dc36c0086b8f7b3c361fd89278cbfe84b07ea75306724` |
| Prior full software log | `133df6a2dbf050f50f6048612701adca62c6dcc0528366f3f55721069d78d650` |

**Partial-score candidate серію на цих fixtures завершено.** Нові variants,
block tuning/SIMD/kernel rewrite відкладено до іншого workload/profile evidence;
це не доказ неможливості lossless optimization.

Наступний bounded slice — **read-only retained non-exact top-k/window30 baseline**.
У current 44 scenarios top-k=5 PGM має window=0/один query, а window30 self-match
має top-k=0. Окремо охарактеризувати комбінацію window30/top-k5 на тих самих 13
bound PGM/route mappings: scale OFF/ON, neutral bias, initial global query +
8 retained follow-ons. First query має збігтися з existing historical match;
наступні window-limited results потребують explicit new reference/oracles і
config/schema description. Старі 44 oracles не змінювати й нові scenarios не
видавати за їхню equivalence. Freeze latency protocol лише після characterization;
це unpaced repeated-image workload, без freshness/live/accuracy acceptance або
threshold tuning. Production edits/hardware/output і новий candidate поза scope.
