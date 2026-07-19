# Pi Native Verification Capture Benchmark — 2026-07-19

## Висновок

Pi Zero 2W з OV9281 стабільно обробляє native `1280x800` Gray8 camera path і sparse publication interval `10 s` протягом `600 s` без thermal throttling, OOM або publication failure. Це підтверджує придатність повної sensor resolution для sparse keyframes за такої cadence.

Поточний synchronous writer path водночас не можна підключати до production tracking loop: cumulative full-package verification збільшила publication latency від `110.024 ms` для першої revision до `3072.75 ms` для 60-ї. Під час цього виклик `observe()` блокується, camera queue відкидає проміжні кадри, а effective observed rate знижується. Наступний runtime slice має винести publication у bounded background worker із явною backpressure/failure state, не послаблюючи правило commit-after-verified-publication.

## Конфігурація

```text
commit=4c4458ed0662b35562a40195cd5ca903808b0bdc
hardware=Raspberry Pi Zero 2W + Arducam/OV9281 mono
requested_capture=1280x800@10
duration_seconds=600
sparse_capture_interval_seconds=10
warmup_frames=5
flight_authority=false
fc_uart=false
mavlink_output=false
all live/external-nav output CMake flags=OFF
```

`frame_rate_hz=10` залишається requested/timeout параметром поточного `PiCameraSource`, а не libcamera frame-duration control. Тому нижче використовується виміряний effective rate.

## Артефакти На Pi

```text
run_log=/home/pi/Visual_Homing_Codex/artifacts/logs/verification-capture-benchmark-20260719T200745Z.log
system_log=/home/pi/Visual_Homing_Codex/artifacts/logs/verification-capture-system-20260719T200745Z.csv
build_log=/home/pi/Visual_Homing_Codex/artifacts/logs/verification-capture-build-20260719T200745Z.log
package=/home/pi/Visual_Homing_Codex/artifacts/verification_benchmarks/verification-benchmark-20260719T200745Z
latest_manifest=.../route-verification-0060.vhrm
```

Build log підтверджує Pi CTest `44/44` перед camera run.

## Camera І Publication Metrics

```text
frames_seen=6684
frames_observed=6679
effective_observed_fps=11.1314
invalid_observations=0
capture_requests=60
publications=60
processing_failures=0
publication_failures=0
gates_published=0
descriptor_latency_ms_avg=16.0278
descriptor_latency_ms_max=24.9462
selector_latency_ms_avg=0.00871596
selector_latency_ms_max=0.056458
publication_latency_ms_avg=1598
publication_latency_ms_first=110.024
publication_latency_ms_last=3072.75
publication_latency_ms_max=3072.75
package_files_checked_cumulative=1952
final_revision_files_checked=62
output_bytes_published=61742700
package_files=124
package_du=60M
```

Frame sequence gaps є очікуваним наслідком bounded two-frame libcamera queue під час synchronous publication stalls, а не порушенням identity: кожна опублікована revision пройшла package verification до selector commit.

## System Metrics

System sampler записав `573` приблизно односекундні samples:

```text
rss_kib_min=2856
rss_kib_max=25668
temperature_c_min=63.376
temperature_c_max=66.066
arm_clock_hz_min=699998000
arm_clock_hz_max=1000002000
get_throttled=0x0 у кожному sample
disk_available_before=55560364032
disk_available_after=55498137600
disk_delta_bytes=62226432
```

Зниження ARM clock до приблизно `700 MHz` при незмінному `get_throttled=0x0` є normal DVFS evidence, не thermal/undervoltage throttle flag.

## Acceptance Межа

Прийнято:

- native `1280x800` Gray8 capture/resize/descriptor path на Pi Zero 2W;
- sparse `10 s` verification cadence протягом 10 хвилин;
- bounded RSS і temperature без throttle flags;
- 60 послідовних immutable verified revisions без selector/publication failure;
- wrapper/system metrics contract із повністю заблокованими MAVLink/output paths.

Не прийнято:

- synchronous publication у production tracking/matching thread;
- extrapolation 60-keyframe latency до kilometer-scale packages;
- physical SD power-loss durability;
- restart/resume, real route geometry/gates, global search, multi-frame lock або `reset_reference`;
- будь-яка flight/FC/JT_Zero readiness.
