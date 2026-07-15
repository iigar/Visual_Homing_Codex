# Hardware And Access Baseline

Цей документ є коротким source-of-truth для апаратної конфігурації, SSH-доступу та ArduPilot/MAVLink інтеграції Visual Homing. Його треба читати перед будь-якою роботою з Raspberry Pi, Matek, serial telemetry, FC/JT_Zero acceptance або зовнішньою навігацією.

Останнє оновлення: `2026-07-16`.

## Статуси

- `CONFIRMED` — підтверджено кодом, tracked evidence, офіційною схемою або успішною перевіркою на обладнанні.
- `OPERATOR_REPORTED` — відтворено оператором із пам'яті; не слід перетворювати на safety-critical припущення без перевірки.
- `REQUIRES_VERIFICATION` — значення відсутнє, могло змінитися або має бути прочитане з поточного обладнання.
- `CURRENTLY_OFFLINE` — конфігурація відома, але live-перевірка неможлива, доки Pi/FC не підключені.
- `NOT_TRACKED` — файл існує у поточному checkout, але не збережений у Git; не припускати його наявність після нового clone.

## Взаємодія

| Поле | Значення | Статус | Джерело/примітка |
|---|---|---|---|
| Мова | Українська | `CONFIRMED` | Поточна домовленість |
| Звертання | На `ти`, без формального `Ви` | `CONFIRMED` | Поточна домовленість |
| Роль оператора | Людина підтверджує фізичні safety-умови; Codex не припускає їх автоматично | `CONFIRMED` | Проєктна safety-межа |

Неточний запит оператора не є дозволом домислювати hardware state, wiring, firmware, параметри або armed/props state. Якщо факт можна прочитати з локальної пам'яті чи read-only telemetry, спочатку треба зробити це.

## Апаратна Конфігурація

| Поле | Значення | Статус | Джерело/примітка |
|---|---|---|---|
| Companion computer | Raspberry Pi Zero 2W | `CONFIRMED` | `PROJECT_MEMORY.md`, Pi field evidence |
| Flight controller | Matek H743 Slim V3, ArduPilot/ArduCopter | `CONFIRMED` | `PROJECT_MEMORY.md`, `SESSION_LOG.md` |
| Камера | Arducam/OV9281 wide mono | `CONFIRMED` | Field evidence 2026-07-08..2026-07-13 |
| FC physical UART pins in use | Matek `TX3/RX3` | `OPERATOR_REPORTED` | Повторно фізично перевірити перед зміною wiring |
| Matek UART mapping | Physical UART3/`USART3` maps to ArduPilot `SERIAL4` on the MatekH743 target | `CONFIRMED` | ArduPilot `MatekH743/hwdef.dat` `SERIAL_ORDER` |
| Wiring direction | Pi TX -> Matek RX3; Pi RX <- Matek TX3; common GND | `REQUIRES_VERIFICATION` | Не змінювати wiring за цим записом без фізичної перевірки |
| Pi serial device | `/dev/serial0 -> /dev/ttyS0` | `CONFIRMED` | Readiness evidence 2026-07-16 |
| Pi serial ownership | `/dev/ttyS0` is `root:dialout 0660`; user `pi` is in `dialout`; udev rule covers `ttyS0` and `ttyAMA0` | `CONFIRMED` | Readiness evidence 2026-07-16 |
| Pi serial console/getty | `console=serial0,115200` removed from boot cmdline; `serial-getty@ttyS0.service` masked/inactive | `CONFIRMED` | Reboot verification 2026-07-16; boot backup `/boot/firmware/cmdline.txt.visual-homing-backup-be133c5` |
| Serial baud | `115200` | `CONFIRMED` | Accepted telemetry/readiness runs |
| MAVLink transport | MAVLink2 | `CONFIRMED` | Accepted telemetry captures and field evidence |
| ArduPilot logical port | `SERIAL4` | `CONFIRMED` | `SESSION_LOG.md` plus official mapping |

Official mapping source:

