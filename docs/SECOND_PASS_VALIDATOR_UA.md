# Offline validator повторного проходу, v1

Реалізовано `2026-10-04` за [контрактом даних](SECOND_PASS_DATASET_CONTRACT_UA.md).
Команда [validate-second-pass-dataset.py](../scripts/validate-second-pass-dataset.py)
читає лише локальні файли, використовує Python standard library, друкує JSON
у stdout. Вона не виконує replay/scoring, не змінює дані й не надає authority.
Legacy core reader і runtime/config залишилися незміненими.

```sh
python3 scripts/validate-second-pass-dataset.py /data/session-a /data/session-b > report.json
python3 scripts/validate-second-pass-dataset.py --disjoint-references /data/session-a /data/session-b
python3 scripts/tests/test_second_pass_dataset.py -v
```

Передавати **всі splits разом**: перевірка перетинів охоплює лише названі
каталоги. Report записувати поза dataset root. Дані під час перевірки мають
бути незмінними. Перевіряється size/mtime відкритого файла до/після читання
та hash при повторному читанні того самого path; це не filesystem snapshot.
Майбутній importer/scorer повинен повторно звірити report hashes перед run.

## Результат і межі

| Exit | `status` | Значення |
|---|---|---|
| 0 | `structurally_valid` | Структура узгоджена, review заявлено завершеним, unresolved немає, completeness claim узгоджений |
| 2 | `invalid` | Невірні файли/metadata, суперечність або заборонений перетин splits; usage error також exit 2 |
| 3 | `needs_review` | Структура узгоджена, але review pending/rejected, є unresolved, непідтверджена повнота або exact query/reference pixel overlap |

`semantic_independence_verified` завжди `false`. Навіть exit 0 перевіряє
наявність і узгодженість заяв/evidence, не правдивість фізичного provenance,
якість анотації, clock alignment, losslessness conversion або accuracy.
`complete_claim_consistent` не доводить, що оператор записав увесь прохід.
Null uncertainty залишається видимою в metadata; вона не стає нульовою
похибкою. Labels із невідомою correspondence рахуються окремо, не видаляються.
`label_counts` — категорії розмітки, не остаточні scorable counts: policy
видимості/uncertainty й метрики має визначити наступний scorer slice.

Report містить першу структурну помилку кожного dataset, незалежні результати
інших datasets, collection errors і cross-split overlaps. Overlap analysis
виконується лише для структурно валідних наборів; invalid input залишає весь
report invalid. `input_sha256` — hashes прочитаних bytes, включно з
dataset.json, обома CSV, reference, config, evidence, PGM та originals.
Для invalid набору список може бути частковим. JSON порядок детермінований
для тих самих inputs/root paths; wall clock у звіт не додається.

## Конкретна схема dataset.json

Цей файл уточнює вкладені поля prose-контракту v1. У всіх описаних objects
потрібні **рівно** перелічені keys; зайві/пропущені keys відхиляються.
Винятки: довільні JSON objects `parameters` і map `unknown_reasons`.
Duplicate JSON keys, NaN/Infinity, wrong types і bool замість integer
відхиляються. IDs/описові strings непорожні. SHA-256 — 64 lowercase hex.

