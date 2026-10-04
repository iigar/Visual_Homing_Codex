# Контракт незалежного повторного проходу, v1

Зафіксовано `2026-10-03` після [локального audit](DATASET_AUDIT_2026-10-03_UA.md).
Це вхідний контракт для offline validator та scorer.
Validator реалізовано `2026-10-04`: [точна JSON-схема, CLI та межі](SECOND_PASS_VALIDATOR_UA.md).
Scorer також реалізовано `2026-10-04`: [pinned inputs, policy та метрики](SECOND_PASS_SCORER_UA.md).
Controlled replay exporter ще не реалізовано. Чинний core replay reader
приймає тільки три колонки `id,timestamp_ns,path` без header; він не читає
labels і сам не перевіряє цей контракт. Дані не надають output authority.

## Одиниця даних і прив'язка

Один dataset — одна query capture session щодо одного незмінного reference
VHRS. Якщо clock reset, змінилася камера/геометрія/обробка або query session,
потрібен інший dataset ID; фізично пов'язані clips зберігають спільний
`capture_group_id` для split. Нова сесія повинна бути окремим спостереженням,
а не експортом/augmentation кадрів reference route.

Мінімальний каталог:

```text
dataset.json
frames.csv
labels.csv
frames/...
reference/route.vhrs
evidence/...
```

`dataset.json` містить:

| Поле | Обов'язковий зміст |
|---|---|
| `schema_version` | Integer `1` |
| `dataset_id`, `query_session_id`, `capture_group_id` | Непорожні сталі IDs; фізично споріднені query clips мають спільну group |
| `split` | `development`, `validation` або `test`; присвоюється до scoring |
| `reference` | Relative `path`, SHA-256, `entry_count`, окремий `recording_session_id`, посилання на provenance evidence |
| `capture` | Camera/profile ID і hash конфігурації, native dimensions, pixel format, preprocessing method/parameters, умови освітлення/висоти/руху або явні unknowns |
| `clock` | `clock_id`, unit `ns`, source `host_callback` / `sensor_exposure` / `host_read`, опис epoch і timestamp provenance; наявна alignment uncertainty |
| `sequence` | `complete` boolean, planned/actual boundaries, acquired/stored/dropped counts з джерелом лічильників, перелік відомих gaps і причини |
| `independence` | Явна заява, що query не походить із reference captures, хто й за яким evidence це перевірив; unresolved items не приховуються |
| `annotation` | Revision, annotator/reviewer IDs, метод, definition of endpoint/visibility, зв'язок із source frames, evidence records з IDs і relative paths/hashes |

Unknown metadata — JSON `null` із поясненням, не вигадане значення.
Обов'язкові identity/hash/clock поля не можуть бути unknown для replay
admission. Заява `independence` сама не доводить фізичної незалежності;
validator може перевірити її структуру, але повинен показувати стан review.
Однаковий basename, інша дата, інший hash після resize або historical
matcher index не є достатнім provenance evidence.

## Кадри, час і пропуски

`frames.csv` має header і рівно такі колонки:

```csv
sequence,frame_id,timestamp_ns,path,sha256,width,height
```

- `sequence` — `0..N-1` у порядку source capture, без сортування за matcher
  progress. `frame_id` — унікальний unsigned integer до `2^64-1`; gap IDs
  може бути реальним drop і потребує пояснення, а не перенумерації source.
- `timestamp_ns` — десятковий integer `1..2^63-1`, строго зростає в одному
  clock domain. Зберігається source timestamp; час файла, export, matcher
  або UTC у назві не підставляється замість нього. Timestamp старішого
  запису не порівнюють з поточним wall/monotonic clock без явного mapping.
- `path` — relative до dataset root, regular file усередині root; без
  absolute paths, `..` escape або symlink escape. Для майбутнього експорту
  у чинний трьохколонковий manifest шлях не містить comma/newline; labels
  та інші CSV metadata не передаються безпосередньо у legacy reader.
- SHA-256 перевіряється за bytes, dimensions — за decoded pixels. Для v1
  replay artifact — Gray8 binary PGM P5, max value 255, точний payload.
  Lossless conversion із іншого джерела має зберігати original hash,
  mapping і conversion parameters; overlay/preview не є query input.
- Зберігати всю заявлену query sequence, зокрема невдалі/negative кадри,
  pre/post-endpoint і періоди втрати/відновлення. Не відбирати лише кадри,
  які пройшли matcher, selector або publication interval.
- Explicit gaps містять frame/time bounds, reason і provenance. Якщо
  acquire count невідомий або sequence обрізана, completeness лишається
  непідтвердженою; відсутність ID gaps сама не доводить повноти.
- Clock для actual replay caller задається окремо з тією самою epoch.
  Scripted processing delay — synthetic experiment; його не називати
  виміряною latency джерела. [Runtime timing boundaries](RUNTIME_TIMING_CONTRACT_2026-10-02_UA.md)
  лишаються чинними. Capture timestamps і completion times не змішуються.

