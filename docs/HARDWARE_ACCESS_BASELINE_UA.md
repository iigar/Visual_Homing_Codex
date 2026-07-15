# Hardware And Access Baseline

Цей документ є коротким source-of-truth для апаратної конфігурації, SSH-доступу та ArduPilot/MAVLink інтеграції Visual Homing. Його треба читати перед будь-якою роботою з Raspberry Pi, Matek, serial telemetry, FC/JT_Zero acceptance або зовнішньою навігацією.

Останнє оновлення: `2026-07-15`.

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
| Pi serial device | `/dev/serial0 -> /dev/ttyS0` | `CONFIRMED` | Readiness evidence 2026-07-15 |
| Pi serial ownership | user `pi`, group `dialout`; udev rule covers `ttyS0` and `ttyAMA0` | `CONFIRMED` | `SESSION_LOG.md`, readiness evidence |
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
| Pi SSH hostname | `jtzero` | `CONFIRMED`, `CURRENTLY_OFFLINE` | Used by prior Pi sessions and backup docs; Pi is currently not connected |
| Pi SSH user | `pi` | `CONFIRMED`, `CURRENTLY_OFFLINE` | Readiness evidence and Pi paths |
| Dedicated key name | `id_ed25519_jtzero` | `CONFIRMED`, `CURRENTLY_OFFLINE` | Key access recorded working on 2026-07-08 |
| Repo-local host-key file | `codex_jtzero_known_hosts` | `CONFIRMED`, `NOT_TRACKED` | Exists in the current repo root and contains a public host fingerprint only; preserve/recreate explicitly after a fresh clone |
| Last recorded SSH key confirmation | `2026-07-08` | `CONFIRMED` | `SESSION_LOG.md` |
| Current live SSH availability | Pi not connected; hostname resolution/live login is not expected | `CURRENTLY_OFFLINE` | Operator report 2026-07-15 |

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
| Exact ArduCopter version | unknown | `REQUIRES_VERIFICATION` | Read `AUTOPILOT_VERSION` or an FC parameter/log export after reconnect |
| Firmware git hash | unknown | `REQUIRES_VERIFICATION` | Read `AUTOPILOT_VERSION` after reconnect |
| FC board/firmware target string | MatekH743 family; exact reported string unknown | `REQUIRES_VERIFICATION` | Capture current FC version/status |
| `SERIAL4_PROTOCOL` | current value not stored | `REQUIRES_VERIFICATION` | Read-only parameter snapshot |
| `SERIAL4_BAUD` | effective link is 115200; raw parameter value not stored | `REQUIRES_VERIFICATION` | Read-only parameter snapshot |
| Other `SERIAL4_*` | not stored | `REQUIRES_VERIFICATION` | Read-only parameter snapshot |
| `SR4_*` stream-rate parameters | useful stream was restored historically; exact values not stored | `REQUIRES_VERIFICATION` | Read-only parameter snapshot |
| `EK3_SRC*` external-nav source parameters | not stored | `REQUIRES_VERIFICATION` | Read-only parameter snapshot before any changes |
| ExternalNav-related parameters | partially configured historically; exact set not stored | `REQUIRES_VERIFICATION` | Full parameter export and reviewed diff |

No FC parameter may be changed merely to make an acceptance probe pass. First capture the current full parameter set, firmware version, relevant status messages and a reversible parameter diff. Parameter writes require a separate reviewed action and explicit operator authorization.

## Last Verified Physical/Runtime State

Latest recorded readiness evidence:

```text
date=2026-07-15
log=/home/pi/Visual_Homing_Codex/artifacts/logs/pi-field-readiness-20260714T204714Z.log
pi_user=pi
serial=/dev/serial0 -> /dev/ttyS0
serial_access=true
dialout_membership=true
mavlink_opened=true
mavlink_version=2
malformed_frames=0
heartbeat_seen=true
mode=AltHold
armed=false
relative_altitude_m=0.279/0.3054/0.322
readiness_passed=true
```

This state is historical evidence, not a statement about the currently powered hardware. At the time of this document update, Pi is not connected.

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

## Source Priority

When sources disagree, use this priority:

1. Current read-only hardware/FC output and exported parameters.
2. Current official ArduPilot board/protocol documentation for the installed firmware.
3. Accepted field/readiness artifacts with timestamps.
4. `SESSION_LOG.md` and `PROJECT_MEMORY.md` summaries.
5. Operator recollection, clearly marked `OPERATOR_REPORTED` until verified.

Do not silently convert an older confirmed value into a claim about current hardware state.
