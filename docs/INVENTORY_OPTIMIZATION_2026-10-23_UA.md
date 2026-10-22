# Скорочення побудови відносних шляхів у closed inventory

Початок: `2026-10-22`, завершення: `2026-10-23`. Початковий checkpoint:
`27532ceb92da8379bb806feffcbb0169eee7d18f`, remote SHA перевірено.
Продовження [плану після Dataset.path](DATASET_PATH_OPTIMIZATION_2026-10-21_UA.md).

## Зміна та файловий контракт

Production change обмежена `inventory()` у `scripts/check-second-pass-bundle.py`.
Стек обходу тепер зберігає native directory path і відносний POSIX prefix.
Для кожного запису prefix додається до `DirEntry.name`, який є одним native
component. Це усуває повторні `Path(entry.path).relative_to(root).as_posix()`
та створення `Path` для каталогу. Регістр, Unicode, literal POSIX backslashes,
приховані файли й порожні каталоги зберігаються в inventory.

Кожен каталог знову проходить `os.scandir`; кожен запис —
`entry.stat(follow_symlinks=False)` у тому самому порядку. Symlink/nonregular
rejection, спільний file/directory limit 200000, stack traversal, error
propagation і закриття scandir зберігаються. Немає кешу filesystem metadata
або hashes. Усі callers, membership/extra-file checks, content hashes,
runner guards, admission і final audits незмінені.

Прямо прочитано bundle/collection/runner audit, runner guard і mutation tests.
Graph metadata досі `6d8f547`; використано source/caller/diff/tests fallback.
Inventory перелічує імена; portable dataset path validation лишається окремою
перевіркою. Contained dataset symlinks і суворе archive symlink rejection не
змішуються. Root aliases приймаються як раніше; public checkers передають
resolved Dataset root. Захист від довільної конкурентної підміни filesystem
між stat і наступним scandir не додано й не заявляється.

## Characterization і перевірка

Нові `scripts/tests/test_second_pass_inventory.py`: **12/12** до й після.
Набір включено до `scripts/test-software.sh`. Перевірено:

- exact nested/empty/hidden membership, Unicode/case, POSIX names із backslash,
  colon/comma/whitespace/control characters, 80 levels і 50 sibling files;
- relative/string/dot/root-alias inputs та simulated POSIX/Windows drive/UNC
  DirEntry paths; останнє не є native Windows/junction acceptance;
- internal/external/file/directory/broken/cyclic symlinks, FIFO/socket,
  спільний file/directory limit на межі;
- fresh scandir і no-follow stat для кожного запису в повторних викликах,
  відсутність content reads усередині inventory, membership/type replacement,
  removal між enumeration і stat, errors та закриття scanner.

Існуючі bundle/collection/outer-audit suites перевіряють зайві files/directories,
mutation під час повної перевірки, hashes, frozen toolchain, readonly relocation
і відсутність виконання archived code. Campaign suite перевіряє failed,
incomplete та pending slots і незмінені denominators.

Повний WSL software runner після патчу пройшов: Debug/Release CTest **64/64**,
telemetry CLI **7/7**, exporter **21/21**, collection runner **23/23**,
campaign **4/4** кожна; readiness **15/15**, recorded-route benchmark **5/5**,
validator **31/31**, paths **12/12**, inventory **12/12**, scorer **27/27**,
bundle **24/24**, collection **27/27**, outer audit **30/30**. Немає skipped
symlink/special-file cases у WSL. Нові native sanitizer/NoTests/MSVC/hardware
runs не виконувалися.

Зафіксовано **236** core/scripts/config hashes. Із попередніх 235 змінилися
лише bundle checker, software runner і tests README; додано inventory tests.
C++/config незмінені; сім camera/output flags OFF у Debug і Release.

## Метод вимірювання

Незмінений [scaling probe](SECOND_PASS_SCALING_2026-10-18_UA.md): нові before і
after directories, 12/24/48 runs × 3 rounds, окремі 12-run warmup і 48-run
profile, той самий Release binary. Fresh Python workers, native WSL `/tmp`,
warm cache, три related synthetic datasets, 8 кадрів і 2×2 pixels. Під час
timed runs full software suite та readback audit не запускалися.

Wall time охоплює API call, виключає imports/setup/oracles/persistence.
RSS — high-water mark Python worker із imports, без replay children; це не
whole-process-tree peak. Host load/CPU clocks не фіксувалися. Before і after
виконані послідовно, не randomized crossover; три повтори не дають performance
guarantee. Усі samples збережено. Ці виміри не є Pi, native Windows, real-route
latency, фізичною незалежністю captures або matcher accuracy evidence.

## Результат before/after

Медіана трьох samples; у дужках minimum–maximum, секунди:

