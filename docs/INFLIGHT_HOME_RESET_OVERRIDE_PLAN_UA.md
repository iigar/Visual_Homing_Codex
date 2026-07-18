# In-Flight Home/Reset Override Plan

Цей документ визначає fail-closed контракт для аварійного RC-triggered reset у нестандартних умовах. Він не дозволяє flight use, не підключає RC input до runtime і не додає MAVLink Home writer.

## Дві Незалежні Дії

`LocalEstimatorReset`:

- скидає тільки route-local estimator/tracking state;
- наступна ODOMETRY оцінка повинна мати збільшений `reset_counter`;
- не змінює EKF global origin або ArduPilot `HOME_POSITION`.

`FlightControllerHomeChange`:

- не скидає estimator автоматично;
- надсилає окремий explicit `MAV_CMD_DO_SET_HOME` тільки через майбутній reviewed executor;
- вимагає ACK `MAV_RESULT_ACCEPTED` і readback `HOME_POSITION`;
- змінює RTL target і тому має сильніший окремий дозвіл.

Один дозвіл ніколи не розблоковує іншу дію.

## Реалізований Library-Only Gate

`InflightHomeResetSafetyGate` зараз існує лише як library/test boundary. Він не має runtime, writer, UART або RC callers.

Default-OFF поля:

- `override_available=false` — master availability, яку майбутній compile/runtime integration не може вважати true за замовчуванням;
- `allow_inflight_local_reset=false`;
- `allow_inflight_fc_home_change=false`;
- `local_reset_operator_confirmed=false`;
- `fc_home_change_operator_confirmed=false`;
- `audit_log_ready=false`.

Для будь-якої дії gate вимагає:

- edge-trigger, а не постійно утримуваний high PWM;
- свіжий heartbeat/armed state;
- свіжий RC sample;
- готовий audit log.

Armed local reset додатково вимагає master availability, свій allow flag, своє operator confirmation і валідний reset reference. Armed FC Home change окремо вимагає свій allow flag/confirmation, валідну ExternalNav position і explicit valid Home target.

Disarmed preflight action не потребує in-flight allow flags, але спільні freshness/audit/reference gates залишаються. Це дозволяє штатно зафіксувати точку перед зльотом, не вмикаючи emergency override.

## Майбутня Runtime Мапа

Під час reviewed integration library fields мають отримати окремі зовнішні controls, орієнтовно:

```text
VISUAL_HOMING_ENABLE_INFLIGHT_HOME_RESET_OVERRIDE=OFF
VISUAL_HOMING_ALLOW_INFLIGHT_LOCAL_RESET=0
VISUAL_HOMING_ALLOW_INFLIGHT_FC_HOME_CHANGE=0
```

Master compile capability не замінює два runtime allow flags. Кожен armed run також потребуватиме власного точного confirmation token, one-shot latch/cooldown і audit-before-action. Назви env/CMake controls не вважаються реалізованими, доки integration не додана й не протестована.

## RC Channel Audit

Поточний RC7/8/12 mapping невідомий. `scripts/capture-fc-rc-baseline-pi.py` підготовлений для reconnect і робить лише:

- `PARAM_REQUEST_READ` для fixed RC7/8/12 option/calibration allowlist;
- `MAV_CMD_REQUEST_MESSAGE(RC_CHANNELS)`;
- запис локального JSON evidence з PWM min/max/last та ознакою зміни.

Він не має parameter-set, Home/origin-set, mode, arm, mission або actuator path. Під час контрольованого capture оператор має перемістити лише підозрюваний перемикач; канал можна вибрати лише після observed PWM change і перевірки `RC*_OPTION`.

## Невирішені Межі

- Pi `jtzero` був недоступний через hostname resolution під час підготовки цього контракту, тому live RC mapping не заявляється.
- Gate не виконує reset або Home change; executor/runtime attachment ще відсутні.
- In-flight local reset потребує окремої SITL discontinuity/recovery acceptance.
- In-flight FC Home change потребує окремої SITL mode/RTL-semantics acceptance і props-off real-FC review.
- Старий JT_Zero in-flight VO reset не є acceptance evidence для нового ODOMETRY reset-counter/Home contract.
