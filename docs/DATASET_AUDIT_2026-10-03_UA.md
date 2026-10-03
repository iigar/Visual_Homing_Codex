# Локальні дані для незалежної оцінки — 2026-10-03

Продовження checkpoint `6e5964d` у `refactor/optimization-tech-debt`.
Наявні дані придатні для artifact checks, regression/equivalence та
matcher benchmark. Повної незалежної послідовності другого проходу з
timestamps і ground truth у перевіреному локальному наборі не підтверджено.
False matches, misses, endpoint accuracy або reacquisition rate з цих даних
не обчислювали. Пороги й runtime не змінено; hardware не використовувався.

## Межі пошуку й актуальна перевірка

Перелічено файли активного checkout, включно з ignored `artifacts/` і
`keyframes/`: VHRS/PGM/VHRM/VHIX/CSV, PNG/JPEG/TIFF/BMP і поширені локальні
відеоформати; окремо шукали manifest/ground-truth/annotation/labels у назвах.
Виключено `reference`, Git/індекси/agent tooling, `node_modules` та каталоги,
назви яких починаються з `build`. Сусідні checkout, інші диски, від'єднані
носії та борт не перевірялися. Відсутність файлу в цьому пошуку не доводить
відсутності даних у всіх можливих форматах або місцях.

Знайдено 6 VHRS, 50 PGM та 33 PNG; у вибраному наборі немає VHRM/VHIX,
CSV replay або відеопослідовності. Додаткові filename searches не виявили
окремого набору ground truth. Переглянуто збережені session summaries,
попередні inventory/benchmark reports та чинний replay reader.

Повторно звірено всі **19/19** файлів основної копії з її збереженим
hash manifest: **41 624 113 байтів**, ті самі SHA-256. Це звірка з локальною
копією попереднього доказу, не нова перевірка стану борту.
Поточний Release executable успішно прочитав усі 6 VHRS через
`--inspect-route`; сім camera/output build flags перевірено як OFF.

## Що саме є в наборі

| Файли | Встановлений факт | Межа висновку |
|---|---|---|
| 6 VHRS основної копії | 2583 Gray8 кадри 160×100, строго зростають timestamps та IDs, пропусків IDs усередині файлів немає | Не доводить повного фізичного проходу, відсутності втрат до присвоєння ID або незалежності від reference route |
| 13 stop-PGM основної копії | Для кожного є explicit path→route binding у збереженому session log; жоден pixel payload не дорівнює кадру шести VHRS | Sparse observations; historical route index є рішенням matcher, не ground truth |
| 2 stop-PGM у `artifacts/stop_frames/` | Побайтові копії двох із цих 13 | Нових спостережень немає |
| 35 PGM у `keyframes/` | 7 груп по 5 оглядових зображень; розміри 320×200, 320×240 або 800×500 | Назва файлу/групи не є доказом source identity |
| 20 із цих keyframes | Exact pixel equality з кадрами 4 VHRS після перевіреного nearest-neighbour enlargement ×2/×5 | Це кадри reference route, їх не можна зарахувати як незалежний test set |
| Решта 15 keyframes | 10 належать за назвою двом групам 20260707 без відповідного VHRS у копії; 5 із групи 20260712 не збігаються з поточним однойменним VHRS за перевіреним pixel mapping | Походження/перетворення не встановлено; не призначати їм reference index за назвою |
| 33 PNG | 27 мають ті самі opaque grayscale pixels і dimensions, що й відповідні PGM; 6 не мають такого exact match | Шість назв позначають processed-area/ROI overlays; pixel inequality сама не підтверджує новий прохід |

Exact keyframe відповідності: `20260708T174000Z`, `20260709T151301Z`
мають індекси `0,74,149,224,299`; `20260709T161017Z`, `20260710T155821Z`
— `0,149,299,449,599`. Це збігається з selection rule production exporter,
перевірено pixels, а не лише очікувані filename indices.

| VHRS timestamp у назві | Кадрів | Last − first timestamp, с | Median interval, мс | `quality_pass` |
|---|---:|---:|---:|---|
| 20260708T174000Z | 300 | 9.966678 | 33.340952 | true |
| 20260709T151301Z | 300 | 9.969065 | 33.338226 | true |
| 20260709T161017Z | 600 | 19.968805 | 33.330957 | true |
| 20260710T154326Z | 183 | 21.886126 | 120.224305 | false |
| 20260710T155821Z | 600 | 19.957834 | 33.315551 | true |
| 20260712T164651Z | 600 | 19.965930 | 33.324556 | true |