```text
https://raw.githubusercontent.com/ArduPilot/ardupilot/master/libraries/AP_HAL_ChibiOS/hwdef/MatekH743/hwdef.dat
```

The relevant `SERIAL_ORDER` places `USART3` at the ArduPilot `SERIAL4` position. Physical `TX3/RX3` labels and `SERIAL4_*` parameters use different namespaces and are not contradictory.

## SSH Access

| Поле | Значення | Статус | Джерело/примітка |
|---|---|---|---|
| Pi SSH hostname | `jtzero` | `CONFIRMED` | Live connection verified 2026-07-16 |
| Pi SSH user | `pi` | `CONFIRMED` | Live connection and readiness evidence 2026-07-16 |
| Dedicated key name | `id_ed25519_jtzero` | `CONFIRMED` | Batch-mode key access verified 2026-07-16 |
| Repo-local host-key file | `codex_jtzero_known_hosts` | `CONFIRMED`, `NOT_TRACKED` | Exists in the current repo root and contains a public host fingerprint only; preserve/recreate explicitly after a fresh clone |
| Last recorded SSH key confirmation | `2026-07-16` | `CONFIRMED` | Current session |
| Current live SSH availability | Connected and reachable as `pi@jtzero` | `CONFIRMED` | Current session; reverify after power/network changes |

PowerShell command template from the repository root:

```powershell
$key = Join-Path $HOME '.ssh\id_ed25519_jtzero'
ssh -i $key -o BatchMode=yes -o StrictHostKeyChecking=yes -o UserKnownHostsFile="$PWD\codex_jtzero_known_hosts" pi@jtzero
```

If `jtzero` does not resolve while Pi is disconnected, that is not evidence that the key or SSH configuration was lost. After Pi is connected, first verify hostname/IP reachability; do not regenerate keys or replace host fingerprints automatically.

Never commit or paste private-key contents, passwords, recovery codes, or unrestricted credentials into project memory. The key filename and public host fingerprint are sufficient for the baseline.

## ArduPilot And Parameter Snapshot

| Поле | Поточне значення | Статус | Як закрити |
|---|---|---|---|
| Vehicle firmware family | ArduPilot/ArduCopter | `CONFIRMED` | Heartbeat decoding and prior mode evidence |
| Exact ArduCopter version | `4.3.6 official` | `CONFIRMED` | `AUTOPILOT_VERSION`, 2026-07-16 snapshot |
| Firmware git hash | `0c5e999c` | `CONFIRMED` | `flight_custom_version`, 2026-07-16 snapshot |
| FC board/firmware target string | MatekH743 family; `board_version=66387968` (`0x03F50000`), USB vendor/product `4617/22336`; exact target string still not emitted | `CONFIRMED` family/IDs, `REQUIRES_VERIFICATION` exact string | `AUTOPILOT_VERSION`; physical Matek H743 Slim V3 identification |
| `SERIAL4_PROTOCOL` | `2` (MAVLink2) | `CONFIRMED` | Full parameter snapshot 2026-07-16 |
| `SERIAL4_BAUD` | `115` (115200 baud) | `CONFIRMED` | Full parameter snapshot 2026-07-16 |
| Other `SERIAL4_*` | `SERIAL4_OPTIONS=0` | `CONFIRMED` | Full parameter snapshot 2026-07-16 |
| `SR4_*` stream-rate parameters | `ADSB=0`, `EXTRA1=5`, `EXTRA2=0`, `EXTRA3=0`, `EXT_STAT=2`, `PARAMS=0`, `POSITION=1`, `RAW_CTRL=0`, `RAW_SENS=2`, `RC_CHAN=0` | `CONFIRMED` | Full parameter snapshot 2026-07-16 |
| `EK3_SRC*` external-nav source parameters | source 1: `POSXY=6`, `POSZ=2`, `VELXY=6`, `VELZ=0`, `YAW=6`; source 2: `0/1/0/0/0`; source 3: all `0`; `EK3_SRC_OPTIONS=2` | `CONFIRMED` | Full parameter snapshot 2026-07-16 |
| ExternalNav-related parameters | `EK3_ENABLE=1`, `AHRS_EKF_TYPE=3`, `VISO_TYPE=1`, `VISO_DELAY_MS=10`, `VISO_ORIENT=0`, offsets `0/0/0`, `VISO_SCALE=1`, position/velocity/yaw noise `0.2/0.1/0.2`; `FLOW_TYPE=7`, `RNGFND1_TYPE=32`, `GPS_TYPE=9` | `CONFIRMED` | Full parameter snapshot 2026-07-16 |

