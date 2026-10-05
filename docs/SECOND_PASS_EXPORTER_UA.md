# Контрольований offline second-pass exporter

Початок: 2026-10-04; завершення: 2026-10-05. Продовження checkpoint `2c6964e`.

Після exporter commit `dc458ec` реалізовано
[read-only checker збережених bundles](SECOND_PASS_BUNDLE_CHECKER_UA.md).
Нижче збережено contract та evidence самого exporter checkpoint.

`scripts/export-second-pass.py` зберігає перевірені копії dataset, split peers,
executable, конфігурації й scoring policy **до запуску** окремого
`second_pass_replay`. C++ adapter викликає чинний `match_replay_route` та отримує
результати через синхронний observer. CSV передається чинному
[scorer](SECOND_PASS_SCORER_UA.md), без розбору legacy logs або генерації GT.

Це локальний software workflow. Використовувати треба довірений локально
зібраний `second_pass_replay`: runner не є sandbox для довільного executable.
Adapter працює лише з файлами, `DryRunCommandSink` і `DryRunMavlinkBridge`;
не має camera/UART/live-output CLI. Наявні runtime thresholds/gates незмінені.

## Запуск

Потрібні Python 3.9+ standard library та зібраний C++ executable. Перевірена
платформа — WSL/GCC/Python 3.12; native Windows/MSVC тут не перевірено.
Dataset має відповідати [validator contract](SECOND_PASS_VALIDATOR_UA.md).

```sh
# Збирати у звичайному software build із сімома camera/output flags OFF.
cmake --build artifacts/telemetry-source-20261002/build/Release --target second_pass_replay

python3 scripts/export-second-pass.py path/to/dataset artifacts/new-second-pass-run \
  --binary artifacts/telemetry-source-20261002/build/Release/second_pass_replay \
  --configuration path/to/replay-configuration.json \
  --policy path/to/scoring-policy.json \
  --code-revision DECLARED_BUILD_REVISION --run-id development-repeat-01 \
  --timeout-seconds 300

# Передати решту splits через повторюваний --split-peer ROOT.
# --disjoint-references додатково забороняє спільну reference між splits.
```

Output має бути новим каталогом поза вхідними datasets. Наявний output,
зокрема symlink, не перезаписується. Run не змінює оригінали. Невдалий каталог
залишається для діагностики; для повтору потрібне нове ім'я.

Exact configuration schema, без прихованих CLI defaults:

```json
{
  "schema_version": 1,
  "target_width": 2,
  "target_height": 2,
  "window_radius": 0,
  "minimum_confidence": 0.99,
  "max_direction_shift_px": 0,
  "radians_per_pixel": 0.02,
  "navigator_minimum_confidence": 0.95,
  "navigator_max_match_age_ms": 200,
  "navigator_yaw_gain": 1,
  "navigator_max_yaw_rate_radps": 0.35,
  "navigator_max_yaw_accel_radps2": 1,
  "navigator_forward_speed_mps": 0
}
```

Ці числа — **синтетичний тестовий приклад**, не нові production thresholds.
`target_width/height`: integer 1..4096, обидва точно відповідають усім VHRS
entries. Query може мати інші dimensions: застосовується існуючий
`Gray8ResizePreprocessor` із rounded area averages. Query обмежено 16 Mi pixels,
dimensions — чинним validator 1..65535; ці межі не допускають переповнення
integer operations поточного resize у цьому adapter.
`window_radius`: integer 0..100000; `max_direction_shift_px`: integer
0..target_width-1. Confidence — finite number [0,1], решта чисел — finite
nonnegative. Boolean замість числа, duplicate/extra fields, NaN/Infinity
відхиляються. Camera profile не вибирається: scale задається radians_per_pixel.
Інші matcher options залишаються compiled defaults чинного replay caller;
точний binary hash закріплює їхню реалізацію.

Scoring policy має повну схему з [опису scorer](SECOND_PASS_SCORER_UA.md).
Runner не підбирає policy або matcher thresholds за отриманими результатами.

## Порядок фіксації та артефакти

1. Перевірити configuration/policy, dataset і всі передані peers. `needs_review`
   або `invalid` не запускає executable.
2. Скопіювати кожен hashed dataset input у `dataset/` і `peers/0001/…`.
   Кожна копія звіряється з попереднім SHA-256; початково перевірений hash
   зберігається в plan і не замінюється hash зміненої копії. Snapshots повторно проходять
   validator і перевірку точного повного input map.
3. Зафіксувати binary у `implementation/replay`, `configuration.json`,
   `policy.json`, три Python sources у `toolchain/`. Створити `manifest.csv`
   та `clock.txt` з точними source IDs/timestamps і відносними snapshot paths.
4. Закрити `plan.json` до subprocess launch. Він містить hashes усіх input
   copies і generated files, argv, declared revision, timeout, peer policy,
   clock/state/telemetry contract та інформацію про платформу. Hash самого
   plan теж перевіряється до й після виконання. Timeout integer 1..3600 s;
   subprocess запускається без shell, із run directory як cwd.
