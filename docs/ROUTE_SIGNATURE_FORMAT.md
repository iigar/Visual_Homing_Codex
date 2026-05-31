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

## Inspection

Route files can be inspected offline:

```bash
visual_homing_core --inspect-route <route.vhrs>
```

The inspector reads the file through the normal `VHRS` reader and reports entry count, frame and timestamp ranges, signature dimensions, payload byte totals, monotonic timestamp status, dimension/payload uniformity, and Gray8-only status.

## Self-Match Check

Route files can also be self-matched offline:

```bash
visual_homing_core --self-match-route <route.vhrs> [minimum_confidence]
```

The self-match check feeds each route entry payload back through the baseline `Gray8RouteMatcher` using stateless full-route matching so repetitive low-texture route entries cannot pin the checker to an earlier ambiguous window. It reports checked entries, valid matches, exact index matches, minimum and average confidence, last progress, monotonic progress status, and pass/fail status. Exact index and progress monotonicity are route-distinctiveness diagnostics; the pass gate is high-confidence valid matching for all entries.

## Perturbation Check

Route files can be checked against deterministic perturbations:

```bash
visual_homing_core --perturb-route <route.vhrs> [minimum_confidence]
```

The perturbation check applies brightness offset, small deterministic byte noise, and horizontal shift cases to route entries, then reports valid-match counts and minimum confidence for each case. It also verifies malformed-payload rejection through the matcher boundary.

## Distinctiveness Diagnostic

Route files can be analyzed for basic visual distinctiveness:

```bash
visual_homing_core --route-distinctiveness <route.vhrs>
```

The diagnostic reports low-texture entries, exact duplicate entries, ambiguous nearest-neighbor entries, payload range, adjacent mean absolute byte difference, nearest-neighbor mean absolute byte difference, and route-quality policy fields. It is an offline diagnostic, not a flight permission gate. A warning means the route may contain repetitive samples even when self-match and perturbation checks pass. `quality_pass=false` means the artifact does not satisfy the current bench/field route-quality policy.

Current route-quality policy:

- low-texture entry fraction must be `<= 0.05`;
- ambiguous nearest-neighbor entry fraction must be `<= 0.10`;
- average nearest mean absolute byte difference must be `>= 5.0`;
- exact duplicate entries are not allowed.
