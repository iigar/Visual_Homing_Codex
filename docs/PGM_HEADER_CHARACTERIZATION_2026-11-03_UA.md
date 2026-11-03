# PGM header: characterization і поточна вартість

Дата: `2026-11-03`. Resume із remote-verified
`82e8c9100b1af24c4cb1f58c3a793b73e3d65db8`, за
[handoff 2026-10-31](SESSION_HANDOFF_2026-10-31_UA.md).

**Рішення: production parser залишено без змін.** Додано 14 characterization
tests і виміряно вартість чинного parser. У цьому обмеженому етапі candidate
не реалізували: весь `pgm`, включно з payload reading і hashing, займає лише
приблизно 5.4% instrumented audit, а header — його частина. Це рішення про
пріоритет за cost, не результат порівняння candidate і не доказ неможливості
speedup. Немає before/after, paired-candidate чи performance acceptance claim.

## Семантика, зафіксована тестами

`scripts/tests/test_second_pass_pgm.py` запускається один раз у full software
runner. Focused run: **14/14**, exit 0, без skips.

- До кожного token дозволені шість ASCII whitespace bytes і comments від `#`
  до LF; CR сам по собі comment не завершує. `#` усередині token не є delimiter.
- Magic — рівно `P5`, maximum — рівно `255`; dimensions — unsigned ASCII decimal
  до 20 digits, кожен розмір `1..65535`. Leading zeroes dimensions дозволені.
  Non-ASCII dimension викликає `UnicodeDecodeError`, який є `ValueError` і
  входить у чинний invalid-input шлях; production exception behavior не змінено.
- Після maximum читається рівно один separator або CRLF. CR + перший LF завжди
  трактується як CRLF, навіть якщо автор файлу мав на увазі LF pixel. Не можна
  strip-нути наступні whitespace/`#`: це raster. Перевірено всі 256 можливих
  перших pixel bytes, включно з окремим CRLF випадком.
- Ліміт 4096 header bytes охоплює comments, separator й optional LF. Межі
  4095/4096/4097 перевірені; помилка на 4097-й спробі не споживає цей byte.
- Перевірено справжній 64 MiB zero raster (`8192×8192`) із chunked reads до
  1 MiB, а також rejection до payload read при перевищенні bound.
- Усі truncations двох representative headers/rasters відхилені з SHA-256 саме
  спожитого prefix. Trailing rejection читає й хешує лише перший зайвий byte.
  Buffered `peek`/read-ahead не додає неспожиті bytes до digest.
- File hash включає header, comments і CRLF; pixel hash — лише raster.
  Перевірено expected hash, повторне читання після same-size/mtime зміни,
  fstat mutation guard і відсутність partial input records після parser/I/O error.

Прямо прочитано `HashedReader`, `pgm`, `Dataset.read`, caller `Dataset.frames`
і шлях validator/scorer/exporter/bundle/collection/runner audit. Також прочитано
`core/src/replay_frame_source.cpp`: спільна single-separator/CRLF convention
не означає повної acceptance parity. C++ reader більш permissive щодо деяких
числових tokens/trailing bytes; цей slice не змінює й не уніфікує readers.

## Runtime та методика

- Python `3.12.3 (main, Aug 31 2026, 10:18:26) [GCC 13.3.0]`.
- Resolved executable `/usr/bin/python3.12`, SHA-256
  `e50d468e8b0adfb05733f5b87b3cff34829c4a8c1aea50c865aa8bdfe4bb150f`.
- WSL kernel `6.6.87.2-microsoft-standard-WSL2`, native temporary storage.
  Build string збігається з warm-audit study; старий study не фіксував executable
  hash, тому рівність executable bytes між датами не заявляється.
- Той самий hash-bound `frames_per_dataset-64-repeat-00` archive із
  [large-archive baseline](LARGE_ARCHIVE_BASELINE_2026-10-30_UA.md):
  12 catalog datasets, 12 saved runs, 64 frames, 32×32 pixels, 4 references,
  **11137 files / 768 унікальних frame-run pairs**.
- Trusted current sources скопійовано на native WSL storage і звірено з baseline
  hashes. Archived toolchain/binary не виконується; subprocess і runner resume
  заблоковані під час audit. **Нових replay executions у study — 0.**
- Шість fresh workers: warmup, три unprofiled timed audits, окремий cProfile
  audit і окремий audit із timer навколо чинного `pgm`. Default GC збережено.
  Snapshots до/після кожного audit перевіряють bytes/mtime та прогрівають cache.
  API timer виключає imports/startup, snapshots, oracle checks і persistence.
- Окремий in-memory probe читає 768 PGM inputs поза timer і виконує 25 passes,
  тобто 19200 calls на sample: один warmup і сім timed samples. Усередині timer
  є stream/reader construction, весь parser і обидва hashes; filesystem/open/
  resolve/stat/mutation guards не вимірюються. Це ізоляція cost, не replacement.

## Результати

