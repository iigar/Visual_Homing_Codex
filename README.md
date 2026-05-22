# Visual Homing Codex

Experimental GPS-denied visual return system for Raspberry Pi Zero 2W class hardware.

This repository keeps the previous implementation as a reference baseline and builds a new C++ flight core from a clean architecture.

## Layout

- `reference/` - imported baseline project, kept for comparison and reuse.
- `core/` - new deterministic C++ core.
- `docs/` - architecture notes, roadmap, test strategy.
- `notes/` - informal research notes and ideas.
- `scripts/memory.ps1` - helper for project memory entries and session startup prompt.

## Direction

The new core is not intended to be a long-range metric SLAM system. The target is coarse visual return:

1. Record a compact visual route signature.
2. Match current camera frames against route progress.
3. Produce bounded course corrections for ArduPilot.
4. Keep all realtime stages measurable, bounded, and replay-testable.

Python and web components may remain useful for tooling, monitoring, and offline experiments. Flight-critical logic should live in the C++ core.

## Project Memory

The repository is the source of truth for long-term project context.

Use the helper script from the repository root:

```powershell
.\scripts\memory.ps1 startup
.\scripts\memory.ps1 decision "Decision title" "Decision body"
.\scripts\memory.ps1 session "Session title" "Progress summary"
.\scripts\memory.ps1 memory "Context title" "Stable project context"
.\scripts\memory.ps1 note "Research title" "Idea or research note"
```

Estimate the size of the standard startup context pack:

```powershell
.\scripts\context-status.ps1
.\scripts\context-status.ps1 -MaxTokens 128000
```

This is not a live Codex context meter. It estimates the token size of the project memory files plus `git log -3`, which is the context pack normally read at the start of a session.

Rules:

- Technical project decisions go to `docs/DECISIONS.md`.
- Current progress goes to `docs/SESSION_LOG.md`.
- Stable project context goes to `docs/PROJECT_MEMORY.md`.
- Ideas and research go to `notes/`.
- Code changes are captured in detailed git commits.
