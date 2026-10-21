# Скорочення Python overhead у Dataset.path

Дата: `2026-10-21`. Початковий checkpoint: `74d73bd`.

## Зміна та її межі

Обмежений slice із [handoff](SESSION_HANDOFF_2026-10-20_UA.md): усунути
зайву роботу Python у `Dataset.path`, зберігши чинний файловий контракт.
Production change обмежена `scripts/validate-second-pass-dataset.py`.

- Один compiled regex перевіряє backslash, colon, comma та U+0000–U+001F
  замість двох Python generator scans. Перевірки nonempty string, зовнішніх
  і component-edge Unicode whitespace, absolute/empty/dot/parent components
  лишаються. DEL і interior Unicode spaces не стали забороненими.
- Після свіжого strict resolve обчислюється різниця кількості components.
  Якщо вона додатна, з root порівнюється лише предок відповідної глибини,
  використовуючи ту саму `Path` equality. Інші предки не можуть дорівнювати
  root за кількістю components. Root itself відхиляється. Це зберігає native
  POSIX case sensitivity та Windows drive/UNC/case semantics; строковий
  prefix не використовується.
- `resolve(strict=True)`, regular-file `stat`, `open/read/fstat`, SHA-256
  прочитаних bytes, expected hashes та repeated-read hashes залишаються
  на тих самих межах. Немає кешу resolved paths, stat або content hashes.
  Runner guards, admission, previous-bundle checks і final audit незмінені.

Validator дозволяє внутрішні symlinks, якщо fresh resolved target є regular
file всередині root. Closed bundle/collection/archive inventory окремо
відхиляє всі symlinks. Root aliases resolve під час створення Dataset;
пізніша підміна ancestor знову перевіряється. Зміна не обіцяє захист від
довільних конкурентних adversarial filesystem races між перевіркою й open.

Прямо прочитано `resolved`, `Dataset.path/read/artifact`, exporter
`binding/verify_frozen/copy_checked`, runner `guard`, bundle inventory та
callers у scorer/collection/outer audit. GitNexus metadata досі `6d8f547`;
для цього slice використано direct source/diff/tests, без graph safety claims.

## Перевірка еквівалентності

Новий `scripts/tests/test_second_pass_paths.py`: 12 tests, включені до
software runner. Той самий набір проходить на початковій та зміненій
реалізації. 783 character/position cases, portable syntax, реальні nested
names/case, strict ancestor semantics, drive/UNC/root/case/deep paths,
internal links/root aliases, prefix sibling escape, missing/cycle,
підміна link/root, directory/FIFO/socket, same-size/same-mtime content
mutation, expected digest і зміна під час читання.

Pure Windows path cases виконуються у WSL: це перевірка `pathlib` semantics,
не native Windows filesystem/junction acceptance. Синтетичні fixtures
не доводять фізичної незалежності captures або accuracy matcher.

## Before/after measurement

Використано незмінений [scaling probe](SECOND_PASS_SCALING_2026-10-18_UA.md):
свіжий baseline поруч із candidate, 12/24/48 runs × 3 rounds, окремі warmup
і 48-run profile, fresh Python workers, native WSL `/tmp`, той самий Release
binary. Під час timed runs не запускали full software suite або інший важкий
benchmark. Host load/CPU clocks не фіксовані; порядок before → after не є
рандомізованим crossover experiment. Усі samples зберігаються.

API wall time виключає imports/fixture setup/oracles/persistence. Worker
peak RSS включає imports, але виключає replay children і не є tree peak.
Dataset/frame size сталий: три splits, 8 кадрів, 2×2; росте run count.
Числа не переносити на Pi, Windows filesystem, довгі реальні маршрути
або sensor/live latency.

Медіана трьох samples; у дужках мінімум–максимум часу, секунди:

| Runs | Фаза | Before | After | Зменшення медіани часу |
| ---: | --- | ---: | ---: | ---: |
| 12 | Campaign | 4.519 (4.453–4.750) | 3.928 (3.632–4.771) | 13.1% |
| 24 | Campaign | 12.646 (11.974–13.517) | 10.694 (10.326–10.774) | 15.4% |
| 48 | Campaign | 38.599 (38.346–41.298) | 32.257 (32.210–32.669) | 16.4% |
| 12 | Audit | 1.254 (1.199–1.285) | 1.050 (0.992–1.166) | 16.2% |
| 24 | Audit | 2.593 (2.339–3.023) | 2.171 (2.157–2.358) | 16.3% |
| 48 | Audit | 4.680 (4.461–5.277) | 4.073 (3.812–4.162) | 13.0% |

