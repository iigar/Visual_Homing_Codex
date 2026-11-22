# Retained non-exact top-k5/window30: characterization і latency baseline

Завершено `2026-11-21`; artifact directory почато як
`artifacts/retained-window-20261120/`. Відновлено remote-verified
`5d74d7236f8da8ad61a87bc9037d48f0b142f7fd`. Production, tests, runner, core,
config і сім camera/output flags незмінні. Нового candidate немає.

## Workload і новий oracle contract

Current historical PGM scenarios мають top-k5, global window0 і один query/pass;
self window30 scenarios мають top-k0. Нова комбінація — ті самі **13 bound
PGM/route mappings × scale OFF/ON = 26 scenarios**, window radius30, top-k5,
minimum_confidence0, scale radius1, neutral bias, direction/yaw/initial bounds
disabled. Один fresh matcher виконує **initial global query + 8 retained calls**,
без reset. Frame bytes/id/timestamp однакові в усіх дев'яти calls; PGM loader
зберігає historical id0/time0. Це unpaced repeated-image workload.

Загальний inventory незмінний: шість routes / 2583 frames / 13 PGM, 19 input
files. Ці 13 PGM mappings використовують дві routes по 600 entries, 160×100
Gray8. Повні filenames, route associations, case IDs, input hashes/mtime і
session binding записані в `characterization.json` та `preparation.json`.

Новий diagnostic schema version1 має modes `retained-window30` і
`retained-window30-scale`, explicit config та `top_capacity`. Characterization
додає integer witness rows; measurement додає init/match timing. Це explicit
schema/config/sequence drift. Перший match, після вилучення лише доданого
`top_capacity`, exact historical match row. Follow-ons мають **26 окремих нових
oracles** у `oracles/`; старі **44 oracle files не змінені**. Їхню еквівалентність
для нових window-limited queries не припускали.

## Незалежна characterization

Probe компілює current production matcher із canonical Git-bound core tar,
без production edits або counters. Independent C++ reference використовує
scalar uint64 full raw sums і row-major scaled mapping, повний sort top-k замість
incremental insertion/pop. Він відтворює strict first winner, rounded-confidence
later-tie-first top-k, fixed coarse-winner refinement neighborhood, strict scale
improvement та last-index commit лише для valid match. Кожен RouteMatch double
і top-k double звірені bit-exact. Capacity reference не передбачає; actual
capacity записана й перевірена на сталість усередині sequence та між runs.

Independent Python replay перевіряє raw selection/top-k/state, integer witness
bounds, усі scale choices й overlap counts через незалежний axis-count/lround
replay. Він не перераховує pixel sums із зображень: їх independently обчислює
scalar C++ reference. FE_TONEAREST зафіксовано; це не нова four-rounding-mode
кампанія. Synthetic reference checks: **40 cases / 360 queries**, route sizes
1/2/33/65/79, shapes17×11/32×32, duplicates/ties, changing perturbed queries,
scale OFF/ON, valid і reject branches; **180 invalid matches** перевіряють, що
rejected match не комітить last index.

Усі **26 characterization processes / 234 queries** пройшли reference і Python
readback. Усі PGM queries non-exact. Witness totals: **26256 raw scores / 7776
scaled scores**; це reference work counts, не production CPU attribution.
У **208 retained calls** window sizes становлять **31/43/51/52/61 entries**,
winner та confidence незмінні від initial match. Top-k змінюється від initial
global output у **16 follow-ons**. Final refined winner відрізняється від coarse
у **18 queries** (два scale scenarios × 9). Такі зміни кандидатів очікувані від
window contract і збережені в нових oracles.

## Зафіксоване вимірювання

`timing-plan.json` створено після успішної characterization і до measurement,
із hashes reference/oracles/binary/scripts. Один excluded whole-sequence warmup
для кожного case, потім **три fixed timed batches** усіх 26 cases у зафіксованому
порядку. Кожен process має fresh matcher; окремий warmup process не переводить
timed matcher у retained state до initial query.

Разом **104 exact measurement processes**: 26 excluded warmups і 78 timed
sequences, **78 timed initial queries + 624 timed retained queries**. Усі outputs
exact новим oracles, включно з top-k/capacity. Input loading, constructor,
reference work, top-k copy й JSON serialization поза steady-clock interval;
timer охоплює match return/assignment. У measurement mode reference не працює.
Результати серіалізуються після всіх дев'яти calls. Native temporary input copies
hash/mtime перевірені до й після; extra samples або favorable selection немає.

