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

Live request-only audit `2026-07-18` закрив mapping для поточного FC/transmitter setup:

- RC7: `999 us`, `RC7_OPTION=4` (`RTL`) — зайнятий, не використовувати для Visual Homing trigger;
- RC8: `1503 us`, `RC8_OPTION=65` (`GPS Disable`) — зайнятий, не використовувати для Visual Homing trigger;
- RC12: операторський перемикач підтверджено як `999..2000 us`, `RC12_OPTION=0`, після тесту повернуто до `999 us` — єдиний схвалений кандидат із перевірених каналів.

Evidence: `/home/pi/Visual_Homing_Codex/artifacts/fc_baseline/fc-rc-baseline-20260718T160510Z.json`, `41` RC samples, RC7/RC8 незмінні. `scripts/capture-fc-rc-baseline-pi.py` робить лише:

- `PARAM_REQUEST_READ` для fixed RC7/8/12 option/calibration allowlist;
- `MAV_CMD_REQUEST_MESSAGE(RC_CHANNELS)`;
- запис локального JSON evidence з PWM min/max/last та ознакою зміни.

Він не має parameter-set, Home/origin-set, mode, arm, mission або actuator path. Runtime integration має використовувати edge-trigger із hysteresis для RC12, а не level-trigger; конкретні пороги, debounce, cooldown і action selection ще мають бути реалізовані та протестовані. RC12 mapping не означає, що одна фізична дія може одночасно скидати estimator і змінювати FC Home.

## Невирішені Межі

- Live RC12 mapping підтверджений лише для поточного transmitter/receiver/FC setup; його треба повторити після remap, firmware/parameter restore або зміни пульта/приймача.
- Gate не виконує reset або Home change; executor/runtime attachment ще відсутні.
- In-flight local reset потребує окремої SITL discontinuity/recovery acceptance.
- In-flight FC Home change потребує окремої SITL mode/RTL-semantics acceptance і props-off real-FC review.
- Старий JT_Zero in-flight VO reset не є acceptance evidence для нового ODOMETRY reset-counter/Home contract.