Full request-only artifacts:

```text
JSON=/home/pi/Visual_Homing_Codex/artifacts/fc_baseline/fc-baseline-20260715T214550Z.json
PARAM=/home/pi/Visual_Homing_Codex/artifacts/fc_baseline/fc-parameters-20260715T214550Z.param
parameters=1288/1288
operation=request_only_no_parameter_writes
```

The capture used `scripts/capture-fc-baseline-pi.py` with `pymavlink 2.4.49` and `pyserial 3.5` inside `/home/pi/.venvs/visual-homing-diagnostics`. Its only outbound operations are `MAV_CMD_REQUEST_MESSAGE(AUTOPILOT_VERSION)` and `PARAM_REQUEST_LIST`; it exposes no parameter-set, arm, mode, mission, or actuator command path.

No FC parameter may be changed merely to make an acceptance probe pass. First capture the current full parameter set, firmware version, relevant status messages and a reversible parameter diff. Parameter writes require a separate reviewed action and explicit operator authorization.

## Last Verified Physical/Runtime State

Latest recorded readiness evidence:

```text
date=2026-07-16
log=/home/pi/Visual_Homing_Codex/artifacts/logs/pi-field-readiness-20260715T213722Z.log
head=be133c5
pi_user=pi
serial=/dev/serial0 -> /dev/ttyS0
serial_owner=root:dialout
serial_mode=0660
serial_console=false
serial_getty=masked/inactive
serial_access=true
dialout_membership=true
mavlink_opened=true
mavlink_version=2
malformed_frames=0
heartbeat_seen=true
mode=AltHold
armed=false
relative_altitude_m=0.494/0.5256/0.542
ctest=28/28
readiness_passed=true
```

This state is timestamped evidence, not a guarantee about later powered hardware state. At the time of this document update, Pi is connected; re-run readiness after any power, wiring, boot-config, FC, or serial change.

## Reconnect Checklist

When Pi and FC are available again:

1. Confirm physical setup and that no hardware-output test is being authorized implicitly.
2. Verify `jtzero` hostname/IP reachability and the existing host fingerprint.
3. Verify existing key-based SSH; do not rotate the key unless it actually fails for a diagnosed reason.
4. Run only the read-only/field readiness gate first.
5. Capture exact `AUTOPILOT_VERSION`, firmware target/hash and a full FC parameter export.
6. Record current `SERIAL4_*`, `SR4_*`, `EK3_SRC*` and ExternalNav-related parameters here with evidence paths.
7. Reconfirm physical `TX3/RX3` wiring and common GND without changing it.
8. Update the last-verified date and keep historical values distinguishable from current values.
9. Only then consider a bounded props-off provider acceptance probe under the controlling safety plan.

The 2026-07-16 reconnect completed steps 2-6 and refreshed the readiness evidence. Physical `TX3/RX3` direction/common GND and the new route/altitude alignment contract remain unverified; provider-send must stay blocked until both are closed explicitly.

## Source Priority

When sources disagree, use this priority:

1. Current read-only hardware/FC output and exported parameters.
2. Current official ArduPilot board/protocol documentation for the installed firmware.
3. Accepted field/readiness artifacts with timestamps.
4. `SESSION_LOG.md` and `PROJECT_MEMORY.md` summaries.
5. Operator recollection, clearly marked `OPERATOR_REPORTED` until verified.

Do not silently convert an older confirmed value into a claim about current hardware state.
