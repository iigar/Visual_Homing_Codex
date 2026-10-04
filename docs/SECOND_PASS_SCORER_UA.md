# Deterministic offline scorer повторного проходу, v1

Оновлення 2026-10-05 після `2c6964e`: наступний
[controlled offline exporter](SECOND_PASS_EXPORTER_UA.md) реалізовано.
Нижче збережено contract і evidence саме scorer checkpoint.

Реалізовано `2026-10-04` поверх checkpoint `00e4b13`. Scorer
[score-second-pass.py](../scripts/score-second-pass.py) оцінює явно збережені
покадрові результати щодо незалежної розмітки за
[контрактом dataset](SECOND_PASS_DATASET_CONTRACT_UA.md). Він повторно
застосовує [validator](SECOND_PASS_VALIDATOR_UA.md), читає pinned inputs і
друкує JSON із кожним рішенням та агрегатами. Усі залежності — Python standard
library; файли не змінюються, matcher/binary не виконуються.

```sh
python3 scripts/score-second-pass.py /data/query-session /data/evaluation-run > report.json
python3 scripts/score-second-pass.py /data/query-session /data/evaluation-run \
  --split-peer /data/holdout-a --split-peer /data/holdout-b --disjoint-references
python3 scripts/tests/test_second_pass_scorer.py -v
```

Exit **0** / `scored` означає, що оцінку обчислено, навіть якщо всі matches
хибні. Exit **2** / `invalid` — порушено dataset/run/hash/schema contract;
exit **3** / `needs_review` — validator не підтвердив завершення review або
повноти. Для 2/3 metrics не видаються. Інші splits треба явно передати через
`--split-peer`; автоматичного пошуку datasets немає. Один report — одна query
session/run, peer datasets лише перевіряються на leakage. Підсумки різних
сесій не змішуються автоматично.

## Прив'язка run і policy

Run root містить `run.json`, `policy.json`, `predictions.csv` та копії
implementation/config artifacts. Конкретні paths задає manifest; до них
застосовуються ті самі containment/regular-file/hash перевірки, що й до
dataset. JSON metadata має ліміт 4 MiB, CSV — 64 MiB/100000 rows; binary
implementation/config files хешуються потоково. Нічого не імпортується з
цих artifacts як виконуваний код.

У кожному object потрібні рівно перелічені fields. Duplicate JSON keys,
non-finite numbers, bool замість integer, null замість hash,
зайві keys та невірні CSV headers відхиляються.

| Object | Поля |
|---|---|
| `run.json` | `schema_version`: integer 1; `run_id`, `dataset_id`: непорожні strings; `input_sha256`: path/hash map; `inputs_frozen_before_run`: boolean true; `producer`, `policy`, `predictions`: objects |
| `producer` | `kind`: synthetic/external_export; `code_revision`, `state_reset_policy`, `preprocessing`: непорожні strings; `implementation`, `configuration`: artifact objects |
| Artifact (`policy`, `predictions`, `implementation`, `configuration`) | `path`: relative regular file у run root; `sha256`: 64 lowercase hex |

`input_sha256` має **точно** збігтися з повним `input_sha256` dataset validator:
dataset.json, обидва CSV, reference, config, frames, evidence, originals.
Не можна замінити labels/route після run або підкласти partial hash map.
Metadata/CSV після validation повторно читаються зі звіркою тих самих hashes;
PGM/VHRS вдруге не декодуються. Inputs під час операції мають бути незмінними;
це не atomic filesystem snapshot. Validator має бути поруч зі scorer script.

До run потрібно зафіксувати dataset hashes, policy, implementation/config,
state/reset і preprocessing. Після run додаються predictions та їхній hash;
початкові bindings не перераховуються за новими inputs. `code_revision` —
заявлена revision, implementation hash — перевірена тотожність bytes. Tool
не доводить, що саме ці bytes реально виконувалися, що config містить усі
потрібні thresholds або що файли були зафіксовані саме до перегляду results.
Ця межа відображена у `pinning_provenance`; її має забезпечувати наступний
controlled exporter/runner та review. Retuning test data змінює його роль
на development за чинним dataset contract.

Policy без прихованих defaults:

| Поле | Тип і семантика |
|---|---|
| `schema_version`, `policy_id` | Integer 1 та непорожній ID |
| `scorable_visibility` | Непорожній array унікальних `clear`/`degraded`; unobservable/unknown у v1 завжди excluded |
| `target_endpoint` | `start` або `end`; задає мету run, зокрема для reverse |
| `endpoint_early_tolerance_ns`, `endpoint_late_tolerance_ns` | Integers 0..2^63-1; розширення annotated region до першого/після останнього target sample |
| `reacquisition_min_correct_frames` | Integer 1..100000; потрібна кількість послідовних correct samples |
| `reacquisition_min_duration_ns` | Integer 0..2^63-1; мінімальний span тієї самої correct streak |
| `max_interframe_gap_ns` | Integer 1..2^63-1; найбільший interval, через який дозволено continuity; межа включна |

