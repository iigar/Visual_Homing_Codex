# Route Signature Format

## VHRS v1

Route signature files use a dependency-free binary format for compact replay and matching.

All integer fields are little-endian. Floating-point fields are IEEE-754 32-bit floats written little-endian.

## File Header

| Field | Size | Description |
| --- | ---: | --- |
| Magic | 4 | ASCII `VHRS` |
| Version | 2 | Format version, currently `1` |
| Header size | 2 | Header byte size, currently `16` |
| Endian | 1 | `1` means little-endian |
| Reserved | 1 | Must be `0` |
| Reserved | 2 | Must be `0` |
| Entry count | 4 | Number of route entries |

## Entry

| Field | Size | Description |
| --- | ---: | --- |
| Frame id | 8 | Source frame id |
| Timestamp ns | 8 | Monotonic timestamp since replay clock epoch |
| Altitude band m | 2 | Rounded altitude band in meters |
| Heading hint rad | 4 | Coarse heading/course hint |
| Width | 2 | Signature width |
| Height | 2 | Signature height |
| Pixel format | 1 | `1` Gray8, `2` Bgr8, `3` Thermal16 |
| Reserved | 1 | Must be `0` |
| Reserved | 2 | Must be `0` |
| Payload size | 4 | Payload byte count |
| Payload | N | Signature bytes |

## Current Limits

- The current recorder writes Gray8 preprocessed frames only.
- The reader preserves the pixel format field for future formats, but matching currently accepts Gray8 only.
- Format extensions must either use reserved fields compatibly or bump the version.
