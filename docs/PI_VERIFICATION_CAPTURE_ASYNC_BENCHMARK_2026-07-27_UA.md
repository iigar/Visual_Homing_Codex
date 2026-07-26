# Pi Bounded Verification Publication Benchmark — 2026-07-27

## Висновок

Bounded background publication remediation пройшла 600-секундний camera/storage acceptance на Raspberry Pi Zero 2W з OV9281 у native `1280x800` Gray8 режимі.

Camera polling більше не виконує cumulative package publication синхронно. За fixed active+queued capacity `2` перевантаження було явним і bounded: `1514` submissions повернули backpressure, `8439` були прийняті та всі `8439` завершені. Worker опублікував `60/60` immutable verified revisions без processing/publication failure, abandoned або discarded jobs. Final drain зайняв `0.163331 ms`.

Це закриває Pi CPU/SD/RSS/thermal acceptance для isolated background publisher і дозволяє перейти до окремого production route-progress/local-pose composition design. Воно не є route reacquisition, FC/UART/MAVLink, ODOMETRY, reset, command-output або flight evidence.

## Конфігурація

```text
commit=e1d9ac2be418a098cd9cac0011f663b3e64ab944
hardware=Raspberry Pi Zero 2W + Arducam/OV9281 mono
requested_capture=1280x800@10
actual_sensor_mode=1280x800-Y10
duration_seconds=600
sparse_capture_interval_seconds=10
background_queue_capacity=2 (active + queued)
altitude_m=1.0 synthetic benchmark context
flight_authority=false
fc_uart=false
mavlink_output=false
all live/external-nav output CMake flags=OFF
Pi CTest=45/45
```

Tool створив synthetic one-entry source package, тримав `route_progress=0`, не надавав local pose і не створював gates. Тому artifacts нижче є camera/storage/load evidence, а не реальним маршрутом.

## Основні Metrics

```text
passed=true
frames_seen=9958
frames_observed=8439
effective_camera_fps=16.5967
effective_observed_fps=14.0646
invalid_observations=0
capture_requests=60
publications=60
processing_failures=0
publication_failures=0
gates_published=0

background_submissions=9953
background_accepted=8439
background_completed=8439
background_backpressure=1514
background_failed=false
background_abandoned=0
background_discarded=0
background_max_outstanding=2
background_queue_wait_ms_avg=11.1126
background_queue_wait_ms_max=3029.95
background_processing_ms_avg=26.6591
background_processing_ms_max=3095.98
background_drain_ms=0.163331

descriptor_latency_ms_avg=15.3027
descriptor_latency_ms_max=23.9607
selector_latency_ms_avg=0.00876036
selector_latency_ms_max=0.066094
publication_latency_ms_avg=1594.07
publication_latency_ms_max=3080.83
capture_elapsed_ms=600000
publisher_elapsed_ms=600018
```

`background_queue_wait_ms_max` близько трьох секунд означає один accepted queued frame позаду найдовшої active cumulative publication. Це не camera-loop stall: producer продовжував polling і повертав explicit backpressure для наступних frames, доки fixed capacity була зайнята.

Порівняно з synchronous run `2026-07-19`, effective camera rate зріс із `11.1314` до `16.5967 fps`, хоча cumulative publication cost залишився практично тим самим (`3072.75` проти `3080.83 ms` max). Отже покращення походить саме від ізоляції I/O/verification у bounded worker, а не від прихованого скорочення package verification.

## System Metrics

System sampler записав `574` samples:

```text
rss_kib_min=4404
rss_kib_max=25580
temperature_c_min=51.540
temperature_c_max=61.224
arm_clock_hz_min=700000000
arm_clock_hz_max=1000002000
get_throttled=0x0 у кожному sample
disk_available_max=55496851456
disk_available_min=55434784768
```

Final package:

```text
publications=60
package_files_recursive=124
package_bytes=61759959
package_du=60M
latest_manifest=route-verification-0060.vhrm
latest_manifest_sha256=4176c724e84c58d99faace6af7f975135cd82770e190fd60890583f978510fa3
output_bytes_published=61742700
package_files_checked_cumulative=1952
final_revision_files_checked=62
```

## Evidence На Pi

```text
launch_log=/home/pi/Visual_Homing_Codex/artifacts/logs/verification-capture-launch-20260726T214746Z.log
launch_log_sha256=d3be3bf2c6afbc818919f8693c95f2dbbd4bacff2effb0039c1dde1bee623aaa

run_log=/home/pi/Visual_Homing_Codex/artifacts/logs/verification-capture-benchmark-20260726T214746Z.log
run_log_sha256=73c259046cc2bef07876f8dc3cae5c517c1b5b95da6e905817ed4b143db877d8

system_log=/home/pi/Visual_Homing_Codex/artifacts/logs/verification-capture-system-20260726T214746Z.csv
system_log_sha256=56d0f731eb481ba21a2e6b92d215e4eaade96bcc38b07686e44f327cd7b223a5

build_log=/home/pi/Visual_Homing_Codex/artifacts/logs/verification-capture-build-20260726T214746Z.log
build_log_sha256=ce56d55629e0db2c14b8ba3e85fc4ee2350db8e99cd24a129a611603d1ab4172

package=/home/pi/Visual_Homing_Codex/artifacts/verification_benchmarks/verification-benchmark-20260726T214746Z
```

UTC artifact stamp is `20260726T214746Z`; у часовому поясі Europe/Kiev це вже `2026-07-27`.

## Acceptance Межа

Прийнято:

- fixed-capacity single-worker publication на Pi Zero 2W;
- explicit bounded backpressure без camera-loop publication stall;
- `accepted == completed`, zero terminal/processing/publication failure;
- zero abandoned/discarded jobs і negligible final drain;
- native `1280x800` OV9281 path, `60/60` verified revisions;
- bounded RSS/temperature і `get_throttled=0x0`;
- all-output-off Pi build та CTest `45/45`.

Не прийнято:

- production route-progress/local-pose attachment;
- restart/resume immutable revision recovery;
- physical SD sudden-power-loss durability;
- kilometer-scale package growth beyond 60 sparse revisions;
- VHIX search, high-resolution content verification або multi-frame route lock;
- global reacquisition, `reset_reference`, JT_Zero handoff;
- будь-яка FC/UART/MAVLink/ODOMETRY/command або flight readiness.
