# Visual_Homing: контекст і черга для наступної сесії

Початково збережено `2026-11-03` за запитом «збережи все» поверх PGM checkpoint
`42541d41258eb47b8b1d9c98bc3a0b905b6594b0`; опубліковано як `c927c93`.
Оновлено `2026-11-04` після завершення парного read-only
[path-depth study](PATH_DEPTH_SENSITIVITY_2026-11-04_UA.md) поверх опублікованого
filesystem attribution checkpoint `68c8f9b`.
Production/tests/runner/core/config незмінні; нової candidate patch немає.
Цей handoff замінює [жовтневу точку входу](SESSION_HANDOFF_2026-10-31_UA.md);
старі документи зберігають історію, evidence та ширшу функціональну чергу.

## Команда для відновлення

> Продовжуй Visual_Homing із docs/SESSION_HANDOFF_2026-11-03_UA.md.
> Прочитай AGENTS.md і docs/CURRENT_PROJECT_STATUS_UA.md, перевір HEAD,
> native Windows tracked status та remote. PGM, filesystem attribution і
> path-depth study завершені; production незмінний, candidate не реалізовано.
> Наступний bounded software slice — warm end-to-end CLI latency / worker
> VmHWM на тому самому bound archive з fixed shallow native layout.
> Наперед визнач warmup + 15 fresh workers; parent startup→JSON-output wall
> time й API-only time відокрем, запиши runtime/source/storage/depth identities.
> Збережи exact full reports/oracles/peers, bytes/mtime, fresh filesystem,
> content і mutation guards. Не кешуй checks/hashes і не повторюй завершені
> lexical/constructor experiments. Працюй локально, без Pi/camera/FC/UART/output
> і subagents. Завершений checkpoint документуй, перевір diff/tests,
> commit/push, remote SHA та чистий tracked status.

- Windows checkout: `D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex`.
- WSL checkout: `/mnt/d/LLM/ChatGPT/Codex/Visual-Homing/Visual_Homing_Codex`.
- Branch: `refactor/optimization-tech-debt`.
- Origin: `https://github.com/iigar/Visual_Homing_Codex.git`.
- Останній full-suite checkpoint: `42541d41258eb47b8b1d9c98bc3a0b905b6594b0`.
- Filesystem study відновлено з remote-verified `c927c933ae71eac6c5110591194770330e800b5e`;
  його publication commit перевіряти через live Git, не припускати dated SHA.
- Path-depth study відновлено з remote-verified `68c8f9bbe3aed63c526c90901c6a1b2c1bff49b7`;
  новий publication commit теж перевіряти live.
- Збережена база `main`: `62c772b961917347471996a157c8a6e50d9c5449`;
  rollback tag: `baseline/pre-optimization-2026-09-17`. History не переписувати.
- На вході filesystem study tracked tree чистий. Calibration/probe/finalizer
  завершилися exit 0. Нового full runner у read-only study немає; попередній
  same-source/runtime suite log повторно звірено. Production/candidate patch немає.
- Untracked `.claude/`, `.codex/`, `codex_jtzero_known_hosts` збережені локально;
  їх не додавати до commit і не видаляти автоматично.

Актуальний HEAD після збереження буде documentation commit цього handoff;
перевіряти live Git, а не вважати наведений попередній SHA завжди поточним.
«Продовжуй» означає наступний обмежений slice, а не повтор попередніх етапів.

## Завершені етапи та рішення

| Етап | Checkpoint / рішення | Evidence |
|---|---|---|
| Dataset.path character/containment overhead | `27532ce`, fresh guards збережено | [Звіт](DATASET_PATH_OPTIMIZATION_2026-10-21_UA.md) |
| Closed inventory relative paths | `48f551e`, closed inventory збережено | [Звіт](INVENTORY_OPTIMIZATION_2026-10-23_UA.md) |
| Frame / resolution / catalog scaling | `9ac0172` / `87ee902` / `5acd288` | [Frames](FRAME_COUNT_SCALING_2026-10-23_UA.md), [resolution](RESOLUTION_SCALING_2026-10-24_UA.md), [catalog](DATASET_COUNT_SCALING_2026-10-24_UA.md) |
| Path-constructor candidate | `90b48b2`, API gain не підтверджено, production залишено | [Звіт](PATH_CONSTRUCTION_2026-10-25_UA.md) |
| Large-archive baseline | `f704a2a`, fixed 12 datasets / 12 runs / 32×32 / 4 references, 8/64 frames | [Звіт](LARGE_ARCHIVE_BASELINE_2026-10-30_UA.md) |
| Fixed-archive warm latency | `e4d5a57`, warmup + 15 timed audits | [Звіт](WARM_AUDIT_LATENCY_2026-10-31_UA.md) |
| PGM characterization/cost | `42541d4`, 14 tests, production незмінний; candidate не реалізовано | [Звіт](PGM_HEADER_CHARACTERIZATION_2026-11-03_UA.md) |
| Filesystem attribution | 6 exact audits, 19 call totals звірені; production незмінний | [Звіт](FILESYSTEM_ATTRIBUTION_2026-11-03_UA.md) |
| Absolute path-depth sensitivity | 20 exact audits, 8 balanced pairs, median paired reduction 11.99%; production незмінний | [Звіт](PATH_DEPTH_SENSITIVITY_2026-11-04_UA.md) |

