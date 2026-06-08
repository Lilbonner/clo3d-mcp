# clo3d-mcp

An MCP server that lets Claude (or any MCP host) drive **CLO3D** for AI-assisted
garment design — import projects, assign fabrics, run cloth simulation, render,
and export, all from a chat.

Unlike the only prior attempt ([`gregor124/clo-mcp`](https://github.com/gregor124/clo-mcp),
a single-commit C++ stub with no working MCP server), this project targets the
**built-in CLO Python API** (CLO 6.0.374+) and ships a real, working tool surface.

## Architecture

```
Claude  ◄── stdio ──►  clo_mcp (Python MCP server)  ◄── TCP/JSON ──►  clo_addon (listener inside CLO)  ◄──►  CLO Python API
```

- **`clo_mcp/`** — the MCP server. Runs on the host, talks stdio to Claude,
  forwards each tool call as one JSON command over a TCP socket. No CLO
  dependency, fully unit-testable.
- **`clo_addon/clo_mcp_listener.py`** — runs *inside* CLO (Main Menu → Edit →
  Python Script → Run). Holds a **non-blocking** socket server and dispatches
  commands to the CLO API.

### The one constraint that shapes everything

CLO runs Python on its **main UI thread**. A blocking `accept()` loop would
freeze the UI. So the listener uses a Qt timer to poll the socket and drain a
command queue on the main thread — the same pattern Blender-MCP uses with a
modal timer. This is the part to validate first inside real CLO (see
`clo_addon/clo_mcp_listener.py` header).

## MVP tool surface

| Tool | CLO API call |
|---|---|
| `import_project(path)` | `import_api.ImportFile` |
| `import_avatar(path)` | `import_api.ImportAvatar` |
| `auto_hang(garment, hanger, bottom)` | `utility_api.AutoHang` |
| `simulate(frames)` | `utility_api.Simulate` |
| `add_fabric(zfab_path)` | `fabric_api.AddFabric` |
| `assign_fabric(fabric_idx, pattern_idx)` | `fabric_api.AssignFabricToPattern` |
| `set_fabric_color(...)` | `fabric_api.SetFabricPBRMaterialBaseColor` |
| `render_image()` | `export_api.ExportRenderingImage` |
| `export_zprj(path)` | `export_api.ExportZPrj` |
| `copy_colorway / set_colorway` | `utility_api.CopyColorway / SetCurrentColorwayIndex / SetColorwayName` |

## Run (dev)

1. In CLO: Edit → Python Script → Run `clo_addon/clo_mcp_listener.py`.
   It prints the port it's listening on (default `5005`).
2. Register the MCP server with your host, e.g. Claude Code:
   ```
   claude mcp add clo3d -- python -m clo_mcp.server
   ```
3. Ask: "import C:/work/dress.zprj and simulate 80 frames".

## Status

Scaffold. The MCP server side is complete; the CLO listener's threading model
and the exact API-handle import names need validation inside CLO (marked
`# VERIFY` in the listener). Packaging as a one-file `.mcpb` is a later step.