## Незалежна розмітка

`labels.csv` має один рядок на кожен `frame_id`, без зайвих або відсутніх IDs:

```csv
frame_id,location,reference_index_min,reference_index_max,endpoint,direction,visibility,evidence_id
```

`location`: `on_route`, `off_route`, `unknown`. `endpoint`: `start`, `end`,
`neither`, `unknown`. `direction`: `forward`, `reverse`, `stationary`,
`unknown`. `visibility`: `clear`, `degraded`, `unobservable`, `unknown`.

Для scorable `on_route` потрібен independently annotated inclusive interval
`0 <= reference_index_min <= reference_index_max < entry_count`.
Якщо correspondence невідомий, обидві index cells порожні: location може
бути відомим, але точний matching result на такому кадрі не оцінюється.
Для `off_route`/`unknown` interval порожній. Взаємно суперечливі labels
або відсутній `evidence_id` відхиляються. Порожня cell не означає index 0.

Прийнятні джерела: незалежна синхронізована розмітка проходу/орієнтирів,
виміряна траєкторія з описаною похибкою, або reviewed manual correspondence
із reference images без перегляду output оцінюваного matcher. Останній
варіант оцінює візуальну відповідність, не доводить метричну позицію.
Кожен evidence record описує метод, uncertainty, synchronization та review.

Не використовувати `route_index`, confidence, tracked progress, endpoint
stop або command validity самого оцінюваного алгоритму як істину. Не
призначати progress лінійно за часом/номером query frame: швидкість і
напрямок можуть змінюватися. Для route-normalized index `i/(N-1)` потрібен
`N>1`; це індексна координата, не відстань у метрах. Endpoint region
визначається annotation protocol до оцінки, не копіює спрацювання gate.

## Розділення наборів і правила оцінки

Сусідні кадри однієї фізичної сесії та їхні derivatives не розносяться між
development/validation/test. Split робиться за `capture_group_id`; якщо
мета — узагальнення на нові маршрути/сцени, також за reference route/scene.
До test run фіксуються hashes reference/data/labels/config, code/binary,
thresholds, matcher state/reset policy, preprocessing та scoring policy.
Будь-яке налаштування за test results переводить цей набір у development;
для незалежного підтвердження потрібен інший holdout.

До запуску слід зафіксувати, які labels є scorable, endpoint tolerance та
правило reacquisition confirmation. Не підбирати їх після перегляду outputs.
Непридатні, unknown та missing кадри показуються окремими counts/durations;
їх не можна мовчки видалити для покращення метрик.

- **Correct match (TP):** valid matcher result усередині незалежного interval
  для scorable `on_route`. **FP:** valid result поза interval або на scorable
  `off_route`. **FN:** scorable `on_route` без correct match; неправильний
  valid match дає і FP, і FN. Precision = TP/(TP+FP), recall = TP/(TP+FN);
  нульовий denominator дає absent, а не 0/100%.
- Окремо рахувати FP fraction на negative кадрах, index error distribution,
  abstention, unknown coverage та результати за session/direction/condition.
  Frame count не є числом незалежних trials; adjacent frames корельовані.
- Endpoint: ранні/хибні stops, пропущений annotated endpoint, затримка від
  independently annotated entry. Invalid-gap dwell continuity враховується
  за чинною policy, а не виправляється всередині scorer.
- Reacquisition: час від annotated повернення у scorable route region до
  стабільних correct matches. Число підтверджень/тривалість фіксуються до
  run; випадки без recovery до кінця clip та gaps показуються окремо,
  не виключаються з результатів як «немає виміру».
- Matcher validity, navigation command, route-only readiness, verification
  acceptance та publication — різні результати з різними gates. Звітувати
  кожен окремо; match TP не означає command/flight readiness.

Контракт не задає довільного цільового відсотка accuracy або дозволу змінити
production thresholds. Спершу потрібні достатні data/annotations і наперед
визначений evaluation protocol; hardware/flight acceptance — окремий етап.

## Реалізація та наступний крок

Завершений перший slice — [offline validator](SECOND_PASS_VALIDATOR_UA.md): schema/paths/hashes/dimensions, unique IDs,
strict clocks/order, complete label coverage, index ranges, evidence links,
explicit gaps і group/hash overlap між splits. Відсутні або непідтверджені
дані дають зрозумілу причину; synthetic fixtures перевіряють
негативні випадки. Немає автоматичного виправлення timestamps/labels,
додавання фіктивної ground truth, threshold tuning або hardware викликів.
Semantic independence/annotation quality залишаються явним review status.

Окремий [deterministic offline scorer](SECOND_PASS_SCORER_UA.md) перевіряє
прийнятний dataset та pinned per-frame results, записує всі рішення й policy.
Далі — controlled offline exporter/runner із freeze plan і actual replay
binding. Самі validator/scorer не доводять real accuracy; наявні sparse/self
артефакти не стають labeled second pass від факту імпорту.