Path-depth study завершено: **20/20 full normalized reports/oracles/peers**,
незмінні bytes/mtime, **243 source hashes**, сім flags OFF; probe/finalizer exit 0.
Root depths **4/10**, однакова довжина **64 characters**, той самий native
filesystem/device, shared trusted source/input/runtime/cwd. Два excluded
warmups, **8 pairs** з чергуванням порядку, два окремі raw cProfiles.
Shallow швидший у **8/8 pairs**: medians **10.111/11.896 s**, median paired delta
**1.410 s**, median pair-wise reduction **11.99%**, delta range **0.025–3.388 s**.
Order-group median deltas **1.174/1.769 s** залишають host/time effects видимими.
Profiles: **939417/1465395 lstat**, різниця **525978 = 6 × 87663 archive resolves**;
інші **18 tracked totals** однакові, integrity boundaries збережені.
Це measured warm layout effect одного archive; production candidate не вводили.
Нових replay/full-suite/native sanitizer/NoTests/MSVC/hardware runs немає;
prior same-source/runtime full log reverified. Один physical copy/layout та
host variability обмежують узагальнення. Record absolute path depth/length у
наступних вимірюваннях; глобальний cwd або cached resolve не вводити.

Filesystem study: **6/6** full normalized reports/oracles/peers і bytes/mtime
незмінні, **243 source hashes** звірені, сім flags OFF. Dataset.path виконав
**87383 resolves / 1023002 lstat**, constructors — ще **496 / 4078**. Closed
inventory: **28 traversals / 3954 scandir / 68922 DirEntry.stat**, без
resolve/open/read. **87215 Dataset.read / 174430 fstat** зберігають before/after
guards; **32498 reads** належать фінальним verify_frozen. Безпечного вилучення
checks не встановлено. Усі **19 Python/C API call totals** незалежно збігаються
з raw cProfile; це не kernel syscall counts. Сума depth × resolve calls точно
дорівнює 1027080 lstat для цього layout без symlinks.

Unprofiled samples **9.832 / 10.335 / 9.942 s**, median **9.942 s**. Окремий
observer **166.410 s**, cProfile **22.641 s**, inventory cumulative **0.427 s**;
diagnostic overhead ratios **16.74× / 2.28×**. Не переносити diagnostic cost
у звичайну latency; немає candidate/cross-date speedup чи p90/p99 claim.
Нових replay executions — 0; це повтори одного archive / 768 unique pairs.

PGM tests фіксують P5/comments/ASCII whitespace, single separator/CRLF, усі
first-pixel bytes, 4096-byte header, справжній 64 MiB payload, truncations,
trailing-byte rejection, consumed-byte/pixel hashes, expected hashes,
same-size/mtime mutation, fstat guards та parser/I/O error behavior.
C++ reader поділяє CRLF convention, але не є oracle повної acceptance parity.

PGM study: **6/6** read-only audits одного relocated archive відтворили exact
full normalized baseline reports, prediction/metric/coverage/peer oracles та
незмінні bytes/mtime. Це **11137 files / 12 saved runs / 768 унікальних pairs**,
всі 11 peers/run; **нових replay executions у study — 0**.

Три unprofiled API samples: **10.526 / 11.330 / 10.011 s**, median **10.526 s**.
Окремий PGM timer: **0.600 s / 5.44%** від **11.041 s** audit. In-memory 19200
parses: median **0.470 s** із семи samples, range **0.457–0.484 s**; includes
stream/reader creation і обидва hashes, excludes filesystem/guards.
Candidate відкладено через обмежений header-only potential і пріоритети.
Це **не** paired candidate comparison або доказ неможливості малого speedup.

