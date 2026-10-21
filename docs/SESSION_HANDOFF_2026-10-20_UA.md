# Visual_Homing: handoff для нової сесії

Оновлення `2026-10-21`: описаний нижче path/containment slice завершено;
див. [результати та наступний inventory slice](DATASET_PATH_OPTIMIZATION_2026-10-21_UA.md)
і [current status](CURRENT_PROJECT_STATUS_UA.md). Цей handoff зберігає
попередній checkpoint та повну чергу; path optimization не починати повторно.

Збережено `2026-10-20` за прямим запитом користувача після переривання
попередньої спроби збереження. Це документаційний checkpoint: нову
оптимізацію, зміну policy чи hardware роботу під час збереження не починати.

## Старт наступної сесії

> Продовжуй Visual_Homing із docs/SESSION_HANDOFF_2026-10-20_UA.md. Прочитай
> AGENTS.md та docs/CURRENT_PROJECT_STATUS_UA.md, перевір HEAD/status і remote.
> Останній кодовий checkpoint — 658220f; наступний bounded software slice —
> скоротити overhead portable paths/containment у Dataset.path, спираючись на
> baseline 12/24/48 runs. Збережи свіжі resolve/stat/read/hash і всі mutation
> guards. Працюй локально, без Pi/camera/FC/UART/output та без subagents.
> Підтвердь еквівалентність, виміряй before/after, виконай повну software suite,
> онови документацію, commit/push і звір remote SHA та чистий tracked status.

- Windows repo: `D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex`.
- WSL repo: `/mnt/d/LLM/ChatGPT/Codex/Visual-Homing/Visual_Homing_Codex`.
- Гілка: `refactor/optimization-tech-debt`.
- Origin: `https://github.com/iigar/Visual_Homing_Codex.git`.
- Останній кодовий checkpoint: `658220f0ce0c6ac5c5b9675df452917bf45f49a9`;
  remote SHA перевірено перед збереженням. Цей handoff публікується поверх
  нього окремим documentation commit; актуальний HEAD щоразу читати через Git.
- `main`: `62c772b961917347471996a157c8a6e50d9c5449`; rollback tag:
  `baseline/pre-optimization-2026-09-17`. Не переписувати main/history.
- Перед збереженням tracked tree чистий; незавершеної optimization patch немає.
  Збережені сторонні untracked `.claude/`, `.codex/`, `codex_jtzero_known_hosts`
  не додавати, не видаляти й не переносити автоматично.

«Продовжуй» означає наступний обмежений крок із цього checkpoint. Не починати
з нуля й не повторювати старі закриті audits. Старий
[handoff 2026-10-01](SESSION_HANDOFF_2026-10-01_UA.md) зберігає історію;
його тодішній next source/mode step уже завершено. Дати й claims усередині
історичних milestone sections не є поточною інструкцією.

## Що завершено і де лежать контракти

| Робота | Стан та документ |
| --- | --- |
| Telemetry source/mode | Явна nonzero sysid/compid authority, unsupported heartbeat → Unknown; [контракт](TELEMETRY_SOURCE_CONTRACT_2026-10-02_UA.md) |
| Runtime timing/freshness | Межі stages і evaluation time, без rebasing source timestamps; [контракт](RUNTIME_TIMING_CONTRACT_2026-10-02_UA.md) |
| Доступні дані | [Аудит](DATASET_AUDIT_2026-10-03_UA.md) не підтвердив повного незалежного annotated second pass |
| Dataset/scoring | [Validator](SECOND_PASS_VALIDATOR_UA.md), [scorer](SECOND_PASS_SCORER_UA.md): exact source/hash/split/policy contracts, missing/unknown і явні denominators |
| Replay/export | [Exporter](SECOND_PASS_EXPORTER_UA.md), [bundle checker](SECOND_PASS_BUNDLE_CHECKER_UA.md): frozen inputs, actual local binary, failed replay без partial credit, read-only audit |
| Collection/execution | [Collection checker](SECOND_PASS_COLLECTION_UA.md), [runner](SECOND_PASS_COLLECTION_RUNNER_UA.md): explicit request, усі peers, один attempt/run, без resume/retry/overwrite |
| Outer archive | [Audit](SECOND_PASS_RUNNER_AUDIT_UA.md): request/initial/final bindings, progress prefix, saved-report reproduction, без виконання archived code |
| End-to-end campaign | [Матриця](SECOND_PASS_CAMPAIGN_2026-10-13_UA.md): actual Debug/Release, 3 splits × 2 configs × 2 policies, 12 runs / 96 frame-run pairs, failure/interruption/relocation oracles |
| Scaling baseline | [Вимірювання](SECOND_PASS_SCALING_2026-10-18_UA.md): 12/24/48 runs, fresh workers, native WSL storage; оптимізацію production ще не починали |

