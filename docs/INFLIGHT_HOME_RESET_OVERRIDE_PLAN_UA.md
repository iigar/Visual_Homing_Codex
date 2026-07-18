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

Він не має parameter-set, Home/origin-set, mode, arm, mission або actuator path. RC12 mapping не означає, що одна фізична дія може одночасно скидати estimator і змінювати FC Home.

## Реалізований RC12 Decoder І Dry-Run

`RcSwitchTriggerDecoder` є окремою library boundary без runtime/reset/Home caller. Default policy відповідає live PWM evidence:

- valid PWM `800..2200 us`;
- LOW `<=1200 us`, HIGH `>=1800 us`, середина є hysteresis band без зміни stable state;
- debounce `150 ms`;
- cooldown `3000 ms`;
- HIGH після startup не створює edge, доки не підтверджено LOW;
- одна дія на debounced LOW->HIGH; held HIGH не повторюється;
- bounce/hysteresis скасовують candidate transition;
- cooldown-blocked HIGH споживає цикл і вимагає нового LOW->HIGH;
- out-of-range PWM і timestamp rollback fail closed.

`rc12_local_reset_dry_run` читає line trace `time_boot_ms rc12_pwm` і може вивести тільки `would_request_local_estimator_reset`. Він завжди звітує `executor_attached=false` та `fc_home_change_attached=false`.

`scripts/run-rc12-local-reset-dry-run-pi.sh`:

- вимагає точний confirmation token `I_CONFIRM_RC12_DRY_RUN_NO_RESET_OR_HOME`;
- request-only перечитує fixed RC allowlist і fail closed, якщо `RC12_OPTION` відсутній або не дорівнює `0`;
- пише RC12 trace та dry-run log;
- не має reset/Home/origin/mode/arm/mission/actuator/provider path.

WSL/Ninja, MSVC/Ninja і ordinary Pi build проходять `35/35`; Pi log: `/home/pi/Visual_Homing_Codex/artifacts/logs/test-core-pi-20260718T164528Z.log`, усі live/external-nav output CMake flags `OFF`. Два перші live dry-run captures коректно fail closed із `0` events на steady `999 us`. Синхронізований третій capture `/home/pi/Visual_Homing_Codex/artifacts/logs/rc12-local-reset-dry-run-20260718T165944Z.log` пройшов: `52` samples, `0` rejected, observed `999 -> 2000 -> 999 us`, рівно один `would_request_local_estimator_reset`, held HIGH без повтору, обидва executors `false`. Post-capture request-only baseline залишив FC disarmed в AltHold і зібрав `1288/1288` без записів.

## Невирішені Межі

- Live RC12 mapping підтверджений лише для поточного transmitter/receiver/FC setup; його треба повторити після remap, firmware/parameter restore або зміни пульта/приймача.
- Live LOW->HIGH->LOW decoder event прийнятий лише як dry-run input evidence; reset executor, safety-gate runtime composition і ODOMETRY recovery acceptance ще відсутні.
- Decoder і gate не виконують reset або Home change; executor/runtime attachment ще відсутні.
- Dry-run trace не містить сам по собі свіжий armed/heartbeat snapshot і не є дозволом дії; майбутня runtime integration повинна передавати edge у `InflightHomeResetSafetyGate` разом із live telemetry, RC freshness, audit readiness і valid reset reference.
- In-flight local reset потребує окремої SITL discontinuity/recovery acceptance.
- In-flight FC Home change потребує окремої SITL mode/RTL-semantics acceptance і props-off real-FC review.
- Старий JT_Zero in-flight VO reset не є acceptance evidence для нового ODOMETRY reset-counter/Home contract.
