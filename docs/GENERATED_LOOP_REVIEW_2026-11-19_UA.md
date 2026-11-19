# Generated-loop/vectorization review

Завершено `2026-11-19`, розпочато `2026-11-18`. Відновлено remote-verified
`ddedf0be4d95ff799f8a4165f841aa6aae5c1dd0` після
[відхиленого cached-cutoff candidate](CACHED_CUTOFF_CANDIDATE_2026-11-16_UA.md).
**Production/tests/runner/core/config незмінні. Нового candidate немає;
нових matcher/API/profile processes — 0.** Software-only, сім camera/output
flags OFF, без hardware/output чи subagents.

## Прив'язка до виміряних binaries

Frozen plan і evidence — `artifacts/generated-loop-20261118/`. Original before/
after executables, canonical sources/headers, compiler/options узято з
`artifacts/cached-cutoff-20261116/preparation.json`; усі **1359 prior evidence
hashes** звірено. GCC driver/cc1plus/collect2 та binutils paths/versions/hashes
збережені. Flags: C++20 `-O3 -DNDEBUG -Wall -Wextra -Wpedantic -pthread`;
target reports: **x86-64 / tune generic / SSE2 enabled / AVX disabled**.

Дві повні diagnostic rebuilds використовують ті самі input paths/order/flags,
додаючи лише vectorization report та новий output path. **Обидва повні ELF
byte-for-byte однакові з original binaries**, `.text` також однакові. Original
executables не замінювали й не запускали; ELF tools лише читають їх, objcopy має
окремий output path. Matcher verbose assembly з тими самими options після
assembly дає **byte-identical object** до звичайного `-c` на обох variants.
Отже reports/source-comment assembly прив'язані до фактично виміряного codegen,
а не до іншої debug/instrumented реалізації.

Independent finalizer сам декодує ELF64 sections/symtab, зіставляє nm addresses/
sizes, original `.text` bytes і encoded backward branches. Жодного flat gprof
label або stale graph для встановлення matcher symbol не використано.

## SIMD збережено

GCC reports для raw inner loop (baseline line 86 / candidate line 87) і bounded
inner loop (candidate line 119) підтверджують **16-byte та 8-byte vectorization**.
Reports `missed` для enclosing outer loops із control flow не означають, що
внутрішній pixel loop scalar. У linked ELF основний body має той самий pattern:

```text
movdqu, movdqu, add, psadbw, paddd, cmp, jne
```

| Loop | Original virtual address | Main body | Evidence |
|---|---:|---:|---|
| Before full raw | `0xc5b8` | 7 instructions / 29 bytes | `.L49`, raw line 86 |
| After full raw fallback | `0xc968` | 7 instructions / 29 bytes | `.L106`, raw line 87 |
| After bounded, inlined into match | `0xdd50` | 7 instructions / 28 bytes | `.L290`, bounded line 119 |

**Raw main-loop bytes до/після однакові**, SHA-256
`00fc023b530e8d879479d8e31c1a7aa3e9bce55cb5b0bfc5e5c4240561b5fe60`.
Різниця 29/28 bytes не означає меншу кількість instructions чи швидшу роботу.
Full raw function size — **678 bytes** в обох binaries; scaled scorer — **732**.
Bounded helper не має окремого linked symbol: його source-comment body всередині
`Gray8RouteMatcher::match`. Hot match symbol зріс **2421 → 3732 bytes**; це static
size, не instruction-cache miss, CPU-time або regression measurement.

## Block boundaries і tuple checks

Assembly показує block bounds **65536** для full raw та **1024** для bounded,
horizontal reduction після vector loop, 8-byte/scalar tail paths і cutoff
control/search. Frozen 13-image scalar/prefix traces із
`artifacts/cutoff-20261115/` перечитано без нового matcher run. Model для
non-exact coarse stage, 600 entries/image, 16000 pixels, neutral bias/top-k=5:

| Trace-derived work | Full baseline | Cached prototype |
|---|---:|---:|
| Logical pixels | 124800000 | 108311040 |
| 16-byte main-loop iterations | 7800000 | 6769440 |
| Block finalizations | 7800 | **105597** |
| Required 8-byte/scalar pixel tails | 0 | 0 |

