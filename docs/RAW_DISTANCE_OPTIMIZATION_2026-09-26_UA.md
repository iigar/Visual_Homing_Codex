# Швидше звичайне порівняння Gray8 — 2026-09-26

Продовження `0936cf1`: після оптимізації scale kernel окремо виміряно
coarse scan і формування top candidates. Змінено лише накопичення суми
у `normalized_mean_absolute_difference`; повний matcher на 13 stop-знімках
із refinement має медіану **3.504 → 1.381 ms** на локальній WSL x86_64 збірці.

## Підстава і реалізація

Таймери у тимчасовій копії baseline показали медіанні частки часу `match`:
звичайне порівняння **76.94%**, scale kernel **21.37%**, вставка кандидатів
**0.58%**. На query припадає 600 звичайних порівнянь, 600 вставок та 48 або
72 scale-виклики. Три свіжі процеси на кожен із 13 PGM, чотири queries у процесі
включно з прогрівальним. Це приблизна атрибуція з накладними витратами таймерів;
остаточні виміри нижче виконано без інструментації.

Різниця пікселів тепер накопичується у `uint32_t` блоками до 65536 пікселів,
а підсумок блоків — у попередній `uint64_t`. Максимальна часткова сума
`65536 × 255 = 16711680` гарантовано вміщується у `uint32_t`; межу захищено
`static_assert`. Останній неповний блок обробляється тим самим циклом.
Загальна ціла сума, фінальне ділення, validation та всі правила вибору
кандидатів зберігаються. Heap-виділень, нового кешу чи стану matcher немає.

Це дає компілятору простіший внутрішній цикл: у перевіреному GCC x86_64 assembly
є `psadbw` та `paddd`. Платформних intrinsics або нових compiler flags у проєкті
немає. Інші локальні варіанти перевірено й відкинуто: приведення різниці до
байта не дало виграшу, `max-min` стало повільнішим, блоки з `uint16_t` були
повільнішими за вибраний варіант. Експериментальні копії лишаються в artifacts.

Функція має три production consumers у тому самому модулі:
`Gray8RouteMatcher::match`, `probe_progress_zones`, `probe_edge_diagnostics`.
Camera/output-файли не редагувалися, апаратних підключень не було.

## Еквівалентність і перевірки

Новий `gray8_raw_distance_test` порівнює публічні результати matcher та progress
zones із незалежним початковим scalar-підсумовуванням у `uint64_t`:

- **65622 випадки**, confidence порівнюється побітово без tolerance;
- усі 65536 пар байтів окремо та ще раз у спільному payload 256×256;
- детерміновані випадкові, однакові та максимально відмінні пікселі;
- короткі/непарні розміри, межі SIMD-ширин, 16000, 65535 та 65538 пікселів;
- 4097×4113 пікселів із різницею 255: сума **4296995055 > UINT32_MAX**;
- confidence, valid, progress, route index, timestamp, direction та top candidate.

Тест спочатку пройшов на архіві `0936cf1`, потім на блочній реалізації.
Debug і Release проходять **57/57 CTest**, Python suites **15/15** та **5/5**.
Новий Release test має активні assertions (`-UNDEBUG` після `-DNDEBUG`).
Окремий запуск із AddressSanitizer + UBSan завершився exit 0 без діагностик.
`BUILD_TESTING=OFF` збирається і реєструє 0 тестів; усі сім camera/output flags
OFF у кожній із трьох конфігурацій. Builds інкрементальні на попередніх чистих
каталогах; нового Pi/MSVC acceptance цей крок не містить.

## Виміри на записах

Незмінений benchmark і comparison runner перевірили **44 сценарії**:
шість маршрутів у трьох self-режимах і 13 PGM у двох режимах. Усі поля match
та top candidates збігаються до/після в кожному повторі: **69741 вимірюване
self-query та 234 PGM-query на варіант**, прогрівальні queries не враховані.

| Режим | До, ms | Після, ms |
|---|---:|---:|
| 13 PGM, глобальне порівняння | 2.727 | 0.579 |
| 13 PGM, глобальне порівняння + scale refinement | 3.504 | 1.381 |
| 6 маршрутів, self-global | 1.949 | 0.339 |
| 6 маршрутів, self-window30 | 0.245 | 0.031 |
| 6 маршрутів, self-window30-scale | 0.248 | 0.031 |

Для PGM у таблиці — медіана 13 медіан, кожна з дев'яти вимірюваних queries
(три процеси × три проходи). Для self — медіана шести маршрутних медіан p50
steady queries; перший query кожного проходу виключений. Маршрути різної
довжини, тому self-global не є часом довільного маршруту. Медіана парних
відношень після/до: `pgm=0.212`, `pgm-scale=0.392`, `self-global=0.161`,
`self-window30=0.126`, `self-window30-scale=0.126`.

