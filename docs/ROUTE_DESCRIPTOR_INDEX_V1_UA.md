# Route Descriptor Index `VHIX v1`

## Статус

`VHIX v1` — deterministic library/offline-tool формат coarse descriptors для `VHRM v1`. Він не підключений до live camera, matcher, reacquisition state machine, reset, ODOMETRY, FC або UART і не має flight authority.

## Binary Contract

Header має magic `VHIX`, version `1`, little-endian policy, encoding ID, source і grid dimensions, descriptor dimensions та item count. Кожний item містить:

```text
frame_id: u64
manifest chunk_index: u32
entry_index inside VHRS chunk: u32
descriptor: grid_width * grid_height signed int8 values
```

Parser вимагає exact file size без trailing bytes, strictly increasing frame IDs і source locations, positive dimensions, не порожній index, максимум `1024` descriptor dimensions, `250000` items і `64 MiB` descriptor payload.

## Descriptor v1

Encoding `gray8-centered-block-mean-i8-v1`:

1. Gray8 frame ділиться на deterministic rectangular grid, default `16x10`.
2. Для кожної cell обчислюється mean intensity.
3. Від cell mean віднімається global frame mean.
4. Результат округлюється та clamp-иться до signed `[-127,127]`.

Це робить descriptor нечутливим до рівномірного brightness offset без clipping, але не дає rotation, perspective, сезонної або сильної scale invariance. Він призначений лише для дешевого coarse candidate generation. Низька VHIX distance не є route-lock, gate confirmation або `reset_reference`.

## Offline Package Builder

`build_route_descriptor_index_package()` та CLI `route_descriptor_index_builder`:

- читають і повністю size/SHA-256 verify-ять початковий `route.vhrm`;
- вимагають Gray8 tracking layer і package без попереднього index;
- обмежують byte size кожного source chunk до читання;
- читають один VHRS chunk за раз і застосовують global `sample_stride`;
- записують index через `.partial -> final`;
- додають `global_descriptor` layer та `RouteSearchIndexRecord`;
- створюють окремий derived manifest, наприклад `route-indexed.vhrm`;
- ніколи не перезаписують початковий manifest, index або partial artifact.

Приклад:

```text
route_descriptor_index_builder \
  package/route.vhrm \
  package/route-indexed.vhrm \
  index/coarse-v1.vhix \
  16 10 5
```

`chunk_index` посилається на позицію chunk record у derived manifest; `entry_index` — на entry всередині відповідного VHRS artifact. Майбутній search consumer мусить повторно звіряти provenance з manifest і не довіряти index окремо.

## Межі

- Builder використовує чинний `read_route_signature_file`, але не змінює його; перед викликом діють package digest та configurable chunk-byte bound.
- Source chunk завантажується у RAM цілком, хоча chunks обробляються по одному. Pi memory/timing benchmark ще потрібен.
- Якщо index уже фіналізовано, але derived manifest не вдалося записати через I/O failure, може лишитися orphan index; автоматичне видалення не виконується.
- Формат не містить ANN tree, inverted file або search scores. Перший search може бути bounded linear scan, а сильніший index буде новою version/encoding.
- High-resolution verification/gate selection і multi-frame temporal acceptance залишаються наступними етапами.

## Перевірка

Desktop tests покривають brightness-offset invariance, exact expected descriptor, deterministic binary round-trip, derived package build across rotated chunks, stride/provenance, package verification, non-overwrite, unsafe path, trailing bytes, empty index та invalid ordering. WSL/Ninja і MSVC 19.44/Ninja проходять `41/41`.