| Object | Обов'язкові keys та типи |
|---|---|
| root | `schema_version`: integer 1; `dataset_id`, `query_session_id`, `capture_group_id`: strings; `split`: development/validation/test; `reference`, `capture`, `clock`, `sequence`, `independence`, `annotation`, `unknown_reasons`: objects |
| `reference` | `path`, `sha256`, `recording_session_id`, `provenance_evidence_id`: strings; `entry_count`: integer 1..100000 |
| `capture` | `camera_id`, `profile_id`, `config_path`, `config_sha256`, `pixel_format`: strings; `native_width`, `native_height`: integers 1..65535; `preprocessing`, `conditions`: objects; `conversion`: object або null |
| `capture.preprocessing` | `method`: string; `parameters`: object |
| `capture.conditions` | `lighting`, `altitude`, `motion`: strings або null із поясненням; units altitude зазначати в описі |
| `clock` | `clock_id`, `epoch`, `timestamp_provenance`: strings; `unit`: ns; `source`: host_callback/sensor_exposure/host_read; `alignment_uncertainty_ns`: unsigned integer або null із поясненням |
| `sequence` | `complete`, `boundaries_confirmed`: booleans; `planned_start`, `planned_end`, `counter_evidence_id`: strings; `actual`: object; `acquired_count`, `dropped_count`: unsigned integers або null із поясненням; `stored_count`: integer 1..100000; `gaps`: array |
| `sequence.actual` | `first_frame_id`, `last_frame_id`, `first_timestamp_ns`, `last_timestamp_ns`: integers, точно рівні першому/останньому rows frames.csv |
| `independence` | `not_reference_derived`: boolean true; `review_status`: pending/reviewed/rejected; `reviewer_id`: string або null; `evidence_ids`: непорожній array унікальних evidence IDs; `unresolved`: array непорожніх унікальних strings |
| `annotation` | `revision`, `annotator_id`, `method`, `endpoint_definition`, `visibility_definition`, `source_frame_binding`: strings; `review_status`, `reviewer_id`: як вище; `unresolved`: array strings; `evidence`: непорожній array objects |
| `annotation.evidence[]` | `id`, `path`, `sha256`, `method`, `uncertainty`, `synchronization`: strings; `review_status`, `reviewer_id`: як вище |

`reference.recording_session_id` мусить відрізнятися від query session.
Config bytes із `config_path` звіряються з `config_sha256`; це не лише
неперевірена hash string. Усі evidence IDs унікальні. Reference, independence,
counters, gaps, labels і conversion links повинні вказувати на records
`annotation.evidence[]`; файли evidence теж проходять path/hash перевірку.
Review status `reviewed` вимагає непорожнього reviewer ID. Validator не
інтерпретує текст endpoint protocol або метод анотації як matcher result.

`unknown_reasons` — map exact dotted field path → непорожнє пояснення:

```json
{
  "capture.conditions.altitude": "Independent altitude measurement unavailable",
  "clock.alignment_uncertainty_ns": "Cross-clock alignment not measured",
  "independence.reviewer_id": "Review pending",
  "evidence.manual-01.reviewer_id": "Review pending"
}
```

Ключ evidence використовує його ID: `evidence.<id>.reviewer_id`. Gap index
нульовий: `sequence.gaps.0.missing_count`. Nested null у parameters теж
потребує exact path, наприклад `capture.conversion.parameters.exposure`.
Невикористані/misspelled unknown reasons відхиляються. Identity/hash/clock
identity не можуть бути null. `conversion: null` і null gap anchors мають
визначену структурну семантику нижче й не потребують unknown reason.

## Файли, кадри й conversion

`frames.csv` та `labels.csv` мають exact headers із
[базового контракту](SECOND_PASS_DATASET_CONTRACT_UA.md). UTF-8, звичайний
CSV quoting; blank/extra columns, порожні rows і неправильні headers
відхиляються. Labels можна подати в довільному порядку, join тільки за
`frame_id`; кадри не сортуються й не перенумеровуються.

Source frame IDs у цій конкретній v1 схемі **строго зростають** в одній
сесії та лежать у `0..2^64-1`; increment відповідає одному acquired frame.
Reset/wrap або інший ID policy потребує окремого dataset/контракту, не
автоматичного виправлення. Timestamp — `1..2^63-1`, strictly increasing,
integer nanoseconds без float conversion. З поточним host clock його
не порівнюють. Geometry query frames стала в межах dataset.