Warm-audit October 31: median **10.461 s**, nearest-rank p90 **11.032 s**
(14-й із 15), range **10.371–11.111 s**, VmHWM median **60.78 MiB**.
Large-archive medians для 8/64 frames: campaign **11.603/52.264 s**, audit
**2.748/10.981 s**; eight archives / 96 runs / 3456 pairs verified.
Усі ці timings — synthetic warm WSL evidence, не cold cache, Windows filesystem,
Pi/live latency, physical accuracy, process-tree RSS або p99 acceptance.
Legacy ru_maxrss має inherited parent floor; не підміняти ним current-image VmHWM.

## Остання повна валідація

На `2026-11-03` full software runner і independent finalizer — **exit 0**.
Debug/Release CTest **64/64** кожна; CLI/exporter/runner **7/21/23**,
campaign/frame/resolution/catalog/large-archive **4/6/7/7/5** на кожній збірці.
Common readiness/benchmark/validator/paths/PGM/inventory/scorer/bundle/collection/
audit: **15/5/31/12/14/12/27/24/27/30**. Загалом **357 Python tests**, без skips
чи failures. Сім camera/output flags OFF у кожному build cache.

Runtime: Python `3.12.3 (main, Aug 31 2026, 10:18:26) [GCC 13.3.0]`, executable
`/usr/bin/python3.12`, SHA-256
`e50d468e8b0adfb05733f5b87b3cff34829c4a8c1aea50c865aa8bdfe4bb150f`;
WSL kernel `6.6.87.2-microsoft-standard-WSL2`.
Це закриває попередню full-suite прогалину для поточного Python build.
Причину зміни build metadata між March/August не досліджували; cross-date
timings не є before/after. Нових native sanitizer/NoTests/MSVC/hardware runs немає.

Production Python/C++/config не змінено. З попередніх 242 source files змінені
лише runner і tests README; новий PGM test дає **243 entries** у manifest.
Independent finalizer перечитав reports/oracles, micro inputs проти bound tar,
raw pstats проти JSON, перерахував medians, звірив interpreter/source/log hashes.
Для початкового documentation-only збереження нових tests/benchmarks не було.
Наступний filesystem study мав власні шість audits і calibration/readback;
нового full runner не запускав, бо всі 243 source files і runtime ідентичні.
Попередній full log перевірено за hash, success/counts; це historical validation.

## Наступний bounded slice

1. Звірити runtime, HEAD/status/remote, локальні artifacts і trusted-source
   identities. Новий experiment робити в новій artifact directory.
2. Виміряти **warm end-to-end CLI latency / worker VmHWM** на тому самому archive,
   fixed shallow native layout; записати roots/depth/length/storage/source/runtime.
   Наперед визначити один excluded warmup + 15 fresh workers і estimator median/p90.
3. Parent wall time startup/imports/check/JSON-output відокремити від API-only
   time; oracle/snapshots/persistence з боку вимірювача — поза timing. Показати
   current worker VmHWM з imports/serialization; inherited ru_maxrss не є заміною.
   Default GC і однакові pre/post snapshots прогрівають cache; це warm study,
   fresh process не є cold-cache доказом. Global cache dropping поза scope.
4. Exact full normalized reports/oracles/peers, bytes/mtime та no replay/resume.
   CLI має використовувати тільки trusted source і не виконувати archived tools.
   Зберегти subprocess/runner stop guards, filesystem/content/mutation semantics.
5. Filesystem/path-depth stages завершені. Не кешувати checks/hashes між guards,
   не замінювати content hashing size/mtime, не вводити global chdir/cached resolve.
   Validator дозволяє contained resolved regular-file symlinks; closed inventory
   відхиляє symlinks. Не змішувати контракти і не послаблювати mutation guards.
6. Якщо candidate виправданий, потрібні characterization/regressions,
   alternating paired full-audit equivalence і виміряний API gain; за малого
   gain залишити production незмінним. Code change потребує нового full runner,
   документації, actual staged diff/check, commit/push, remote SHA, clean tracked
   state. Завершені lexical/path-constructor/PGM stages не відкривати без evidence.

## Ширша черга не втрачена

- Cold/warm methodology, latency/RSS studies: fresh workers не доводять cold
  cache; global cache dropping не входить у поточний scope.
- Незалежний annotated second pass із timestamps, positive/negative/unknown,
  splits та frozen policy. Synthetic copies, repeats і self-match не доводять
  accuracy/independence; matcher output не є ground truth.
- Matcher coarse scan/scale/cache/reparse/queue changes лише за profile і
  regressions; зберегти історичні 44 scenarios/stop-frame oracles.
- Compact global index, bounded local window, multi-chunk consumer, top-N native
  verification, multi-frame lock, global reacquisition/reset. VHIX builder ще
  не означає реалізованого search consumer.