Slot — запис одного заздалегідь запланованого запуску. Перевірене переривання
лишає всі 12 записів: 4 verified, 2 incomplete, 6 pending. Audit рахує pending
як incomplete: 32 verified + 64 incomplete frame-run pairs. Це облік повноти,
не автоматичний повтор чи гарантія відновлення після довільної втрати живлення.

## Останні результати та межі доказів

Медіани трьох вимірювань із checkpoint `658220f`:

| Runs | Campaign API, с | Audit API, с | Worker RSS campaign/audit, MiB |
| ---: | ---: | ---: | ---: |
| 12 | 4.476 | 1.156 | 31.36 / 32.25 |
| 24 | 11.881 | 2.623 | 32.11 / 35.82 |
| 48 | 37.617 | 4.383 | 34.59 / 43.25 |

Python 3.12.3, WSL2, i5-10300H, native `/tmp`, warm caches. Dataset size,
resolution 2×2 і три splits сталі; зростає тільки run count. Час — API call,
без imports/fixture setup/oracles/persistence. RSS — high-water mark самого
Python worker із imports; replay children і сумарний tree peak не виміряні.
Три повтори не дають performance guarantee; sample 24-run audit 4.192 с
залишено в наборі. cProfile pass вимірювався окремо, не входить до медіан.

48-run profile: `guard()` 147 calls / 60.143 с cumulative із 81.312 с runner;
`Dataset.path()` 269582 calls / 54.944 с cumulative; 2628372 lstat calls.
Часи вкладених функцій перекриваються. Immutable inventory росте після кожного
admitted bundle й повторно перевіряється, створюючи квадратичний компонент
роботи. Audit profile: inventory 100 calls / 4.098 с; Dataset.path 22583 /
4.469 с із 11.387 с. Самі guards прибирати не можна.

Останній повний software runner: Debug/Release CTest **64/64**, telemetry CLI
**7/7**, exporter **21/21**, runner **23/23**, campaign **4/4** кожна;
readiness **15/15**, recorded-route benchmark **5/5**, validator **31/31**,
scorer **27/27**, bundle **24/24**, collection **27/27**, outer audit **30/30**.
Сім camera/output flags OFF. Нових sanitizer/NoTests/MSVC/hardware runs для
benchmark slice не було. 234 core/scripts/config hashes зафіксовано.

11 archives / 312 replay runs / 2496 frame-run pairs пройшли oracles і
повторний audit після hash-bound tar readback/relocation без зміни bytes/mtime.
Це related synthetic fixtures, не physical independence або accuracy.
Chronology/authorship і binary-source attestation не підтверджуються.

## Наступний bounded slice: path/containment overhead

1. Прочитати `scripts/validate-second-pass-dataset.py`: `resolved`,
   `Dataset.path/read/artifact`; `scripts/export-second-pass.py`:
   `binding/verify_frozen`; runner `guard`; inventory у bundle/collection/audit.
   Звірити фактичні callers та tests; graph використовувати лише як підказку.
2. Зафіксувати чинну семантику portable paths, root containment, absolute/dot/
   parent/control/backslash/colon paths, case/alias behavior, missing/nonregular
   files, symlink escape/cycles і mutation між reads. Validator використовує
   fresh strict resolve + containment; це не blanket ban на кожен внутрішній
   symlink. Окремий closed archive inventory прямо відхиляє symlinks. Не
   змішувати ці контракти й не змінювати policy під виглядом оптимізації.
