# Native Verification Package Writer

## Статус

`VerificationPackageWriter` — ізольований library-only writer для sparse native-resolution verification keyframes. Він композиційно володіє `VerificationKeyframeSelector`, приймає вже захоплений `RouteSignatureEntry`, публікує VHRS/VHRM artifacts і лише тоді змінює selector state. Окремий `LiveVerificationCaptureSession` тепер формує synchronized entry/descriptor з одного native кадру, але сам writer не відкриває камеру, не виконує matcher/search, не створює `reset_reference`, не передає ODOMETRY і не має FC/UART/flight authority.

## Вхідний Package

Writer вимагає:

- повністю валідний і size/SHA-256 verified source VHRM з tracking chunks та configured search index;
- source package без verification layer/chunks і без gate records;
- окремий verification-layer template з role `verification`, Gray8 і dimensions, що точно дорівнюють native camera capture dimensions;
- source і всі derived manifest revisions у тому самому package directory, щоб portable relative paths не змінювали значення;
- safe relative directory для verification chunks і явний bounded maximum keyframe count.

Source manifest ніколи не перезаписується.

## Transactional Publication

Для кожного selector request:

```text
evaluate observation
    -> verify decision + frame/timestamp/native dimensions
    -> write one-entry verification/chunk-NNNN.vhrs.partial
    -> checkpoint + close + rename to final VHRS
    -> inspect native chunk + compute byte size/SHA-256
    -> append chunk and optional gate to candidate manifest
    -> write immutable route-verification-NNNN.vhrm revision
    -> verify every package artifact referenced by that revision
    -> selector.commit()
```

Кожний sparse кадр має окремий immutable одно-кадровий VHRS chunk. Це не найкомпактніша фінальна схема, але дає точну process-level межу: selector не переходить до наступної reference, доки native artifact і manifest revision не існують як final files та package verification не пройдено.

Writer володіє selector-ом, тому зовнішній код не може легально змінити його generation між publication і commit. Stale/forged decision, mismatch native frame/timestamp, неправильні dimensions/format, invalid gate envelope, corrupt source, unsafe config, maximum-count або будь-яка output/partial collision відхиляються до commit.

## Gate Publication

Якщо рішення має `gate_candidate=true`, derived revision отримує `RouteGateKeyframeRecord` з:

- verification layer/chunk/frame та configured search-index references;
- route segment/progress і forward/reverse mask;
- local pose, yaw, uncertainty та approach radius із observation;
- explicit altitude і scale envelopes з capture metadata.

VHRM cross-reference validation і full package verification виконуються до selector commit. Збережений gate усе ще є лише landmark/candidate metadata, не route lock і не дозволом руху.

## Межі Поточного Slice

- Writer і далі приймає готовий native entry. Нова library session закриває one-frame identity/descriptor synchronization, а окремий benchmark tool може polling-увати `PiCameraSource`; production route-progress/local-pose producer ще не підключено.
- Кожна publication створює новий chunk і нову manifest revision та повторно перевіряє весь referenced package. I/O зростає разом із route package; Pi cadence/load benchmark обов'язковий до runtime attachment.
- Restart/resume з наявних verification revisions ще не реалізовано. Collision блокує продовження fail-closed; автоматичного видалення або вгадування latest revision немає.
- Старі immutable manifest revisions не видаляються автоматично.
- Чинні `flush/close/rename` дають deterministic process-level publication, але не доводять physical SD durability при раптовій втраті живлення. Потрібні file/directory sync semantics і Pi fault injection.
- High-resolution content verification, multi-frame temporal acceptance, global search і `reset_reference` залишаються окремими етапами.

## Перевірка

Desktop test будує реальний tracking package, VHIX indexed manifest, а потім перевіряє кілька verification revisions і gate records. Negatives охоплюють corrupt source index, wrong output parent, native identity mismatch, invalid gate envelope, chunk/manifest collisions, directory-symlink escape, stale decision і maximum-keyframe bound. Source manifest залишається незмінним, а всі успішні revisions проходять VHRM package verification.

MSVC 19.44/Ninja та WSL/GCC проходять `43/43`; dedicated writer test проходить `100` повторів на кожній платформі.

Pi Zero 2W benchmark із `60` native `1280x800` revisions підтвердив correctness/cadence без throttling, але також виміряв зростання synchronous publication від `110.024 ms` до `3072.75 ms`. Тому writer не можна викликати з production tracking thread напряму; потрібен окремий bounded worker. Evidence: `docs/PI_VERIFICATION_CAPTURE_BENCHMARK_2026-07-19_UA.md`.
