# Наскрізна синтетична second-pass кампанія

Дата: `2026-10-13`. Початковий checkpoint: `187ee2c`.

Додано `scripts/tests/test_second_pass_campaign.py`: одна явно задана матриця
проходить через actual `second_pass_replay`, collection runner, scorer і
[read-only archive audit](SECOND_PASS_RUNNER_AUDIT_UA.md). Перевірка зв'язує
початковий request із per-frame predictions, scores, progress і всіма
очікуваними runs після завершення, збоїв та переривання.

Це test/evidence slice. Production C++/Python, config profiles, matcher
thresholds і output gates не змінені. Значення нижче існують лише в
тимчасових synthetic fixture configurations. Hardware не підключається.

## Фіксована матриця та oracle

### Що означає «зберігаються всі очікувані slots»

Slot — це запис одного заздалегідь запланованого запуску: dataset +
configuration + policy, з власним run ID. Це не комірка пам'яті. У перевіреному
сценарії partial export + interruption із 12 запусків 4 завершено успішно,
2 залишилися незавершеними, 6 ще не почато (`pending`). У звіті залишаються всі
12 записів. Аудит враховує pending як incomplete: 32 verified + 64 incomplete
пар «кадр–запуск», разом 96. Не можна видалити невдалі записи й показати лише
4 успішні як повністю завершену кампанію.

Це облік повноти запланованої роботи, а не відновлення втрачених результатів
чи автоматичний повтор. Твердження стосується перевірених сценаріїв і вже
зафіксованого плану; довільна втрата живлення або диска не має гарантії
збереження всіх файлів. Якщо потрібні controls відсутні, audit не підтверджує
завершеність і не відновлює їх самостійно.

Три datasets мають splits `development`, `validation`, `test`, різні query /
reference sessions, frame IDs і timestamps. Кожен містить 8 source frames та
4 reference entries, geometry 2×2 Gray8. Pixel permutations зберігають
покадрові відстані, але уникають точних cross-split query/reference copies;
constant invalid queries теж різні. `disjoint_references=true`, кожен run
отримує обидва інші datasets як peers.

**Ці fixtures споріднені й синтетичні.** Відсутність exact hash/pixel overlap
та `structurally_valid` не доводять фізичної або статистичної незалежності.

Кожен dataset має чотири runs: дві configurations × дві policies. Порядок
явний у request, разом **12 runs / 96 frame-run pairs**, по **4 runs / 32
frame-run pairs** на dataset і split. Усі inputs і повний initial manifest
фіксуються до першого child; тест повторно звіряє plan перед кожним child.

GT: шість on-route positives, один off-route negative, один unknown. Кадр 6
є correct positive із `visibility=degraded`. Policy `inclusive` оцінює clear
та degraded, `clear` виключає degraded. Пороги confidence для fixture `near`
та `exact` — 0.99 та 1.0; pixels валідних queries відрізняються від reference
на +1, тому exact конфігурація відхиляє всі збіги.

| Configuration | Policy | TP | FP | FN | Positive denominator | Unscorable frames |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| near | inclusive | 3 | 2 | 3 | 6 | 1 |
| near | clear | 2 | 2 | 3 | 5 | 2 |
| exact | inclusive | 0 | 0 | 6 | 6 | 1 |
| exact | clear | 0 | 0 | 5 | 5 | 2 |

Ці очікування визначені до запуску, а не отримані зі scorer output. Для near
очікувані reference indices: `0, 1, absent, 2, 0, absent, 3, 1`; для exact
усі absent. Перевіряються точні sequence/frame_id/timestamp_ns, observed /
missing, match/navigation validity та unknown unsupported outcomes.
Observed coverage успішного replay — 8/8 навіть коли matcher відхиляє все.

За однієї configuration обидві policies мають однакові prediction bytes,
але різні score denominators; configurations дають різні predictions.
Collection не має pooled accuracy. Metrics звіряються окремо для кожного run.

## Збої та переривання

| Сценарій | Exporter calls | Verified slots | Failed slots | Incomplete/pending slots | Frame-run coverage |
| --- | ---: | ---: | ---: | ---: | --- |
| Complete | 12 | 12 | 0 | 0 | verified 96 / total 96 |
| Failed children | 12 | 10 | 2 | 0 | verified 80, failed 16 / total 96 |
| Partial + interrupt | 6 | 4 | 0 | 8 | verified 32, incomplete 64 / total 96 |