Окремий незмінений edge-cache runner запущено сімома свіжими процесами
кожного варіанта на кожному із шести маршрутів. Match, progress zones,
перша та повторна edge-діагностика однакові. На останньому 600-frame маршруті
медіана повторної edge-діагностики зменшилася приблизно `2.73 → 0.58 ms`.
Зменшення RSS не встановлено: кеш і його життєвий цикл не змінювалися.

Обидві версії зібрані однаковим GCC Release набором
`-std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -pthread`, з тим самим harness
та окремими source/header до/після. Baseline — `git archive 0936cf1 core`.
У matcher comparison порядок before/after чергується; кожен із трьох свіжих
процесів виконує один прогрівальний і три вимірювані проходи. Час охоплює
лише `match`, без завантаження маршруту та конструктора, без timestamp pacing.
Усі 19 вхідних файлів перевірені за SHA-256; PGM прив'язані до маршрутів через
явні поля журналів, не індекс у назві файла.

Це WSL2 x86_64 виміри обчислень на збережених даних. Вони не встановлюють
live latency, швидкодію Pi або точність повторного проходу. Незалежний повний
повторний прохід із розміткою відсутній серед перевірених локальних записів.

## Відтворення й артефакти

Локальна робота почалася 2026-09-25; артефакти збережено в ignored
`artifacts/raw-distance-20260925/`: `profile.py`, `profile-results.json`,
`after-profile-results.json`, `variants/`, `block-variants/`, baseline archive,
окремі executable, compiler assembly, `software-tests.log`, `sanitized-test.log`,
`comparison/summary.json`, `edge-comparison/summary.json`, `timing-summary.*`.
Comparison зберігає executable/input/source hashes; перед commit повторно
перевірено, що виміряні source files не змінилися.

```sh
python3 scripts/compare-scale-refinement.py \
  --before artifacts/raw-distance-20260925/before-benchmark \
  --after artifacts/raw-distance-20260925/after-benchmark \
  --inventory artifacts/board-inventory-20260922 \
  --output-dir artifacts/raw-distance-20260925/comparison-repeat
python3 scripts/compare-edge-cache.py \
  --before artifacts/raw-distance-20260925/before-edge-benchmark \
  --after artifacts/raw-distance-20260925/after-edge-benchmark \
  --inventory artifacts/board-inventory-20260922 \
  --output-dir artifacts/raw-distance-20260925/edge-comparison-repeat
bash scripts/test-software.sh artifacts/config-validation-20260921/clean-build
```

## Область впливу і наступний крок

Pre-edit GitNexus impact повернув неповний `UNKNOWN`. Після оновлення індексу
unstaged `detect_changes` показав `critical`/208 процесів, усі приписані
`normalized_mean_absolute_difference` з порожнім ID, включно зі сторонніми
JSX-процесами. `context` також помилково приписує функції сторонні
navigator/publisher accesses. Прямий перегляд підтвердив три callers вище;
рівність перевірено для кожного з них. Graph output збережено в
`graph-unstaged-review.json`; цей неповний граф не є доказом acceptance.
Graphify оновлено до 5779 nodes / 8856 edges, GitNexus CLI — до
12644 nodes / 24485 edges; FTS недоступний.

Фінальний staged scope містить шість очікуваних файлів: matcher, реєстрацію
тесту, сам тест і три документи. `detect_changes(staged)` показав 163 процеси,
знову всі лише через empty-ID raw helper. Порівняння всієї гілки з `main`
охопило 43 файли, включно з попередніми кроками; граф приписав усі 208 процесів
іншому empty-ID helper — `live_route_match_update_endpoint`. Це та сама
аномалія атрибуції, а не нова зміна endpoint у цьому кроці. Прямий staged diff
обмежує production-зміну одним циклом; `git diff --cached --check` пройшов.
Деталі збережено в `graph-final-review.json`.

Після оптимізації ті самі діагностичні таймери дають медіанні частки:
raw distance **43.28%**, scale kernel **52.79%**, candidates **1.24%**.
Кількість викликів збережено. Дані не обґрунтовують окремої оптимізації
вставки кандидатів зараз. Наступний обмежений крок — перевірити наявні
offline API й тести для контрольованого проходу `match → progress →
endpoint/readiness` зі змінами часу та відмовами, використовуючи фактичні
функції проєкту. Спочатку встановити покриття й конкретну прогалину;
не переносити весь camera runtime і не змінювати policy в межах цієї роботи.
