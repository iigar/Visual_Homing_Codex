# Warm guarded CLI latency та worker memory

Дата: `2026-11-04`. Відновлено remote-verified
`fe2b331d7c755cfe02a6e18747b5bba21407beda` з чистим tracked tree після
[path-depth study](PATH_DEPTH_SENSITIVITY_2026-11-04_UA.md).

## Що саме вимірюється

Чинний trusted `scripts/check-second-pass-runner.py:main` виконує власний
argparse, `check` і `print(json.dumps(...))` без source changes. Невеликий
native wrapper імпортує trusted auditor, блокує subprocess run/Popen і runner
resume, додає два timer reads навколо `check` та викликає цей CLI `main` з
обома зовнішніми plan/state anchors. Stdout flush виконується явно; metrics
пишуться в окремий sidecar після flush. Це **guarded CLI**, а не вимір bare
script без wrapper; instrumentation/setup overhead входить у відповідні фази.

Наперед визначено **один excluded warmup + 15 fresh workers**. Default GC
увімкнений, profiler відсутній, Python `-B`/PYTHONDONTWRITEBYTECODE; bytecode cache
не створюється. Samples не відкидаються/не додаються за результатом.
Median — 8-й sorted sample із 15; nearest-rank p90 — **14-й**, без interpolation.

Parent бере `perf_counter_ns` безпосередньо перед process launch і після wait.
Worker записує timestamps module entry, trusted import complete, API start/end,
stdout flush complete. Linux monotonic clock спільний для цих processes;
raw timestamps і всі межі перевіряє independent finalizer.

| Вимір | Межі |
|---|---|
| Process wall | Parent перед launch → process exit; включає sidecar/shutdown |
| Output-ready wall | Parent перед launch → worker після stdout flush |
| API | Лише чинний `check` у тому самому CLI call |
| Startup-to-module | Launch → початок wrapper після import time |
| Imports/setup | Wrapper module entry → trusted imports complete |
| Parse/setup | Imports complete → API start; guards, argv, argparse |
| Serialization/flush | API end → CLI JSON print/main return/explicit flush |
| After-output | Flush complete → parent wait returns; HWM/sidecar/shutdown |
| Non-API-to-output | Output-ready wall − API для кожного worker |

Native stdout file відкрито до parent timer. JSON output до native filesystem
входить у timing, подальше копіювання до ignored artifacts — поза ним. Flush
не є fsync чи durability acceptance. Oracle checks, snapshots, source/runtime
checks, копіювання logs/reports і aggregates виконуються parent поза timing.
VM high-water mark читається з `/proc/self/status` після output flush і до
sidecar persistence: current-image worker peak охоплює startup/imports/check/
serialization, не весь можливий shutdown peak. Це не process-tree RSS;
inherited `ru_maxrss` не використовується. Wrapper і trusted imports відрізняються
від API benchmark harness, тому між studies memory-improvement claim немає.

## Runtime та inputs

Python `3.12.3 (main, Aug 31 2026, 10:18:26) [GCC 13.3.0]`, resolved executable
`/usr/bin/python3.12`, SHA-256
`e50d468e8b0adfb05733f5b87b3cff34829c4a8c1aea50c865aa8bdfe4bb150f`;
WSL kernel `6.6.87.2-microsoft-standard-WSL2`. Runtime збігається з bound
PGM/path-depth studies. Trusted source/worker bytes звірено до кожного launch.

Один bound `frames_per_dataset-64-repeat-00` archive: **11137 files / 12 catalog
datasets / 12 saved runs / 64 frames / 32×32 / 4 references / 768 unique
frame-run pairs**, усі 11 peers/run. Root має fixed depth **4** без `/`, довжину
**64 characters**; stdout/worker/source на тому самому native storage.
Actual root: `/tmp/cli-warm-2fwc3fci/sssssssssssssssssssssssssssssssss/archive`;
source root `/tmp/cli-warm-2fwc3fci/trusted-source`, stdout root
`/tmp/cli-warm-2fwc3fci/output`, device `2096`, filesystem family `ext2/ext3`
за `stat -f` (family label, не конкретна версія filesystem).
Pre/post bytes/mtime snapshots archive та inputs прогрівають cache.
Archived programs не виконуються, нових replay executions — **0**.
Fresh workers не доводять cold cache. Warm WSL evidence не є Windows/Pi/live
latency, accuracy, p99 або physical independence acceptance.

## Результати та рішення

**Probe і independent finalizer — exit 0.** Усі **16/16 audits** (warmup +
15 timed) мають ідентичний raw stdout **6356740 bytes**, exact normalized full
reports/oracles/peers і незмінні archive/input bytes/mtime. Нових replays — 0.