12-run candidate sample 4.771 с повільніший за всі три baseline samples;
його збережено. Три повтори і послідовний before/after порядок не дають
performance guarantee. Для 24/48-run campaigns діапазони цього вимірювання
не перекриваються. Усі raw samples і RSS ranges є в summary/comparison JSON.

| Runs | Campaign RSS before → after, MiB | Audit RSS before → after, MiB |
| ---: | ---: | ---: |
| 12 | 31.09 → 31.25 | 32.29 → 32.20 |
| 24 | 32.17 → 32.07 | 35.88 → 35.66 |
| 48 | 34.67 → 34.44 | 43.41 → 43.06 |

Це медіани worker high-water marks; суттєвого memory improvement не заявляємо.

Окремий 48-run profile (час із profiler overhead): campaign 80.911 → 65.755 с;
`Dataset.path` 54.855 → 39.907 с cumulative, `guard` 60.160 → 46.178 с.
Audit 11.080 → 9.875 с; `Dataset.path` 4.385 → 3.302 с, inventory
3.919 → 3.857 с. Вкладені cumulative times перекриваються.

Кількості викликів до/після точно збігаються, зокрема campaign:
`Dataset.path` 269582, `guard` 147, `Path.resolve` 277335,
`Path.stat` 279013, `lstat` 2628372, buffered read 632106 і hash update
644598. Audit: path 22583, resolve/stat 23574, lstat 226692,
buffered read 71314, hash update 74902, inventory 100. Це разом із direct
diff і mutation tests підтверджує, що перевірки не скорочувалися.

## Локальні докази

Повний WSL software runner після патчу: Debug/Release CTest **64/64**,
telemetry CLI **7/7**, exporter **21/21**, collection runner **23/23**,
campaign **4/4** кожна; readiness **15/15**, recorded-route benchmark
**5/5**, validator **31/31**, paths **12/12**, scorer **27/27**, bundle
**24/24**, collection **27/27**, outer audit **30/30**. Campaign regressions
охоплюють failed/interrupted/pending slots, незмінені denominators і
no-execution audit. Symlink tests у WSL не skipped.

Збережено **235** source hashes. Із попередніх 234 змінилися тільки
validator, software runner та tests README; додано path tests. C++/config
незмінені, сім camera/output flags OFF у Debug/Release. Нові native
sanitizer/NoTests/MSVC/hardware runs не виконувалися.

Hash-bound tar readback і повторний relocation audit пройшли для всіх
11 archives кожної версії: разом 624 runs / 4992 frame-run pairs. Prediction
hashes, повні metrics та coverage ідентичні before/after. Перевірено actual
source frame IDs/timestamps, peers і policy-dependent denominators. Audit
з plan/state anchors відтворює saved reports, не змінюючи archive bytes/mtime;
контрольна втрата slot або зміна TP відхиляється measurement oracle.

Tar SHA-256:

- Before: `1af17198a70684f03eed9dee4f7578005ab169ab4b495bd9fa3dd82514660b2e`.
- After: `01cc6ab007f2a3add8402281bb080ce869c9deae8d28ac762f7ec13f1a1446f2`.
- Незмінний Release binary: `a6f8c9f3ead1c5b1384bfae9f60ace667cc14195d2cab06f7633fccbf8ac2bd6`.

`artifacts/path-overhead-20261021/`: before/after summary з усіма samples,
raw profiles, hash-bound native tar archives, baseline trusted source copy,
readback/relocation verification, equivalence report, path-test logs,
software log і tested-source hashes. Великі artifacts ignored і локальні.

Зміна trusted Python bytes очікувано змінює frozen tool hashes. Baseline
archives перевіряються окремою попередньо збереженою trusted source copy,
звіреною з baseline hashes; candidate — поточним trusted checkout.
Archived Python/binary під час read-only перевірки не виконуються.

Наступний bounded slice — characterization і скорочення побудови relative
paths у closed archive `inventory()`: він тепер займає близько 39% profiled
audit time. Перед реалізацією звірити callers, directory traversal, Windows/
POSIX names, symlink/nonregular/extra-file rejection і mutation semantics.
Зберегти кожен fresh `scandir`/`stat`, closed inventory, hashes, guards та
повторні перевірки. Вимагати equivalence/full suite і ту саму before/after
матрицю; за відсутності виграшу не приймати production patch. Resolve/I/O
і квадратичний обсяг повторних guards лишаються витратами, не дозволом
послабити checks. Повна довгострокова черга збережена в handoff.
