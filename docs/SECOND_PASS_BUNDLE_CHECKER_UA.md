# Read-only checker збереженого second-pass bundle

Дата: 2026-10-05. Продовження exporter checkpoint `dc458ec`.

`scripts/check-second-pass-bundle.py` перевіряє закритий набір артефактів
[offline exporter v1](SECOND_PASS_EXPORTER_UA.md), узгодженість його plan,
completion та результатів, повторно валідовує datasets/splits і перераховує
score. Ні executable, ні Python sources **із bundle** не виконуються.
Перевірка не записує файлів у bundle й не виправляє hashes або результати.

## Використання

```sh
python3 scripts/check-second-pass-bundle.py path/to/saved-run

# Незалежно збережений до/поза перевірюваним bundle plan hash:
python3 scripts/check-second-pass-bundle.py path/to/saved-run \
  --expected-plan-sha256 EXTERNALLY_RETAINED_SHA256 \
  --expected-peer-ids test-session-a validation-session-b

# Явно очікується нуль peers:
python3 scripts/check-second-pass-bundle.py path/to/saved-run --expected-peer-ids
```

Python 3.9+ standard library; перевірено WSL/Python 3.12. Native Windows тут
не перевірено. Поруч із checker мають бути довірені поточні exporter, scorer
та validator scripts. Їхні **байтові** hashes повинні збігатися зі збереженими
`toolchain/` copies. Несумісна версія відхиляється; checker не завантажує
старий довільний Python-код із архіву і не мовчки мігрує результати.
Для формування тестових bundles не потрібні binary build, Pi, камера, FC,
UART чи мережа. Actual exporter integration окремо входить у software suite.

JSON іде лише у stdout; shell redirection для збереження report має вказувати
файл **поза bundle**, інакше створений файл стане зайвим artifact closed set.

## Статуси

| Exit | Status | Значення |
|---|---|---|
| 0 | `verified` | Complete run, увесь bundle узгоджений, archived score відтворено |
| 2 | `invalid` | Schema/hash/path/scope/status/score mismatch або read error |
| 3 | `replay_failed` | Failed run узгоджено з усіма missing rows, його coverage/FN score відтворено; run лишається невдалим |
| 4 | `incomplete` | `execution.json` відсутній; це не завершений run і не підтверджена цілісність |

Порожній або пошкоджений `execution.json` — `invalid`. Відсутній root теж
`invalid`. `failure.json` без completion допускає лише `incomplete`, а разом
із completion є суперечністю. На `invalid`/`incomplete` report не містить score,
`bundle_integrity_verified=false`, `score_reproduced=false`.
У всіх статусах `binary_executed=false`, `semantic_independence_verified=false`,
`chronology_authenticated=false`. Exit 0 не є accuracy/flight acceptance.

## Що перевіряється

1. Вміст root — regular files і directories; symlinks, спеціальні файли,
   unbound files та зайві порожні directories відхиляються. Це включає peers,
   які лежать у bundle, але не оголошені в plan. Межа inventory — 200000
   entries; максимум peers — 1000. Імена та containment додатково проходять
   чинний validator path contract. Парсинг тексту обмежений за розміром:
   control JSON 4 MiB, archived score 256 MiB, manifest 64 MiB, clock 4 MiB.
   Інші artifacts хешуються chunks; сумарний час/обсяг bundle не лімітується.
2. `execution.json` має exact v1 fields, п'ять output bindings і plan hash.
   Complete вимагає integer returncode=0 та null missing_reason.
   Failed reason має відповідати returncode/error: timeout/launch — null,
   replay_failed — ненульовий integer, replay_output_invalid — integer 0.
   Boolean замість integer не приймається.
3. Plan має exact v1 schema, simulation constants, аргументи й повний input
   hash map; inputs не можуть перекривати outputs/control files. Hash кожного
   frozen input і output звіряється, включно з binary, raw CSV, log та score.
   За наявності зовнішнього expected-plan hash перевіряється і цей anchor.
4. Порівнюються pinned/trusted hashes трьох tools. Configuration повторно
   перевіряється довіреним exporter helper; всі embedded datasets і peers
   проходять validator разом із recorded disjoint-reference policy.
   Dataset hashes мають точно покривати plan; peers — `peers/0001`, `0002`, …
   без пропусків чи прихованих залишків. За `--expected-peer-ids` перевіряється
   також точна зовнішня множина IDs; дублікати очікуваних IDs — помилка.
