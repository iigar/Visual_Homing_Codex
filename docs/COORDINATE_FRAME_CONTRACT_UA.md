# Coordinate Frame Contract

Цей документ визначає системи координат для Visual Homing до підключення route-progress estimates до ArduPilot external navigation. Він не авторизує provider send або flight.

## Канонічні Системи

- ArduPilot local pose: `LOCAL_NED` — X North, Y East, Z Down.
- ArduPilot body/twist: `FRD` — X Forward, Y Right, Z Down.
- ROS local pose: `ENU` — X East, Y North, Z Up.
- ROS body: `FLU` — X Forward, Y Left, Z Up.
- Visual route frame: `ROUTE_FRD` — X уздовж записаного маршруту вперед, Y праворуч від route axis, Z вниз. Це не North/East без явного alignment.

## Векторні Перетворення

```text
ENU -> NED: (N, E, D) = (y_enu, x_enu, -z_enu)
FLU -> FRD: (F, R, D) = (x_flu, -y_flu, -z_flu)
yaw_ned = wrap(pi/2 - yaw_enu)
```

Для route alignment із heading `psi` від North за годинниковою стрілкою:

```text
North = origin_north + cos(psi) * route_forward - sin(psi) * route_right
East  = origin_east  + sin(psi) * route_forward + cos(psi) * route_right
Down  = origin_down  + route_down
```

Quaternion conversion не виконується перестановкою компонентів. Для body-to-local rotation:

```text
R_ned_frd = C_ned_enu * R_enu_flu * C_flu_frd
```

## Реалізований Library Boundary

`core/include/visual_homing/coordinate_frames.hpp` і `core/src/coordinate_frames.cpp` надають:

- типізовані `LocalCoordinateFrame` та `BodyCoordinateFrame`;
- `ENU <-> NED`;
- `FLU <-> FRD`;
- ENU/NED yaw conversion із wrapping;
- `ROUTE_FRD -> LOCAL_NED` через explicit origin/heading;
- quaternion basis conversion `ENU/FLU <-> NED/FRD`;
- fail-closed rejection non-finite vectors, angles і invalid quaternions.

`core/tests/coordinate_frames_test.cpp` перевіряє одиничні осі, round trips, yaw `0/90` degrees, route heading `0/90` degrees, quaternion identity/north-facing cases та invalid inputs.

## Runtime-Контракт External Navigation

`ExternalNavEstimate` тепер несе explicit `pose_frame`, route origin/heading, `frame_alignment_known` та `altitude_origin_aligned`. Estimator спочатку утворює діагностичну позицію в `ROUTE_FRD`, а в `LOCAL_NED` переводить її лише за відомого alignment.

`valid_for_fc=true` можливе тільки якщо одночасно:

- route match, telemetry, altitude і scale пройшли наявні gates;
- `route_frame_alignment_known=true`;
- pose вже має frame `LOCAL_NED`;
- `altitude_origin_aligned=true`;
- `yaw_source_independent=true`: yaw походить з незалежного ExternalNav observation, а не з `ATTITUDE.yaw` того самого FC.

Інакше причина є `frame_alignment_not_known`, `altitude_origin_not_aligned` або `yaw_source_not_independent`. `LiveExternalNavOutputSession`, `LiveMavlinkExternalNavWriter` і прямий `VISION_POSITION_ESTIMATE` encoder повторно перевіряють цей контракт та fail closed. `ROUTE_FRD` або `LOCAL_ENU` не можуть бути неявно закодовані як NED.

Pi/CLI параметри:

```text
VISUAL_HOMING_EXTERNAL_NAV_ROUTE_FRAME_ALIGNMENT_KNOWN
VISUAL_HOMING_EXTERNAL_NAV_ROUTE_ORIGIN_NORTH_M
VISUAL_HOMING_EXTERNAL_NAV_ROUTE_ORIGIN_EAST_M
VISUAL_HOMING_EXTERNAL_NAV_ROUTE_ORIGIN_DOWN_M
VISUAL_HOMING_EXTERNAL_NAV_ROUTE_HEADING_NED_RAD
VISUAL_HOMING_EXTERNAL_NAV_ALTITUDE_ORIGIN_ALIGNED
```

Heading задається від North за годинниковою стрілкою в радіанах. `origin_down` є NED Down у метрах. Для runtime provider-send обидва boolean-підтвердження обов'язкові; defaults дорівнюють `0`, а всі чотири числові origin/heading env мають бути задані явно, навіть коли перевірене значення дорівнює нулю. Не ставити confirmations у `1` лише для проходження readiness: origin/heading і відповідність vertical origin мають походити з перевіреної процедури.

Поточний estimator зберігає FC `ATTITUDE.yaw` лише як `telemetry_yaw_rad` для діагностики та не використовує його як yaw authority. Для операторськи підтвердженого прямого forward route незалежний yaw утворюється як:

```text
yaw_ned = wrap(route_heading_ned_rad + direction_error_rad)
```

`direction_error_rad` походить із bounded horizontal pixel-shift поточного кадру відносно matched route frame. Observation вважається незалежним лише коли route match valid, `max_direction_shift_px > 0`, `radians_per_pixel > 0`, значення finite і найкращий shift не лежить на межі search window. Межа означає можливе saturation, тому `direction_observation_valid=false`, `yaw_source_independent=false`, estimate отримує `yaw_source_not_independent` і fail closed. Stored `VHRS` heading hints не використовуються: вони були записані з FC telemetry і містять підтверджений startup transient. Операторського env override для незалежності yaw немає.

