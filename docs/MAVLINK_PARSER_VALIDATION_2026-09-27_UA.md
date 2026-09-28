# MAVLink parser: CRC, payload та межі довіри

Початок: `2026-09-27`, завершення checkpoint: `2026-09-28`. Початкова версія: `db699fb`. Локальна перевірка у WSL/GCC, без запуску камери, serial transport або output session.

Виправлено CRC-перевірку семи декодованих повідомлень, неправильне читання optical flow та підтримку скорочених MAVLink 2 payloads. Це зміна поведінки приймання телеметрії. Вибір довіреного джерела й інтерпретація режиму для різних автопілотів залишаються відкритими: нижче наведено фактичне відтворення.

## Що відтворено до зміни

Пакети сформовано незалежним `pymavlink 2.4.49`, діалектами `common` v1/v2, і передано справжньому `inspect_mavlink_telemetry_bytes`.

| Вхід | Старий результат | Результат після виправлення |
|---|---|---|
| `OPTICAL_FLOW`, v1: distance 0.75 m, quality 220 | −0.0294018 m, quality 0 | 0.75 m, quality 220 |
| `OPTICAL_FLOW_RAD`: distance 1.25 m, quality 219 | близько −3.8843e16 m, quality 0 | 1.25 m, quality 219 |
| `GLOBAL_POSITION_INT` v2: 42.5 m, payload 18 bytes | malformed, altitude відсутня | 42.5 m |
| Сім v2 повідомлень із нульовими полями, payload 1 byte | усі відхилено за довжиною | нульові поля декодовано |
| Змінений байт heartbeat без перерахунку CRC | heartbeat прийнято | checksum error, heartbeat не оновлено |

