# Контрольований запуск evaluation collection

Дата: `2026-10-06`. Продовження checkpoint `4bedae8`.

[`run-second-pass-collection.py`](../scripts/run-second-pass-collection.py)
фіксує весь запит і копії inputs перед першим викликом
[offline exporter](SECOND_PASS_EXPORTER_UA.md). Виконує очікувані runs послідовно,
по одній спробі, зберігає failed/incomplete slots та перевіряє отриману collection
існуючим [read-only checker](SECOND_PASS_COLLECTION_UA.md).

Це локальний software workflow. Використовувати лише довірений, локально
зібраний `second_pass_replay`; binary передається окремим CLI argument, не
береться із request або старого archive. Немає hardware connections, threshold
search, output enabling, автоматичних retries чи resume. Runtime/C++ і попередні
production exporter/checkers не змінені.

## Виклик

```sh
python3 scripts/run-second-pass-collection.py /inputs/request.json /results/new-run \
  --binary /local-build/second_pass_replay --code-revision DECLARED_REVISION \
  > /results/runner-report.json
```

Output має бути новою директорією **поза директорією request**. Існуючі outputs
і output symlinks відхиляються; жодні попередні результати не перезаписуються.
Request directory містить inputs; усі dataset/config/policy paths відносні до
неї та залишаються всередині. Binary може лежати в окремій build directory.
`--code-revision` — явна декларація, не binary/source attestation.

CLI exits: `verified=0`, `invalid=2`, `replay_failed=3`, `incomplete=4`,
`needs_review=5`. Exit 0 означає успішний audit всіх очікуваних runs, не
physical accuracy acceptance. JSON stdout і `runner-state.json` містять
статус runner, список runs, diagnostics, `exporter_calls`, plan/manifest/report
hashes за наявності. `exporter_calls` рахує спроби exporter, а не гарантовані
запуски OS processes: exporter може відмовити ще до child launch.

## Request v1

Точні поля кореня: `schema_version`, `collection_id`, `disjoint_references`,
`datasets`, `runs`. Version — integer 1, split policy — boolean. Dataset
entries такі самі, як у collection checker: ID, split, source path та повний
`input_sha256` map із validator. Усі IDs/input hashes мають збігатися з
джерелами; навіть pending review не приховує hash mismatch.

Run має рівно `run_id`, `dataset_id`, `configuration`, `policy`,
`timeout_seconds`. Binding для configuration/policy — рівно `path` і `sha256`.
Кожен dataset повинен мати щонайменше один run; IDs runs унікальні. Повторне
оцінювання dataset задається окремими entries. Runner не будує Cartesian
product, не вибирає best run і не добирає параметри.

Структурний приклад нижче потребує реальних hashes і повного input map:

```json
{
  "schema_version": 1,
  "collection_id": "evaluation-001",
  "disjoint_references": false,
  "datasets": [
    {
      "dataset_id": "query-001",
      "split": "development",
      "path": "datasets/query-001",
      "input_sha256": {"EVERY_VALIDATOR_INPUT_PATH": "LOWERCASE_SHA256"}
    }
  ],
  "runs": [
    {
      "run_id": "query-001-config-a",
      "dataset_id": "query-001",
      "configuration": {"path": "config/a.json", "sha256": "LOWERCASE_SHA256"},
      "policy": {"path": "policy.json", "sha256": "LOWERCASE_SHA256"},
      "timeout_seconds": 300
    }
  ]
}
```

Ліміти: JSON 4 MiB, datasets 1–256, runs 1–4096, timeout 1–3600 s на run.
Duplicate keys, зайві поля, non-finite values, bool замість integer,
malformed hashes/paths відхиляються. Config і scoring policy перевіряються
для **всіх** runs до першого export. Dataset structural/review/split gates
перевіряються спільно. Непридатна adapter geometry може бути виявлена самим
exporter пізніше; такий slot залишається incomplete з його diagnostic.

## Archive layout і порядок

```text
new-run/
  request.json                 exact copied request bytes
  initial-manifest.json        immutable full inventory, null result anchors
  runner-plan.json             immutable request/input hashes and execution policy
  runner-state.json            progress and final runner result
  collection-report.json       final read-only collection audit, when available
  frozen/
    replay                     admitted binary bytes
    0000-configuration.json    one explicit configuration per run
    0000-policy.json           one explicit policy per run
    toolchain/                 six trusted local Python sources as data
  collection/
    collection.json            full inventory; only result anchors are filled
    datasets/0000/...          complete catalog snapshots
    bundles/0000/...           existing exporter format, including partial runs
```

Нумерація paths походить із порядку request; IDs не використовуються як
filesystem names. Кожен bundle отримує **всі інші catalog datasets** як peers.
Єдина global split policy застосовується до всіх runs; exact peer/catalog
snapshots повторно перевіряються при admission.

1. Перевірити request, всі datasets/configs/policies та прочитати hash binary.
2. Створити новий output, скопіювати перевірені bytes з первісними hashes.
   Подальша зміна оригіналів не впливає на наступні runs.
