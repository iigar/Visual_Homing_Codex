# Аудит Референсного Репозиторію

Дата аудиту: `2026-07-18`.

## Обсяг І Походження

- Зовнішній snapshot: `D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_System_Claude-conflict_250226_1330`.
- Tracked baseline: `reference/Visual_Homing_System_Claude-conflict_250226_1330`.
- GitNexus-копія: `D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_System_Claude-conflict_250226_1330_gitnexus_indexed_copy`.
- Зовнішній snapshot не мав `.git` і не був репозиторієм.
- SHA-256 звірка всіх відносних шляхів і файлів: `242` файли в кожному дереві, `0` відмінностей. Отже зовнішня conflict-папка не містить новішого прихованого коду порівняно з tracked `reference/`.
- Оригінальний snapshot не змінювався. Git і GitNexus metadata створені лише в окремій `_gitnexus_indexed_copy`.

## Стан GitNexus-Копії

```text
repository=Visual_Homing_System_Claude-conflict_250226_1330_gitnexus_indexed_copy
baseline_commit=c5c9cca
metadata_commit=405e7ac
git_status=clean
indexed_files=215
symbols=3436
relationships=5172
clusters=79
execution_flows=84
```

`context`, `impact`, `cypher` і process resources працюють. BM25/FTS не завантажився у поточній Windows/LadybugDB конфігурації, тому natural-language `query` повертає деградований/порожній результат. Це обмеження пошуку, а не пошкодження графа.

## Класифікація Компонентів

| Компонент | Оцінка | Що можна використати | Що блокує пряме перенесення |
|---|---|---|---|
| Python TF-Luna parser | `REWRITE/TEST` | Пошук `0x59 0x59`, 9-byte frame, checksum, freshness skeleton | Перевірити temperature formula, signal/range policy, додати raw-capture test vectors |
| Python Matek/MSP V2 optical flow | `REWRITE/TEST` | Function IDs, payload layout, threaded read skeleton | Немає CRC validation, payload-size bound, чіткого timestamp/freshness contract і перевіреної axis/sign mapping |
| Python LK visual odometry | `DIAGNOSTIC_ONLY` | RANSAC/stability/deadband/recovery ідеї | Hard-coded `fx=500`/camera matrix, немає camera/body/route frame contract, altitude projection і drift не дозволяють FC authority |
| ORB/homography route follower | `CANDIDATE_REFINEMENT_ONLY` | Local keyframe search, serialized features, reacquisition ideas | Homography translation напряму перетворюється на velocity/yaw commands без metric/frame/attitude/safety contract |
| Python MAVLink bridge | `REJECT_FOR_OUTPUT` | Лише read-only telemetry/API wiring як довідка | Старий `VISION_POSITION_ESTIMATE`/speed path без current covariance/reset/frame/alignment contracts; ambiguous body-frame commands |
| SmartRTL state model | `CONCEPT_ONLY` | Назви фаз і UI/state-machine vocabulary | IMU+barometer не дають horizontal GPS-denied home position; unsafe fallback може породжувати command dictionaries без current gates |
| C++ implementation | `LOW_VALUE_SCAFFOLD` | Загальна структура класів | MAVLink parsing/sending і route loading містять TODO; VO decomposition і safety coverage слабші за current core |
| Android Kotlin/Compose | `REUSE_PATTERNS` | Retrofit DTO/API, repositories, mDNS/manual IP, WebSocket telemetry, Settings/UI structure | Додати reconnect/backoff, error visibility, auth/TLS as needed; Android не має вирішувати readiness/safety |
| Backend/frontend | `TOOLS/DEMO_ONLY` | UI mockups, endpoint shape ideas | Random/simulated route and position behavior; web tests не є sensor/navigation/flight evidence |

## Підтвердження З Графа

- `OpticalFlowSensor`, `LidarSensor`, `RouteFollower` і `VisualOdometry` знайдені як окремі Python classes, але не беруть участі в основних GitNexus execution flows; incoming links обмежені package `__init__.py`.
- `SmartRTL` має incoming import також із `firmware/python/tests/test_smart_rtl.py`, але не утворює завершений hardware-to-navigation provider flow.
- Наявність класів і web test counts не доводить наскрізну інтеграцію, flight readiness або коректність сенсорних протоколів.

## Правило Подальшого Використання

1. Не переносити reference tree або scheduler цілком.
2. Для кожної потрібної ідеї сформулювати поточний `core/` interface і safety contract.
3. Брати з reference тільки test vectors, protocol clues, UI/API patterns або small-candidate algorithm ideas.
4. Переписувати з bounded buffers, CRC/length validation, explicit frames/units/signs, freshness/health/reset/rate gates і fail-closed outputs.
5. Перед редагуванням current-core symbols запускати GitNexus upstream impact; після змін — тести й `detect_changes`.

## Висновок

Reference snapshot корисний як каталог ідей, UI patterns і parser prototypes. Він не є альтернативною готовою flight implementation і не змінює валідність проведених current-core тестів. Найближчий current-core етап залишається незмінним: окремий route-local estimator зі start-relative height, forward/reverse yaw, reset/rate/health gates, потім SITL перевірка exact ArduCopter `4.3.6` ODOMETRY contract без підключення реального FC.
