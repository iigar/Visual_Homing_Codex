# Route Package Manifest `VHRM v1`

## Статус

`VHRM v1` є library-only контрактом для багатошарового route package. Він не підключений до FC, UART, route matcher, search controller або автоматичного польоту. Чинний `VHRS v1` reader/writer не змінено: manifest лише описує та перевіряє окремі `VHRS v1` chunks і search-index artifacts.

## Призначення

Один довгий маршрут більше не розглядається як один необмежений файл або один масив у RAM:

```text
route.vhrm
tracking/chunk-0001.vhrs
tracking/chunk-0002.vhrs
verification/chunk-0001.vhrs
index/coarse-v1.bin
```

Manifest фіксує route identity, coordinate conventions, camera compatibility, layers, chunks, global-search indexes і high-resolution gate keyframes.

## Шари

`VHRM v1` вимагає рівно один `tracking` layer і дозволяє:

- `tracking`: часті low-resolution signatures, наприклад `160x100`;
- `global_descriptor`: компактне представлення для coarse full-route search;
- `verification`: sparse native-resolution keyframes, наприклад `1280x800`, для top-N і gate confirmation.

Кожний layer має власні dimensions, pixel format, camera-profile ID та altitude envelope. У v1 усі layers належать одному записувальному camera profile; підтримка cross-camera normalization є окремим майбутнім контрактом.

## Chunks Та Integrity

Кожний chunk містить:

```text
chunk id
layer id
portable relative path
VHRS format version
first/last frame id
entry count
byte size
SHA-256
```

Parser обмежує кількість records до `16` layers, `4096` chunks, `64` indexes і `4096` gates до allocation. Paths повинні бути відносними, без `..`, `.`, root, drive prefix, backslash або повторних separators. Package verifier canonicalize-ить шлях і відхиляє artifact, який через symlink опинився поза manifest directory.

Writer створює `route.vhrm.partial`, flush/close і лише потім перейменовує його у `route.vhrm`. Наявний final або partial не перезаписується.

SHA-256 доводить цілісність transfer/copy, але не доводить авторство. Для недовіреного обміну між людьми/системами пізніше потрібен окремий signed-package contract.

## Camera Compatibility

Package зберігає:

- profile ID, sensor type і pixel format;
- native capture dimensions і FOV;
- camera-to-body translation `x/y/z`;
- camera-to-body roll/pitch/yaw.

Інший апарат не повинен приймати gate або route lock, якщо його calibration/profile не пройшли окрему compatibility policy. Однакова назва камери без перевірених intrinsics/extrinsics не є достатнім доказом.

## Gate-Keyframe

Gate є sparse verification keyframe, який може бути входом у маршрут для іншої системи:

```text
local navigation prior
    -> approach radius around known gate pose
    -> coarse index candidate
    -> 1280x800 verification
    -> multi-frame consistency (future)
    -> confirmed segment/progress/direction
    -> concrete reset_reference (future)
```

Gate record містить:

- gate ID, verification layer, chunk і frame ID;
- search-index ID;
- route segment і normalized progress;
- forward/reverse permission mask;
- altitude та scale envelope;
- optional pose у конкретній local-frame identity/revision;
- local position uncertainty й approach radius.

Якщо local pose присутня, package мусить вказати `LOCAL_ENU` або `LOCAL_NED`; `approach_radius` повинен бути більшим за записану position uncertainty. Gate pose є landmark/approach metadata, не velocity command і не дозволом польоту.

## Важливі Межі

- Local coordinates корисні лише тоді, коли друга система поділяє той самий datum, axes convention і revision або має перевірене перетворення до них.
- Підведення до gate потребує runtime freshness/covariance/geofence/obstacle/altitude gates, яких у цьому етапі немає.
- Один великий кадр не є достатнім `reset_reference`: майбутній reacquisition gate має вимагати top-N verification та кілька узгоджених observation frames.
- Gate діє тільки у записаному altitude/scale/view envelope.
- Manifest зараз описує chunks/index, але ще не створює їх автоматично й не реалізує descriptor algorithm.

## Поточна Перевірка

Desktop test покриває:

- стандартні SHA-256 vectors, включно з multi-block padding;
- deterministic binary round-trip;
- atomic final/partial collision behavior;
- package file size/digest verification;
- corruption detection;
- trailing bytes, unsafe path, invalid local frame, wrong gate layer, invalid digest/chunk version;
- preservation camera extrinsics, local axes, layer/chunk/index/gate references.

## Наступні Кроки

1. Реалізувати package builder і crash-recovery scanner, який ротує finalized tracking chunks та оновлює manifest лише після digest.
2. Зафіксувати compact descriptor/index binary format і offline builder.
3. Додати sparse `1280x800` keyframe/gate selection policy.
4. Провести Pi load/storage/thermal benchmark.
5. Лише після цього реалізувати offline global reacquisition і producer конкретного `reset_reference`.