Значення policy — параметри оцінювання, не зміна production matcher/gates.
Немає довільного цільового відсотка accuracy чи автоматичного вибору thresholds.

## Покадрові результати

Exact CSV header:

```csv
sequence,frame_id,timestamp_ns,observation,missing_reason,match_valid,reference_index,endpoint_stop,navigation_command_valid,route_readiness,verification_accepted,published
```

Рівно один row на кожний **stored** dataset frame, у тому самому порядку.
Sequence/ID/source timestamp мають чисельно збігатися з frames.csv; сортоване
відновлення, перенумерація, float timestamps або rebasing заборонені.
Integers читаються точно, включно з ns вище 2^53. Labels можуть мати інший
порядок і приєднуються лише за ID.

- `observation=observed`: `missing_reason` порожній; `match_valid` — exact
  `true`/`false`. Valid match має integer index у reference range; invalid
  match має **порожній** index, не default index 0 з raw runtime struct.
- `observation=missing`: потрібна непорожня reason; усі наступні prediction
  fields порожні. Це явний брак результату для наявного source frame.
  Пропущені rows відхиляються, не доповнюються автоматично.
- `endpoint_stop` та чотири downstream fields — `true`/`false` або порожня
  cell, якщо outcome не записано. Порожня cell не є false. Endpoint stop —
  **подія** на цьому frame, не latched state на всіх наступних frames.
- Command validity, route-only readiness, verification acceptance і
  publication — окремі source-frame outcomes. Для async publication потрібна
  явна прив'язка до source frame; пізній completion timestamp не підставляють
  замість його capture timestamp. Взаємні permits scorer не вигадує.

Після раннього завершення producer решта stored frames повинна мати explicit
missing rows з reason на кшталт `stopped_at_endpoint`. Фізично dropped captures
без pixels/labels залишаються в dataset gaps; їм не створюють штучні rows/GT.
`capture_counts` та `declared_gaps` показують їх окремо від missing predictions.

## Matching metrics і coverage

Scorable on_route потребує дозволеної visibility і annotated interval;
scorable off_route — дозволеної visibility. Unknown location, excluded
visibility та absent correspondence мають окремі reason/counts. Label/GT
ніколи не береться з prediction.

- TP: valid index усередині inclusive interval.
- FP: valid index поза interval або на scorable off_route.
- FN: scorable on_route без correct prediction, включно з missing result.
  Неправильний valid index дає **FP і FN одночасно**.
- TN: observed invalid prediction на scorable off_route. Missing negative
  не стає TN; unscorable prediction не отримує вигаданої correctness.

Precision = TP/(TP+FP), recall = TP/(TP+FN). Кожна ratio містить numerator,
denominator та value; denominator 0 дає **null**. Negative FP fraction має
denominator лише **observed scorable negatives**; поруч обов'язково є
negative prediction coverage. Інакше missing negatives штучно знизили б FP.
Abstention fraction = observed invalid / усі observed frames, включно з
unscorable labels. Також звітуються prediction coverage, unscorable fraction,
missing reasons, per-frame рішення, розподіл за direction/visibility та
session/умови capture. Absent sparse counter означає zero; null ratio —
невизначений результат, не zero. Counts/durations різних категорій можуть
перетинатися й не повинні механічно підсумовуватися. Diagnostic outcome `fn`
позначає FN без FP, `fp_fn` — одночасну помилку; загальні `fn`/`fp` на рівні
metrics і counters `fn_total`/`fp_total` включають обидва відповідні outcomes.

Index error — мінімальна відстань valid prediction до inclusive interval:
0 всередині; немає значення для abstention/missing/off_route/unscorable.
Normalized error = error/(N-1), лише для N>1; це **індексна**, не метрична
відстань. Distribution містить count/min/max/p50/p95 з nearest-rank rule.
Integer counts/timestamps не переводяться у float; ratio/normalized error —
звичайні скінченні floating-point значення.

Frame count не дорівнює кількості незалежних trials. Scorer не оцінює
confidence intervals і не називає synthetic результати real accuracy.

## Час, endpoint і reacquisition

Кожний declared interior gap або interval `> max_interframe_gap_ns` розриває
temporal continuity. Для звичайної пари кадрові labels представляють
`[t_i,t_next)`; останній frame сегмента має duration 0. Через gap ця тривалість
не приписується сусідньому label: весь interval окремо входить у
`unrepresented_span_ns`. Represented + unrepresented дорівнюють span між
першим/останнім stored timestamp. Boundary gaps лишаються explicit metadata,
за межами цього stored span. Це convention coverage, не вимірювання motion,
sensor latency або processing delay.

