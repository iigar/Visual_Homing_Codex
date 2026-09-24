# Дострокове завершення уточнення масштабу — 2026-09-24

Уточнення масштабу тепер припиняє пошук, коли подальші обчислення математично
не можуть змінити переможця. Це оптимізація часу обчислень зі збереженням
наявних оцінок, порогів та порядку вибору при однакових оцінках.

## Чому зміна еквівалентна

`scaled_normalized_mean_absolute_difference` повертає невід'ємне число або
нескінченність за відсутності перекриття. Він не змінює стан. Тому:

- після нульової похибки перевірка наступних масштабів того самого кадру зайва;
- якщо поточна **рангова** оцінка `best_distance <= 0`, наступні кадри refinement
  не можуть виконати умову заміни `distance < best_distance`;
- рангова оцінка може відрізнятися від raw distance через directional bias.
  Саме тому перевіряється рангова оцінка, а не confidence або raw distance.

Початковий coarse scan повністю виконується: перевіряє кадри, формує top candidates
і враховує directional bias. Список 24 масштабів, сам pixel kernel, границі
refinement window, напрямок та стан matcher збережені. Наближеного порога/epsilon
немає. Production diff обмежено двома функціями `core/src/gray8_route_matcher.cpp`:
шість доданих рядків і одна заміна умови циклу.

## Контроль поведінки

Новий `gray8_scale_refinement_test` спочатку зібрано та успішно виконано проти
архівованого production-коду `6da72f435a69c33b27b17ae8b1e49138d037d716`, потім
проти зміненого matcher. Він перевіряє:

- coarse exact winner перед раннішим кандидатом з таким самим scaled score;
- однакові нульові оцінки сусідів, масштаб 1.5 та нуль уже на першому масштабі;
- відсутність перекриття, додатну похибку, незмінні raw top candidates;
- додатну, нульову й від'ємну рангові оцінки у двох напрямках;
- відхилення низької confidence та збереження попереднього стану після нього;
- timestamp, progress, direction observation, initial progress window;
- відхилення несумісних розмірів пізнішого coarse entry навіть після exact match.

Окремо зафіксовано наявну поведінку: при reverse bias `1.0` ранніший кадр із raw
distance `1` може мати ту саму нульову рангову оцінку, що й exact кадр. Перший
виграє за правилом strict `<`, але має нульову confidence. Початкове очікування
нового тесту було уточнено після запуску на baseline; алгоритм цього вибору
в межах оптимізації не змінювався.

WSL/GCC 13.3: Debug і Release **55/55 CTest**, Python readiness **15/15**, benchmark
helpers **5/5**. Новий Release test має `-UNDEBUG` після `-DNDEBUG`. Збірка
`BUILD_TESTING=OFF` успішна, реєструє **0** тестів. Усі сім camera/output flags OFF.
Логи: `artifacts/scale-refinement-20260924/software-tests.log` та `no-tests-build.log`.

## Методика порівняння

Локальні hash-verified дані: шість маршрутів, загалом 2583 кадри Gray8 `160×100`,
та 13 stop-frame PGM. Прив'язка PGM до маршруту береться з явних start/done полів
того самого історичного журналу. Назва кадру не вважається незалежною розміткою.

Той самий оновлений `gray8_matcher_benchmark.cpp` зібрано окремо з baseline
source/header та зі зміненими source/header, однаковими прапорцями
`g++ -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -pthread`. В обох executable
прямо включено відповідні `gray8_route_matcher.cpp`, `route_signature.cpp`,
`replay_frame_source.cpp`; production-бібліотеки двох версій не змішуються.

Новий режим `pgm-scale` вмикає refinement для незалежного одиночного PGM.
JSONL тепер містить усі поля `RouteMatch` і `RouteMatchCandidate`, включно
з timestamp/progress/direction; тому нові result hashes порівнюються між цими
двома однаковими harness, а не з хешами старої, вужчої JSON-схеми.

На кожен сценарій — три свіжі процеси кожного варіанта, порядок before/after
чергується. У процесі один прогрівальний і три вимірювані проходи з новим matcher
для кожного проходу. `steady_clock` охоплює лише `match`, без I/O/копіювання кадру.
Перша query виділена окремо; steady percentiles — nearest rank без першої query.
Результати кожного проходу та кожного процесу порівнюються точно, без tolerance.
Маршрутні timestamps тут не відтворюються в реальному часі.

## Виміри

Усі **44 сценарії** дали однакові результати в усіх процесах і проходах:
18 маршрутних (6 × 3 режими) та 26 одиночних PGM (13 × 2 режими).
На кожен варіант виконано **69741 вимірюване self-зіставлення** та **234 PGM**;
прогрівальні проходи до цих чисел не входять.

