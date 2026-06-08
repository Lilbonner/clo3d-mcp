# Project audit — clo3d-mcp

Audit of the initial scaffold (MCP server + in-CLO listener). Scope: the code in
`clo_mcp/` and `clo_addon/`, the wire protocol, and packaging. Reviewed against
the goal of being a working, safer alternative to `gregor124/clo-mcp`.

Legend: ✅ fixed in this branch · 🔶 open / needs live CLO · ⬜ deferred (post-MVP).

## Architecture verdict

Sound. Splitting networking (background thread, may block) from CLO API
execution (main thread via Qt timer) is the right call given CLO runs Python on
its UI thread. This is the single thing the prior art got wrong and the reason
to not just fork it.

## Findings

### High

| # | Finding | Status |
|---|---|---|
| H1 | **Listener lifetime.** CLO may discard the script's module globals once *Run Python Script* returns, garbage-collecting the timer and socket and silently killing the listener. | ✅ References parked on `builtins._CLO_MCP`. Still 🔶 to confirm this survives in your CLO build — watch whether `clo_ping` keeps working a minute after Run. |
| H2 | **Re-run safety.** Re-running the script double-bound the port and spawned orphan threads, with no way to stop. | ✅ `start()` tears down the prior instance; `stop()` added; bind happens on the main thread so a port clash raises immediately instead of dying in a thread. |

### Medium

| # | Finding | Status |
|---|---|---|
| M1 | **Stuck client pins a thread.** A half-open connection could block a daemon thread forever. | ✅ 30s per-connection recv timeout. |
| M2 | **`AutoHang(..., None)` options.** The API expects an options object; passing `None` may be rejected. | 🔶 Verify the real options type in your SDK; wire it through `auto_hang`. |
| M3 | **Shared-filesystem assumption.** `server.py:_require_file` validates paths on the machine running the MCP server, which only equals CLO's machine because we bind to localhost. | ⬜ Fine for the local MVP; document if remote is ever introduced. |

### Low

| # | Finding | Status |
|---|---|---|
| L1 | **Hardcoded port in two places.** | ✅ `CLO_MCP_HOST` / `CLO_MCP_PORT` / `CLO_MCP_TIMEOUT` env vars, read by both ends. |
| L2 | **No tests.** | ✅ Round-trip tests for `clo_client` (success + error paths). |
| L3 | **No protocol version / handshake.** A `ping` returning a version would let the server detect a stale listener. | ⬜ Add `version` to the `ping` result post-MVP. |
| L4 | **No structured logging.** `print()` to CLO's console is fine for now. | ⬜ |

## `# VERIFY` items (only checkable inside running CLO)

1. **API handles** — how CLO exposes `import_api` / `export_api` / `fabric_api`
   / `pattern_api` / `utility_api` to the Python editor. Edit `_apis()` in
   `clo_addon/clo_mcp_listener.py`; nothing downstream changes.
2. **Qt binding** — which of `PySide6` / `PySide2` / `PyQt5` is importable, so
   the main-thread timer pump engages instead of the unsafe fallback.
3. **H1** — that the listener keeps answering after the script returns.

## What's intentionally not here yet

- OBJ/FBX/glTF and techpack/BOM export tools (only `ExportZPrj` +
  `ExportRenderingImage` are wired) — add once exact API names are confirmed.
- `.mcpb` packaging (one-file install) — after the live path works end to end.
- CI. Add a GitHub Action running the tests once a remote exists.

## Test status

`PYTHONPATH=. python tests/test_clo_client.py` → `ok` (2 passing). All modules
compile under Python 3.13.
