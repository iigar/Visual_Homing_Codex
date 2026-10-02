# Явне джерело телеметрії та heartbeat/mode contract

Локальний software slice після `dcdc367`, гілка `refactor/optimization-tech-debt`.

## Проблема та відтворення

Перед патчем зібрано попередній inspector із reproducer: heartbeat `42:17`,
attitude `43:18` і position `44:19` разом проходили validation; `0:0` також
проходив; PX4 fixed-wing heartbeat із `custom_mode=4` отримував `Guided`.
Усі пакети мали правильні CRC. Локальне свідчення:
`artifacts/telemetry-source-20261002/source-audit-before.cpp`, executable і log.

## Прийнятий контракт

- `MavlinkTelemetrySourceId` — одна явно задана пара `system_id/component_id`,
  обидва значення `1..255`. Немає wildcard, default `1:1` чи вибору першого пакета.
  Одна пара використовується для всіх семи декодованих IDs, включно з heartbeat,
  attitude, GLOBAL_POSITION_INT/ALTITUDE і допоміжними sensor fields. Підтримки
  окремо дозволених sensor components у цьому slice немає.
- Inspector приймає цю пару як параметр; byte buffer зберігає її незмінною для
  всього свого життя, stream передає її з `expected_source`. Зміна джерела
  потребує нового buffer/stream. Stream відхиляє відсутню/часткову пару в
  конструкторі, до запуску worker або відкриття device.
- Без заданого джерела inspector виконує лише структурну перевірку: framing,
  flags, відомі layouts і CRC; не створює decoded telemetry або receipt offsets.
  Validation додатково вимагає явний `expected_source`, його рівність source
  summary, вибрані повідомлення та підтриманий останній heartbeat. Нульові
  minimum counts не обходять source/heartbeat contract.
- Після структурної перевірки та CRC пакет із нульовим source збільшує
  `invalid_source_frames`; інший або ще не вибраний source —
  `unselected_source_frames`; прийнятий source — `selected_source_frames`.
  Сторонній пакет не змінює decoded values, offsets або receipts. Валідна
  вибрана телеметрія може співіснувати зі стороннім трафіком. Structural error
  policy лишається глобальною: malformed/CRC errors навіть іншого source
  враховуються попереднім `maximum_malformed_frames` gate.
- `frames_seen` і `message_id_counts` описують увесь stream; decoded message
  counts і statistics — тільки вибраного producer. Source counters стосуються
  лише структурно прийнятих CRC-checked known IDs. Unknown IDs лишаються
  undecoded/unverified і не підтверджують джерело чи health.

Підтриманий heartbeat: `MAV_AUTOPILOT_ARDUPILOTMEGA=3`,
`mavlink_version=3` і один із явно дозволених типів ArduCopter:
quadrotor `2`, coaxial `3`, helicopter `4`, hexarotor `13`, octorotor `14`,
tricopter `15`. Інші типи, зокрема generic, plane/VTOL, rover, GCS, companion
та інші autopilot families, блокують validation. Це консервативний список
підтримки, а не твердження, що будь-який інший MAV_TYPE некоректний.

Mode mapping додатково потребує `MAV_MODE_FLAG_CUSTOM_MODE_ENABLED=1`.
Підтримані значення ArduCopter: `0=Stabilize`, `2=AltHold`, `3=Auto`,
`4=Guided`, `6=Rtl`, `9=Land`. Інші значення (включно з `GuidedNoGPS=20`)
та відсутній custom-mode flag дають `Unknown`; відомий heartbeat усе ще
може підтверджувати telemetry health. `Unknown` не задовольняє чинний
Guided permission gate. Кожний новий вибраний heartbeat перезаписує mode:
непідтриманий heartbeat не залишає попередній `Guided`. Armed залишається
окремим прапорцем та окремою вимогою adapter/output gates.

