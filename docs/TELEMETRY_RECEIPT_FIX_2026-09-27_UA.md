# Час отримання read-only телеметрії

Дата: `2026-09-27`. Початкова версія коду: `6b7060b`; перед змінами окремо створено опис системи в `SYSTEM_DESCRIPTION_UA.txt` (commit `1a6ddbe`).

Виправлено два дефекти, відтворені в [timestamp audit](RUNTIME_TIMESTAMP_AUDIT_2026-09-27_UA.md): retained snapshot більше не отримує новий час під час читання камерою, а оновлення altitude не залежить від кількості samples у sliding window. Зміна охоплює початковий telemetry warmup і кадри `match_live_camera_route`. Налаштування age limits та output authority збережено.

## Механізм

Inspector додатково повертає exclusive end offsets останніх успішно декодованих heartbeat, attitude і relative-altitude повідомлень. Decoder, framing rules, counters та structural validation залишені незмінними; це перевірено механічним source comparison.

`MavlinkTelemetryByteBuffer::append` приймає receive timestamp. Старий двоаргументний API збережено; реальний read loop знімає `now()` після успішного `read`, перед mutex/розбором. Для кожного з трьох компонентів зберігається absolute byte position (`bytes_dropped + relative_end`) та час читання, що завершило повідомлення. Ідентичність не залежить від window count, значення altitude або MAVLink sequence number. `clear` починає новий stream run і скидає metadata.

Повторний snapshot, noise, unrelated packet та retained message не змінюють цей час. Незавершений packet отримує receipt тільки після надходження всього frame, включно з checksum/signature bytes. Якщо після зсуву tail parser уперше побачить повідомлення, що повністю лежить у старих bytes, час залишається невідомим — новий timestamp не вигадується.

Замість розбору tail під час кожного camera snapshot, inspection тепер обчислюється на кожному непорожньому append і копіюється разом із receipt metadata під тим самим mutex. Додано лише три receipt records, без історії timestamps для кожного byte/read. Розбір залишається O(retained bytes) на append; продуктивність при різних serial read rates на Pi в цьому кроці не вимірювалася. Це зміна місця роботи parser, не доведене прискорення.

Спільний `live_route_match_telemetry_observation` перевіряє structural validation і наявність ненульового receive time без майбутніх timestamps. Aggregate telemetry time — найстаріший із heartbeat/attitude/altitude, щоб оновлення одного поля не освіжало інші. Altitude має власний receive time. Caller передає aggregate time в adapter та output-gate evidence, а власний altitude time — у verification scalar. Старий sample counter видалено.

Warmup тепер проходить лише зі свіжою телеметрією за наявним adapter limit. Невалідний/неповний snapshot у кадровому loop одразу дає degraded telemetry health; старий valid adapter state його не перекриває. Перевірки NaN/negative altitude для verification scalar збережено. Це навмисне посилення admission за коректним походженням часу, а не behavior-preserving refactor.

## Що виконано локально

До редагування повторено точний старий runtime block із реальними inspector/buffer/adapter: при 9000 ms тиші й limit 500 ms health знову ставав true; при заміні 102-byte tail нове значення 1 m не замінювало cached 42.5 m. Старі reproduction artifacts залишені в `artifacts/runtime-timestamps-20260927/` як історичні дані.

Новий `telemetry_receipt_integration_test` виконує справжні buffer, inspector, спільний caller helper та adapter. Перевірено:

- вік рівно 500 ms приймається, на 1 ns більше — відхиляється;
- повторне читання старих даних і zero-byte append не освіжають час;
- повна й часткова заміна bounded tail, однаковий sample count, повторене значення altitude та sequence number;
- noise/unrelated message, лише нова altitude, лише новий heartbeat, відновлення після надходження решти компонентів;
- усі **126** місць розділення між двома читаннями для fixture GLOBAL_POSITION_INT у MAVLink v1, v2 і v2 із signature-sized suffix;
- 102 однобайтові append із порівнянням cached counters і повного inspector;
- час окремих компонентів, витіснення всіх релевантних повідомлень та recovery;
- ALTITUDE замість GLOBAL_POSITION_INT лише за чинною explicit config;
- missing/zero/future receipt, malformed payload, clear/restart metadata, NaN/negative altitude.

Fixtures структурні, як у наявному inspector test: вони не є перевіркою CRC або автентичності signature. Повний camera/serial loop не запускався. Source comparison підтверджує незмінність `camera_smoke.cpp` поза новим helper, видаленням старого counter і двома блоками споживання telemetry. Transport mode, packet writers, output gates, thresholds і решта runtime не змінені.

## Валідація

- WSL/GCC Debug і Release: **61/61** CTest; Python suites **15/15** і **5/5**.
- `BUILD_TESTING=OFF` успішно зібрано, 0 tests; сім camera/output flags OFF у всіх трьох caches.
- Новий Release test має `-UNDEBUG` після `-DNDEBUG`; hashes протестованих code/build files збережено.
- Targeted ASan/UBSan test пройшов із `detect_leaks=1`, `halt_on_error=1`: новий test, camera helper, stream/inspector/adapter/clock/health translation units інструментовані. Для RTTI-залежностей невиконуваних частин camera-модуля підключено звичайну Debug library; це не повний sanitized build усіх core targets. Логи: `sanitized-build.log`, `sanitized-test.log`.
- Команда: `bash scripts/test-software.sh artifacts/config-validation-20260921/clean-build`.
- Локальні artifacts: `artifacts/telemetry-receipts-20260927/`, зокрема `software-tests.log`, `no-tests-build.log`, `verify-scope.py`, `tested-source-hashes.json`.

GitNexus pre-edit impact залишився `UNKNOWN`; прямий огляд установив stream/file-inspection/test callers і live matcher CLI. Після CLI refresh (14165 nodes / 27784 edges / 205 flows) staged/main checks показали `CRITICAL` для 199/204 flows. Переглянуто реальні `append -> parser/read_u32/mode` та `append -> now` зв'язки; вони очікувані. Решта attribution переважно походить від порожнього symbol ID (навіть README `Direction`), а compare-main додатково містить попередні benchmark flows. Context також неповний і неправильно називає read-loop caller `lock`. Це не чистий graph verdict; accepted scope визначено actual diff, direct calls, source comparison і тестами. Graphify оновлено до 5928 nodes / 9175 edges. Сирі responses збережено в artifacts.

## Межі та наступна робота

Host receive time означає завершення читання packet у програмі, не час вимірювання на FC і не затримку до host read. Timestamp камери й охоплення processing latency залишаються описаними в попередньому audit. Поточний framing inspector не перевіряє CRC/signature content/source sysid/compid; виправлення receipt metadata не додає довіри до payload. Наступний software review має окремо закрити packet integrity/source contract і точні payload bounds без змішування цього з дозволом output.

Виправлення не змінює історичне recording metadata і не є перевіркою точності розпізнавання. Hardware/Pi/MSVC acceptance, повний повторний прохід і повна runtime latency distribution залишаються окремими прогалинами evidence.