У `OPTICAL_FLOW` попереднє читання `float` від offset 24 виходило за 26-byte payload у два checksum bytes; це логічне порушення меж payload, хоча доступ ще був усередині загального string packet. Правильні offsets distance/quality — 16/25; для `OPTICAL_FLOW_RAD` — 36/43. `DISTANCE_SENSOR` v2 з 13-byte payload також тепер зберігає id/orientation: відсутній covariance доповнюється нулем. Wire layouts звірено з незалежними generated definitions та [OPTICAL_FLOW](https://raw.githubusercontent.com/mavlink/c_library_v2/master/common/mavlink_msg_optical_flow.h), [OPTICAL_FLOW_RAD](https://raw.githubusercontent.com/mavlink/c_library_v2/master/common/mavlink_msg_optical_flow_rad.h).

## Поточний контракт parser

Для `HEARTBEAT`, `ATTITUDE`, `GLOBAL_POSITION_INT`, `OPTICAL_FLOW`, `OPTICAL_FLOW_RAD`, `DISTANCE_SENSOR`, `ALTITUDE` перед декодуванням перевіряється CRC із відповідним `CRC_EXTRA`. MAVLink 1 вимагає повної базової довжини. У MAVLink 2 дозволено скорочення до одного byte, відсутні байти відновлюються нулями в локальному payload buffer. Довші payloads дозволено для сумісності з майбутніми extension fields; CRC охоплює також цей хвіст, декодується лише відомий prefix. Правила CRC/truncation/extensions: [MAVLink serialization](https://mavlink.io/en/guide/serialization.html).

- Пошкоджені відомі повідомлення не змінюють значення, semantic counters або receipt offsets. `checksum_errors` також збільшує `malformed_frames`.
- Невідомі message IDs потрапляють у `message_id_counts` та `unsupported_message_frames`, але не декодуються і не підтверджують health. Їхня CRC не перевіряється: таблиця діалекту обмежена сімома потрібними повідомленнями. Самі лише `frames_seen` або нуль `malformed_frames` не доводять валідність усіх повідомлень потоку.
- Усі ненульові incompatibility flags відхиляються. Пакети з signed flag окремо рахуються в `unsupported_signed_frames`: реалізації key/signature/replay verification немає. Навіть коректно підписані fixture packets відхиляються. Для їх підтримки потрібен окремий контракт і реалізація, описані в [MAVLink signing](https://mavlink.io/en/guide/message_signing.html). Unsigned packets із коректною CRC досі дозволені; це не автентифікація.
- `unsupported_incompatibility_frames` рахує інші невідомі incompatibility bits. Невідомі compatibility bits дозволені після CRC-перевірки.
- `frames_seen`, version counters і message ID counts лишаються лічильниками повних структурних пакетів, включно з відхиленими. Нові діагностичні поля виводяться CLI inspection.
- Неповний хвіст, як і раніше, дає malformed. Стандартний `maximum_malformed_frames=0` блокує validation до отримання повного пакета або витіснення пошкоджених даних із bounded tail. Явно послаблений користувачем threshold може залишити старі дані придатними до validation, але rejected packets не освіжають їхній receipt time.

Reader, byte-buffer receipt algorithm, camera caller, adapter thresholds і packet writers не змінено. Відомі поля декодуються з локального 44-byte zero-padded buffer; CRC/signature bytes не можуть стати частиною значення payload.

## Тести й відтворюваність

Новий `mavlink_telemetry_protocol_test` використовує 35 зафіксованих golden packets: 28 unsigned v1/v2 із звичайними й нульовими полями та сім signed v2. Генератор `core/tests/generate_mavlink_telemetry_golden.py` потребує `pymavlink==2.4.49`; C++ build і CTest цієї залежності не мають. Повторна генерація дала байтово тотожний header. Детермінований ключ у генераторі — публічні тестові bytes, не ключ пристрою.

Перевірено 5384 одиночні bit corruptions, 3584 комбінації message/version/payload length (0–255) і 1089 incomplete prefixes, а також unknown 24-bit ID, compatible/incompatible flags, extensions до 255 bytes, values/offsets, retained old evidence та recovery. CRC helper старих fixture tests перевіряється незалежними golden bytes; виправлено неправильний layout старого `OPTICAL_FLOW_RAD` fixture.

Receipt integration зберігає всі 126 packet split cases; для signed packets тепер очікує відхилення і незмінний старий час. Додано пошкоджені heartbeat/attitude/position через справжні buffer, caller helper та adapter: навіть дозволені три malformed packets не роблять старий стан свіжим. Після витіснення пошкодженого tail свіжі дані відновлюють допуск.

- Debug і Release: **62/62** CTest; Python: **15/15** і **5/5**.
- `BUILD_TESTING=OFF`: успішна збірка, **0** тестів. Сім camera/output flags OFF у всіх трьох caches. Release assertions активні.
- Новий protocol test очікувано впав на parser із `db699fb` через прийнятий пакет із неправильною CRC; після виправлення пройшов.
- Targeted ASan/UBSan: protocol test та повна inspector translation unit, `detect_leaks=1`, `halt_on_error=1`, без помилок. Це не повний sanitizer-прогін camera/runtime.
- Команда suite: `bash scripts/test-software.sh artifacts/config-validation-20260921/clean-build`.
- Локальні artifacts: `artifacts/mavlink-parser-20260927/`: `before.txt`, `after.txt`, `protocol-before.log`, `protocol-sanitized.log`, `software-tests.log`, `no-tests-build.log`, `tested-source-hashes.json`, `source-audit.log`.

## Підтверджені залишкові обмеження та наступний крок

Окремий локальний `source-audit.cpp` із правильними CRC виконав фактичні inspector/validation functions після виправлення:

1. Heartbeat від `42:17`, attitude від `43:18`, position від `44:19` разом проходять стандартну validation.
2. Ті самі три повідомлення від `0:0` також проходять validation.
3. Heartbeat із `type=1`, `autopilot=12`, `custom_mode=4` інтерпретується як `Guided`: mapping ArduCopter застосовується без перевірки сімейства автопілота/типу апарата.

Отже наступний bounded software slice — визначити явне очікуване джерело, перевіряти його узгодженість для heartbeat/attitude/altitude та застосовувати mode mapping лише за відповідного heartbeat contract. Автоматично приймати перший побачений sysid або припускати, що справжній борт має `1:1`, підстав немає. CRC не захищає від навмисної підміни чи повторного передавання; host receipt time не є FC measurement time. Перевірка фізичної правдоподібності всіх полів, повний runtime timing, Pi/MSVC acceptance і незалежний повторний прохід маршруту цим кроком не закриті.

GitNexus pre-edit impact був неповним `UNKNOWN`; прямі виклики ведуть до file-inspection CLI і byte-buffer append, далі до telemetry health. Graphify AST index оновлено: 5971 nodes / 9237 edges. GitNexus CLI refresh завершився: 14239 nodes / 29075 edges / 209 flows, FTS/BM25 недоступний. Повторні staged/main checks після відновлення сесії повернули `CRITICAL` для 189/193 flows (попередній staged результат — 187). Усі staged changed steps приписано одному empty-ID `checksum_errors`, включно з нерелевантними JSX-процесами. Compare-main також показує очікувані append → parser/layout/CRC/mode/time та попередні benchmark flows. Context має порожні UID, хибні references й застарілу metadata після CLI refresh. Це не чистий graph verdict: межі зміни перевірено 14-file staged diff, direct calls, hashes протестованих файлів та suites. Camera/stream/adapter/writers додатково порівняно з baseline. Сирі graph responses збережено в artifacts.

Після переривання через нестачу місця на диску повторно звірено SHA-256 усіх протестованих code/build files: збігаються. Фактичний Release CLI відхилив пошкоджений heartbeat із exit code 2, `checksum_errors=1`, `heartbeat_messages=0`. Розбіжність Windows/WSL Git у трьох документах виявилася лише CRLF-нормалізацією; сторонні файли не переписувались. Фінальні докази: `verify-final.py`, `verify-final.log`, `cli-corrupt.log`.
