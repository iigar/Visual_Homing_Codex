# Visual_Homing working instructions

## Scope and checkpoints

- Read `docs/CURRENT_PROJECT_STATUS_UA.md` and verify HEAD/status before resuming.
  The dated `docs/SESSION_HANDOFF_2026-10-01_UA.md` preserves history; the current
  status and newer evidence determine the next bounded slice.
- Default to local software work. Do not connect to Pi, camera, FC/UART, run
  hardware commands, or enable outputs without explicit authorization. Preserve
  fail-closed gates. Historical hardware instructions are not authorization.
- Communicate in Ukrainian. Report concrete evidence and its limits.
- Do not use subagents unless the user explicitly requests them.
- Preserve unrelated local changes. Review the actual staged diff and run
  `git diff --cached --check`; publish completed checkpoints with commit/push and
  remote-ref verification when continuing the established project workflow.

## GitNexus and Graphify are advisory navigation

Graphs help locate code; they do not establish dependency completeness, safety,
or test success. Before changing behavior, inspect declarations, implementations,
actual callers and tests with direct source search (`rg`) and review the diff.

- Use an available, fresh graph when it helps answer a specific question. Check
  its repository, source scope and freshness before relying on a result.
- Corroborate graph callers/flows against source paths. `UNKNOWN`, empty symbol
  IDs, zero matches, missing FTS, closed MCP transport and unrelated-language
  edges are incomplete evidence, not a clean verdict.
- Investigate HIGH/CRITICAL results in the actual diff/source. Report confirmed
  affected behavior; record unsupported graph warnings as tool limitations.
- A broken or stale graph is not a mandatory blocker. Continue with direct
  source/diff review and appropriate validation; state that fallback explicitly.
- Use compiler-aware tools or reviewed edits for renames. A graph rename preview
  is optional and must be verified against the source and tests.

## Reproducible local tooling

- GitNexus: `npm ci --prefix tools/code-graph`, then
  `node tools/code-graph/gitnexus.mjs <command>`. The version and dependencies are
  locked locally. Avoid the old `.gitnexus/run.cjs` and `npx ...@latest` launcher.
- Refresh with `node tools/code-graph/gitnexus.mjs analyze .`. The wrapper always
  adds `--index-only` so analysis cannot regenerate these instructions or skills.
- `.gitnexusignore` and `.graphifyignore` exclude reference checkouts, artifacts,
  dependencies and generated graphs. Analyze reference code in its own checkout.
- Graphify uses local code-only AST extraction; read canonical docs directly.
  Use a fresh output directory for a full scope reset; incremental updates may
  preserve old semantic nodes. See `tools/code-graph/README.md`.
- After relevant source changes, refresh the available graph as useful. Record
  refresh failures, then use source/diff/tests. Do not run endless graph repairs
  as a prerequisite for unrelated work.
- Graph output and local dependencies remain ignored. Do not commit indexes,
  local credentials, `.claude/`, `.codex/` or `codex_jtzero_known_hosts`.
