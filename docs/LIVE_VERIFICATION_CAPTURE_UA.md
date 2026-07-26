# Live Native Verification Capture

## Статус

`LiveVerificationCaptureSession` — окрема library boundary між native Gray8 кадром і `VerificationPackageWriter`. Вона не відкриває камеру сама, не виконує route matching і не визначає геометрію маршруту. Caller передає один native кадр та явно синхронний route/health/altitude/scale/yaw/local-pose context.

Session:

1. перевіряє native format, dimensions і payload;
2. бере `frame_id` та monotonic timestamp лише з цього native кадру;
3. downsample-ить цей самий кадр до source dimensions із VHIX;
4. будує descriptor саме з параметрами VHIX, referenced source manifest;
5. створює observation і виконує selector `evaluate()`;
6. при capture request створює native `RouteSignatureEntry` з того самого кадру;
7. передає synchronized entry/observation/decision у transactional package writer.

Таким чином caller не може окремо підставити high-resolution frame identity або descriptor від іншого кадру. Геометричні поля context усе ще є зовнішніми untrusted inputs і проходять fail-closed перевірку; ця межа не доводить їх фізичну істинність.

## Bounded Background Publisher

`BoundedVerificationPublisher` виносить увесь синхронний `LiveVerificationCaptureSession::observe()` у один background worker. Черга має фіксовану додатну capacity, яка рахує і поточну активну роботу, і queued frames. Producer ніколи не чекає завершення SD publication: `submit()` повертає один із явних станів `accepted`, `backpressure`, `not_running` або `failed`.

Контракт навмисно не робить silent drop:

- `accepted` означає, що native `Frame` разом із route/health context отримав монотонний submission sequence;
- `backpressure` означає, що bounded capacity вичерпано; caller бачить цю подію в результаті та metrics і може продовжити tracking без блокування;
- exception із writer/session переводить publisher у terminal failed state, відкидає ще не початі jobs із точним `abandoned_after_failure` count і забороняє подальші submissions;
- `stop(true)` дренує прийняті jobs, а `stop(false)` зберігає активну операцію, але явно рахує та відкидає queued jobs;
- restart одного instance заборонений, щоб після failure/stop не успадкувати неоднозначний selector/package state.

Worker не змінює transactional правило: selector `commit()` як і раніше відбувається всередині synchronous session лише після повного запису та перевірки immutable package revision. Publisher також надає queue-wait/processing/max-outstanding metrics і останню успішну publication. Читання underlying capture metrics або manifest дозволене лише коли worker idle.

## Pi Benchmark Tool

`live_verification_capture_benchmark` використовує реальний `PiCameraSource`, але лише як camera/storage/load harness. Для ізоляції від польотних маршрутів він створює у новому каталозі synthetic one-entry tracking package, будує реальний VHIX, а потім публікує native verification frames через maximum-interval trigger.

Benchmark навмисно тримає `route_progress=0`, не надає local pose і не створює gate records. Його artifacts не є польотним маршрутом або evidence маршрутизації. Tool не відкриває FC/UART, не читає або пише MAVLink, не створює ODOMETRY і не має command authority.

Pi wrapper:

```bash
./scripts/run-verification-capture-benchmark-pi.sh
```

Default profile:

- requested capture: `1280x800`, Gray8;
- requested camera rate: `10 fps`;
- duration: `600 s`;
- sparse publication interval: `10 s`;
- background outstanding-job capacity: `2`;
- all live-command and external-nav output CMake flags: `OFF`.

Wrapper перед запуском збирає core і виконує весь CTest. Під час benchmark він щосекунди записує CSV із process RSS, SoC temperature, ARM clock, `get_throttled` і доступним місцем на файловій системі. Tool окремо логгує actual observed FPS, descriptor/selector/publication latency, publication count, записані bytes і cumulative package-file verification count.

Оновлений tool додатково логгує camera-loop FPS окремо від final drain, submissions/accepted/completed/backpressure, maximum outstanding jobs, queue-wait і worker-processing latency, terminal failure/abandoned/discarded counts та drain time. Значення capacity можна змінити через `VISUAL_HOMING_VERIFICATION_BACKGROUND_QUEUE_CAPACITY`; це benchmark tuning knob, а не дозвіл робити чергу необмеженою.

`PiCameraConfig::frame_rate_hz` у поточному backend ще не програмує libcamera frame-duration control; це requested/timeout parameter. Тому acceptance використовує виміряний `effective_observed_fps`, а не саме requested значення.

## Fail-Closed Межі

- Source VHRM/VHIX і native camera compatibility перевіряються до session use.
- VHIX descriptor type/dimensions мають точно збігатися з selector contract.
- Native identity походить тільки з одного `Frame`; descriptor і stored native entry створюються всередині одного `observe()`.
- Writer failure не commit-ить selector; той самий frame/context можна повторити після явного усунення collision/fault.
- Queue saturation повертає explicit backpressure; вона не блокує camera loop і не маскується як accepted frame.
- Worker failure є terminal для instance; pending jobs не публікуються і не commit-ять selector.
- Existing output directory, artifact/partial collision, malformed frame або invalid context завершують benchmark/session помилкою; artifacts автоматично не видаляються.

## Pi Evidence

Канонічний 10-хвилинний Pi Zero 2W + OV9281 run на commit `4c4458e` пройшов із `1280x800`, `60/60` publications, `0` failures, effective `11.1314 fps`, RSS max `25668 KiB`, temperature max `66.066 °C` і `get_throttled=0x0` у всіх `573` system samples. Деталі та artifact paths: `docs/PI_VERIFICATION_CAPTURE_BENCHMARK_2026-07-19_UA.md`.

Publication latency у першому run зросла від `110.024 ms` до `3072.75 ms`, бо кожна revision синхронно перевіряє весь cumulative package. Це було прийнятне storage/cadence evidence, але не production-loop architecture; воно стало baseline для bounded-background remediation нижче.

Повторний bounded-background run на commit `e1d9ac2` пройшов `2026-07-27`: Pi CTest `45/45`, native OV9281 `1280x800`, `60/60` publications, `8439/8439` accepted/completed, max outstanding `2`, explicit backpressure `1514`, zero failed/abandoned/discarded jobs, camera-loop `16.5967 fps`, RSS max `25580 KiB`, temperature max `61.224 °C`, `get_throttled=0x0`, final drain `0.163331 ms`. Cumulative publication max залишився `3080.83 ms`, але більше не блокував camera polling. Деталі: `docs/PI_VERIFICATION_CAPTURE_ASYNC_BENCHMARK_2026-07-27_UA.md`.

## Що Ще Не Закрито

- Physical SD durability при раптовій втраті живлення і directory/file sync.
- Restart/resume з immutable revisions.
- Library-level production composition boundary тепер реалізована як `LiveRouteVerificationProducer`: same-frame match, tracked progress, health і fresh altitude/scale/yaw перевіряються до bounded publisher; optional local pose потребує повністю названого frame contract. Деталі: `docs/LIVE_ROUTE_VERIFICATION_COMPOSITION_UA.md`.
- Operational caller у live matcher/route-enrichment runtime ще не підключений. First-pass recorder не може чесно дати route progress до finalize/index; для нового route потрібен окремий двопрохідний workflow.
- Trusted metric local-pose producer з uncertainty/approach evidence. Pi all-output-off acceptance нового composition module завершено `46/46` на commit `135942f`; log і digest наведені в `docs/LIVE_ROUTE_VERIFICATION_COMPOSITION_UA.md`.
- High-resolution content verification, multi-frame route lock, global reacquisition і `reset_reference`.
