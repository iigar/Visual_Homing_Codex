# Characterization lossless partial-score rejection

Дата: `2026-11-14`. Відновлено remote-verified
`a254d83212cd4d9741e87cd03f43091841ac4347` після
[stop-frame CPU attribution](STOP_FRAME_CPU_ATTRIBUTION_2026-11-12_UA.md).
**Єдиний локальний candidate відхилено: API gain не підтверджено.**
Production/tests/runner/core/config незмінні; characterization і prototype
залишені в ignored artifacts. Сім camera/output flags OFF, software-only scope,
без hardware/output чи subagents.

## Контракт і characterization

До запусків зафіксовано `plan.json`: один candidate, block **1024 pixels**,
13 PGM feasibility cases, чотири balanced timing rounds усіх 44 scenarios.
Baseline archive створено з Git із `core.autocrlf=false`; кожен extracted файл
звірено з canonical Git blob. Обидва executables напряму збирають ті самі
matcher/route-signature/replay sources та headers, без змішаних бібліотек.

Prototype застосовується лише за configured bias=0, positive best coarse
distance і заповненого nonempty top-k. Після кожного non-final block:

```text
lower = double(partial_sum) / (double(full_pixel_count) * 255.0)
reject iff lower >= best_distance
       and clamp(1.0 - lower, 0.0, 1.0) < worst_top_k_confidence
```

Nonnegative remaining differences не можуть зменшити distance. Strict winner
comparison зберігає першого equal-distance winner. Rounded confidence equality
не дозволяє rejection: пізніший equal-confidence candidate вставляється першим.
Full structural validation кожного eligible entry лишається перед scoring.
Суми: uint32 block, uint64 total; denominator і фінальне нормування ті самі.
Surviving candidates/winner мають exact full score. Full raw scorer, diagnostic
APIs, zero-skip, scale refinement, bias/window/initial policy не змінені.

Прямий review виявив observable semantics: `insert_top_candidate` завжди
вставляє candidate, потім робить pop, навіть для очевидного loser. Пропуск
insertion міг би змінити capacity та allocation failure/state behavior.
Prototype повертає proven losing lower bound і зберігає звичайний insert/pop.
Це не optimization top-k insertion. Getter повертає const vector reference;
capacity та prefix після exception перевіряються окремо.

Baseline characterization виконано до створення candidate. Обидва пройшли
той самий assert-enabled executable (`-UNDEBUG` після `-DNDEBUG`): **675 bit-exact
RouteMatch comparisons, 90 capacity comparisons, 28 paired allocation-fault
cases, із них 13 paired throws**. Є forced-full comparator із незаповненим top-k,
same-K inactive-bias comparator, windows/initial/rejected-state/direction/scale
controls, confidence-boundary ties, malformed later entry/repeated exception,
recovery state та 4097×4113 worst-sum >uint32/zero-confidence ties.
Це bounded characterization, не exhaustive доказ усіх конфігурацій/allocators.
GCC `-Wmismatched-new-delete` warnings від malloc/free global-new fault scaffold
збережені в build logs; warning-free або sanitizer acceptance не заявлено.

## Feasibility окремо від timing

Ті самі 19 bound files: шість VHRS routes / 2583 Gray8 frames і 13 stop-frame PGM,
explicit session mappings із inventory. Original/native bytes/mtime й input set
звірено. Нових captures або незалежних annotated samples немає.

Для 13 PGM кожен із 600 entries незалежно повністю пораховано scalar oracle і
shadow partial scorer. Збережено per-entry full/partial sums, examined pixels,
thresholds і rejection witnesses. Python finalizer іншим способом відтворює
кожен winner/top-k prefix; worker звіряє final historical oracle і capacity.

| Logical coarse work | Pixels |
|---|---:|
| Full scoring | 124800000 |
| Bounded scoring | 108311040 |
| Reduction | **13.2123%** |

Це logical pixel reads scorer, не hardware counters або API speedup. Diagnostic
full-score/oracle work не входить у timing. У pgm-scale той самий coarse stage;
scaled refinement потенційно обмежує загальний gain, але тут окремо не профілювався.

## Paired API timings і рішення

Обидва unprofiled Release binaries: GCC 13.3.0, C++20, `-O3 -DNDEBUG -pthread`
та `-Wall -Wextra -Wpedantic`. Один excluded warmup process/variant/case;
чотири rounds із case/round parity, два before-first і два after-first на case.
**176 timed pairs / 352 timed processes + 88 excluded warmups = 440 processes**.
Усі full normalized historical JSONL oracles однакові, вилучено лише timing fields;
schema, winner, candidates/order та coverage збережені. Обидва self-tests пройшли.