3. Додати необхідні characterization/regressions і найменшу еквівалентну зміну
   Python path overhead. Алгоритм ще не обрано; benchmark не є доказом, що
   будь-яка конкретна заміна containment безпечна на всіх потрібних платформах.
4. Зберегти fresh resolve/stat/read/hash на кожній чинній межі, hashes саме
   прочитаних bytes, всі guard/admission/final-audit checks і перевірку раніше
   прийнятих bundles. Не кешувати content hashes між guards, не замінювати
   content verification самим mtime/size, не послаблювати inventory.
5. Виконати релевантні path/mutation тести та повний software runner. Звірити
   source identities, predictions, scores, failure/incomplete/pending coverage,
   no-execution audit і relocation. Незмінність output/freshness gates обов'язкова.
6. Повторити ту саму 12/24/48 матрицю, Release binary і storage scope із новим
   output directory; бажано свіжий baseline поруч для host variance. Порівняти
   all samples/median/range, worker RSS та окремий profile. Не вилучати повільні
   або невдалі samples; improvements визнавати лише за перевіреної еквівалентності.
7. Actual staged diff review, `git diff --cached --check`, docs, commit/push,
   remote SHA і чистий tracked status. Якщо еквівалентність або виграш не
   підтвердяться, зберегти negative evidence і не приймати production change.

Зміна trusted production Python bytes робить старі frozen tool copies
несумісними з новим checker. Не послаблювати цю перевірку заради benchmark.
Для історичного audit потрібен відповідний перевірений source checkout;
не імпортувати й не виконувати довільний archived toolchain. Новий measurement
створює нові archives із поточними trusted sources.

## Подальша черга оптимізації та функціональних робіт

1. Після path slice знову профілювати. Repeated inventory/containment, report
   copying/serialization і validator/scorer повтори — кандидати для окремих
   виміряних кроків, не дозвіл прибрати guards або переписати всю evaluation систему.
2. Розширення performance evidence: окремо frame count/resolution, dataset count,
   великі archives, cold/warm cost, RSS і latency distributions. Не переносити
   поточні 2×2/8-frame числа на реальні маршрути, Pi чи Windows filesystem.
3. Реальна якість: незалежний повний second pass із timestamps, ground truth,
   positive/negative/unknown frames, splits та зафіксованою policy. Наявні
   route/PGM придатні для characterization/equivalence, не для підгонки thresholds
   за self-match. Отримання hardware captures — окремо дозволений етап.
4. Matcher performance: coarse scan, scale kernels, cache/reparse/queue costs
   лише після нового profile/regression. Історичні 44 benchmark scenarios та
   реальні stop-frames зберегти як відповідний oracle, коли зачіпається matcher.
5. Long-route architecture: compact global index, bounded local tracking window,
   multi-chunk consumer, top-N native verification і multi-frame lock. VHIX
   builder не означає готового search consumer. Межі RAM/queues і fail-closed
   поведінка зберігаються; ці кроки не входять до поточного path slice.
6. Спрощення: конкретні testable seams/дублювання після characterization,
   окремі shell log parsers зі своїми fixtures. Не робити загальний component
   manager чи blanket rewrite camera loop і не міняти hardware wrappers побічно.

Уже завершені lazy edge-cache, precise scale early exits, coordinate blocks,
raw partial sums, CMake registration і progress/endpoint/readiness extraction
не відкривати знову без нового evidence. Invalid gap може зберігати endpoint
dwell history; route-only quality не має самостійного age gate. Різні consumers
мають різні timestamp/confidence/yaw/scalar вимоги. Джерела:
[offline integration](OFFLINE_INTEGRATION_VALIDATION_2026-09-26_UA.md),
[freshness](DOWNSTREAM_FRESHNESS_VALIDATION_2026-09-26_UA.md).

Повна функціональна черга й незмінні рішення збережені в
[SYSTEM_DESCRIPTION_UA.txt](../SYSTEM_DESCRIPTION_UA.txt),
[ROADMAP](ROADMAP.md), [DECISIONS](DECISIONS.md),
[PROJECT_MEMORY](PROJECT_MEMORY.md), [SESSION_LOG](SESSION_LOG.md) і розділі
«Функціональний план готової системи» старого handoff. Зокрема: revision
recovery/SD durability; trusted pose/frames/gates/ownership; route transfer та
off-corridor entry; reacquisition/reset contracts; JT_Zero handoff; camera/
thermal/cross-camera/scale envelopes; Android/GS readiness; fleet sync.
Pi all-output-off acceptance, MSVC repair/retest, no-send capture,
real-FC props-off provider/origin/Home acceptance та подальші restrained/
HOVER/RTL/flight plans мають окремі gates й авторизацію. Історичні «immediate
next» у milestone sections не дозволяють виконувати їх у software resume.

