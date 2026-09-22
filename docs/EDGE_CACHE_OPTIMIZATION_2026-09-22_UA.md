# Відкладене створення кешу контурів — 2026-09-22

Для маршруту з 600 кадрів `160×100` matcher після створення займає приблизно
на **9.2 MiB менше резидентної пам'яті процесу**. Кеш контурів тепер створюється
лише при першому `probe_edge_diagnostics`. Звичайний `match` ним не користується.
Коли edge-діагностика потрібна, перший виклик оплачує побудову кешу:
приблизно 20–22 ms замість 2.8–3.0 ms на цьому WSL-комп'ютері.

## Реалізація та межі зміни

- Конструктор відразу перевіряє формат, ненульові розміри та повний payload
  кожного route entry, включно з останнім. Типи/тексти помилок збережено.
- `std::call_once` синхронізує одноразову побудову. Готовий кеш публікується
  після успішної побудови всіх елементів; виняток дозволяє повторну спробу.
  Це зберігає можливість паралельних `const`-викликів діагностики.
- Копії matcher ділять кеш незмінного маршруту через `shared_ptr`; їхній стан
  відстеження і top-candidate результати залишаються окремими. Копіювання,
  переміщення та присвоєння доступні. CMake явно підключає `Threads::Threads`.
- Алгоритм `match`, raw/scale distance kernels, candidate ordering, progress/zone
  scoring та hardware/output consumers не змінені. Нової гарантії паралельного
  виклику stateful `match` немає.

## Порівняння з початковим кодом

Baseline узято з commit `a33217aae20d0efb1556bef5751b7c14f95cd7cf` через
`git archive`, без зміни робочих файлів. Однаковий новий вимірювач зібрано
проти початкових і поточних `gray8_route_matcher.cpp`/`route_signature.cpp`:
GCC 13.3, `-std=c++20 -O3 -DNDEBUG -pthread`, WSL2 x86_64, Intel i5-10300H.

Для кожного з шести hash-verified маршрутів виконано сім запусків кожного
варіанта в нових процесах із чергуванням порядку before/after. Зчитування VHRS
та створення вхідного кадру відбуваються до таймера. Маршрут передається
конструктору через `std::move`, тому час **не включає копіювання маршруту**.
Потім виконуються один match середнього кадру, перша і повторна edge-діагностики.
Порівнюються всі top candidates, п'ять zones, їхні оцінки та основний match.
Результати збігаються між варіантами й усіма сімома повторами.

RSS — поточне `Rss` із `/proc/self/smaps_rollup`, а не maximum RSS.
Це пам'ять цілого процесу, включно з уже завантаженим маршрутом, кодом і allocator.
Невеликі відмінності можуть залежати від сторінок/алокатора. Час читання RSS
не входить до таймерів. CPU frequency/affinity не фіксувалися.

Значення нижче — медіани семи свіжих процесів; кожна пара означає **до → після**.

| Маршрут | Кадрів | RSS після конструктора, KiB | Конструктор без копії, ms | Перша edge-діагностика, ms | Повторна edge-діагностика, ms |
|---|---:|---:|---:|---:|---:|
| 20260708T174000Z | 300 | 12984 → 8296 | 8.431 → 0.002 | 1.361 → 9.996 | 1.375 → 1.348 |
| 20260709T151301Z | 300 | 12984 → 8296 | 9.100 → 0.002 | 1.386 → 9.744 | 1.344 → 1.370 |
| 20260709T161017Z | 600 | 22396 → 13004 | 16.867 → 0.003 | 2.789 → 19.794 | 2.691 → 2.664 |
| 20260710T154326Z | 183 | 9320 → 6460 | 5.110 → 0.001 | 0.834 → 5.929 | 0.835 → 0.824 |
| 20260710T155821Z | 600 | 22396 → 13000 | 17.204 → 0.003 | 2.758 → 20.008 | 2.661 → 2.936 |
| 20260712T164651Z | 600 | 22396 → 13004 | 18.368 → 0.003 | 3.016 → 21.608 | 2.828 → 2.895 |

На останньому маршруті RSS після першої діагностики — 22412 → 22416 KiB:
економія майже зникає, коли кеш використано. Для програми, яка завжди викликає
edge-діагностику, робота переноситься з конструктора в перший виклик; загального
прискорення цієї послідовності не заявлено. Це не FPS усього pipeline або Pi.

Попередні `run1/run2` виміри з копією маршруту використовували іншу методику
і несинхронізований проміжний варіант. Їхні середні 12.3–12.8 → 1.4–2.5 ms
не є підсумковим доказом пам'яті/першого edge-виклику; для цього служить
нове порівняння вище. Початковий [звіт matcher](MATCHER_BENCHMARK_2026-09-22_UA.md)
зберігає первинні результати без підміни історичного baseline.