3. Записати initial manifest і runner plan. Plan містить `schema_version`,
   `code_revision_declared`, `initial_manifest`, `input_sha256`,
   `request_sha256`, `execution_policy`. Map охоплює request, snapshots,
   binary/config/policy/toolchain та initial manifest; власний plan hash
   зберігається окремо у state/report. Змінені копії не переприв'язуються.
4. Записати повну collection із null result anchors і всі slots `pending`.
   Перевірити frozen collection до першого exporter call.
5. Перед спробою записати `running`, `attempted=true`. Викликати exporter
   один раз, із frozen inputs; зберегти його status/diagnostics.
6. Complete/failed bundle перевірити без повторного replay; звірити request
   dataset/peers, binary/config/policy, revision, timeout, split policy та
   status. Лише тоді вписати plan/completion hashes у collection manifest.
7. Перевірити первісні hashes, controls і раніше прийняті bundles; після
   всіх спроб запустити collection checker та зберегти audit/state.

Незмінний runner plan зберігає весь початковий inventory. Mutable controls
оновлюються через same-directory temporary file, file flush/fsync і replace;
перед заміною попередні bytes мають збігтися з відомим hash. Чужу зміну
control file runner не перезаписує. Atomic file replacement не є транзакцією
між кількома файлами або гарантією directory durability після power loss.

## Помилки й переривання

- Failed/timeout/launch-error/invalid-output replay, який exporter закрив
  коректно, отримує sealed `replay_failed` і all-missing score. Наступний run
  може виконатися; жодного retry того самого run немає.
- Exporter exception або незакритий результат лишає slot `incomplete`:
  partial artifacts зберігаються, anchors null, metrics не вигадуються.
  Після перевірки незмінності shared inputs наступний slot може виконатися.
- Внутрішньо узгоджений bundle з іншою policy/config/binary/revision не
  приймається: state `invalid`, anchors не заповнюються. Collection checker
  бачить цей slot як incomplete, а runner повертає суворіший `invalid`.
- Зміна shared inputs/controls/trusted tools, раніше sealed bundle або помилка
  збереження progress зупиняє подальші exports і дає `invalid`.
- Оброблений `KeyboardInterrupt` лишає поточну спробу incomplete, наступні
  slots pending; за можливості зберігає audit і state. Hard kill, повторне
  переривання під час cleanup, OS/disk failure можуть лишити останній успішно
  записаний state (`running`) чи `.tmp`. Автоматичного recovery/resume немає.

Standalone checker можна викликати для `new-run/collection` з фінальним
manifest hash. Він зберігає усі expected frame-run denominators. Він перевіряє
collection, але **не** зв'язок outer request/runner plan/progress із нею:
окремий read-only audit outer archive — наступний bounded software slice.
Після переривання не заповнювати null anchors автоматично на підставі файлів.

## Валідація та межі

23 integration tests у
[`test_second_pass_collection_runner.py`](../scripts/tests/test_second_pass_collection_runner.py)
запускаються проти Debug і Release: actual C++ oracle TP=3/FP=2/FN=3,
plan-before-launch, незмінність originals, frozen-copy mutations, exact peers,
request/schema/review/split gates, real nonzero/timeout child injections,
launch/malformed-output failures, partial export, interruption і pending slots,
controls/previous-result drift, final-audit outer-input mutation, disk-write failure, wrong-policy rejection,
repeat/relocation і CLI exits 0/2/3/4/5.

Full WSL suite: Debug/Release CTest **64/64**, telemetry CLI **7/7**, exporter
**21/21**, runner **23/23** кожна; readiness **15/15**, benchmark **5/5**,
validator **31/31**, scorer **27/27**, bundle checker **24/24**, collection
checker **27/27**. Сім camera/output flags OFF. Evidence:
`artifacts/second-pass-collection-runner-20261006/`.

Final review відтворив пропуск повторної outer-input перевірки після collection
audit: mutation `frozen/0000-policy.json` могла залишити status verified.
`final-guard-before.log` фіксує failing regression; runner тепер виконує guard
і після audit. Остаточний full suite містить цей regression. Accepted persisted
archives мають suffix `-final`; попередні development artifacts не підмінені.

Два persisted Release CLI collections мають однакові predictions і runner plan;
кожен містить два runs по 8 frames. Controlled failed, partial та interrupted
scenarios збережені окремо, без вилучення slots. Повторні read-only checks
дають однаковий JSON і не змінюють bytes/mtime. Frozen request inputs незмінені.
З 228 попередніх source hashes змінені runner suite registration і tests README;
два нові scripts дають **230** hashes. Native code/build/config і попередні
production Python tools незмінені; нові sanitizer/NoTests збірки не виконувалися.
Graph metadata досі `6d8f547`; evidence — direct source/callers/diff/tests.

Runner забезпечує описаний порядок у звичайному локальному виконанні, але
hashes/state не є незалежним доказом історичної chronology, авторства, повноти
реальних captures або відсутності selection bias. Немає sandbox для binary,
FS locking/adversarial race protection, compiler/environment/dynamic-library
attestation чи process-tree containment. Успадковані source-time zero-work clock,
synthetic Guided telemetry та unsupported outcomes exporter не змінені.
`semantic_independence_verified` і `chronology_authenticated` лишаються false.
Точність на реальних даних потребує незалежних повних annotated captures;
threshold tuning і hardware/output enabling у цей slice не входять.
