# Local code navigation tools

These tools are separate from the Visual_Homing runtime/build. The graph policy
is in [AGENTS.md](../../AGENTS.md); measured behavior and remaining limitations
are in the [2026-10-03 repair report](../../docs/CODE_GRAPH_TOOLING_2026-10-03_UA.md).

## GitNexus

From the repository root, with Node/npm installed:

```powershell
npm ci --prefix tools/code-graph
node tools/code-graph/gitnexus.mjs --version
node tools/code-graph/gitnexus.mjs analyze .
node tools/code-graph/gitnexus.mjs query "refresh route frame health" -r Visual_Homing_Codex
node tools/code-graph/gitnexus.mjs context refresh_route_frame_health -f core/src/route_frame_timing.cpp -r Visual_Homing_Codex
```

`package-lock.json` locks GitNexus 1.6.12 and its dependencies. Installation may
download/build native packages; indexing uses local sources, without embeddings
or a hosted LLM. The runner passes arguments directly to Node without a shell
and adds `--index-only` to analysis. It preserves the caller's working directory,
so run repository operations from the repository root or pass an explicit path.

On Windows the FTS extension needs the VC runtime and OpenSSL 3. When the two
OpenSSL DLLs exist under `%ProgramFiles%\Git\mingw64\bin`, the runner prepends
that directory to its child's PATH. It does not alter the system/user PATH or
install another runtime. If FTS was unavailable during analysis, run:

```powershell
node tools/code-graph/gitnexus.mjs analyze . --repair-fts
```

Verify a real query, not only `doctor` or a successful indexing exit code. If
DLLs are installed elsewhere, configure the process environment for that
installation and check the error before installing anything else. Upstream
[GitNexus CLI documentation](https://github.com/abhigyanpatwari/GitNexus/blob/main/gitnexus/README.md)
describes FTS diagnostics; the locked local CLI's `--help` is the version-specific
reference. `.gitnexusignore` limits the index to active project files, including
project documentation. Process extraction and call resolution remain incomplete.

For Codex MCP, use the same runner instead of `npx gitnexus@latest`. Set absolute
paths for the local machine in the existing GitNexus section of
`~/.codex/config.toml`, following the [Codex MCP configuration documentation](https://developers.openai.com/codex/mcp):

```toml
[mcp_servers.gitnexus]
command = 'C:\Program Files\nodejs\node.exe'
args = ['D:\LLM\ChatGPT\Codex\Visual-Homing\Visual_Homing_Codex\tools\code-graph\gitnexus.mjs', 'mcp']
```

Reconnect/restart the MCP host to adopt a changed launch command. An already
closed transport is not repaired by writing the config. Test the new server
separately and distinguish that result from the current host connection. Do not
commit local Codex config or credentials. The old generated `.gitnexus/run.cjs`
is not the maintained launcher.

## Graphify

Validated locally with Graphify 0.9.25. Dependencies are managed by the existing
Graphify installation, separately from the npm lockfile above.

```powershell
graphify --version
graphify update .
graphify query "refresh_route_frame_health"
graphify path "match_replay_route" "refresh_route_frame_health"
```

`.graphifyignore` excludes reference copies, artifacts, dependencies, docs and
Markdown/text files. This also keeps `update` within the intended code scope;
`extract --code-only` alone does not constrain later updates. Read canonical
project documentation directly.

When resetting an old index's scope, use a **new** output directory:

```powershell
graphify extract . --code-only --force --out artifacts/graphify-fresh-UNIQUE
```

Inspect `artifacts/graphify-fresh-UNIQUE/graphify-out/graph.json` for excluded
paths, duplicate IDs, dangling edges and known callers. Preserve the old
`graphify-out/` as a backup before replacing it with the validated directory.
Check that `.graphify_root` points to this repository. Then run `graphify update
.` and verify excluded paths do not reappear. Incremental updates alone do not
reliably remove old semantic nodes. Code-only extraction/update needs no LLM API.

Indexes, graph reports, local dependencies, probes and backups stay ignored.
Missing symbols and unresolved type nodes are not evidence of unused code.