Endpoint event — contiguous region відомих target endpoint labels із
дозволеною visibility, в одному сегменті. Correspondence interval для нього
не обов'язковий: endpoint має незалежну анотацію. Window розширюється за
policy й обрізається межами сегмента. Вікна різних events у сегменті не
можуть перетинатися: така policy/annotation combination відхиляється до
видачі metrics замість довільного вибору вигіднішого event.

Перший stop із відомими location/endpoint/visibility у window зараховується
одному event; повторні — `duplicate_stop`. Stop до наступного window —
`early_stop`; решта поза windows — `false_stop`. Невідома endpoint truth дає
`unscorable_stop`, не FP. Early/duplicate — окремі види незарахованих stops,
не додаткові accepted events. Наявні events без stop показані як not_detected,
із missing stop-observation counts; праве обрізання window/region позначене
right-censored. Сам runtime dwell, включно з invalid-gap continuity, не
симулюється і не переписується: оцінюються фактичні exported stop events.

Sample delay = stop timestamp − перший target sample; він може бути від'ємним
при дозволеній early tolerance. Entry delay видається лише коли попередній
sample у тому самому сегменті має відомий non-target endpoint. На початку
clip/gap або після unknown truth entry left-censored і entry delay **null**;
sample delay зберігається. Це discrete annotation delay з невиміряною
міжкадровою невизначеністю, не точний фізичний час входу.

Reacquisition episode починається з кожного contiguous scorable on_route
region. Початок clip — `initial_acquisition`, початок сегмента після gap —
`after_gap`; ці дві категорії не входять у aggregate return latency.
Після scorable negative або unknown/unscorable sample у тому самому сегменті
це `return_to_scorable_route`: повернення до **scorable evidence**, не
обов'язково фізичний вихід/повторний вхід у corridor.

Confirmation потребує одночасно мінімального count і duration послідовних
correct samples. Wrong/invalid/missing скидає streak. Gap ніколи не переносить
її далі. Report зберігає всі episodes, confirmed/not_confirmed, region exit,
gap/clip end, observed span і right-censoring; незавершені returns не
видаляються з counts заради кращої статистики. Delay distributions містять
лише confirmed returns, поряд показана кількість unconfirmed/censored.

## Валідація та межа наступного кроку

[27 synthetic tests](../scripts/tests/test_second_pass_scorer.py) включають
ручну confusion matrix, 40 exhaustive interval/index oracle cases, нульові
denominators, unknown/missing coverage, independent gate counts, hash/policy
binding, malformed schema/paths, review/split gates, endpoint boundaries та
`+1 ns`, duplicate/unknown stops, censoring, reacquisition reset, declared/
time gaps і timestamps біля `2^63-1`. Два actual CLI runs дають однаковий JSON;
вхідні files залишаються незміненими. Тести включені у software runner.

Виконаний WSL software runner: Debug/Release **64/64**, telemetry CLI
**7/7** кожна, Python readiness **15/15**, benchmark **5/5**, dataset validator
**31/31**, scorer **27/27**. Symlink tests виконані, не skipped. У двох CMake
caches підтверджено сім camera/output flags OFF. Native Windows Python та
hardware у цьому slice не запускалися.

З попередніх 219 tested hashes змінилися тільки runner і tests README;
збережено нові **221** hashes. Додатково два CLI runs на збереженому synthetic
наборі дали побітово однаковий JSON та ручний oracle **TP=1, FP=2, FN=3,
precision=1/3, recall=1/4**. Ці числа перевіряють арифметику scorer, не
точність реального matcher. Локальні ignored artifacts:
`artifacts/second-pass-scorer-20261004/software.log`, `verification.json`,
`tested-source-hashes.json`, `synthetic-dataset/`, `synthetic-run/`,
`synthetic-report.json`, `verify.py`. У Git збережені code/tests/docs;
новий clone відтворює fixtures через committed test factory.

Scorer повторно використовує validator API; його код, core/config, thresholds,
runtime clocks/gates та legacy logs не змінені. Прямо прочитано validator,
replay/live log producers і runner. Графи від попереднього checkpoint не
підтверджують новий Python call graph; review спирається на source/diff/tests.

Наявні `match_frame`/`live_route_match_frame` logs не мають повного нового
source-time/outcome contract; автоматично перетворювати їх у повний run тут
не намагалися. Наступний bounded software slice — controlled offline
exporter/runner: freeze plan до replay, зв'язати фактичний per-frame matcher
output із source IDs/timestamps, зберегти explicit missing/unsupported
outcomes і перевірити весь dataset → replay → scorer шлях синтетичним oracle.
Endpoint/verification/publication, які replay не виконує, мають бути absent,
а не вигаданими false або успіхом. Для real accuracy досі потрібен незалежний
повний annotated capture. Hardware/output і threshold tuning не входять у
цей checkpoint.
