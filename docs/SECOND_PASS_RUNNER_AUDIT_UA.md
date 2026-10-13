# Read-only audit collection runner archive

Дата: `2026-10-12`. Початковий checkpoint: `14258cd`.

`scripts/check-second-pass-runner.py` звіряє збережений archive
[collection runner](SECOND_PASS_COLLECTION_RUNNER_UA.md) із початковим request:
snapshots, список runs, admission bindings, progress і весь saved collection
report. Використовує сусідні trusted Python tools; archived executable і копії
Python є лише даними для hashing. Replay, exporter, resume, repair та hardware
не запускаються.

## Використання

```sh
python3 scripts/check-second-pass-runner.py /path/to/runner-archive
python3 scripts/check-second-pass-runner.py /path/to/runner-archive \
  --expected-plan-sha256 <independently-retained-plan-hash> \
  --expected-state-sha256 <independently-retained-final-state-hash>
```

JSON пишеться у stdout; archive read-only. Зберігайте report **поза archive**:
зайвий файл усередині порушує closed inventory. API:
`check(archive_root, expected_plan_sha256=None, expected_state_sha256=None)`.

| Status / exit | Значення |
| --- | --- |
| `verified` / `0` | Фінальний archive узгоджений; усі очікувані runs verified. |
| `replay_failed` / `3` | Archive узгоджений, але містить failed replay із all-missing score. |
| `incomplete` / `4` | Є pending/incomplete slots або незавершений запис archive; дивіться integrity/result flags. |
| `invalid` / `2` | Порушено контракт/hash/status або runner зафіксував invalid outcome; results withheld. |

`needs_review` dataset не приймається як verified runner archive: runner має
відхилити його до створення output. Exit 0 означає узгодженість archive,
не досягнення accuracy threshold або дозвіл на live output.

## Ланцюжок перевірки

1. Regular-file inventory без symlinks/devices/FIFO; external anchors за
   наявності відповідних файлів; strict JSON без duplicate keys/nonfinite.
2. Trusted `request_contract` відтворює initial manifest із `request.json`.
   Він мусить точно збігатися з `initial-manifest.json` і копією в plan,
   включно з порядком runs, IDs, splits, paths і peers.
3. Точний plan schema, execution policy і повна input hash map: request,
   initial manifest, frozen binary, шість tools, catalog inputs, configuration
   і policy кожного run. Копії tools побайтово відповідають сусіднім trusted
   sources. Config/policy повторно валідовано.
4. Final manifest може відрізнятися від initial лише парою `plan_sha256` /
   `execution_sha256` кожного run; обидва anchors з'являються разом.
   Видалення, перестановка, додавання runs або зміна peers неприпустимі.
5. Повторний [collection audit](SECOND_PASS_COLLECTION_UA.md) перевіряє весь
   catalog, splits, sealed bundles і scores. Кожен прийнятий bundle додатково
   звіряється з outer request: binary/config/policy hashes, timeout, declared
   revision та disjoint-reference policy.
6. Progress відповідає початковому порядку, manifest і свіжому audit.
   Attempted runs утворюють суцільний prefix; `exporter_calls` дорівнює їхній
   кількості. Pending не має результату, running може бути лише останньою
   спробою. Verified/failed progress мусить мати відповідний sealed результат.
7. Final state фіксує plan, manifest, saved report і collection status.
   **Весь** saved report порівнюється зі свіжим через type-sensitive JSON
   equality: metrics, coverage, hashes, diagnostics і flags.
8. Перед публікацією results повторно перевіряються всі прочитані hashes,
   inventory membership і сім trusted sources, включно з самим checker.

Outer inventory дозволяє лише заявлені snapshots, controls і collection
subtree, що має власну closed-inventory перевірку. Незавершений bundle без
anchors лишається unassessed; partial regular files не стають trusted output.

## Переривання та progress

`archive_integrity_verified`, `saved_report_reproduced`, `results_released`
стають true лише після повної узгодженої фінальної перевірки. Це можливо й
для `incomplete`, коли runner зберіг final audit після переривання: completed
runs оцінені, pending/incomplete slots лишаються у знаменниках
`collection.coverage`, `by_dataset`, `by_split`. Метрики різних policy/config
не об'єднуються в pooled accuracy.

Незавершена transaction має `incomplete` і всі три flags false:

- немає runner plan: preparation не перевірена;
- немає manifest/state/saved report або state ще не має всіх final bindings;
- state лишився running, зокрема після manifest replace перед state replace;
- є відомий temporary: `runner-plan.json.tmp`, `initial-manifest.json.tmp`,
  `runner-state.json.tmp`, `collection-report.json.tmp` або
  `collection/collection.json.tmp`.

