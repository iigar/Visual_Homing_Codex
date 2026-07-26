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

## Поточний Статус

Library implementation і deterministic tests завершені. WSL/GCC full suite проходить `46/46`, новий test проходить `100/100` повторів, MSVC 19.44/Ninja проходить три пов’язані tests.

Ще не виконано:

- operational caller у `match_live_camera_route` або окремому route-enrichment runtime;
- CLI/environment config для source VHRM/VHIX, output revision base і scalar/local-pose sources;
- trusted metric local-pose source з uncertainty/approach contract;
- Pi all-output-off build/test цього нового composition module;
- revision resume, physical SD fault injection, high-resolution content verification, multi-frame route lock і global reacquisition.

Жодний FC/UART/MAVLink output, ODOMETRY, reset, Home або command path не підключений.