Для кожного case initial median має три samples; retained median і nearest-rank
p95 — 24 samples (8×3). Таблиця показує **медіану 13 case statistics**, а не pooled
latency або різницю stage medians. Значення у мікросекундах.

| Mode | Initial global median | Retained median | Retained p95 |
|---|---:|---:|---:|
| raw | 697.593 | 31.7275 | 55.916 |
| scale | 1454.456 | 812.5625 | 1064.001 |

Ranges case medians: initial **548.418–1098.725 raw / 1164.234–1645.897 scale**;
retained **15.974–49.5305 raw / 520.325–971.041 scale**. Case p95 ranges
**34.226–116.284 raw / 586.424–2259.136 scale**; host variability залишається
видимою. Три sequences/case не встановлюють production tail guarantees.

Це baseline різних workload phases, не before/after speedup. Initial global і
retained search мають різний обсяг роботи та allocation/cache state. Різниця
raw/scale timings мотивує наступний diagnostic slice, але не встановлює частку
CPU конкретного helper або вартість coordinate preparation.

## Перевірка та provenance

Characterization і timing exit0; independent finalizer exit0 повторно звірив
witness replay, fixed schedule, усі raw outputs, нові/historical oracles та
самостійно перерахував median/p95. **244 current source hashes**, canonical core
Git blobs, compiler, probe source/binary, original 19 input bytes/mtime і session
identity незмінні. Build warning-free. Сім BOOL flags OFF у Debug і Release;
official Release benchmark hash незмінний.

Новий full software runner не запускали: production/tests/runner/core/config
незмінні. Hash-bound prior **357 Python tests / Debug і Release CTest65/65** log
перевірено на тих самих 244 sources, Python executable hash/version; failures
або skips немає. Нових native MSVC/sanitizer/NoTests/hardware runs немає.
Compiler — prior bound GCC; Python3.12.3, WSL2 kernel6.6.87.2, Intel i5-10300H.
Це warm native-WSL workload, не Windows filesystem/Pi/cold-cache evidence.

Local bound evidence: **306 files**, без duplicate `baseline-source/` і finalizer
stdout. Native Windows readback звірив усі 306 evidence files, 244 sources,
19 inputs, 44 historical oracles та 148 local links у семи docs. Основні bindings:

| Artifact | SHA-256 |
|---|---|
| `validation.json` | `bbd4e6f62e006c0e4cf38b6443abf13ba633db1e164f77d0d4d024f673f9998f` |
| `evidence-sha256.json` | `8b4d3d3cbaeeb9c48b93c99cf158054fa9a2678f43a0966048addd5fdb0a5920` |
| source manifest | `c6f89be0bcdd08606c0dc36c0086b8f7b3c361fd89278cbfe84b07ea75306724` |
| prior full-suite log | `133df6a2dbf050f50f6048612701adca62c6dcc0528366f3f55721069d78d650` |

Evidence залишається local ignored artifacts; commit містить звіт і canonical
docs. Sandbox helper після daemon recovery повертав Win32 error193; approved
escalated commands виконали необхідну локальну роботу без system repair. Перший
Python import зупинився до build через reserved `pass` keyword; failure log/source
збережені в `syntax-preflight-01/`, запис виправлено, syntax перевірено до build.
Повторної timing кампанії для виправлення цього preflight не було.

Graph metadata досі stale/advisory `6d8f547`; використано пряме читання matcher,
headers, callers, tests і actual diff. Clean graph verdict не заявляється.
Freshness/live/accuracy acceptance, annotated coverage, thresholds, hardware і
output authority цим workload не перевіряються й не змінюються.

## Наступний bounded slice

**Read-only retained-window scale-work attribution** на frozen 26 scenarios:
окремий diagnostic copy для production raw/scaled call counts, logical pixel /
overlap counts і coordinate-preparation work. Перед counters — зафіксований
plan та source/binary/oracle identities; counters мають зберегти всі дев'ять
matches/top-k/capacity і synthetic boundary/state controls. Diagnostic runs
відокремити від latency; counts не подавати як CPU shares або latency savings.
Поточний baseline не повторювати без нової причини. Новий candidate можливий
лише після конкретного cost evidence й окремої characterization/API перевірки.
Partial-score series залишається закритою; ці repeated images самі по собі не
є підставою відновлювати block/cutoff/SIMD tuning. Software-only/output gates
та filesystem/content/mutation guards збережені.