Temporary bytes хешуються й лишаються на місці; їхній JSON не приймається.
У цьому випадку перевіряються доступні outer bindings, але повний collection
audit не виконується. Без plan перевіряються лише inventory, external anchors,
JSON parsing доступних основних controls і request schema, якщо request є;
це не підтвердження цілісності preparation. За наявності plan malformed control
object є invalid, а не еквівалентом відсутнього файлу.

Missing frozen input після записаного plan, неправильний наявний hash,
inconsistent progress або невалідний sealed bundle дають invalid. Checker не
приймає run за самою наявністю `execution.json`, не дописує anchors, не прибирає
`.tmp`, не повторює run і не створює state. Recorded `invalid` не перетворюється
на success; results withheld навіть за наявності узгоджених внутрішніх bundles.

## Перенесення, anchors та межі

Для relocation нормалізуються лише `dataset_validation.datasets[*].root`
у collection report та вкладених scores. Решта полів порівнюється точно.
`state.output` — historical display string; checker не відкриває цей шлях.

Plan hash фіксує початкові inputs/inventory, але **не** результати: узгоджене
переписування output + completion + manifest + report + state можливе без
зміни plan. Незалежно збережений final state hash додатково фіксує manifest і
report із результатами. Відсутній anchored файл не дає verified; відповідний
`external_*_anchor_checked` лишається false.

Hashes без зовнішньої довіри не доводять chronology, authorship, кількість
реальних запусків, compiler/binary-source provenance, повноту прихованих
captures чи фізичну незалежність dataset. Завжди `binary_executed=false`,
`semantic_independence_verified=false`, `chronology_authenticated=false`.
Error strings — diagnostics, не незалежний доказ причини переривання.
`observed_sha256` означає прочитані bytes, не прийняття всього archive.

Limits: 256 datasets, 4096 runs, 200000 inventory entries; controls до 4 MiB,
saved collection report до 256 MiB; успадковані limits dataset/bundle readers.
Надто великий report не приймається. Final hash checks виявляють звичайний
drift, але не забезпечують adversarial filesystem isolation чи multi-file
crash durability.

## Перевірки checkpoint

Синтетичні тести: `python3 scripts/tests/test_second_pass_runner_audit.py -v`.
Fixtures містять inert executable; API tests забороняють process launch,
runner та exporter calls. Suite включена один раз у `scripts/test-software.sh`.
Evidence: `artifacts/second-pass-runner-audit-20261012/`.

Final full WSL runner: Debug/Release CTest **64/64**, telemetry CLI **7/7**,
exporter **21/21**, collection runner **23/23** кожна; readiness **15/15**,
benchmark **5/5**, validator **31/31**, scorer **27/27**, bundle checker **24/24**,
collection checker **27/27**, новий archive checker **30/30**. Сім camera/output
flags OFF у двох caches; нові native sanitizer/NoTests builds не виконувалися.

`verify.py` двічі перевірив п'ять `*-final` archives попереднього runner
checkpoint та relocated copy: три verified, один replay_failed, два incomplete.
JSON детермінований, bytes/mtime незмінені, external plan/state anchors
збігаються. Успішні synthetic runs зберігають TP=3/FP=2/FN=3; failed run має
FN=6; expected frame-run denominators 16 або 24 зберігають усі slots.
Predictions hash успішних runs:
`9f1dd3a8319176925f8dbe11a14f63cee558901cc0be83497f5f331abd619a67`.
Replay цих archives не повторювався. Попередні 230 source hashes відрізняються
лише для software runner registration і tests README; два нові scripts дають
**232** hashes у `tested-source-hashes.json`. Підсумок: `verification.json`;
повний validation log: `software.log`.
Expected plan/state hashes для цих checks обчислено перед audit із наявних
archives: це перевірка anchor API, не незалежна історична квитанція.

Core/build/config, matcher thresholds, runner/exporter/scorer/validator та
попередні checkers не змінені. GitNexus metadata лишається на `6d8f547`;
використано пряме читання sources/callers, diff і tests.

Наступний bounded software slice — end-to-end synthetic campaign із кількома
datasets/splits і policy/config variants через actual Debug/Release runner
та цей archive checker, зі failures і expected denominators. Реальна accuracy
досі потребує незалежних повних annotated captures.

## Campaign validation — 2026-10-13

[Наскрізна синтетична кампанія](SECOND_PASS_CAMPAIGN_2026-10-13_UA.md) тепер
перевіряє цей audit із actual Debug/Release runner: три datasets/splits,
дві configurations, дві policies, 12 runs / 96 frame-run pairs, включно
з failed children, partial exporter та interruption. Production audit
і runner у цьому наступному slice не змінювалися.