| Вимір | Median | Nearest-rank p90 | Min–max |
|---|---:|---:|---:|
| Process wall, s | **10.642** | **11.108** | 9.538–11.290 |
| Output-ready wall, s | 10.633 | 11.100 | 9.530–11.281 |
| API-only, s | **10.498** | **10.943** | 9.399–11.129 |
| Non-API-to-output, ms | **142.552** | 175.078 | 128.166–225.135 |
| Startup-to-module, ms | 13.850 | 15.688 | 11.882–17.548 |
| Imports/setup, ms | 41.920 | 54.696 | 38.371–54.988 |
| Parse/setup, ms | 2.275 | 3.740 | 1.998–3.947 |
| Serialization/flush, ms | 84.992 | 102.731 | 71.592–167.910 |
| After-output, ms | 8.276 | 13.541 | 6.942–19.331 |
| Worker VmHWM, MiB | **58.65** | **58.89** | 56.60–58.90 |

Raw VmHWM median/p90/min/max = **60060/60304/57956/60316 KiB**; MiB = KiB/1024.
Фази додаються для кожного sample за timestamps; medians/p90 різних фаз
не додаються і не віднімаються для відновлення іншого aggregate. Median
non-API residual отримано з 15 per-worker differences, не з двох medians.
15 samples одного warm archive не є p99/tail acceptance чи незалежними captures.

**Рішення:** CLI startup/output залишити без змін. Вартість поза API до stdout
у цьому workload мала порівняно з audit API; candidate не реалізовано, speedup
не вимірювали. Цей baseline закриває пропущені раніше startup/output boundaries.
Нижчий VmHWM відносно іншого harness не є доказом memory improvement: склад
imports та точка sampling різняться. Cold-cache study залишається окремою.

Наступний bounded software крок — **актуальний offline matcher baseline/profile**.
Локально наявні `scripts/benchmark-recorded-routes.py`, target
`gray8_matcher_benchmark`, шість VHRS routes, 13 stop-frame PGM та історичне
44-scenario evidence. Спочатку перевірити input hashes/session bindings,
записати current source/compiler/build/binary identities та сім flags OFF.
Новий artifact directory і trusted current Release benchmark; історичні bound
scripts/finalizers не перезапускати як універсальні health checks.

Відновити exact match/candidate/stop-frame oracle для відповідних 44 scenarios
з однаковими mode/config/workload, нормалізуючи лише timing/display metadata;
якщо схема чи поведінка відрізняються — спочатку пояснити різницю, не припускати
equivalence. Окремі warmup/timed runs та CPU profile оберуть один hotspot.
Cache/coarse scan/scale kernels/reparse/queues — лише після цього evidence;
завершені early exits/lazy edge-cache без нового evidence не відкривати.
Self-match і sparse stop-frames не є незалежною accuracy validation.
Поточний scope — software-only, без hardware/output, policy/threshold changes
чи широкого rewrite. Ширша функціональна черга збережена у handoff.

## Валідація та evidence

Local ignored directory: `artifacts/cli-latency-20261104/`: probe, minimal worker,
independent finalizer, 16 jobs/raw stdout/raw metrics/oracles/results/stderr,
snapshots, source manifest, summary/validation та evidence SHA-256 manifest.
Hash-bound warm helper імпортується лише parent для підготовки/oracle checks;
worker не імпортує benchmark/test fixture harness. Старі studies не переписуються.
Fresh clone отримує docs/source; generated evidence лишається локальним.

| Binding | SHA-256 |
|---|---|
| Input tar | `bedb5f838258e006b378a7cc30b42b279524c58a81b5f0fb5361fd0bc279453b` |
| Normalized report | `aee128b3105f6825340ce96cb93ef4cf590b1e98506a7f338eb020b569183da1` |
| Raw stdout | `7d67964503b67cf7046bda4b3b8be7b531a5d7d861782416507934e268b4fddd` |
| Summary | `1b53a8e7937abdf10cb7307bc0d9e4187ff20b5af7f23bcc676c08a51629a5bc` |
| 243-source manifest | `a565452a0cfa785d58d2bc8f2b0856032575180b4e090106fc4732c30e958cdc` |

Production/tests/runner/core/config незмінні. Нового full runner не було;
попередні same-source/runtime **357 Python tests / Debug і Release CTest
64/64** перевірено за log hash/success/counts. Hardware/output робота і
subagents не використовувалися. Graph metadata `6d8f547` advisory; прямий
source/caller review CLI та integrity guards — чинний fallback.
Independent finalizer перечитав усі jobs/stdout/metrics/oracles/results/stderr,
tar/snapshots, перевірив clock sequence та всі phase deltas, перерахував median/
p90/min/max і raw stdout hashes. **243 source hashes**, executable identity та
сім OFF flags звірено. Manifest містить **108 bound evidence files**.
