# Visual_Homing: збереження контексту для наступної сесії

Збережено `2026-10-31` за прямим запитом користувача «збережи все».
Це documentation-only checkpoint поверх опублікованого й remote-verified
`e4d5a57a71a8db7294d00e35a77b1662c6afa76f`. Новий optimization slice під час
збереження не починати. Цей handoff оновлює точку входу після
[handoff 2026-10-20](SESSION_HANDOFF_2026-10-20_UA.md); старий документ
зберігає історію й ширшу функціональну чергу.

## Команда для відновлення

> Продовжуй Visual_Homing із docs/SESSION_HANDOFF_2026-10-31_UA.md.
> Прочитай AGENTS.md і docs/CURRENT_PROJECT_STATUS_UA.md, перевір HEAD,
> tracked status та remote. Large-archive baseline і warm-audit distribution
> завершені. Наступний bounded software experiment — PGM header parsing:
> спочатку зафіксуй поточний Python build/executable identity, підтвердь
> hotspot і охарактеризуй чинну семантику. Збережи consumed-byte hashes,
> усі fresh filesystem/content/mutation guards. Candidate приймати лише
> за exact equivalence і виміряним API виграшем; за малого виграшу залишити
> production без змін. Працюй локально, без Pi/camera/FC/UART/output і subagents.
> Завершений checkpoint документуй, перевір diff/tests, commit/push, remote SHA
> та чистий tracked status.

- Windows checkout: `D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex`.
- WSL checkout: `/mnt/d/LLM/ChatGPT/Codex/Visual-Homing/Visual_Homing_Codex`.
- Branch: `refactor/optimization-tech-debt`.
- Origin: `https://github.com/iigar/Visual_Homing_Codex.git`.
- Останній code/test checkpoint: `f704a2a63c0fada78968efd1ea5a33d4f42372ec`.
- Останній study/docs checkpoint перед цим збереженням:
  `e4d5a57a71a8db7294d00e35a77b1662c6afa76f`.
- `main`: `62c772b961917347471996a157c8a6e50d9c5449`; rollback tag:
  `baseline/pre-optimization-2026-09-17`. Main/history не переписувати.
- Перед збереженням tracked tree чистий. Незавершеної production/candidate
  patch немає; PGM implementation не починали.
- Сторонні untracked `.claude/`, `.codex/`, `codex_jtzero_known_hosts`
  збережено локально; не додавати до commit і не видаляти автоматично.

Актуальний HEAD буде documentation commit цього handoff, тому читати Git,
а не вважати наведений попередній SHA завжди поточним. «Продовжуй» означає
наступний обмежений slice, а не повтор завершених benchmark/audit етапів.

## Що вже завершено

| Етап | Checkpoint / результат | Джерело |
|---|---|---|
| Dataset.path character/containment overhead | `27532ce`, fresh guards збережено | [Звіт](DATASET_PATH_OPTIMIZATION_2026-10-21_UA.md) |
| Closed inventory relative paths | `48f551e`, symlink/nonregular/extra-file rejection збережено | [Звіт](INVENTORY_OPTIMIZATION_2026-10-23_UA.md) |
| Frame / resolution / catalog scaling | `9ac0172` / `87ee902` / `5acd288` | [Frames](FRAME_COUNT_SCALING_2026-10-23_UA.md), [resolution](RESOLUTION_SCALING_2026-10-24_UA.md), [catalog](DATASET_COUNT_SCALING_2026-10-24_UA.md) |
| Path-constructor experiment | `90b48b2`, candidate не дав API виграшу, production залишено | [Звіт](PATH_CONSTRUCTION_2026-10-25_UA.md) |
| Large-archive baseline | `f704a2a`, 8/64 frames при 12 datasets / 12 runs / 32×32 / 4 references | [Звіт](LARGE_ARCHIVE_BASELINE_2026-10-30_UA.md) |
| Fixed-archive warm latency | `e4d5a57`, warmup + 15 timed audits одного 64-frame архіву | [Звіт](WARM_AUDIT_LATENCY_2026-10-31_UA.md) |

Large-archive medians: campaign **11.603/52.264 s**, audit **2.748/10.981 s**
для 8/64 frames. Усі **8 archives / 96 runs / 3456 pairs** пройшли hash-bound
readback/relocation з exact oracles/coverage/peers і незмінними bytes/mtime.