У failed case run 1 (`development-near-clear`) отримує реальний child exit 7,
run 6 (`validation-exact-inclusive`) — child exit 0 із malformed CSV. Це
контрольована заміна цих двох child processes; решта виконує actual adapter.
Runner продовжує матрицю по одній спробі на run, загальний status
`replay_failed`. Усі 8 predictions failed runs є missing, TP/FP=0, FN=5/6
відповідно до policy; partial credit не надається.

У interrupted case exporter run 1 залишає partial file і кидає `OSError`;
run 5 отримує контрольований `KeyboardInterrupt`. Progress лишається
`verified, incomplete, verified, verified, verified, incomplete`, далі шість
`pending`. Archive audit дає `incomplete` із перевіреним final report;
жоден slot не зникає. Existing output відхиляє повторний runner call без
exporter invocation або змін bytes/mtime.

Окрема preflight перевірка всього catalog: pending review або точне query
pixel leakage між development/test блокує **всю** кампанію до output
directory і першого exporter call. Validator evidence лишається доступним.

## Відтворення й валідація

```sh
python3 scripts/tests/test_second_pass_campaign.py \
  artifacts/telemetry-source-20261002/build/Release/second_pass_replay -v
bash scripts/test-software.sh artifacts/telemetry-source-20261002/build
```

Чотири integration tests виконуються на обох збірках. Final full WSL runner:
Debug/Release CTest **64/64**, telemetry CLI **7/7**, exporter **21/21**,
collection runner **23/23**, campaign **4/4** кожна; readiness **15/15**,
benchmark **5/5**, validator **31/31**, scorer **27/27**, bundle checker
**24/24**, collection checker **27/27**, archive audit **30/30**.
Нові native sanitizer/NoTests builds не потрібні для test-only slice і
не виконувалися. Native Windows/MSVC acceptance не оновлювалася.

Успішний actual runner CLI, повторний API run і перенесений archive
проходять audit; два повтори audit дають однаковий JSON і не змінюють archive
bytes/mtime. API audit запускається з явно забороненими process/runner calls.
Original request/dataset/config/policy inputs залишаються незмінними.

Evidence directory: `artifacts/second-pass-campaign-20261013/`:
`software.log`, `verify.py`, `verify-native.py`, `verification-native.log`,
`persistence.json`; прийняті archives, audit JSON, `verification.json` і
`tested-source-hashes.json` розміщено в **`accepted/`**. Історичні evidence
roots не перезаписуються; existing archives перевіряються без resume.

Прийнято actual Debug/Release complete campaigns, Release failed campaign,
Release partial+interrupted campaign та relocated complete copy. Усі 12
prediction hashes і per-run metrics у Debug/Release однакові. Checker CLI
підтвердив exits 0/0/3/4 та тотожність API reports. Expected plan/state hashes
обчислено перед audit — це не незалежні історичні receipts.

Evidence execution/audit виконано на native WSL temporary storage, потім
перенесено в `accepted/`: **3420 файлів побайтово однакові**. Збережені bytes
прочитано назад у WSL і повторно перевірено всі п'ять archives після relocation;
audit не змінює їхні bytes/mtime. Historical `state.output` та display roots
у reports можуть посилатися на вже прибраний temporary path — він не є input
для audit. Початкову спробу прямого запису/audit на Windows-диску зупинено
через I/O cost; root-level exploratory archives не входять до acceptance.
Це не порівняльний performance benchmark файлових систем.

Сім camera/output flags OFF у Debug і Release caches. З попередніх 232 source
hashes змінилися лише `scripts/test-software.sh` і tests README; новий campaign
test дає **233** hashes. Production sources побайтово незмінні.

Graph metadata лишається `6d8f547`; використано direct source/caller/diff/tests.
Synthetic source-time clock, Guided telemetry й unsupported outcomes
попереднього exporter не змінені. Chronology/authorship, binary-source
attestation, physical independence та accuracy не підтверджуються.

Запланований тут baseline часу та peak RSS зі збільшенням кількості runs
продовжено у [вимірюванні 2026-10-18](SECOND_PASS_SCALING_2026-10-18_UA.md).
Незалежні повні annotated
captures залишаються необхідними для реальної accuracy; hardware/output
або threshold tuning цей checkpoint не авторизує.
