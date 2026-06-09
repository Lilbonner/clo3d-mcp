# clo_plugin — native real-time listener for CLO (C++ / Qt)

A C++ CLO plugin that replaces the Python `clo_addon/clo_mcp_listener.py` with a
**non-blocking, real-time** listener. It runs a `QTcpServer` on CLO's main thread,
so commands dispatch on the UI thread (where the CLO API is safe) **without
freezing the UI** — the Blender-MCP model, done natively.

The MCP server (`clo_mcp/`) and the wire protocol are unchanged: same
newline-delimited JSON on `127.0.0.1:5005` (see `protocol.md`). Only the
in-CLO listener is swapped.

## Why a plugin (and not Python)

CLO 7's embedded Python exposes no Qt and no main-thread timer/idle hook, so a
Python listener can only *block* the UI (see `memory/clo7-no-realtime-hook.md`).
A compiled plugin loads into CLO's process and shares its Qt event loop, which is
exactly the main-thread pump Python lacks.

## Layout

| File | Role | Needs SDK? |
|---|---|---|
| `clo_api.h` | `ICloApi` — the seam between protocol and CLO | no |
| `clo_mcp_server.{h,cpp}` | `QTcpServer` + JSON protocol + dispatch | no |
| `clo_api_stub.cpp` | offline fake backend | no |
| `standalone_main.cpp` | console test harness (`clo_mcp_test`) | no |
| `clo_api_clo.cpp` | **real** backend: `CLOAPI::APICommand::getInstance()...` | **yes** |
| `plugin_main.cpp` | CLO plugin entry point (starts the server at load) | **yes** |

`clo_api_clo.cpp` and `plugin_main.cpp` are written against a specific CLO SDK —
authored once the target CLO version + SDK are chosen (see below).

## Build & test the stub TODAY (no CLO, no SDK)

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\clo_mcp_test.exe        # stub server on 127.0.0.1:5005
```
Then point the MCP server at it and call `clo_ping`, `simulate`, etc. — this
verifies the whole `clo_mcp` ↔ server ↔ protocol path before CLO is involved.

## Build the real plugin (needs the SDK)

**Target: CLO 7.3.240 → SDK v4.3.4 → Qt 5.x.** (The originally installed 7.0.228
has no downloadable SDK; 7.3.240 is the oldest version that does.) Get the SDK:
`CLO_SDK_v7.3.240_WIN.zip` from developer.clo3d.com → API/SDK Download, unzip it.

Prereqs: Visual Studio (MSVC), CMake ≥ 3.20, and **Qt 5.15.x** (open source) with
the Network module. Point CMake at both:

```powershell
cmake -S . -B build -DCLO_SDK_DIR=C:/path/to/CLO_SDK_v7.3.240 -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019_64
cmake --build build --config Release
```
Copy **both** `build/Release/clo_mcp_plugin.dll` and the auto-copied
`CLOAPIInterface.dll` to `C:/Users/Public/Documents/CLO/Plugins`, then restart CLO.
Release only — CLO will not load Debug binaries. No hot-reload (replace + restart).

Register/locate it via **Settings → Plug-in**: the action **"MCP Listener
(start / stop)"** appears there. Click it to start the listener on `127.0.0.1:5005`
(UI stays live); click again to stop.

## Gotcha: CLO loads the plugin DLL per call

CLO 7.3 does **not** keep the plugin DLL loaded: it `LoadLibrary`s the .dll
before *every* export call (`GetActionName`, `DoFunction`, …) and
`FreeLibrary`s it right after the call returns. Any static state — including
the listener — dies with the unload: the server starts and is destroyed
milliseconds later, with no crash and no visible error. (Tell-tale symptom:
overwriting the .dll *succeeds while CLO is running*, because a loaded DLL
would be write-locked.)

The fix in `plugin_main.cpp`: `DoFunction()` pins the module via
`GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | …_PIN, …)`.
After the pin, the next `LoadLibrary` returns the same instance, so state
persists and the start/stop toggle works.

Debugging channel: CLO swallows plugin exceptions and `qWarning`. The plugin
appends every export call, the attach path, and listen results to
`C:/Users/Public/Documents/CLO/clo_mcp_plugin.log` — read that file to see
where the chain breaks.

## What's wired

All `clo_mcp_listener.py` commands map to the C++ API (`clo_api_clo.cpp`), except
two that the v4.3.4 C++ API doesn't expose: `auto_hang` (Python-only binding) and
`import_avatar` (needs the `ImportExportOption` struct — TODO; use `import_project`).