Первинні визначення перевірено 2026-10-02:
[MAVLink heartbeat identity](https://mavlink.io/en/services/heartbeat.html),
[HEARTBEAT, MAV_TYPE, MAV_AUTOPILOT, MAV_MODE_FLAG](https://mavlink.io/en/messages/minimal.html),
[ArduCopter mode numbers](https://github.com/ArduPilot/ardupilot/blob/master/ArduCopter/mode.h).
ID компонента сам по собі не визначає його тип; `compid=1` не вимагається.

## API / CLI migration

```cpp
const vh::MavlinkTelemetrySourceId source{42, 17}; // приклад, не ID реального борту
const auto summary = vh::inspect_mavlink_telemetry_bytes(bytes, source);
const auto validation = vh::validate_mavlink_telemetry(summary, {.expected_source = source});
vh::MavlinkTelemetryByteBuffer buffer(65536, source);
```

CLI використовує дві змінні середовища, прочитані самим процесом core:
`VISUAL_HOMING_TELEMETRY_SYSTEM_ID` і `VISUAL_HOMING_TELEMETRY_COMPONENT_ID`.
Обидві відсутні — structural-only inspection; одна відсутня, порожнє,
нечислове, нульове або позадіапазонне значення — configuration error.
Існуючі positional arguments збережено.

Ці змінні використовують file inspector/validator, file snapshot recording,
live recording і matching telemetry config. Wrapper має передавати environment
саме процесу core; налаштування shell на іншому host цього не робить. Старі
команди без source тепер fail closed при validation/використанні telemetry.
Не підставляти ID борту за припущенням. `--inspect-mavlink-telemetry` без source
може завершитися з exit 0: це лише структурний результат, не health/permission.

CLI показує вибрані та очікувані IDs, три source counters,
`source_passed` і `heartbeat_contract_passed`. Runtime stream-start log показує
очікувану пару. Декодований `mode=Unknown` зберігає відмінність між відомим
heartbeat і непідтвердженим режимом.

## Перевірки та межі

Новий `mavlink_telemetry_source` test перевіряє v1/v2, обидві частини source,
нульові та граничні IDs, усі сім decoded IDs, чужий трафік до/після вибраного,
heartbeat families/types/version/flags/modes, source-config mismatch,
recovery, clear, tail eviction та partial packets. Справжні
buffer → runtime observation → adapter перевіряють збереження старого receipt
time, втрату permission після age boundary і відновлення лише після свіжих
вибраних повідомлень. Пізній правильний heartbeat сам не освіжає стару attitude.

Попередні 35 golden packets залишено побайтово незмінними. Їх heartbeat без
custom-mode flag тепер очікувано має `Unknown`; нульовий heartbeat не відповідає
підтриманому контракту. Старі tests отримали явне fixture source; позитивні
інтеграційні heartbeat fixtures тепер мають коректні type/autopilot/version/flag.
CRC, payload, receipt freshness і malformed tolerance tests збережено.

Окремі сім Python CLI-тестів запускаються software runner для Debug і Release;
не потребують pymavlink, hardware або network. Команда повного повторення:
`bash scripts/test-software.sh artifacts/telemetry-source-20261002/build`.

Фактичний результат: Debug **63/63**, Release **63/63**, source CLI **7/7**
для кожної збірки, readiness logs **15/15**, recorded-route helpers **5/5**.
Перевірено фінальний CLI edit повторним incremental runner. В усіх трьох
конфігураціях (Debug/Release/NoTests) сім camera/output flags OFF;
`BUILD_TESTING=OFF` успішно зібрано, CTest має **0 tests**. Release test
assertions залишаються активними через `-UNDEBUG` після `-DNDEBUG`.

Source test проходить **166** split positions; попередні receipt integration
cases також збережено. Protocol suite: **35** golden packets, **5384** bit
corruptions, **3584** length combinations, **1089** incomplete prefixes.
ASan/UBSan із leak detection та halt-on-error пройшли для protocol test і
source test. У другому випадку інструментовані test, inspector і byte-buffer/
stream implementation, а інші library objects взято зі звичайної Debug
збірки; це не full-runtime sanitized acceptance.

Локальні докази: `full-software.log`, `final-software.log`, `source-sanitized.log`,
`protocol-sanitized.log`, `no-tests.log` і `validate-extra.sh` у зазначеному
artifact directory. Датовані попередні reports збережено як історичні.

GitNexus CLI refresh завершено: 14892 nodes / 31502 edges / 211 flows;
FTS/BM25 недоступний. Pre-edit impact був неповним `UNKNOWN`. Staged check
повернув `CRITICAL` для 191 процесу, compare-main — 195: 604 staged flow steps
приписано одному empty-ID `system_id`, включно з нерелевантними JSX-процесами.
Решта staged steps — очікувані `append` і `configured`; changed-symbol paths
обмежені telemetry interfaces/implementation, caller/CLI та tests. Це не чистий
graph verdict. Переглянуто фактичний 20-file staged diff і прямі виклики;
окремо звірено незмінність golden packets, adapter/output gates/writers,
receipt-update block і non-test CMake prefix. У 58 стандартних Release test
compile records assertions активні. Перевірені source hashes і raw graph
responses збережено в artifacts. Graphify AST refresh: 6033 nodes / 9360 edges.

Це source selection, не автентифікація: CRC і IDs не захищають від навмисної
підміни/повторення. Signed packets і далі відхиляються; key/signature/anti-replay
не реалізовано. Receipt time — host read completion, не FC measurement time.
Snapshot recording не стає свіжим live evidence. Pi/MSVC і фізичний другий
прохід цим кроком не перевірені. Наступний bounded software slice — чесні межі
повного runtime timing, пізні scale/verification stages і deterministic replay.