5. Зберегти stdout у `raw-predictions.csv`, stderr у `replay.log`. Після run
   перевірити frozen hashes, exact CSV schema, повний source order/identity,
   reference bounds і підтримувані outcomes. Зберегти `predictions.csv`,
   scorer-compatible `run.json`, повний `score.json`; повторно перевірити inputs.
6. Останнім записати `execution.json`: status, return code, missing reason,
   plan hash та hashes raw/log/predictions/run/score. Відсутній або некоректний
   completion record не означає завершений run. Це не atomic/fsync transaction
   для power-loss durability.

Перевірки засвідчують workflow та відсутність виявленої сталої зміни files.
Вони не є захистом від ворожої concurrent mutation, доказом авторства,
binary/source attestation або незалежним timestamped audit. `--code-revision`
залишається заявою; binary hash — перевіреним вмістом. Dynamic libraries,
environment, compiler/build provenance не закріплені повністю. Standalone
scorer і далі вказує, що chronology не перевіряється ним самостійно.
Він не перевіряє `execution.json`; для прийняття exporter run треба також
перевіряти його completion status і artifact hashes.

## Семантика replay

Початок run створює нові matcher/navigator/health у новому процесі. Стан matcher
і navigator зберігається між **усіма stored frames, включно з gaps**. Runner
не вигадує кадрів на місці drops і не скидає matcher за scoring boundaries.
Scorer окремо перериває temporal scoring/streaks на gaps за своїм контрактом.

Ініціалізація clock — timestamp першого кадру; усі шість processing phase
reads кожного кадру дорівнюють його source timestamp. Epoch/ID не rebased,
wall time не використовується для рішень. Це deterministic zero-work
simulation: `latency_ms=0` не є виміряною швидкодією або доказом freshness
реального runtime. Telemetry — синтетичні heartbeat, armed=true, Guided.
Тому command_valid показує результат dry-run navigator за цими умовами,
не поточний стан FC і не дозвіл надсилання.

| CSV outcome | Джерело |
|---|---|
| sequence, frame_id, timestamp_ns | Observer фактично processed frame; runner звіряє кожен запис з source manifest |
| match_valid, reference_index | Фактичний Gray8 matcher; invalid має порожній index |
| navigation_command_valid | Фактичний dry-run navigator із scripted telemetry/clock |
| endpoint_stop | Порожнє: caller не виконує endpoint event logic |
| route_readiness, verification_accepted, published | Порожні: ці стадії/рішення не виконуються |

Навіть `false` у непідтримуваному outcome від adapter — помилка contract.
Відомі labels не заповнюють predictions. Invalid match лишається observed
abstention, а не missing. Observer не змінює logs старих replay overloads;
він синхронний, його exception перериває run.

## Відмови та коди завершення

- `0 exported`: replay завершився, всі rows перевірено, score обчислено.
  Це не accuracy acceptance, semantic independence або flight readiness.
- `3 needs_review`: dataset не пройшов review/completeness admission; запуску немає.
- `2 invalid`: input/config/snapshot/scoring contract помилка. Якщо каталог уже
  створено, `failure.json` зберігає діагностику за можливості; completion відсутній.
- `2 replay_failed`: nonzero exit, timeout, launch error або некоректний stdout.
  Усі кадри стають `missing` із `replay_failed`, `replay_timeout`,
  `replay_launch_failed` або `replay_output_invalid`. Partial output збережено
  окремо, але жоден partial result не зараховується. `execution.status=failed`;
  `score.status=scored` лише пояснює coverage/FN такого failed run.

## Перевірка й наступний крок

[21 exporter tests](../scripts/tests/test_second_pass_exporter.py) виконують
реальний dataset → C++ replay → scorer шлях на синтетичному незалежно
закодованому VHRS/PGM oracle: TP=3, FP=2, FN=3, precision=3/5, recall=1/2.
Це контрприклади для plumbing, не фізично незалежні second-pass кадри.
Перевіряються повторюваний CSV, resize/CRLF, максимальні uint64 IDs/int64 ns,
стан matcher через gap, pre-launch snapshot, hash mutation, review/split/schema
guards, unsupported outcomes, malformed/missing rows, справжній timeout/nonzero
subprocess та відсутність partial credit. C++ timing test окремо звіряє source
identity та незмінність старих logs із/без observer.

Evidence: `artifacts/second-pass-exporter-20261004/`. Підсумкові результати
build/test/hash перевірок фіксуються в CURRENT_PROJECT_STATUS і SESSION_LOG.
Graph metadata лишається на `6d8f547`; використано прямий review declarations,
callers, source/diff і тести, без твердження про свіжий graph impact verdict.

Наступний bounded software slice — окремий read-only checker exporter bundle:
перевірити plan/completion/output hashes, статус і peer scope перед повторним
scoring, без запуску binary; додати tamper/incomplete-bundle negatives.
Далі потрібен незалежний повний annotated dataset для реальної accuracy.
Збір даних на hardware, threshold tuning, endpoint/verification/publication
runtime wiring залишаються окремими задачами.