Цей yaw contract поки обмежений forward-проходом прямого маршруту. Reverse heading/camera orientation потребують окремо визначеної та перевіреної семантики; сама наявність image residual її не доводить.

Так само одна пара `origin + heading` коректно проєктує лише прямолінійну route axis. Поточний `VHRS v1` не зберігає метричні `x/y` координати траєкторії, а estimator зводить progress до `x=progress*nominal_route_length`, `y=0`. Маршрут із реальними поворотами не можна зробити метрично коректним одним alignment; до provider acceptance потрібен або окремо перевірений прямий маршрут, або route geometry з незалежною позою/yaw. Для `field-route-20260712T164651Z.vhrs` прямолінійність підтверджена оператором. Його geographic bearing від North не виміряний і не потрібний для route-local `LOCAL_FRD` ODOMETRY; він знадобиться лише якщо цей route pose навмисно переводитиметься в старий `LOCAL_NED` contract.

## Route-Local ODOMETRY Library Boundary

Окремий library-only encoder `encode_mavlink2_route_local_odometry` тепер реалізує точний wire contract встановленого ArduCopter `4.3.6` (`0c5e999c`):

- MAVLink message `ODOMETRY` (`id=331`, `crc_extra=91`);
- pose `frame_id=MAV_FRAME_LOCAL_FRD(20)` — нерухомий origin у точці початку записаного route, осі Forward/Right/Down;
- twist `child_frame_id=MAV_FRAME_BODY_FRD(12)`;
- yaw-only quaternion `[cos(yaw/2), 0, 0, sin(yaw/2)]`;
- `vx/vy/vz` і angular rates є `NaN`, а не нулями;
- обидві covariance arrays unknown (`NaN`);
- explicit `reset_counter` і `MAV_ESTIMATOR_TYPE_VISION(2)`.

Точний `GCS_MAVLink::handle_odometry` у 4.3.6 відкидає інші frame pairs. Він завжди передає twist у velocity path, але точний `AP_NavEKF3::writeExtNavVelData` відкидає вектор із `NaN`. Це навмисно захищає position/yaw-only provider від помилкового твердження `velocity=0`, особливо за підтвердженого `EK3_SRC1_VELXY=6`.

У pinned MAVLink schema цього firmware payload має `232` bytes і закінчується `estimator_type`; сучаснішого поля `quality` там немає. Independent generation точним pinned `pymavlink` підтвердив test vector: MAVLink2 frame `244` bytes, payload `232`, message id bytes `4b 01 00`, checksum `c6 19`.

Цей encoder ще не підключений до `LiveExternalNavOutputSession`, writer, CLI, Pi wrappers, UART або FC. Чинний runtime як і раніше використовує `VISION_POSITION_ESTIMATE` та його старий explicit `LOCAL_NED` fail-closed contract.

Для операторськи підтвердженого прямого маршруту номінальна довжина для наступного estimator pass становить приблизно `10 m`, тому `route_x=progress*10 m`, `route_y=0`. Висота приблизно `0.5 m` є AGL/telemetry context. `route_z` означає Down displacement від висоти route-start: якщо запис і повернення виконуються на тій самій висоті, `route_z≈0`, а не `-0.5 m`.

Географічний bearing від North та WGS84 route coordinates для цього route-local ODOMETRY contract не потрібні. Окремі вимоги самого ArduPilot до EKF origin/home або mode readiness не можна підміняти route coordinates; їх перевіряють окремо під час SITL/FC acceptance.

Окремий library-only `RouteLocalOdometryEstimator` тепер формалізує pose до encoder:

- route-start altitude задається явно свіжим observation і не вибирається неявно з першого match;
- `x=progress*nominal_route_length`, `y=0`, `z=start_altitude-current_altitude`;
- forward yaw дорівнює незалежному bounded image-direction residual;
- reverse yaw за замовчуванням unavailable; policy `nose_toward_route_start` треба ввімкнути явно, після чого yaw є `pi + signed_residual`;
- зміна forward/reverse вимагає explicit tracking reset;
- match confidence/freshness, altitude freshness, health, direction residual, update interval, directional progress, horizontal/vertical/yaw rates та invalid streak fail closed;
- `reset_tracking()` збільшує `reset_counter`, але зберігає vertical origin; `clear_start_altitude()` також збільшує counter і блокує output до нової ініціалізації.

Estimator та encoder інтегровані лише в deterministic unit tests; MSVC 19.44 + Ninja пройшов `30/30` CTest. Вони не підключені до `LiveExternalNavOutputSession`, writer, CLI, Pi wrappers, UART або FC. Reverse camera orientation, точний residual sign для реального монтажу, а також ArduPilot origin/home/mode/reset/timestamp acceptance залишаються предметом наступної SITL перевірки.

До окремого SITL/props-off review не підключати новий ODOMETRY encoder до UART і не повторювати blind provider-send лише для збільшення лічильника sent messages.

## Офіційні Джерела

- MAVLink `VISION_POSITION_ESTIMATE` визначає локальні `x/y/z` і attitude: <https://mavlink.io/en/messages/common.html#VISION_POSITION_ESTIMATE>.
- ArduPilot пояснює, що non-GPS setup може вимагати ручного `SET_GPS_GLOBAL_ORIGIN`, після чого EKF origin не можна пересунути: <https://ardupilot.org/dev/docs/mavlink-get-set-home-and-origin.html>.
- ArduCopter 4.3 parameter contract визначає `EK3_SRC1_YAW=6` як `ExternalNav`, `POSZ=2` як `RangeFinder` і `VELXY=6` як `ExternalNav`: <https://ardupilot.org/copter/docs/parameters-Copter-stable-V4.3.0.html>.