Це часовий span записів, не довжина маршруту або фактичний рух апарата.
`--route-distinctiveness` повторно виконано з очищеними `VISUAL_HOMING_*`
environment overrides. Для 183-frame route `ambiguous_nearest_fraction=1`.
CLI повертає **exit 0 і при `quality_pass=false`**: це успіх діагностичного
обчислення. Якість треба читати з поля, а не виводити з exit code.
Успішний distinctiveness check також не є незалежною оцінкою matching.

## Чого бракує

Немає підтвердженого зв'язку «весь повторний прохід → кожен source frame →
його timestamp → незалежна позиційна/endpoint/negative розмітка».
PGM не містить replay timestamp. Дата, `id-*` і `route-*` у назві stop-файлу
не замінюють clock contract або незалежну анотацію. Підсумкові логи не
відновлюють pixels усіх кадрів; sparse publications і self-match не замінюють
послідовність. Окремі невідповідні hashes не доводять незалежності сесій.

Зафіксовано [контракт другого проходу](SECOND_PASS_DATASET_CONTRACT_UA.md):
ідентичність reference route, порядок і timestamps усіх query frames,
пропуски, незалежність розмітки, unknowns, розділення сесій і правила метрик.
Це специфікація для наступного validator, не вже реалізований importer,
scorer або дозвіл на нове фізичне збирання.

## Evidence та відтворення

Локальна ignored папка `artifacts/dataset-audit-20261003/` містить:

- `candidate-paths.txt`, `additional-media-paths.txt` — зафіксовані списки;
- `audit.py`, `audit.json`, `audit-summary.json` — size/hash, строгий локальний
  VHRS v1/PGM decode, pixel provenance та explicit stop bindings;
- `*-inspect.log`, `*-distinctiveness.log` — результати справжнього core CLI;
- `png-audit.ps1`, `png-audit.json`, `png-summary.json` — read-only PNG pixels
  через Windows System.Drawing, без встановлення залежностей;
- `gitnexus-query.json` і staged/main graph reports — обмеження аналізу;
- `verification.txt` — hashes поточного software checkpoint та evidence.

З repo у WSL: `python3 artifacts/dataset-audit-20261003/audit.py`;
у PowerShell: `& artifacts/dataset-audit-20261003/png-audit.ps1`.
Ці одноразові scripts читають зафіксовані lists і пишуть лише evidence,
не є production readers/validators та не заявляють підтримки довільних
форматів. Для нового clone потрібні ignored дані й ці локальні artifacts.

Усі **217** code/build/script/config hashes збігаються з перевіреним
runtime checkpoint. Попередня full suite — Debug/Release 64/64,
CLI 7/7 кожна, Python 15/15 і 5/5; її журнал прочитано, full suite повторно
не запускалася для цього documentation-only slice. Новими виконаними
перевірками є data audit і 12 route CLI calls вище.

GitNexus MCP має закритий transport; CLI query повідомляє відсутність FTS
і повертає порожній результат. Context приписує Python `stop_bindings`
сторонні Android relationships. Для висновків використано пряме читання
reader/exporter/benchmark code та фактичні hashes/CLI evidence.

Фінальний CLI index refresh завершено: 15698 nodes / 34129 edges / 211 flows.
Staged/main detect-changes дали `CRITICAL` для 143/170 flows; staged змінює
лише 16 documentation sections, але приписує JSX flows заголовку «Evidence
та відтворення». Перевірено фактичний п'ятидокументний diff, відсутність
code/config змін і 26 relative links; чистого graph verdict не заявлено.
Graphify AST update: 6106 nodes / 9474 edges / 491 communities.

Наступний локальний крок: реалізувати окремий offline validator за цим
контрактом із synthetic fixtures для missing/duplicate/reordered frames,
hash mismatch, invalid clocks/labels і split leakage. Він має перевіряти
структуру та повноту, окремо звітуючи непідтверджену семантичну незалежність;
не оцінювати accuracy на нинішніх sparse/self даних і не вмикати hardware.
