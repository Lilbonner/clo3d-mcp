# Wire protocol (clo_mcp ⇄ clo_addon)

Newline-delimited JSON ("JSON Lines") over a plain TCP socket on
`127.0.0.1:5005` (configurable). One request per line, one response per line.

## Request

```json
{"id": "uuid-string", "command": "simulate", "params": {"frames": 80}}
```

- `id` — opaque correlation id chosen by the client.
- `command` — one of the handlers registered in `clo_addon/clo_mcp_listener.py`.
- `params` — object; keys depend on the command.

## Response

Success:

```json
{"id": "uuid-string", "ok": true, "result": {"fabric_index": 3}}
```

Failure:

```json
{"id": "uuid-string", "ok": false, "error": "AddFabric: file not found"}
```

`result` is always an object (use `{}` when a command returns nothing useful).
Errors are returned, never raised across the socket — the listener catches
exceptions from CLO API calls and reports them as `ok: false`.

## Commands (MVP)

| command | params | result |
|---|---|---|
| `ping` | `{}` | `{"clo": true}` |
| `shutdown` | `{}` | `{"stopping": true}` (ends blocking main-thread mode) |
| `import_project` | `{"path": str}` | `{}` |
| `import_avatar` | `{"path": str, "options": int?}` | `{}` |
| `auto_hang` | `{"garment": str, "hanger": str, "bottom": bool}` | `{}` |
| `simulate` | `{"frames": int}` | `{}` |
| `add_fabric` | `{"path": str}` | `{"fabric_index": int}` |
| `assign_fabric` | `{"fabric_index": int, "pattern_index": int}` | `{}` |
| `set_fabric_color` | `{"colorway": int, "fabric_index": int, "face": int, "r": float, "g": float, "b": float, "a": float}` | `{}` |
| `render_image` | `{}` | `{"paths": [str]}` |
| `export_zprj` | `{"path": str}` | `{"path": str}` (saved file path) |
| `copy_colorway` | `{"index": int}` | `{"new_index": int}` |
| `set_colorway` | `{"index": int, "name": str?}` | `{}` |
| `pattern_count` | `{}` | `{"count": int}` |
| `pattern_info` | `{"pattern_index": int}` | `{"info": str}` (JSON string from CLO) |
| `line_length` | `{"pattern_index": int, "line_index": int}` | `{"length": float}` |
| `arrangement_list` | `{}` | `{"arrangements": [{str: str}]}` |
| `set_arrangement` | `{"pattern_index": int, "arrangement_index": int}` | `{}` |
| `set_arrangement_position` | `{"pattern_index": int, "x": int, "y": int, "offset": int}` | `{}` |
| `add_seam` | `{"pattern_a": int, "line_a": int, "pattern_b": int, "line_b": int, "dir_a": bool?, "dir_b": bool?}` | `{}` |
| `seam_count` | `{}` | `{"count": int}` |