5. Перевіряються adapter pixel/geometry limits, точні argv із configuration,
   manifest order/IDs/timestamps/paths та source clock. Manifest — точний
   exporter LF format; clock приймає цілий LF або CRLF stream, без змішування,
   сортування чи rebasing. Epoch, zero-work clock, synthetic armed Guided
   telemetry і retention state через gaps лишаються явними simulation claims.
6. `run.json` має точно відповідати plan, dataset, producer, policy та
   prediction bindings. Complete потребує байтово однакових raw/final CSV,
   усіх observed rows, відомого dry-run command outcome та порожніх
   endpoint/readiness/verification/publication. Failed потребує всіх missing
   rows з однаковим execution reason; жоден partial result не зараховується.
   Failed raw output може бути пошкодженим: він перевіряється як bytes,
   але не перетворюється на predictions і не доводить причину відмови.
7. Довірений поточний scorer повторно обчислює повний report. Порівнюються
   всі поля archived/recomputed score, включно з metrics, per-frame decisions,
   temporal episodes, coverage, input/tool hashes і validation scope.
   JSON whitespace/key order не мають значення; bool/int/float розрізняються.
8. До повернення успіху повторно перевіряються **початкові**, а не заново
   прийняті hashes всіх прочитаних файлів, inventory та trusted tools.
   Стабільна зміна під час rescore не отримує новий «правильний» hash.

Успішний report містить `bundle_sha256`, checker hash, execution status,
plan hash, external-anchor/scope flags, peer IDs і повний свіжий `score`.
Той самий незмінений bundle дає детермінований JSON report.

## Перенесення та межі доказу

Архів можна перенести. Лише
`score.dataset_validation.datasets[*].root` нормалізується за позицією до
`dataset`/`peers/NNNN` перед порівнянням. У stored score кожне root поле
все одно має бути непорожнім string. Усі інші поля, ID, порядок datasets,
hashes, числа й результати мусять збігатися. Report повертає фактичні поточні
root paths; тому reports із різних filesystem locations можуть відрізнятися.

Closed set hashes встановлюють **внутрішню узгодженість**, не авторство,
час створення чи факт запуску саме цього binary. Узгоджене переписування
files і всіх manifests неможливо відрізнити без зовнішнього доказу.
Optional expected-plan hash виявляє зміну plan щодо переданого anchor;
довіру до способу отримання самого anchor checker не встановлює. Зміна
outputs із узгодженим completion і score теж потребує зовнішнього audit
для автентифікації: plan фіксує inputs, а не майбутні results.

Peer scope стосується лише embedded datasets. Без зовнішнього inventory
checker не знає про вилучені чи взагалі не надані sessions. Навіть точні
IDs/hashes не підтверджують scene/annotation independence. Filesystem locks,
захист від ворожих transient races, dynamic-library/compiler provenance та
durable atomic completion тут не реалізовані. Жодна перевірка не запускає
live gates, не додає endpoint evidence і не змінює matcher thresholds.

## Валідація і наступний крок

[24 synthetic tests](../scripts/tests/test_second_pass_bundle.py) охоплюють
read-only deterministic CLI, no-process assertions, relocated roots,
closed inventory/peers, hashes/schema/types, consistency переписаних plan/
config/run/score, невідомі/недопустимі outcomes, failed/incomplete статуси,
symlinks/FIFO, limits, зовнішній anchor і mutation під час rescore.
Exporter tests тепер також перевіряють actual C++ success та реальні
timeout/nonzero bundles цим checker. Fixture binary — інертні дані;
synthetic TP=3/FP=2/FN=3 не є оцінкою фізичної accuracy.

Evidence: `artifacts/second-pass-bundle-20261005/`. Canonical test results і
source hashes зафіксовано у CURRENT_PROJECT_STATUS та SESSION_LOG.
Graph metadata досі `6d8f547`; використано пряме читання producer/consumer
schemas, source/diff review і тести. Core, config, exporter, scorer та
validator production sources у цьому slice не змінені.

Наступний bounded software slice — manifest керованої evaluation collection:
явно перелічити очікувані dataset IDs/splits, pinned plan/completion hashes і bundles,
перевірити повноту та cross-split leakage разом, зберегти failed/incomplete
coverage у collection report. Не підбирати thresholds і не видавати
синтетичні або sparse/self кадри за незалежну accuracy. Реальний повний
annotated capture лишається окремою потребою; hardware не входить у цей план.

Оновлення `2026-10-06`: цей collection slice реалізовано окремим
[read-only collection checker](SECOND_PASS_COLLECTION_UA.md); актуальний
наступний крок наведено у [current status](CURRENT_PROJECT_STATUS_UA.md).
