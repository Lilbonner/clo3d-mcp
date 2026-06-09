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

## `# VERIFY` items — results from live CLO 7 (Python 3.7.9)

Checked via `clo_addon/diagnose.py`:

1. **API handles** — ✅ Resolved. `import_api` / `export_api` / `fabric_api` /
   `pattern_api` / `utility_api` are importable modules. `_apis()` now imports
   them directly. (`ApiTypes` is **not** importable in CLO 7 — affects only the
   options arg of `AutoHang`, see M2.)
2. **Qt binding** — ✅ Resolved (by design change). None of PySide6/PySide2/PyQt5
   exist in CLO 7's embedded Python, so the main-thread timer pump is impossible.
   The listener now runs a **blocking main-thread server** when no Qt is found:
   CLO API calls happen on the main thread (safe), CLO's UI is busy while
   serving, and a `shutdown` command returns control. The Qt timer path remains
   for builds that do expose Qt.
3. **H1 (listener lifetime)** — N/A in blocking mode: the script never returns
   while serving, so there are no module-scope globals to be collected. Still
   relevant only for the Qt/responsive path.

## What's intentionally not here yet

- OBJ/FBX/glTF and techpack/BOM export tools (only `ExportZPrj` +
  `ExportRenderingImage` are wired) — add once exact API names are confirmed.
- `.mcpb` packaging (one-file install) — after the live path works end to end.
- CI. Add a GitHub Action running the tests once a remote exists.

## Test status

`PYTHONPATH=. python tests/test_clo_client.py` → `ok` (2 passing). All modules
compile under Python 3.13.

### Live validation — CLO 7 (Python 3.7.9), blocking main-thread mode

End-to-end smoke test against a running CLO 7 instance (listener started via
Run Python Script, driven from the host over the socket):

| command | result |
|---|---|
| `ping` | `{"ok": true, "result": {"clo": true}}` |
| `pattern_count` | `{"ok": true, "result": {"count": 7}}` — real `pattern_api.GetPatternCount()` |
| `shutdown` | `{"ok": true, "result": {"stopping": true}}` — CLO UI became interactive again |

Confirms the full chain (host → socket → listener → CLO API → response) and that
`shutdown` cleanly releases the main thread. Write/simulate/render commands not
yet exercised.