## Артефакти, команди та пастки середовища

Основний local evidence: `artifacts/second-pass-scaling-20261018/`:

- `baseline/summary.json`, `runner.profile.*`, `audit.profile.*`;
- `baseline/native-evidence.tar.gz`: всі samples, copied sources/binary,
  inputs, archives, jobs, reports і per-archive SHA/mtime inventories;
- `verify.py`, `verification.log/json`, `tested-source-hashes.json`;
- `software.log`, `final-check.py/json`, `publication.txt`.

Tar SHA-256: `ac12ae1909622685b978de65f0463a3ea286c4ce002ee4ecd2173c3e8d821852`.
Software log SHA-256: `35341034067d4e695934ea7c42c7e9f25946dcefa9d57432c8c5a548a494a80f`.
Release binary SHA-256: `a6f8c9f3ead1c5b1384bfae9f60ace667cc14195d2cab06f7633fccbf8ac2bd6`.

Попередні acceptance archives: `artifacts/second-pass-campaign-20261013/accepted/`
(Debug/Release complete, Release failed/interrupted/relocated, 3420 files);
root-level exploratory archives там не входять до acceptance. Інші докази
зберігаються в датованих `second-pass-*`, `telemetry-source-20261002`, runtime
та matcher artifact directories, перелічених у відповідних звітах.

Великі archives/builds/logs ignored і залишаються **локально**, автоматично
до GitHub не завантажуються; новий clone їх не матиме. Source/tests/docs —
у Git. Не видаляти artifacts як «сміття». Historical temp paths у reports не
є live dependencies. `final-check.py` прив'язаний до pre-commit HEAD/staging,
тому його не запускати як універсальний health check після нового commit.

Із WSL у repo, після наступної code change:

```sh
bash scripts/test-software.sh artifacts/telemetry-source-20261002/build
python3 scripts/tests/benchmark_second_pass_campaign.py \
  --binary artifacts/telemetry-source-20261002/build/Release/second_pass_replay \
  --output artifacts/second-pass-scaling-new --runs 12 24 48 --repeats 3 \
  --work-dir /tmp
```

Результати benchmark не писати прямо на `/mnt/d` під час вимірювань;
probe сам працює на native WSL storage й зберігає tar поза timed intervals.
Output має бути новим. Для цього docs-only handoff tests не перезапускаються:
перевірити source hashes, links/diff і publication, не видавати старий run за новий.

Windows Git — канонічний `status`; WSL може показувати CRLF-only diffs.
Не нормалізувати весь repo. Не запускати Windows/WSL Git index mutations
одночасно: повільний WSL refresh може тримати index.lock; спочатку дочекатися
свого процесу, не видаляти lock навмання. WSL `rg` відсутній у попередній сесії;
Windows `rg` доступний. Windows Python був Store alias; використовувався WSL.

Graph metadata лишався на `6d8f547`; freshness перевіряти перед використанням.
GitNexus/Graphify — navigation, не safety evidence; UNKNOWN/empty-ID/CRITICAL
не замінюють direct source/diff/tests. Поточний AGENTS дозволяє fallback.
Pinned workflow: `npm ci --prefix tools/code-graph`,
`node tools/code-graph/gitnexus.mjs`; не `npx gitnexus@latest` і не старий launcher.
Не починати graph repair заради docs-only збереження.

Спілкування українською, короткі фактичні updates, без subagents за замовчуванням.
Після переривання перевірити статус і забрати final output запущених команд.
Публікація завершена лише після commit/push, незалежного `git ls-remote` і
final tracked status. Цей handoff із посиланнями зберігає робочий контекст,
рішення, докази й усі черги; він не є дослівним експортом усіх реплік чату.