Останній full software runner завершився exit 0 на цьому code checkpoint:
Debug/Release CTest **64/64**, CLI/exporter/runner **7/21/23**, campaign/frame/
resolution/catalog/large-archive **4/6/7/7/5** на кожній збірці; common suites
**15/5/31/12/12/27/24/27/30**. Загалом **343 Python tests**, без skips/failures.

Warm latency: median **10.461 s**, nearest-rank p90 **11.032 s** (14-й із 15),
range **10.371–11.111 s**, worker VmHWM median **60.78 MiB**. Усі **16 audits**
мають однаковий повний normalized baseline report/exact oracles. Це **один
архів / 12 saved runs / 768 унікальних pairs**, без нових replay executions.
Legacy `ru_maxrss` 132.11–142.15 MiB містить inherited parent floor; не
підміняти ним current-image VmHWM. Fresh workers і snapshots дають warm-cache
evidence, не cold cache, process-tree peak, p99 або accuracy acceptance.

Python 3.12.3 build metadata відрізняється між baseline (`Mar 23 2026`) і
latency study (`Aug 31 2026`). Причину не досліджували. Timings між датами
не є before/after; **нового full runner для цього Python build не було**.
Latency probe/finalizer exit 0; 242 source hashes незмінні, сім flags OFF.
Нових native sanitizer/NoTests/MSVC/hardware runs немає.

## Наступний bounded experiment: PGM header

1. Перевірити interpreter executable/build identity і повторно підтвердити
   hotspot на поточному runtime. Попередній profile: `pgm` 19 200 calls /
   1.673 s, `token` 76 800 / 1.003 s, `byte` 249 600 / 0.678 s cumulative
   із 25.759 s audit. Вкладені times не додаються; весь PGM parser лише
   близько 6.5%, header-only potential менший. Не обіцяти speedup наперед.
2. Прочитати `scripts/validate-second-pass-dataset.py`: `HashedReader`,
   `pgm`, `Dataset.read`; actual callers, відповідний core reader та
   `scripts/tests/test_second_pass_dataset.py` і path/mutation tests.
3. До candidate охарактеризувати P5/comments/whitespace, single separator і
   CRLF, 4096-byte header limit, truncations/trailing bytes, geometry/64 MiB
   payload bound, перший binary pixel whitespace/`#`, exact consumed-byte
   hashes та mutation/error semantics. Не припускати, що header whitespace
   можна strip-нути разом із binary raster.
4. Ізолювати parsing cost. Локальну candidate implementation випробовувати
   лише за достатнього cost; за непевної equivalence або малого potential
   зберегти no-change result. Не послаблювати rules для зручності benchmark.
5. Isolated gain підтвердити alternating paired full audits того самого
   hash-bound архіву, same runtime/path/storage, exact full normalized
   reports/oracles, всі peers, незмінні bytes/mtime, fresh checks і guards.
   Зберегти всі samples, warmup/profile окремо; source identities перевіряти.
6. Production adoption потребує релевантних regressions і нового full runner,
   actual staged diff review, документації, commit/push, remote-ref verification
   та чистого tracked tree. Відхилений path-constructor experiment не повторювати.

Validator дозволяє contained resolved regular-file symlinks; closed archive
inventory відхиляє symlinks. Не змішувати ці контракти. Не кешувати hashes
між guards і не замінювати перевірку content самим size/mtime. Trusted-source
binding старих archives зберігається; не виконувати archived toolchain.

## Уся подальша черга збережена

- Cold/warm methodology, ширші latency/RSS studies й profiling конкретних
  overhead залишаються окремими bounded slices. Fresh workers не є підставою
  для cold-cache claim; cache dropping не входить до поточного плану.
- Незалежний annotated second pass із timestamps, positive/negative/unknown,
  splits та frozen policy потрібен для accuracy. Synthetic copies/повтори й
  self-match не доводять незалежності; matcher output не є ground truth.
- Matcher coarse scan/scale/cache/reparse/queue changes лише після актуального
  profile і regressions; зберігати історичні 44 scenarios/stop-frame oracles.