- Endpoint dwell/invalid-gap characterization, consumer freshness, testable seams
  і log parsers. Blanket component/camera-loop rewrite не планується.
- Revision recovery/SD durability; trusted pose/frames/gates/ownership;
  route transfer/off-corridor entry; JT_Zero handoff; camera/thermal/cross-camera/
  scale envelopes; Android/GS readiness і fleet sync.
- Pi all-output-off acceptance, MSVC repair/retest, no-send hand-carried capture,
  real-FC props-off origin/Home/provider і restrained/HOVER/RTL/flight ladder
  потребують окремої авторизації. Поточний scope — software-only, fail closed.

Повні рішення й черги: [ROADMAP](ROADMAP.md), [DECISIONS](DECISIONS.md),
[PROJECT_MEMORY](PROJECT_MEMORY.md), [SESSION_LOG](SESSION_LOG.md),
[SYSTEM_DESCRIPTION_UA.txt](../SYSTEM_DESCRIPTION_UA.txt),
[handoff October 20](SESSION_HANDOFF_2026-10-20_UA.md),
[handoff October 31](SESSION_HANDOFF_2026-10-31_UA.md).
Історичні «immediate next» у milestones не перекривають поточний scope.

## Локальні evidence та робочі правила

- `artifacts/path-depth-20261104/`: probe/finalizer, 20 jobs/reports/oracles/
  results/logs, два raw pstats/JSON profiles, snapshots/source manifest,
  summary/validation, **115 bound evidence files**. Summary SHA-256
  `803850b362962cf33d9927a6c08f91bcaa95b928bb89510dca12a5fa42ce2d9c`.
  Temporary native copies прибрані після study; persisted evidence лишилося.
- `artifacts/filesystem-attribution-20261103/`: calibration/probe/finalizer,
  шість full reports/oracles/jobs/results, attribution rows, raw pstats/profile,
  snapshots, source manifest, summary/validation/logs, **49 bound evidence files**.
  Summary SHA-256 `f047e266feeef8a9475c426fa8a25018379727ebe39805f694470356b27373bf`.
  Source manifest і baseline tar hashes ті самі, що у таблиці нижче.
- `artifacts/pgm-header-20261103/`: probe, finalizer, focused/software logs,
  results/jobs/full reports/oracles, raw profile/pstats, micro samples,
  summary/validation, source manifests, `evidence-sha256.json`.
- `artifacts/large-archive-20261029/`: baseline tar, measurement/summary,
  source/software evidence, readback, validation і frame smoke.
- `artifacts/warm-audit-latency-20261030/`: prior probe/helper, results,
  summary/validation та manifests. Новий probe імпортує цей локальний helper;
  його hash bound, старий study не перезапускається й не перезаписується.
- Попередні path/inventory/frame/resolution/catalog artifacts залишені; bindings
  є у датованих reports. Ignored artifacts/builds/logs збережені **локально**,
  не в GitHub. Fresh clone отримує source/tests/docs, а не tar/logs/evidence.

При початковому PGM збереженні звірено **243 source hashes**, **45 evidence files**
за manifest, baseline tar і поточні summary/full software log:

| Binding | SHA-256 |
|---|---|
| Baseline tar | `bedb5f838258e006b378a7cc30b42b279524c58a81b5f0fb5361fd0bc279453b` |
| PGM summary | `d82734578b975b8dfc5800e744dfb456902a44d3dafd68671adbd38e1596a4e9` |
| Full software log | `d939a3e613a67f33cec9f8574084150781c8294d3cd4ccbf2cfe6da2e86aeb54` |
| Source manifest | `a565452a0cfa785d58d2bc8f2b0856032575180b4e090106fc4732c30e958cdc` |

Не запускати старі finalizers як універсальний health check: вони мають власні
source/revision bindings. Не послаблювати binding для іншого revision.
Після наступної code change стандартна WSL перевірка:

```sh
bash scripts/test-software.sh artifacts/telemetry-source-20261002/build
```

Windows Git — canonical status; не нормалізувати CRLF у всьому repo і не
запускати паралельні Windows/WSL Git index writers. Активний index.lock не
видаляти. WSL/network у sandbox можуть потребувати належного escalation.
Чекати final exit code/log; partial output не є success.

Graph metadata досі `6d8f547`: прямий source/caller/diff review — чинний
fallback. Pinned wrapper `tools/code-graph/gitnexus.mjs`; не ремонтувати graph
заради resume. Українська мова, без subagents і hardware/output роботи.
Це робочий контекст і плани з посиланнями; не дослівний експорт усіх реплік чату.
