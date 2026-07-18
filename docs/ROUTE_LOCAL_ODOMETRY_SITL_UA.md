# Route-Local ODOMETRY SITL Acceptance

Цей документ описує isolated acceptance для `RouteLocalOdometryEstimator` і точного MAVLink2 ODOMETRY encoder проти ArduCopter `4.3.6`. Він не підключає Visual Homing до реального FC, Pi UART, runtime writer або flight-control command path.

## Межа Перевірки

Harness використовує:

- exact ArduPilot tag `Copter-4.3.6`, commit `0c5e999c44785b0b8f53e7758856ceea614ef01b`, той самий hash, що зафіксований на Matek;
- окремий detached ArduPilot worktree, не active `master` checkout;
- `GPS_TYPE=0` і EKF3 ExternalNav sources з `config/sitl/arducopter-4.3.6-route-local-odometry.parm`;
- локальний TCP `127.0.0.1:5760`, без MAVProxy і без serial device;
- C++ `route_local_odometry_sitl_producer`, який викликає реальні `RouteLocalOdometryEstimator` та `encode_mavlink2_route_local_odometry` і видає raw MAVLink2 frames;
- pinned 4.3.6 `pymavlink` тільки для SITL orchestration, telemetry decode, origin/Home requests, explicit `MAV_CMD_DO_SET_HOME` і mode request.

Harness fail closed, якщо firmware hash, параметри, frame IDs, estimator gates або expected reset counters не збігаються.

## Підготовка Exact SITL

Приклад у WSL2 Ubuntu:

```bash
git -C /home/vlasenco/ardupilot fetch --depth 1 origin tag Copter-4.3.6
git -C /home/vlasenco/ardupilot worktree add --detach /home/vlasenco/ardupilot-copter-4.3.6-sitl Copter-4.3.6
git -C /home/vlasenco/ardupilot-copter-4.3.6-sitl submodule update --init --recursive
python3 /mnt/d/LLM/ChatGPT/Codex/Visual-Homing/Visual_Homing_Codex/scripts/build-arducopter-4.3.6-sitl.py \
  --ardupilot-root /home/vlasenco/ardupilot-copter-4.3.6-sitl
```

Ubuntu 24.04 має Python 3.12 і GCC 13, а historical 4.3.6 Waf очікує видалений `imp.new_module`, mode `rU` та непряме включення `<cstdint>`. Build helper додає локальний compatibility shim і compiler `-include cstdint`, не змінюючи жодного tracked ArduPilot source file. Перед acceptance ArduPilot worktree перевіряється як exact hash і без tracked modifications.

## Збірка Visual Homing Producer

```bash
cmake -S /mnt/d/LLM/ChatGPT/Codex/Visual-Homing/Visual_Homing_Codex/core \
  -B /mnt/d/LLM/ChatGPT/Codex/Visual-Homing/Visual_Homing_Codex/core/build-wsl-sitl \
  -G Ninja -DBUILD_TESTING=ON \
  -DVISUAL_HOMING_ENABLE_LIBCAMERA=OFF \
  -DVISUAL_HOMING_ENABLE_LIVE_MAVLINK_OUTPUT=OFF \
  -DVISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_LIVE_OUTPUT=OFF \
  -DVISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_SERIAL_WRITER=OFF \
  -DVISUAL_HOMING_ENABLE_EXTERNAL_NAV_OUTPUT=OFF \
  -DVISUAL_HOMING_ENABLE_BENCH_PROPS_OFF_EXTERNAL_NAV_OUTPUT=OFF \
  -DVISUAL_HOMING_ATTACH_BENCH_PROPS_OFF_EXTERNAL_NAV_WRITER=OFF
cmake --build /mnt/d/LLM/ChatGPT/Codex/Visual-Homing/Visual_Homing_Codex/core/build-wsl-sitl -j 4
ctest --test-dir /mnt/d/LLM/ChatGPT/Codex/Visual-Homing/Visual_Homing_Codex/core/build-wsl-sitl --output-on-failure
```