| Runs | Фаза | Before | After | Зменшення медіани |
| ---: | --- | ---: | ---: | ---: |
| 12 | Campaign | 4.302 (4.119–4.555) | 3.801 (3.749–4.093) | 11.7% |
| 24 | Campaign | 10.975 (10.856–11.249) | 10.087 (9.989–10.242) | 8.1% |
| 48 | Campaign | 31.605 (31.403–31.897) | 31.291 (31.134–31.370) | 1.0% |
| 12 | Audit | 1.101 (1.045–1.167) | 0.755 (0.748–0.860) | 31.4% |
| 24 | Audit | 2.064 (2.053–2.186) | 1.565 (1.497–1.718) | 24.2% |
| 48 | Audit | 4.049 (4.036–4.107) | 3.009 (2.947–3.075) | 25.7% |

Найчіткіший ефект у цій матриці — audit. Зміна 48-run campaign лише 1.0%,
і її не слід подавати як надійну performance guarantee. Before/after host
conditions не контрольовані; навіть неперекриття трьох samples не усуває
систематичних ефектів порядку запусків.

| Runs | Campaign RSS before → after, MiB | Audit RSS before → after, MiB |
| ---: | ---: | ---: |
| 12 | 31.27 → 31.29 | 32.03 → 32.30 |
| 24 | 32.17 → 32.26 | 35.87 → 35.90 |
| 48 | 34.79 → 34.76 | 43.30 → 43.34 |

Це медіани worker RSS; memory improvement не заявляється. Усі raw samples
і ranges залишаються в summary/comparison JSON.

Окремий 48-run profile з profiler overhead: campaign 63.061 → 62.167 с;
inventory 196 calls / 3.670 → 0.165 с cumulative. Audit 10.064 → 6.320 с;
inventory 100 calls / 3.922 → 0.163 с. Guard cumulative campaign time при
цьому 43.663 → 45.577 с: guards не скорочено, різні profile timings
не є ізольованим вимірюванням лише одного алгоритму. Вкладені часи перекриваються.

Кількості викликів до/після точно збігаються:

| Виклик | Campaign | Audit |
| --- | ---: | ---: |
| inventory | 196 | 100 |
| scandir і scanner exit, кожен | 4666 | 4674 |
| DirEntry.stat (no-follow у source/tests) | 21058 | 21378 |
| Dataset.path | 269582 | 22583 |
| Path.resolve | 277335 | 23574 |
| Path.stat | 279013 | 23574 |
| posix.lstat | 2628372 | 226692 |
| buffered read | 632106 | 71314 |
| hash update | 644598 | 74902 |

Campaign guard лишається 147 calls. Source/diff, characterization і ці counts
підтверджують збереження перевірок; самі timings цього не доводять.

## Локальні докази та наступний крок

Hash-bound tar readback і повторний relocation audit пройшли для всіх
**11 archives кожної версії / 624 runs загалом / 4992 frame-run pairs**.
Prediction hashes, повні metrics і coverage ідентичні before/after. Звірено
source frame IDs/timestamps, peers і policy-dependent denominators. Anchored
audit відтворює saved reports і не змінює archive bytes/mtime. Контрольна
втрата slot або зміна TP відхиляється measurement oracle. Усі aggregates
незалежно перераховано з samples.

`artifacts/inventory-overhead-20261022/`: before/after summary, raw profiles,
hash-bound native tar archives, trusted-before source copy, characterization
logs, comparison, readback/relocation verification, equivalence, software log
і tested-source hashes. Великі artifacts ignored і залишаються локально.

Tar SHA-256:

- Before: `ba360ea15379412f1dff09f10758c398eb71838ca992005342711b68686d780b`.
- After: `cec93e3d7ce2a8136ed49ee509c47364940da0b3e6f19e3726f4d04156a533e4`.
- Незмінний Release binary: `a6f8c9f3ead1c5b1384bfae9f60ace667cc14195d2cab06f7633fccbf8ac2bd6`.

Software log SHA-256:
`5048ab70cecd27b37be2519eb98f96fd1140b1dc3a71f40ad9bb274dd8af3767`.

Зміна checker bytes змінює frozen collection/runner tool hashes. Before archives
перевіряються попередньо збереженою trusted source copy, звіреною з measurement
hashes; after — поточним trusted checkout. Archived Python/binary під час
read-only перевірки не виконуються; strict tool compatibility не послаблено.

Наступний bounded крок — окреме вимірювання frame-count scaling при сталих
12 runs / 3 datasets / resolution, із exact frame/prediction/coverage oracles
та fresh guards; потім незалежно resolution scaling. Поточні 8-frame/2×2
числа не характеризують великі routes. Новий profile має визначити наступну
production оптимізацію. Repeated resolve/read/hash і квадратичний обсяг guards
не є дозволом кешувати перевірки. Повна черга лишається в
[handoff](SESSION_HANDOFF_2026-10-20_UA.md); independent annotated captures і
hardware/output stages мають окремі вимоги й авторизацію.