Всі artifact paths — relative POSIX-style paths усередині root. Заборонено
absolute/drive/UNC, backslash, colon, comma/control/newline, `.`/`..`,
порожні path components і surrounding whitespace. Symlinks усередині root
дозволені лише якщо resolved target — regular file у тому самому root;
escape/cycle відхиляється. Це стосується також dataset.json та CSV files.

PGM: binary P5, max value 255, width/height 1..65535, exact raster size.
Comments/whitespace у header і CRLF підтримані. Після 255 споживається один
separator або CRLF, як у чинному core reader; наступний whitespace/# —
pixel, а не текст для trimming. Payload максимум 64 MiB, header 4096 bytes.
Trailing bytes відхиляються, на відміну від більш поблажливого core reader.

Reference: VHRS v1, little endian, header 16 bytes, 1..100000 entries,
Gray8, positive dimensions, exact payload size до 64 MiB на entry,
без truncation/trailing bytes. `entry_count` перевіряється за самим файлом.
Це bounded structural reader для цього dataset contract, не заміна всіх
core VHRS readers і не оцінка reference distinctiveness. Payloads хешуються
блоками до 1 MiB; reference imagery не завантажується цілком у RAM.

Без conversion потрібні `pixel_format: "Gray8"`, preprocessing
`{"method":"identity","parameters":{}}` та query dimensions, рівні native.
Це заява про безпосередні PGM inputs, не автоматичний доказ походження.
Для conversion потрібні рівно поля:

| Field | Значення |
|---|---|
| `method` | Непорожній опис перетворення |
| `parameters` | JSON object параметрів |
| `mapping_evidence_id` | Link на evidence про original → query mapping і losslessness |
| `originals` | Array `{frame_id, path, sha256}`; кожен query frame рівно один раз, жодних зайвих IDs |

Original files перевіряються за bytes/path/hash. Інші формати не декодуються,
conversion не виконується і його losslessness алгоритмічно не доводиться.

Metadata обмежено 4 MiB, кожен CSV — 64 MiB та 100000 rows. Для обох CSV
діє також standard-library csv field limit (131072 characters). Pixels
читаються потоково, але frame metadata/hash sets зберігаються пропорційно
кількості кадрів. Collection порівнюється попарно; це offline tool для
явно переданого списку, не realtime bounded long-route search.

## Gaps і повнота

Gap object має рівно `after_frame_id`, `before_frame_id`,
`start_timestamp_ns`, `end_timestamp_ns`, `missing_count`, `reason`,
`evidence_id`. Anchors — два сусідні **stored** frame IDs; null `after`
означає gap до першого stored frame, null `before` — після останнього.
Обидва anchors null заборонені. Для однієї пари anchors лише один record.

Часовий interval gap включний; він строго всередині timestamps anchors.
Для відсутнього зовнішнього anchor діє загальний timestamp range. Reason
непорожній, evidence link існує. Усі interior ID holes мусять мати gap
record і `missing_count = before - after - 1`; time-only gap між сусідніми
IDs може мати count 0. Unknown missing count дозволено лише для boundary
gap, із поясненням; він залишає completeness непідтвердженою. Boundary
count не може виходити за доступний unsigned ID range.

`stored_count` дорівнює CSV rows; `acquired = stored + dropped`, якщо обидва
лічильники відомі. Сума відомих gaps не перевищує acquired/dropped, а коли
всі gap counts відомі — точно дорівнює dropped. Planned boundaries — описи
наміру; `boundaries_confirmed` — заява reviewer, що прохід охоплює ці межі.
`complete: true` із невідомими counts або непідтвердженими boundaries
відхиляється. `complete: false` дозволяє структурний audit, але exit 3.
Відсутність ID holes не доводить, що лічильник почали до старту проходу.

## Labels і split checks

Для кожного query ID потрібен рівно один label. Enums, evidence links,
парність порожніх index cells, inclusive range і bounds reference count
перевіряються. Interval допускається тільки для on_route. Start/end
потребує on_route; конкретний endpoint region залишається незалежним
annotation protocol, тому validator не прирівнює його автоматично до
index 0/N-1. Unobservable може мати position interval із незалежної
траєкторії; це не автоматичний дозвіл оцінювати visual correspondence.