## Запуск Acceptance

```bash
python3 /mnt/d/LLM/ChatGPT/Codex/Visual-Homing/Visual_Homing_Codex/scripts/run-route-local-odometry-sitl.py \
  --ardupilot-root /home/vlasenco/ardupilot-copter-4.3.6-sitl
```

Кожен запуск створює ignored local artifacts у `artifacts/sitl/route-local-odometry-<UTC>/`: `acceptance.json`, `arducopter-sitl.log` та ephemeral SITL state. Evidence явно містить `hardware_accessed=false` і `serial_opened=false`.

## Прийнятий Результат 2026-07-18

Два початкові повні проходи, origin-aware прохід і два post-acquisition Home проходи завершились `passed=true`. Поточний фінальний evidence: `artifacts/sitl/route-local-odometry-20260718T103804Z/acceptance.json`.

- firmware: `flight_custom_version=0c5e999c`;
- exact ExternalNav parameters прочитані назад із SITL;
- global origin підтверджений як `-35.363261,149.165230,584000 mm`;
- до запуску ODOMETRY з `GPS_TYPE=0` `HOME_POSITION` не повідомлявся, що підтвердило різницю між установленим EKF origin і ще не встановленим Home;
- EKF acquisition: `EKF_STATUS_REPORT.flags=831`, ArduPilot STATUSTEXT для обох IMU: `is using external nav data` та initial NED `0,0,0`;
- після ExternalNav acquisition ArduPilot автоматично повідомив `HOME_POSITION`, рівний origin; окремий `MAV_CMD_DO_SET_HOME` через integer-coordinate `COMMAND_INT` був прийнятий з `MAV_RESULT_ACCEPTED(0)` і підтверджений повторним `HOME_POSITION`;
- disarmed `GUIDED` прийнято (`custom_mode=4`);
- reset frame з `reset_counter=1` зберіг валідну позицію;
- три fail-closed health rejects дали invalid streak `1/2/3`, після третього `reset_required=1`, і жоден invalid estimate не був відправлений;
- після припинення provider frames EKF flags впали `831 -> 39`, тобто horizontal position validity зникла;
- явно встановлений Home залишився незмінним під час provider timeout;
- після explicit reset `reset_counter=2` provider відновив position validity до flags `831` за `2` frames, а Home знову був прочитаний без зміни.

MSVC/Ninja і WSL/Ninja core suites пройшли `31/31`, включно з producer self-test. Повторні acceptance runs відрізнялись лише кількістю initial frames до acquisition залежно від EKF startup timing.

## Що Це Доводить І Не Доводить

Доведено в exact-version SITL:

- exact `LOCAL_FRD/BODY_FRD` ODOMETRY wire contract приймається ArduCopter 4.3.6;
- route-local position/yaw без заявленої velocity може активувати EKF3 ExternalNav position;
- після acquisition ArduCopter створює Home з поточного ExternalNav location; explicit `MAV_CMD_DO_SET_HOME` може зафіксувати той самий configured origin без GPS, а Home переживає втрату й відновлення provider position;
- при explicit global origin і валідному ExternalNav disarmed GUIDED стає доступним;
- estimator invalid streak не пропускає frames, provider timeout забирає position validity, explicit reset дозволяє recovery.

Не доведено:

- приймання на реальному Matek/FC або через Pi UART;
- armed GUIDED, motor output, takeoff, flight, RTL або HOVER;
- реальний FC origin/Home acceptance і RTL semantics; SITL підтвердив Home state, але RTL mode/trajectory не запускався;
- фізично правильний reverse yaw residual sign для встановленої камери;
- runtime writer/session attachment, rate under real Pi load або simultaneous command-output authority.

Наступний крок має залишатися окремим reviewed props-off FC acceptance. До нього route-local estimator/encoder не підключати до чинного runtime writer або UART.
