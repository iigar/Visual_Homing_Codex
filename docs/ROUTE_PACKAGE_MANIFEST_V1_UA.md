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

## Tracking Package Builder І Recovery

Library-only `RoutePackageBuilder` реалізує перший tracking-only producer для цього контракту:

- використовує окремий package directory та рівно один tracking layer;
- ліниво відкриває `tracking/chunk-NNNN.vhrs.partial` після першого entry;
- ротує chunk за bounded `maximum_entries_per_chunk`;
- додає chunk record тільки після finalize, повторного streaming-inspection, byte-size та SHA-256;
- записує `route.vhrm` лише після того, як усі chunks фіналізовані й manifest проходить verification.

Recovery scanner приймає тільки contiguous chunk indices від нуля, strictly increasing frame IDs, monotonic timestamps і точну layer compatibility. Для одного наступного `.vhrs.partial` він читає header checkpoint count, зберігає повний вихідний файл як `.recovery-source-NNNN`, а у final chunk переносить лише checkpointed prefix. Фізично записаний, але не checkpointed хвіст не вважається підтвердженою частиною маршруту. Через це після recovery допустимий пропуск frame IDs, але не зміна їх порядку.

Повністю записаний `route.vhrm.partial` можна підняти до final лише коли він парситься, точно збігається з очікуваними route/local-frame/camera/extrinsics/layer metadata та всі artifacts проходять size/SHA-256 verification. Collision final+partial, symlink, non-contiguous chunks, truncated payload, trailing bytes у final chunk або несумісний template відхиляються без перезапису final artifact.

Builder поки синхронний і не підключений до background camera recorder. Recovery-source archives навмисно не видаляються автоматично.

Поточний checkpoint використовує наявний `fstream` flush. Це достатньо для deterministic process-interruption tests, але ще не гарантує фізичну durability SD-карти при раптовому зникненні живлення. Перед power-loss acceptance потрібні окремі file-data/directory synchronization semantics та Pi fault-injection перевірка.

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
- Offline `VHIX v1` builder створює deterministic coarse descriptor index і окремий derived manifest. Ізольований selector визначає sparse requests/gate candidates, а `VerificationPackageWriter` публікує supplied native entries як одно-кадрові verification chunks та immutable cumulative VHRM revisions до selector commit. Live camera capture, revision recovery, search і high-resolution content confirmation ще відсутні.

## Поточна Перевірка

Desktop test покриває:

- стандартні SHA-256 vectors, включно з multi-block padding;
- deterministic binary round-trip;
- atomic final/partial collision behavior;
- package file size/digest verification;
- corruption detection;
- trailing bytes, unsafe path, invalid local frame, wrong gate layer, invalid digest/chunk version;
- preservation camera extrinsics, local axes, layer/chunk/index/gate references.
- bounded tracking chunk rotation and final manifest publication after digest;
- interrupted checkpoint-prefix recovery with preserved full source;
- direct resume, valid manifest-partial promotion and incompatible-template rejection;
- finalized trailing-byte, truncated payload, ordering, collision and config negatives.

## Наступні Кроки

1. Підключити live `1280x800` capture/synchronization до готових selector/package-writer boundaries без flight authority.
2. Реалізувати bounded offline VHIX coarse search і top-N provenance output без route lock.
3. Підключити package builder до окремого bounded background recorder лише після review черги/backpressure та recovery metrics.
4. Провести Pi load/storage/thermal benchmark.
5. Лише після high-resolution/multi-frame verification реалізувати producer конкретного `reset_reference`.

Descriptor/index contract: `docs/ROUTE_DESCRIPTOR_INDEX_V1_UA.md`.
Selector contract: `docs/VERIFICATION_GATE_SELECTOR_UA.md`.
Verification writer contract: `docs/VERIFICATION_PACKAGE_WRITER_UA.md`.