- Long-route queue: compact global index, bounded local tracking window,
  multi-chunk consumer, top-N native verification, multi-frame lock і global
  reacquisition/reset. VHIX builder не означає готового search consumer.
- Characterization endpoint dwell/invalid gaps, consumer freshness contracts,
  testable seams і log parsers лишаються; завершені оптимізації не відкривати
  повторно без нового evidence. Blanket component/camera-loop rewrite не планується.
- Revision recovery/SD durability; trusted pose/frames/gates/ownership;
  route transfer/off-corridor entry; JT_Zero handoff; camera/thermal/cross-camera/
  scale envelopes; Android/GS readiness і fleet sync збережені у ширшій черзі.
- Pi all-output-off acceptance, MSVC repair/retest, no-send hand-carried capture,
  real-FC props-off provider/origin/Home і подальші restrained/HOVER/RTL/flight
  плани потребують окремої авторизації. Цей resume лишається software-only.

Деталі й незмінні рішення: [ROADMAP](ROADMAP.md), [DECISIONS](DECISIONS.md),
[PROJECT_MEMORY](PROJECT_MEMORY.md), [SESSION_LOG](SESSION_LOG.md),
[SYSTEM_DESCRIPTION_UA.txt](../SYSTEM_DESCRIPTION_UA.txt),
[повна попередня черга](SESSION_HANDOFF_2026-10-20_UA.md).
Історичні «immediate next» у milestone sections не перекривають цей scope.

## Локальні артефакти та відновлення

- `artifacts/large-archive-20261029/`: full software log/source manifest,
  `measurement/summary.json`, raw profiles і `native-evidence.tar.gz`,
  `verify.py`, `verification.json`, `finalize.py`, `validation.json`, frame smoke.
- `artifacts/warm-audit-latency-20261030/`: `probe.py`, `finalize.py`,
  `results/` із jobs/full reports/oracles/raw results, `summary.json`,
  `validation.json`, `evidence-sha256.json`, expected reports і inventory.
- Попередні path/inventory/frame/resolution/catalog artifacts залишено;
  їхні paths і bindings є у відповідних датованих reports.

Під час цього збереження повторно звірено **242 source hashes**, **90 файлів**
latency evidence за manifest, large-archive tar і full software log.
Нові tests/benchmarks для documentation-only handoff не запускали.

| Binding | SHA-256 |
|---|---|
| Large-archive tar | `bedb5f838258e006b378a7cc30b42b279524c58a81b5f0fb5361fd0bc279453b` |
| Full software log | `9b7ea0593ef7055d6a33bfda6098e952e5d226bc8e4873dd4479440f7193d900` |
| Warm-audit summary | `850f49ac95c702ce1493d8388a5d19ad07de04a2c31147857f1b0f6b18127d1c` |
| Release replay binary (baseline) | `a6f8c9f3ead1c5b1384bfae9f60ace667cc14195d2cab06f7633fccbf8ac2bd6` |

Великі artifacts/builds/logs і локальні experiment scripts ignored та
збережені **локально**, не у GitHub; новий clone їх не отримає. Source/tests/
docs опубліковані. Не видаляти evidence й не додавати credentials/indexes.
Temp paths у reports історичні; для нового experiment потрібна нова директорія.
Не перезапускати старі finalizers як універсальний health check: вони мають
конкретні revision/source bindings. Новий production source потребує нового
узгодженого evidence, а не послаблення старого binding.

Після наступної code change відповідна повна перевірка у WSL:

```sh
bash scripts/test-software.sh artifacts/telemetry-source-20261002/build
```

Windows Git — канонічний status; не нормалізувати CRLF у всьому repo,
не запускати паралельні Windows/WSL Git index writers і не видаляти активний
index.lock. WSL у sandbox повертав Access denied; за потреби користуватися
належним escalation, не обходити permissions. Failed/interrupted processes
перевіряти за final exit code/log, не лише за partial output.

Graph metadata лишається `6d8f547`: прямий source/caller/diff review — чинний
fallback. Pinned GitNexus wrapper `tools/code-graph/gitnexus.mjs`; не ремонтувати
графи заради цього handoff. Українська мова, без subagents, software-only gates.
Цей документ і посилання зберігають робочий контекст та плани; це не дослівний
експорт усіх реплік чату.
