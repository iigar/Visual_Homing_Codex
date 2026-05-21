# Visual Homing Codex

Experimental GPS-denied visual return system for Raspberry Pi Zero 2W class hardware.

This repository keeps the previous implementation as a reference baseline and builds a new C++ flight core from a clean architecture.

## Layout

- `reference/` - imported baseline project, kept for comparison and reuse.
- `core/` - new deterministic C++ core.
- `docs/` - architecture notes, roadmap, test strategy.

## Direction

The new core is not intended to be a long-range metric SLAM system. The target is coarse visual return:

1. Record a compact visual route signature.
2. Match current camera frames against route progress.
3. Produce bounded course corrections for ArduPilot.
4. Keep all realtime stages measurable, bounded, and replay-testable.

Python and web components may remain useful for tooling, monitoring, and offline experiments. Flight-critical logic should live in the C++ core.