WSL2 x86_64, Intel Core i5-10300H, GCC 13.3. Таблиця для `self-window30-scale`;
кожне число — медіана відповідного process percentile з трьох свіжих процесів.
Назви файлів мають вигляд `field-route-<timestamp>.vhrs`.

| Маршрут | Кадрів | p50 до, ms | p50 після, ms | p95 до, ms | p95 після, ms |
|---|---:|---:|---:|---:|---:|
| 20260708T174000Z | 300 | 11.253 | 0.240 | 12.07 | 0.33 |
| 20260709T151301Z | 300 | 11.273 | 0.240 | 12.23 | 0.31 |
| 20260709T161017Z | 600 | 11.199 | 0.240 | 11.96 | 0.31 |
| 20260710T154326Z | 183 | 11.172 | 0.235 | 12.19 | 0.32 |
| 20260710T155821Z | 600 | 11.246 | 0.246 | 12.09 | 0.33 |
| 20260712T164651Z | 600 | 11.313 | 0.241 | 12.51 | 0.32 |

Це скорочення медіанного часу приблизно у **46–48 разів саме для exact self-match**:
дорогий уточнювальний пошук більше не запускається після вже знайденого нуля.
Контрольні режими `self-global` та `self-window30` мають медіану парних відношень
часу після/до відповідно `0.998` та `1.000`; виграш для них не заявляється.

На 13 stop-frame PGM зі refinement усі результуючі confidence нижчі за `1`,
тож нульова похибка не досягається. Медіана часу по 13 знімках:
**14.225 → 14.076 ms**; медіана парних відношень після/до `0.994`, діапазон
`0.953–1.031`. Це не свідчить про помітне прискорення цього набору.
Для звичайного `pgm` без refinement медіана парних відношень — `1.000`.
Час виміряно на робочому WSL-комп'ютері, без гарантій real-time latency.

Повні process percentiles, raw timings та результати збережено у
`comparison/summary.json` і JSONL; зведені числа — `timing-summary.json` / `.txt`.

## Повторення

З кореня репозиторію у WSL/Linux, після збирання двох executable з однаковим
harness і різними production source/header:

```sh
python3 scripts/compare-scale-refinement.py \
  --before artifacts/scale-refinement-20260924/before-benchmark \
  --after artifacts/scale-refinement-20260924/after-benchmark \
  --inventory artifacts/board-inventory-20260922 \
  --output-dir artifacts/scale-refinement-20260924/comparison-repeat
bash scripts/test-software.sh artifacts/config-validation-20260921/clean-build
```

Baseline archive отримано командою `git archive --format=tar --output=... 6da72f4 core`.
Для компіляції benchmark використовуються наведені вище прапорці та чотири
translation units: поточний tool і три source-файли відповідного варіанта;
`-I` вказує на `core/include` саме того варіанта. Для baseline characterization
той самий новий test компілюється з baseline matcher та `-UNDEBUG`.

Архів початкового `core/`, вихідні JSONL та підсумковий `comparison/summary.json`
з executable/source/input SHA-256 збережені під ignored
`artifacts/scale-refinement-20260924/`; сирі записи не додаються до Git.

GitNexus impact перед змінами — неповний `UNKNOWN`, без визначених callers/processes.
За прямим source review helper викликається з `Gray8RouteMatcher::match`; споживачі
matcher є у `camera_smoke`, `pipeline_harness`, `route_artifact_check`, тестах
і benchmark. Graphify оновлено після зміни; GitNexus CLI також переіндексовано,
але FTS/BM25 лишається недоступним. Граф не замінює аналіз умови раннього виходу
та порівняння виконуваних реалізацій.

`detect_changes` для production diff повернув `critical` і 163 процеси, але всі
163 приписано лише helper з порожнім symbol ID, включно зі сторонніми JSX-процесами.
`context` також помилково показав доступи до `bounded_verification_publisher`
і `dry_run_command_sink`. Пошук у C++ source/header знаходить лише визначення
helper в anonymous namespace та один виклик у `match`. Це дефект прив'язки
графа, а не підтверджені 163 залежності; статус не трактувався як пройдена
перевірка. Збережено `gitnexus-unstaged-summary.json` у каталозі артефактів.
Фінальна staged-перевірка охопила 8 потрібних файлів і повернула 208 процесів,
усі з тією самою помилкою empty-ID helper. Порівняння всієї гілки з `main`
охопило 39 файлів попередніх кроків; 208 процесів приписано empty-ID секції
README `Local Software Validation`. Ці звіти теж збережено як
`gitnexus-staged-summary.json` та `gitnexus-compare-summary.json`.

Ці дані перевіряють еквівалентність та локальну вартість обчислень. Self-кадри
маршруту не визначають точність повторного проходу; 13 окремих PGM не є повним
повторним проходом. Pi/MSVC/hardware performance цією перевіркою не підтверджені.