Cached total: **105532 bounded blocks + 65 full-score fallback blocks** (перші
п'ять entries кожного image). Це model-defined block finalizations, не hardware
instructions/counters. Усі spans кратні 16 (1024, final 640 або full 16000),
тому tail pixel paths для цих traces не потрібні; branch/setup overhead може
лишатися. Більша кількість block reductions/checks — structural fact, не доказ
причини попередніх API ratios.

Знайдено конкретну надлишкову роботу в cached prototype: після кожного non-final
block source lines 126–127 перевіряють `valid/best_distance/worst_confidence`.
У assembly це valid test, **дві `ucomisd` sites** і conditional branches перед
integer cutoff check; backedge `.L303 → .L322` повертає до наступного block і
тієї самої перевірки. Snippet: `after/per-block-tuple-check-assembly.txt`.

`best/worst` arguments сталі протягом одного helper call; cutoff fields після
першого checkpoint або вже рівні їм, або оновлені search. Інших writes/callbacks
між blocks немає. Тому повторні tuple checks після першого checkpoint того
самого entry не можуть змінити рішення на перевіреному finite-score path:

- **103465** non-final checkpoints;
- **7735** eligible entries з checkpoint;
- **95730** повторних tuple-check opportunities понад first checkpoint;
- **809** справжніх threshold epochs / **18586** search predicate evaluations.

Це counts виклику logical check, не кількість усіх branches/ucomisd executions
(short-circuit paths різні). Перевірку можна дослідити лише на first checkpoint,
зберігши момент cutoff search; **не переносити search раніше за перший block**.
Це concrete removable check, але його вплив на API latency ще не виміряно.
Новий candidate або manual SIMD rewrite у цьому slice не робили.

## Валідація й межі

Probe/analysis/independent finalizer — **exit 0**. Initial readback зупинився на
надто короткому excerpt: `$65536` лишився за snippet window, але був у повній
raw-function assembly. Script/log збережені в `readback-failed-01/`; перевірку
прив'язано до повного відповідного function range. Compiler outputs не змінено.

Native Windows readback: **96 bound evidence files / 244 unchanged source
hashes**. Збережено original/rebuilt ELF reports/disassembly/text dumps, matcher
objects/assembly/vectorization reports, snippets, tools/commands, analysis/
validation manifests і failed readback. SHA manifest виключає себе й finalizer
stdout log. Input bytes/mtime/session mapping, prior sources/binaries і сім
OFF flags незмінні. Prior current Release binary SHA —
`836f380af6fdaa65a5387909200bc589d20a1759a4856302835e747480c7c7b8`.

Нових API/profile/full-suite runs немає. Prior same-source/Python **357 Python
tests / Debug+Release CTest 65/65 кожна** log звірено за hash/counts/success,
без skips/failures. Prior cached-candidate 440 exact processes/176 pairs —
historical bound evidence; повторно не запускали. Compiler reports, static
instruction pattern/size й trace model не є runtime attribution, speedup,
memory-traffic/cache-miss measurement чи calibrated CPU coverage. Це GCC13/
WSL x86 evidence, не Pi/MSVC/Windows-filesystem/cold-cache/live/accuracy acceptance.
Graph at `6d8f547` stale; declarations/implementation/callers/diffs прочитано напряму.

| Binding | SHA-256 |
|---|---|
| validation.json | `94b12ca0947e057207479f78fc9aed57f679b3afadd22ae42bdd0b7fe6c74faf` |
| evidence-sha256.json | `b460da008353bd9b360ef99646b81274be6db7476fc6a86d9ad2725430ffffdc` |
| analysis.json | `72dba6c9baac08b432235d48b0bd7148d881bcd7fdc3c6b29e534e8c81edfd4c` |
| Current source manifest | `c6f89be0bcdd08606c0dc36c0086b8f7b3c361fd89278cbfe84b07ea75306724` |
| Prior full software log | `133df6a2dbf050f50f6048612701adca62c6dcc0528366f3f55721069d78d650` |

Наступний bounded slice — **characterization first-checkpoint tuple refresh**
перед одним local candidate: tuple comparison лише на першому non-final checkpoint
entry, search у тому самому checkpoint, subsequent integer cutoff decisions
незмінні. Зберегти block=1024, lazy behavior для sizes ≤1024, cutoff lifetime,
unsupported-domain/rounding fallback, validation, ties, insert/pop capacity/errors,
zero-skip/scale/full diagnostics. Baseline controls перед candidate; adoption
потребує нового exact paired 44-case API gain та full runner. Статичний removable
check не є дозволом на block tuning/inverse/epsilon або production rewrite.
