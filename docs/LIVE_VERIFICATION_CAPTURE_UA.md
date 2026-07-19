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
- all live-command and external-nav output CMake flags: `OFF`.

Wrapper перед запуском збирає core і виконує весь CTest. Під час benchmark він щосекунди записує CSV із process RSS, SoC temperature, ARM clock, `get_throttled` і доступним місцем на файловій системі. Tool окремо логгує actual observed FPS, descriptor/selector/publication latency, publication count, записані bytes і cumulative package-file verification count.

`PiCameraConfig::frame_rate_hz` у поточному backend ще не програмує libcamera frame-duration control; це requested/timeout parameter. Тому acceptance використовує виміряний `effective_observed_fps`, а не саме requested значення.

## Fail-Closed Межі

- Source VHRM/VHIX і native camera compatibility перевіряються до session use.
- VHIX descriptor type/dimensions мають точно збігатися з selector contract.
- Native identity походить тільки з одного `Frame`; descriptor і stored native entry створюються всередині одного `observe()`.
- Writer failure не commit-ить selector; той самий frame/context можна повторити після явного усунення collision/fault.
- Existing output directory, artifact/partial collision, malformed frame або invalid context завершують benchmark/session помилкою; artifacts автоматично не видаляються.

## Pi Evidence

Канонічний 10-хвилинний Pi Zero 2W + OV9281 run на commit `4c4458e` пройшов із `1280x800`, `60/60` publications, `0` failures, effective `11.1314 fps`, RSS max `25668 KiB`, temperature max `66.066 °C` і `get_throttled=0x0` у всіх `573` system samples. Деталі та artifact paths: `docs/PI_VERIFICATION_CAPTURE_BENCHMARK_2026-07-19_UA.md`.

Publication latency зросла від `110.024 ms` до `3072.75 ms`, бо кожна revision синхронно перевіряє весь cumulative package. Це прийнятне storage/cadence evidence, але не production-loop architecture: перед підключенням до tracking потрібен bounded background publication worker.

## Що Ще Не Закрито

- Bounded background publication/backpressure без блокування tracking loop; synchronous 10-second cadence перевірена лише як benchmark.
- Physical SD durability при раптовій втраті живлення і directory/file sync.
- Restart/resume з immutable revisions.
- Підключення session до справжнього route-progress/local-pose producer під час route collection.
- High-resolution content verification, multi-frame route lock, global reacquisition і `reset_reference`.