## Відтворення та артефакти

Linux/WSL executable `gray8_edge_cache_benchmark ROUTE.vhrs` виводить JSON
із часом, RSS і результатами. `scripts/compare-edge-cache.py` чергує запуск
двох executable-файлів, перевіряє hashes маршрутів та рівність результатів.
Output directory має бути новою.

```bash
# Архів створити в новому каталозі artifacts/edge-cache-repeat/baseline-source.
mkdir -p artifacts/edge-cache-repeat/baseline-source
git archive --format=tar --output=artifacts/edge-cache-repeat/baseline.tar a33217a core
tar -xf artifacts/edge-cache-repeat/baseline.tar -C artifacts/edge-cache-repeat/baseline-source
g++ -std=c++20 -O3 -DNDEBUG -pthread \
  -Iartifacts/edge-cache-repeat/baseline-source/core/include \
  core/tools/gray8_edge_cache_benchmark.cpp \
  artifacts/edge-cache-repeat/baseline-source/core/src/gray8_route_matcher.cpp \
  artifacts/edge-cache-repeat/baseline-source/core/src/route_signature.cpp \
  -o artifacts/edge-cache-repeat/before
g++ -std=c++20 -O3 -DNDEBUG -pthread -Icore/include \
  core/tools/gray8_edge_cache_benchmark.cpp core/src/gray8_route_matcher.cpp \
  core/src/route_signature.cpp -o artifacts/edge-cache-repeat/after
python3 scripts/compare-edge-cache.py \
  --before artifacts/edge-cache-repeat/before --after artifacts/edge-cache-repeat/after \
  --inventory artifacts/board-inventory-20260922 \
  --output-dir artifacts/edge-cache-repeat/results
```

Фактичні артефакти: `artifacts/edge-cache-review-20260922/` — baseline archive,
окремі executable-файли, `comparison/summary.json`, шість JSON із сирими даними,
`software-tests.log` і `tsan.log`. Summary містить шляхи та SHA-256 executable,
script hash, platform, source HEAD і hashes маршрутів. Дані та generated artifacts
не додаються в Git. Борт у цьому кроці не використовувався.

## Перевірка

- Новий `gray8_edge_cache` test спочатку пройшов на початковому eager-коді:
  це characterization попередньої поведінки. Перевіряє constructor rejection,
  невдалий запит перед першою діагностикою, точні edge/zone результати та нічиї,
  повторні запити, незалежність match, copy/move/assignment і одночасні const probes.
- Поточні WSL Debug та Release — **54/54 CTest**, Python suites — **15/15 і 5/5**.
  Усі сім camera/output flags OFF. Assertions активні у всіх 49 стандартних
  Release test targets. Ці builds інкрементальні
  на попередніх чистих builds. Окремий `BUILD_TESTING=OFF` build вимірювача
  пройшов і реєструє нуль тестів.
- Окремий ThreadSanitizer build тесту завершився з exit 0 без повідомлень:
  вісім потоків одночасно починають роботу й виконують по десять перевірок.
- Повний локальний matcher benchmark повторено з фінальним кодом:
  `matcher-results/summary.json` у каталозі артефактів. Усі **31/31** result hashes
  збігаються з початковим `run1`: 18 комбінацій шістьох маршрутів/трьох режимів
  і 13 stop-frame знімків. Це 23247 виміряних self-запитів плюс 39 stop-запитів;
  warmup до підрахунку не входить. Тіла `match`, `recent_top_candidates` і
  `probe_progress_zones` додатково звірено з baseline — вони незмінені.
- Висновок про real second-pass recognition accuracy цим кроком не оновлено;
  Pi/MSVC/hardware acceptance також потребують окремих перевірок.

Graphify та CLI-індекс GitNexus оновлено. Перед комітом `detect_changes`
повернув `critical` і 209, при повторі 154 affected processes. Перевірка виявила
помилкове зіставлення: `initialized` має порожній symbol ID, а `context` приписує
йому доступи з `BoundedNavigator::update`, яких немає в цьому коді. У повторному
звіті 152/154 процеси позначені лише через це поле. Фактичні посилання на приватний
`EdgeCache` перевірено безпосередньо; staged scope містить лише десять очікуваних
файлів. Цей graph risk не вважається достовірною оцінкою впливу. Приклад збережено
в `gitnexus-detect-summary.json`; код під результат інструмента не перейменовували.
