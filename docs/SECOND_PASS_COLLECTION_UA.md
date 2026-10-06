# Перевірка evaluation collection без запуску replay

Дата: `2026-10-06`. Продовження checkpoint `103ecd3`.

[`check-second-pass-collection.py`](../scripts/check-second-pass-collection.py)
перевіряє наперед перелічені datasets і runs як одну collection. Окремий
[bundle checker](SECOND_PASS_BUNDLE_CHECKER_UA.md) бачить лише embedded peers;
collection додає повний явно заданий каталог, зовнішні до bundles hashes
plan/completion та облік runs, які не завершилися або ще не створені.

Це read-only audit: архівні executable і Python copies не виконуються.
Matcher, thresholds, runtime, hardware та output gates не змінені.

## Виклик і статуси

```sh
python3 scripts/check-second-pass-collection.py /absolute/path/to/collection \
  --expected-manifest-sha256 RETAINED_LOWERCASE_SHA256 > /outside/collection-report.json
```

Аргумент — директорія з `collection.json`. Звіт треба писати поза collection:
новий файл усередині порушить її closed inventory. Опційний manifest hash
має походити з окремо збереженої довіреної копії, щоб виявити переписування
самого manifest. Checker не створює manifest, не підбирає datasets, не запускає
replay, не перепідписує змінені файли та не поновлює runs.

| Exit | Status | Значення |
|---|---|---|
| 0 | `verified` | Каталог пройшов перевірку; кожен очікуваний run завершений, зв'язаний hashes і його score відтворено. |
| 2 | `invalid` | Помилка manifest/catalog/inventory або хоча б один invalid run. |
| 3 | `replay_failed` | Усі runs перевірені, але щонайменше один містить failed replay з all-missing predictions. |
| 4 | `incomplete` | Щонайменше один run не має обох anchors або відповідних файлів; invalid runs відсутні. |
| 5 | `needs_review` | Каталог структурно узгоджений, але review не завершено; runs не оцінюються. |

Каталог перевіряється до runs. Для оцінених runs пріоритет статусів:
`invalid` → `incomplete` → `replay_failed` → `verified`. Exit 0 не є accuracy
acceptance або дозволом на physical output.

## Manifest v1