Між різними splits відхиляються спільні capture groups, query sessions,
query/original byte hashes, query pixel hashes, а також query ↔ reference
session/content overlap. Повторний dataset ID — помилка і в одному split;
одну query session не можна приписати різним capture groups.

Shared reference route/session між splits видимий у report; за замовчуванням
він дозволений для оцінки нових проходів відомого route. Для перевірки
узагальнення на інші routes увімкнути `--disjoint-references`. Scene-level
спорідненість та неописані derivatives потребують review; hashes їх не
виявляють. Exact query/reference pixels усередині одного dataset дають
exit 3: це привід перевірити provenance, не доказ фізичного копіювання.

## Валідація реалізації та продовження

Synthetic fixture factory і негативні тести зберігаються в
[test_second_pass_dataset.py](../scripts/tests/test_second_pass_dataset.py).
Файли створюються у temporary directories, включно з paths із пробілами;
wire VHRS формується незалежним struct writer, PGM — explicit bytes.
Фабрика прямо позначає всі claims як synthetic. Green suite не робить їх
реальним незалежним проходом.

Перевірено valid/deterministic/read-only reports; всі truncation prefixes
малого VHRS/PGM; malformed headers/layouts/hashes/dimensions; JSON types,
duplicate keys/non-finite values; timestamp/ID extremes; paths/symlinks;
coverage/ranges/contradictions labels; counts/gaps/review; conversion
mapping; leakage попри зміну PGM header; CLI JSON та exit 0/2/3.
Тести включено до [software runner](../scripts/test-software.sh).

Виконано у WSL/Python 3.12: validator **31/31** (symlink cases не skipped),
повний runner — Debug/Release **64/64**, telemetry CLI **7/7** кожна,
readiness Python **15/15**, benchmark Python **5/5**. Фінальний full runner
включає уточнений row-limit test і regression обов'язкового reference hash:
до виправлення `sha256: null` помилково проходив, тепер відхиляється.
У двох CMake caches перевірено сім flags OFF.
Native Windows Python та hardware тут не запускалися.

Нові readers додатково прочитали **19/19** раніше аудованих artifacts із
незмінними SHA-256: **6 VHRS / 2583 entries + 13 PGM**. Це compatibility
перевірка формату, не admission цих sparse/self даних як незалежного набору.
Із попередніх 217 hashes змінилися лише software runner та tests README;
core/config hashes незмінні. Збережено нові 219 hashes.

Локальний ignored каталог `artifacts/dataset-validator-20261004/` містить
`software.log`, `unit-final.log`, `reader-compatibility.json`,
`null-hash-before.log` (очікуваний regression failure до виправлення),
`tested-source-hashes.json`, `verification.json`, helper `verify.py` і
`synthetic-example/` зі зразком повної JSON/CSV структури та valid report.
Приклад не є реальним capture evidence. У новому clone fixtures відтворює
committed test factory; ignored evidence автоматично не переноситься.

Прямо прочитано core VHRS/replay readers і test runner, перевірено source
references та diff. GitNexus status показав індекс checkpoint `6d8f547`,
stale щодо нових Python files. Його не оновлювали й не використовували як
impact verdict; застосовано source/diff/tests fallback чинного AGENTS.md.

Наступний запланований slice [offline scorer](SECOND_PASS_SCORER_UA.md)
реалізовано `2026-10-04` із pinned scoring policy та synthetic oracle fixtures.
Подальший крок — controlled offline exporter/runner або валідація
реального незалежного dataset, якщо його надано. Поточні sparse/self дані
не перетворювалися на labeled second pass. Threshold tuning, hardware
capture/output і заяви про accuracy цим checkpoint не авторизуються.