| Вимір | Результат |
|---|---:|
| Unprofiled API samples | 10.526 / 11.330 / 10.011 s |
| Median трьох samples | 10.526 s |
| Окремий cProfile audit | 22.412 s |
| `pgm` cumulative, 19200 calls | 1.408 s / 6.28% profiled API |
| `token` cumulative, 76800 calls | 0.847 s |
| `byte` cumulative, 249600 calls | 0.576 s |
| Окремий audit з PGM timer | 11.041 s |
| Сума часу всередині чинного `pgm`, 19200 calls | 0.600 s / 5.44% цього audit |
| In-memory 19200 calls, median семи samples | 0.470 s |
| In-memory range | 0.457–0.484 s |

Вкладені cumulative times не додаються. cProfile помітно змінює абсолютний
час; 0.600 s також містить вартість wrapper/timer і не є абсолютно точним
uninstrumented cost. In-memory median дорівнює приблизно 4.47% unprofiled API
median, але це різні виміри, тому ratio не є прогнозом achievable speedup.
Header-only виграш менший за вартість усього parser. Розкид трьох API samples
не є оцінкою p90/p99 і не доводить ані наявності, ані відсутності малого gain.

Усі **6/6 full normalized reports** і exact prediction/metric/coverage/peer
oracles збігаються з persisted baseline; всі 11 peers на run збережені.
Bytes/mtime archive та inputs не змінилися. Це повторні audits одного archive,
не шість незалежних captures. Worker VmHWM збережено в raw results, без
memory-improvement claim. Warm WSL evidence не є cold-cache, Windows filesystem,
Pi/live latency або physical accuracy evidence.

## Валідація та bindings

Full software runner завершився **exit 0** на записаному Python build:
Debug/Release CTest **64/64** кожна; CLI/exporter/runner **7/21/23**,
campaign/frame/resolution/catalog/large-archive **4/6/7/7/5** на кожній збірці.
Common suites: readiness/benchmark/validator/paths/PGM/inventory/scorer/bundle/
collection/audit **15/5/31/12/14/12/27/24/27/30**. Загалом **357 Python tests**,
без failures/skips. Сім camera/output flags OFF у кожному build cache.

Independent finalizer також **exit 0**: перечитано всі шість reports/oracles
проти bound tar, перевірено input identities, raw pstats проти JSON profile,
перераховано API/micro medians, звірено executable, 243 source hashes і full log.
Це нова full-suite перевірка поточного Python build; жовтневі logs збережені
як окреме історичне evidence.

Production Python/C++/config не змінено. З попередніх 242 source files змінено
лише test runner і tests README; додано один PGM test file. Новий manifest має
243 entries. Старі path/inventory semantics, fresh checks, hashes і output gates
не послаблювалися. Native sanitizer/NoTests/MSVC/hardware runs не виконувалися.
Metadata graph лишається `6d8f547`; застосовано прямий source/caller/diff review.

Evidence локальне й ignored: `artifacts/pgm-header-20261103/` містить probe,
jobs, шість full reports/oracles, raw timings, cProfile/pstats, micro samples,
source manifests, focused/full logs і independent finalizer. Probe використовує
hash-bound локальний helper `artifacts/warm-audit-latency-20261030/probe.py` як
бібліотеку; старий study не перезапускається й не перезаписується.

Input tar SHA-256:
`bedb5f838258e006b378a7cc30b42b279524c58a81b5f0fb5361fd0bc279453b`.
Normalized report SHA-256:
`aee128b3105f6825340ce96cb93ef4cf590b1e98506a7f338eb020b569183da1`.
Summary/software-log hashes збережені у `validation.json`, усі локальні evidence
files — у `evidence-sha256.json`. Fresh clone отримує tests/docs, не ignored tar
чи logs. Повтор потребує нового artifact directory й узгоджених source bindings;
не послаблювати guards старих scripts для іншого source revision.

Summary SHA-256:
`d82734578b975b8dfc5800e744dfb456902a44d3dafd68671adbd38e1596a4e9`.
Full software log SHA-256:
`d939a3e613a67f33cec9f8574084150781c8294d3cd4ccbf2cfe6da2e86aeb54`.

## Наступний bounded software крок

PGM characterization завершено; candidate не переносити у production без нового
evidence. Поточний profile знову вказує на filesystem path work: `Dataset.path`
87383 calls / 13.525 s cumulative, `Path.resolve` 87879 / 8.520 s із 22.412 s.
Наступний slice — read-only attribution фактичних filesystem operations
(`resolve`/stat/open/read) між validator та closed inventory, із recorded runtime,
окремими diagnostic/timed runs і exact archive equivalence. Спочатку визначити,
чи є безпечно усувна зайва робота; не кешувати checks/hashes і не припускати, що повторна
перевірка зайва. Завершені lexical/constructor experiments не повторювати.

Cold-cache methodology, незалежні annotated data і ширша функціональна черга
залишаються у handoff/roadmap. Scope — локальний software-only, без subagents,
Pi/camera/FC/UART/output роботи.