JSON має рівно такі поля; `schema_version` — integer `1`,
`disjoint_references` — boolean, `collection_id` — непорожній string.
Нижче структурний приклад: позначення hashes і `input_sha256` треба замінити
реальними значеннями, тому цей приклад не є готовим fixture.

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
      "path": "bundles/query-001-config-a",
      "plan_sha256": "LOWERCASE_SHA256_OF_PLAN_JSON",
      "execution_sha256": "LOWERCASE_SHA256_OF_EXECUTION_JSON",
      "peer_ids": []
    },
    {
      "run_id": "query-001-config-b",
      "dataset_id": "query-001",
      "path": "bundles/query-001-config-b",
      "plan_sha256": null,
      "execution_sha256": null,
      "peer_ids": []
    }
  ]
}
```

- `datasets`: 1–256 entries, унікальні IDs; split рівно `development`,
  `validation` або `test`. Кожна entry містить точний повний validator input
  map, включно з `dataset.json`, frames/labels, reference, capture config,
  evidence та originals за наявності. Усі hashes — 64 lowercase hex digits.
  Результат validator має відтворити ID, split і весь map без зайвих/пропущених
  bindings. Catalog snapshots потрібні й для runs, які ще не створені.
- `runs`: 1–4096 entries, унікальні `run_id`; кожен dataset має хоча б один
  очікуваний run. Повторне оцінювання dataset дозволено через різні run IDs
  і bundle paths. У sealed bundle ID має точно збігтися з manifest.
- `peer_ids`: точний набір embedded peers цього bundle, без дублікатів,
  власного ID або IDs поза каталогом. Порядок не суттєвий. Empty list явно
  стверджує відсутність embedded peers. Кожен embedded dataset/peer має
  збігатися з catalog snapshot за split і повним input map.
- `plan_sha256` зв'язує inputs/config/policy/arguments; `execution_sha256`
  зв'язує завершення та всі output hashes. Completion hash без plan hash
  заборонений. `null` явно означає незакритий запис і завжди дає `incomplete`:
  checker не приймає наявні файли автоматично. Non-null hash перевіряється,
  якщо відповідний файл існує, навіть коли інший anchor відсутній.
- Відсутність pinned plan/completion або всієї директорії bundle —
  `incomplete`; невідповідність наявного файла pinned hash — `invalid`.
  Незакритий run не отримує score, FP/FN чи вигаданих observation rows.

JSON обмежений 4 MiB; duplicate keys, non-finite numbers, невідомі поля,
bool замість integer, malformed hashes та nested/path aliases відхиляються.
Paths — portable relative POSIX, без абсолютних, `.`/`..`, порожніх components,
colon/comma/backslash/control characters або whitespace на краях components.
Dataset/bundle roots не перекриваються між собою чи з `collection.json`;
також заборонені case aliases для перенесення між Linux і Windows.

## Повнота й cross-split перевірка

Inventory має не більше 200000 files/directories, без symlinks, FIFO та інших
нерегулярних файлів. Дозволені тільки manifest, повний каталог, потрібні parent
directories та явно перелічені bundle subtrees. Extra datasets, bundles,
root files і empty directories поза дозволеними roots відхиляються.
Для sealed bundle діє окремий точний closed inventory checker.
Всередині незакритого bundle дозволені partial regular files: їхній вміст
не оцінюється й не видається за перевірені predictions.

[Dataset validator](SECOND_PASS_VALIDATOR_UA.md) отримує **весь каталог** одним
викликом, незалежно від наявних bundles/peers. Він перевіряє session/group IDs,
cross-split query/original hashes і pixels, query/reference reuse;
`disjoint_references=true` також відхиляє shared references. Ця глобальна
політика може бути суворішою за записану в окремому bundle: embedded snapshots
все одно мають точно збігатися з каталогом. Shared references при `false`
видимі у validator report. Semantic independence залишається недоведеною.

## Звіт і coverage

`runs` зберігає порядок manifest та окремий status/errors для кожного run.
Лише `verified`/`replay_failed` rows мають `bundle` з повним перевіреним score.
Failed replay зберігає усі explicit missing observations і FN за scorer policy;
його не прибирають із report і він не може дати exit 0.

`coverage`, `by_dataset`, `by_split` показують:

- `expected_runs` та `runs_by_status` для всіх очікуваних runs;
- `expected_frame_run_pairs` і `frame_run_pairs_by_status`: число **stored
  frames × очікуваних runs для dataset**, включно з unscorable frames;
- `all_runs_verified`: true лише коли кожен очікуваний run — `verified`.

Це облік виконання, не pooled precision/recall і не число унікальних capture
frames. Missing/incomplete/invalid runs мають відомий catalog denominator,
але не отримують вигаданих метрик. Метрики різних policies/configurations
залишаються окремими, без автоматичного усереднення або selection best run.

За локальної помилки одного run інші перевірені results можуть залишитися у
звіті (`results_released=true`, `catalog_verified=true`), але collection status
залишається nonzero. За global catalog/inventory/review failure або зміни manifest,
catalog, прочитаних bytes чи trusted tools, виявленої фінальною перевіркою, results withheld:
немає score/coverage, `results_released=false`.

`observed_sha256` містить прочитані bytes, включно з файлами invalid runs;
сам по собі цей map не означає успішну перевірку їхніх bindings. Partial files,
які checker не читав, у нього не входять. `toolchain_sha256` фіксує п'ять
adjacent trusted scripts; bundled code не імпортується. Початкові hashes та
inventory повторно перевіряються після всіх runs. Порядок JSON deterministic
на незмінному шляху; абсолютні display roots можуть відрізнятися при relocation.

## Докази та межі

27 standard-library synthetic tests у
[`test_second_pass_collection.py`](../scripts/tests/test_second_pass_collection.py):
readonly bytes/mtime і no-process guards, повторні runs з різними policies,
повні/failed/missing/unsealed runs, усі failure reasons, completion/manifest
anchors, schema/hash/path limits, closed inventory, symlinks/FIFO, cross-split
leakage поза embedded peers, review, exact catalog/peer snapshots, mutation
during checks, relocation і deterministic CLI exits 0/2/3/4/5.

WSL Debug/Release: **64/64** CTest, telemetry CLI **7/7** та exporter **21/21**
кожна; readiness **15/15**, benchmark **5/5**, validator **31/31**, scorer
**27/27**, bundle checker **24/24**, collection checker **27/27**. Сім camera/
output flags OFF. Evidence: `artifacts/second-pass-collection-20261006/`.

Три historical actual-exporter bundles перевірено в окремих collections:
їхній однаковий `run_id` не дозволяє оголосити їх різними runs однієї
collection. Копія collection після relocation також пройшла перевірку.
Кожна CLI перевірка повторена з однаковим JSON і незміненими bytes/mtime;
synthetic TP=3/FP=2/FN=3 збережено. Додатковий expected missing run у копії
збільшує denominator до 16 frame-run pairs та дає `incomplete`, а complete
run зберігає власні 8 frames. Архівні binaries не запускалися.

Core/build/config і production validator/scorer/exporter/bundle checker
незмінені; із попередніх 226 source hashes змінені runner та tests README,
додано collection checker/test — 228 hashes. Нові native sanitizer/NoTests
збірки не потрібні для цього Python-only slice; Windows/MSVC не перевірялися.
Graph metadata досі на `6d8f547`; використано direct source/callers/diff/tests.

Manifest визначає очікувану повноту, але не доводить, що до нього внесли всі
реальні captures/configurations. Post-hoc hashes, навіть зовнішні до bundle,
не доводять pre-run chronology, авторство, незалежність ground truth або
відсутність selection bias. Evidence anchors цієї сесії знято перед audit,
вони не є історично незалежними attestations. Немає FS locking чи захисту
від adversarial transient changes, compiler/dynamic-library pinning або
physical accuracy acceptance. `semantic_independence_verified` та
`chronology_authenticated` завжди false.

Наступний bounded software крок — контрольований collection runner, який
заздалегідь фіксує dataset/config/run inventory, викликає існуючий exporter
і зберігає failed/incomplete slots без неявних retry чи вилучення. Реальний
незалежний annotated dataset лишається необхідним для accuracy; hardware,
output enabling і threshold tuning у цю чергу не входять.

Оновлення `2026-10-06`: [контрольований collection runner](SECOND_PASS_COLLECTION_RUNNER_UA.md)
реалізовано. Він зберігає initial inventory й snapshots до exporter calls;
актуальний наступний крок — read-only audit outer runner archive.
