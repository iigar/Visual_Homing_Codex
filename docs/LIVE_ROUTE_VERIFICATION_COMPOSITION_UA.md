# Live Route Verification Composition

## Призначення

`LiveRouteVerificationProducer` — fail-closed composition boundary між production tracking evidence та вже прийнятим `BoundedVerificationPublisher`.

Він не відкриває камеру, не запускає matcher і не володіє publisher lifecycle. Caller передає native camera frame разом із route/health/scalar evidence, а producer:

1. зв’язує `RouteMatch.timestamp` із timestamp саме цього native frame;
2. вимагає valid match, мінімальну confidence та явний tracked progress у межах `0..1`;
3. перевіряє, що health snapshot не старий, camera/navigation links готові, а MAVLink health потрібен лише коли це явно задано config;
4. вимагає окремі valid, finite і fresh altitude, scale та yaw observations;
5. за наявності local pose вимагає same `frame_id`, freshness, повністю названі frame ID/revision/convention та quality bounds;
6. тільки після цих перевірок формує `LiveVerificationFrameContext` і викликає bounded publisher;
7. без втрат семантики повертає `accepted`, `backpressure`, `not_running` або `failed`; pre-worker validation дає окремий `rejected`.

Ця межа не перетворює backpressure на silent drop і не створює route evidence з відхиленого observation.

## Чому Це Не First-Pass Route Recording

Під час первинного запису маршрут ще не існує як завершений corridor, тому справжній normalized route progress визначити неможливо. Підставляти frame index або elapsed time як progress було б неправдивою геометрією.

Перший production caller має працювати під час live traversal/matching уже існуючого indexed route package:

```text
native camera frame
  + same-frame route match
  + tracked route progress
  + fresh health/altitude/scale/yaw
  + optional trusted local pose
        -> LiveRouteVerificationProducer
        -> BoundedVerificationPublisher
        -> LiveVerificationCaptureSession
        -> immutable verification VHRS/VHRM revision
```

Створення verification layer для нового first-pass route потребує окремого двопрохідного workflow: спочатку finalize tracking route та VHIX, потім повторно пройти corridor або виконати offline/native-frame enrichment із чесною progress прив’язкою.

## Progress-Only І Local Pose

Local pose не вигадується з одного route progress.

- Без local pose producer формує progress-only context. Selector може публікувати звичайні sparse verification keyframes через route displacement, altitude, scale, yaw, scene novelty або maximum interval.
- Gate candidate неможливий, бо `has_local_pose=false`.
- Якщо local pose подано, config повинен містити повний expected local-frame contract, а observation — точно той самий frame ID/revision/convention.
- Pose uncertainty і approach radius мають бути finite, non-negative, причому approach radius не може бути меншим за uncertainty. Остаточний gate margin/separation/novelty усе одно перевіряє selector.

Наявність local coordinates або gate record не є route lock, `reset_reference`, ODOMETRY permission чи flight authority.

## Відхилення До Worker

Producer відхиляє observation до descriptor generation і SD publication, зокрема при:

- invalid або low-confidence route match;
- route-match/native-frame timestamp mismatch;
- відсутньому або некоректному tracked progress;
- stale health-frame context;
- required camera/navigation/MAVLink health failure;
- health/match confidence mismatch;
- invalid, non-finite або stale altitude/scale/yaw;
- local pose без configured frame contract;
- local-pose frame ID, timestamp або coordinate-frame mismatch;
- invalid uncertainty/approach quality.

Metrics рахують observations, rejected, progress-only/local-pose contexts і кожний publisher outcome. Остання причина pre-worker rejection зберігається окремо.

## Operational Progress-Only Caller

`match_live_camera_route` тепер володіє optional bounded publisher/producer lifecycle для другого проходу вже indexed route package:

- source VHRM перевіряється разом з усіма artifacts до відкриття камери;
- source package мусить мати рівно один tracking chunk, фізично тотожний `.vhrs`, який читає live matcher;
- camera profile/native dimensions, search-index ID і descriptor dimensions мусять збігатися;
- producer отримує raw native frame, same-frame processed match, tracked progress, current health, fresh read-only `relative_altitude`, same-frame visual scale та `RouteMatch.direction_error_rad` як image-derived yaw residual; host freshness altitude оновлюється лише коли cumulative inspector бачить збільшення `relative_altitude_samples`, тому повторне читання старого buffer не омолоджує scalar;
- local-frame contract заборонений у цьому режимі, `local_pose` завжди absent, тому published gate count мусить бути zero;
- command, external-nav estimate/output і live-output sessions не можуть бути увімкнені разом із цим evidence-only режимом;
- terminal publisher failure/not-running зупиняє session; backpressure та pre-worker rejection рахуються явно; завершення виконує `stop(drain=true)` і вимагає `accepted == completed`, zero abandoned/discarded/failures, хоча б одну publication і zero gates.

Env contract починається з `VISUAL_HOMING_LIVE_ROUTE_VERIFICATION=1` і явно задає `..._SOURCE_MANIFEST`, `..._OUTPUT_MANIFEST`, `..._SEARCH_INDEX_ID`, `..._LAYER_ID`, `..._DESCRIPTOR_DIMENSIONS`, `..._ROUTE_LENGTH_M`, `..._MIN_ALTITUDE_M`, `..._MAX_ALTITUDE_M`; queue/keyframe/interval/displacement/age bounds мають окремі optional `VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_*` overrides. Visual scale вмикається через `VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1` та `VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M`.

Required variable names:

```text
VISUAL_HOMING_LIVE_ROUTE_VERIFICATION=1
VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_SOURCE_MANIFEST=<indexed-source.vhrm>
VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_OUTPUT_MANIFEST=<output-base.vhrm>
VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_SEARCH_INDEX_ID=<vhix-record-id>
VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_LAYER_ID=<new-verification-layer-id>
VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_DESCRIPTOR_DIMENSIONS=<vhix-dimensions>
VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_ROUTE_LENGTH_M=<measured-route-length>
VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_MIN_ALTITUDE_M=<layer-min>
VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_MAX_ALTITUDE_M=<layer-max>
VISUAL_HOMING_VISUAL_SCALE_DIAGNOSTICS=1
VISUAL_HOMING_VISUAL_SCALE_REFERENCE_ALTITUDE_M=<recording-reference-altitude>
```

Optional exact names: `VISUAL_HOMING_LIVE_ROUTE_VERIFICATION_DIRECTORY`, `..._MAX_KEYFRAMES`, `..._QUEUE_CAPACITY`, `..._MIN_INTERVAL_MS`, `..._MAX_INTERVAL_MS`, `..._MIN_DISPLACEMENT_M`, `..._MAX_CONTEXT_AGE_MS`, `..._MAX_SCALAR_AGE_MS`.

Це навмисно ще не multi-chunk live matcher. Kilometer-scale multi-chunk package потребує окремого bounded chunk/window consumer; поточний strict one-chunk contract не дозволяє випадково збагатити не той corridor.

## Поточний Статус

Library composition та operational live-matcher/CLI progress-only attachment реалізовані. Поточний WSL/GCC all-output-off suite проходить `47/47`, включно з package-binding negative tests; producer tests зберігають stale/provenance/backpressure/terminal-failure coverage. Останній чистий Pi baseline поки лишається `46/46` на exact commit `135942f`; clean Pi test нового caller-а ще pending. Поточний Windows test не стартував через відсутній `cl.exe` у локальній Visual Studio Build Tools installation; попередній MSVC affected baseline був green.

Ще не виконано:

- trusted metric local-pose source з uncertainty/approach contract;
- revision resume, physical SD fault injection, high-resolution content verification, multi-frame route lock і global reacquisition.

Жодний FC/UART/MAVLink output, ODOMETRY, reset, Home або command path не підключений.

## Наступний Integration Slice

Поточний пріоритет станом на `2026-08-13` — clean Pi all-output-off build/CTest для нового caller-а, потім окремий hand-carried second pass уже відомого finalized/indexed маршруту forward/reverse без arm/send. Перед фізичним проходом потрібен wrapper/checker, який перевіряє source-package identity, completed drain, published revisions та `gates=0`.

Перший recording pass не є допустимим caller-ом до finalize/index. Канонічна повна черга: `docs/CURRENT_PROJECT_STATUS_UA.md`.