Незмінений benchmark вимірює steady-clock `match`, без load/init/serialization;
кожен process має internal warmup і три emitted passes. Для self scenarios
sample — p50 queries після першого query кожного pass; для PGM — median трьох
first-query times. Спершу median чотирьох after/before ratios на case, потім
median case ratios на mode. Ranges — між case medians, не confidence intervals.

| Mode | Cases | Median paired ratio | Case range |
|---|---:|---:|---:|
| self-global | 6 | 0.9700 | 0.8528–1.0165 |
| self-window30 | 6 | 1.0044 | 0.9889–1.0263 |
| self-window30-scale | 6 | 1.0174 | 1.0000–1.0292 |
| pgm | 13 | **1.0124** | 0.8647–1.1420 |
| pgm-scale | 13 | **1.0271** | 0.8984–1.2466 |

Ratio <1 означає швидший candidate. На цільових non-exact PGM немає consistent
API gain; mode medians дещо повільніші, case variability велика. Control global
median не є gain нового gate: self workloads мають top-k=0 і його не активують.
**Production candidate не приймається.** Не заявляємо доведену regression кожного
case чи неможливість іншої lossless implementation. Repeated floating division/
branches — можливе джерело overhead, але причину цим experiment не атрибутовано.
Один fixed block size не підбирали після результатів; другого candidate немає.

Це warm WSL/x86 evidence на i5-10300H/kernel 6.6.87.2, не cold-cache, Pi,
Windows-filesystem, live latency, accuracy, tail guarantee або production speedup.
Prior CPU/wall anomaly не використовується: timing тут steady wall, без profiler
і без calibrated process-CPU claims. Чотири pairs не дають tight timing bounds;
старі controls не pooled із цими даними.

## Перевірка checkpoint

Prepare, feasibility/candidate characterization, paired probe і independent
finalizer завершилися **exit 0**. Native Windows readback перевірив **1361 bound
local evidence files** та всі **244 current source hashes**. Sources, input
bytes/mtime і сім OFF flags незмінні. Evidence
`artifacts/partial-score-20261114/` містить frozen plan/tar, source/helper/tests,
compiler commands/hashes, raw witnesses, усі paired JSONL/sidecars/logs,
`validation.json` і `evidence-sha256.json`; extracted baseline-source duplicates
і finalizer stdout log виключені з evidence manifest.

Новий full runner не запускався: code/tests/runner/config незмінні. Попередній
same-source/Python full log звірено за SHA/counts/success: **357 Python tests,
Debug/Release CTest 65/65 кожна**, без skips/failures. Це historical suite
validation; локальні characterization/paired checks — нові. Нових native
sanitizer/NoTests/MSVC/hardware runs немає. Graph metadata stale at `6d8f547`;
declarations/implementation/callers/tests і candidate diff переглянуто напряму,
graph не є impact verdict.

| Binding | SHA-256 |
|---|---|
| validation.json | `9f69597ce7bce4d655387dc92b777f78917175ca43183f4388960b56f5aeb70d` |
| evidence-sha256.json | `fd23c4e9605beefd6813a2ceb9f9a53bbfb0dfb89c4fb9894e9d16d9dc841d84` |
| Local candidate source | `a2394b51f0c31943e5ece8e8aaaf7ad1e466f036a10b54b392d72e6da340a8ea` |
| Current source manifest | `c6f89be0bcdd08606c0dc36c0086b8f7b3c361fd89278cbfe84b07ea75306724` |
| Prior full software log | `133df6a2dbf050f50f6048612701adca62c6dcc0528366f3f55721069d78d650` |

Наступний bounded slice — **read-only characterization exact integer cutoff і
threshold epochs**: чи можна amortize exact rounded rejection predicate між
змінами best/worst threshold без floating division на кожному block. Дослідити
monotone integer-numerator search із тим самим double/clamp predicate, boundary
ties/overflow і вартість cutoff updates. Approximate inverse/epsilon не вводити.
Це hypothesis, не другий candidate: спочатку exactness і cost feasibility;
production edit потребує окремого API gain та нового full runner. Завершені
zero-skip/kernels/cache/policy/thresholds/output gates зберегти.
