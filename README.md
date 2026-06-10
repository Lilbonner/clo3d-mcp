# clo3d-mcp

**English** · [Русский](#русский)

An MCP server that lets Claude (or any MCP host) drive **CLO3D** for AI-assisted
garment design — import projects, dress avatars, assign fabrics, run cloth
simulation, render, and export, all from a chat.

```
Claude ◄─ stdio ─► clo_mcp (Python MCP server) ◄─ TCP/JSON ─► listener inside CLO ◄─► CLO API
```

- **`clo_mcp/`** — the MCP server. Runs on the host, talks stdio to Claude,
  forwards each tool call as one JSON command over a TCP socket
  (`127.0.0.1:5005`). No CLO dependency, fully unit-testable.
- The **listener inside CLO** comes in two flavors — pick one:

| | **Native C++ plugin** (`clo_plugin/`) — recommended | Python script (`clo_addon/`) — fallback |
|---|---|---|
| CLO UI while listening | **stays fully interactive** | frozen until `clo_shutdown` (CLO 7 has no Qt in Python) |
| Lifetime | whole CLO session, Start/Stop button | until the script is stopped |
| Feedback | status window with live request log | Log Console prints |
| Needs | CLO 7.3.240+, one-time DLL build | nothing (script as-is) |

## The custom plugin (`clo_plugin/`)

CLO 7's embedded Python exposes **no Qt and no idle/timer hook**, so a Python
listener can only block the UI thread while serving. The plugin solves this
natively: a `QTcpServer` created **on CLO's main thread** gets its socket
signals delivered by CLO's own Qt event loop — commands run on the main thread
(the only place the CLO API is safe) *without* a blocking loop. The UI only
stalls for the duration of a genuinely long call (simulate/render), same as
clicking the action by hand. Same wire protocol as the Python listener — the
MCP server doesn't know or care which one is on the other end.

Two hard-won implementation notes (details in `clo_plugin/README.md`):

- **CLO loads plugin DLLs transiently** — `LoadLibrary` before *every* export
  call, `FreeLibrary` right after. Static state would die instantly, so the
  plugin pins its module (`GetModuleHandleExW(..._PIN)`) when the listener
  starts; the server then survives across calls and clicks.
- **CLO swallows plugin exceptions and `qWarning`**, so the plugin appends a
  diagnostic trail to `C:/Users/Public/Documents/CLO/clo_mcp_plugin.log`.

Clicking the plugin's menu item opens a small **status window**:
green `● Running on 127.0.0.1:5005` / grey `○ Stopped`, a Start/Stop button,
and a live log — every request with outcome and timing, e.g.
`[14:23:05] simulate — ok (1840 ms)`.

### Build & install the plugin (scripts)

Prereqs: Visual Studio 2022 (MSVC C++), CMake ≥ 3.20, **Qt 5.15.x msvc2019_64**
(CLO 7.3.240 ships Qt 5.15.2 — exact match), and the CLO SDK
(`CLO_SDK_v7.3.240_WIN.zip` from developer.clo3d.com → API/SDK Download).

```powershell
cd clo_plugin
.\build.ps1 -SdkDir C:\path\to\CLO_SDK_v7.3.240   # add -QtDir C:\Qt\5.15.2\msvc2019_64 if Qt is elsewhere
.\install.ps1                                      # copies the DLL into CLO's API_Plug_in folder
```

- `build.ps1` configures CMake (VS 2022, x64, Release) and builds the DLL.
  `-StubOnly` builds `clo_mcp_test.exe` instead — a stub backend speaking the
  full protocol, no SDK needed, to verify the host ↔ server plumbing.
- `install.ps1` copies the DLL to
  `C:\Users\Public\Documents\CLO\Assets\Preferences\API_Plug_in\`. CLO reloads
  the DLL on every menu click, so no restart is needed — unless the listener
  is currently running (the DLL is pinned); then close CLO and re-run, or use
  `.\install.ps1 -WaitForClose` to auto-copy the moment CLO exits.
- `uninstall.ps1` removes the DLL (close CLO first).

If script execution is blocked: `powershell -ExecutionPolicy Bypass -File .\build.ps1 …`

<details><summary>Manual equivalent</summary>

```powershell
cd clo_plugin
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64 `
      -DCLO_SDK_DIR=C:/path/to/CLO_SDK_v7.3.240 `
      -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019_64
cmake --build build-msvc --config Release      # Release only - CLO won't load Debug
# then copy build-msvc/Release/clo_mcp_plugin.dll
#   -> C:\Users\Public\Documents\CLO\Assets\Preferences\API_Plug_in\
```
</details>

### Use

1. In CLO: **Settings → Plug-in → "MCP Listener (start / stop)"** — the status
   window opens and the listener auto-starts on `127.0.0.1:5005`.
2. That's it — CLO stays interactive while Claude works.

## Python listener (fallback, blocking on CLO 7)

1. In CLO: Main Menu → **Edit → Python Script → Run Python Script** →
   `clo_addon/clo_mcp_listener.py`.
2. On CLO 7 it prints `blocking main-thread mode` — the CLO UI is busy while
   serving; ask Claude to call `clo_shutdown` to hand the UI back.

## Hook up the MCP server (host side)

```bash
git clone https://github.com/Lilbonner/clo3d-mcp.git
cd clo3d-mcp && pip install -e .
```

**Claude Code:** `claude mcp add clo3d -- python -m clo_mcp.server`

**Claude Desktop** (`claude_desktop_config.json`):
```json
{ "mcpServers": { "clo3d": { "command": "python", "args": ["-m", "clo_mcp.server"] } } }
```

Then ask: *"call clo_ping"* → `CLO listener is up.` means the whole chain works.
From there, plain language: *"import C:/work/dress.zprj and simulate 80 frames"*.

## Tools

| Tool | What it does |
|---|---|
| `clo_ping` | check the listener is reachable |
| `import_project(path)` | open `.zprj` (replaces scene) / `.avt` (adds avatar) / `.zpac` (replaces garments, keeps avatar) |
| `pattern_count()` | number of pattern pieces in the scene |
| `simulate(frames)` | run the cloth solver |
| `add_fabric` / `assign_fabric` / `set_fabric_color` | fabric workflow |
| `copy_colorway` / `set_colorway` | colorway workflow |
| `render_image()` | render, returns saved PNG path(s) |
| `export_zprj(path)` | save the scene as `.zprj` |
| `clo_shutdown` | stop the listener (frees the UI in Python blocking mode) |

Tip: to dress an avatar, import the `.avt` first, then a `.zpac` garment saved
with arrangement points (e.g. CLO's library `Male_T-shirt.zpac`), then
`simulate` — `auto_hang` is not exposed in the SDK v4.3.4 C++ API.

## Configuration

Set the same values on both sides (host env and CLO side):
`CLO_MCP_HOST` (default `127.0.0.1`), `CLO_MCP_PORT` (`5005`),
`CLO_MCP_TIMEOUT` (`600` s — simulate/render can be long).

## Status

Validated end to end on CLO 7.3.240 with the native plugin: ping → import →
pattern_count → simulate → export (real `.zprj` on disk) → render (real PNG),
including adding an avatar and dressing it. See `AUDIT.md` for the validation
log and `clo_plugin/README.md` for plugin internals.

---

# Русский

MCP-сервер, который позволяет Claude (или любому MCP-хосту) управлять **CLO3D**:
импорт проектов, одевание аватаров, ткани, симуляция, рендер и экспорт — прямо
из чата.

```
Claude ◄─ stdio ─► clo_mcp (MCP-сервер, Python) ◄─ TCP/JSON ─► листенер внутри CLO ◄─► CLO API
```

- **`clo_mcp/`** — MCP-сервер. Работает на хосте, говорит с Claude по stdio,
  каждую команду шлёт одной JSON-строкой в TCP-сокет (`127.0.0.1:5005`).
- **Листенер внутри CLO** есть в двух вариантах — выбери один:

| | **Нативный C++ плагин** (`clo_plugin/`) — рекомендуется | Python-скрипт (`clo_addon/`) — запасной |
|---|---|---|
| UI CLO во время работы | **полностью отзывчивый** | заморожен до `clo_shutdown` (в Python CLO 7 нет Qt) |
| Время жизни | вся сессия CLO, кнопка Start/Stop | пока работает скрипт |
| Обратная связь | окно статуса с живым логом запросов | печать в Log Console |
| Что нужно | CLO 7.3.240+, разовая сборка DLL | ничего |

## Кастомный плагин (`clo_plugin/`)

Встроенный Python в CLO 7 **не имеет ни Qt, ни таймера/idle-хука**, поэтому
Python-листенер может слушать сокет только блокируя UI. Плагин решает это
нативно: `QTcpServer`, созданный **на главном потоке CLO**, получает сигналы
сокета через собственный Qt event loop CLO — команды выполняются на главном
потоке (единственное безопасное место для CLO API) **без** блокирующего цикла.
UI замирает только на время действительно долгой операции (симуляция/рендер) —
ровно как при ручном клике. Протокол тот же, что у Python-листенера: MCP-сервер
не знает и не должен знать, кто на другом конце.

Два выстраданных нюанса реализации (подробности в `clo_plugin/README.md`):

- **CLO грузит DLL плагина на каждый вызов**: `LoadLibrary` перед *каждым*
  экспортом и `FreeLibrary` сразу после. Статическое состояние умирало бы
  мгновенно, поэтому при старте листенера плагин пинит свой модуль
  (`GetModuleHandleExW(..._PIN)`) — сервер переживает выгрузки и клики.
- **CLO молча глотает исключения плагина и `qWarning`** — поэтому плагин ведёт
  диагностический лог в `C:/Users/Public/Documents/CLO/clo_mcp_plugin.log`.

Клик по пункту меню плагина открывает **окно статуса**: зелёный
`● Running on 127.0.0.1:5005` / серый `○ Stopped`, кнопка Start/Stop и живой
лог — каждый запрос с результатом и таймингом, например
`[14:23:05] simulate — ok (1840 ms)`.

### Сборка и установка плагина (скрипты)

Нужно: Visual Studio 2022 (MSVC C++), CMake ≥ 3.20, **Qt 5.15.x msvc2019_64**
(CLO 7.3.240 несёт Qt 5.15.2 — точное совпадение) и CLO SDK
(`CLO_SDK_v7.3.240_WIN.zip` с developer.clo3d.com → API/SDK Download).

```powershell
cd clo_plugin
.\build.ps1 -SdkDir C:\путь\к\CLO_SDK_v7.3.240    # добавь -QtDir C:\Qt\5.15.2\msvc2019_64, если Qt в другом месте
.\install.ps1                                      # копирует DLL в папку API_Plug_in CLO
```

- `build.ps1` конфигурирует CMake (VS 2022, x64, Release) и собирает DLL.
  С ключом `-StubOnly` соберёт `clo_mcp_test.exe` — заглушку с полным
  протоколом без SDK, чтобы проверить связку хост ↔ сервер заранее.
- `install.ps1` кладёт DLL в
  `C:\Users\Public\Documents\CLO\Assets\Preferences\API_Plug_in\`. CLO
  перечитывает DLL при каждом клике по меню, так что перезапуск не нужен —
  кроме случая, когда листенер сейчас запущен (DLL запинена): тогда закрой CLO
  и повтори, либо `.\install.ps1 -WaitForClose` — скопирует сам, как только
  CLO закроется.
- `uninstall.ps1` удаляет DLL (CLO нужно закрыть).

Если выполнение скриптов запрещено политикой:
`powershell -ExecutionPolicy Bypass -File .\build.ps1 …`

<details><summary>Ручной эквивалент</summary>

```powershell
cd clo_plugin
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64 `
      -DCLO_SDK_DIR=C:/путь/к/CLO_SDK_v7.3.240 `
      -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019_64
cmake --build build-msvc --config Release   # только Release - Debug CLO не загрузит
# затем скопируй build-msvc/Release/clo_mcp_plugin.dll
#   -> C:\Users\Public\Documents\CLO\Assets\Preferences\API_Plug_in\
```
</details>

### Использование

1. В CLO: **Settings → Plug-in → «MCP Listener (start / stop)»** — откроется
   окно статуса, листенер сам стартует на `127.0.0.1:5005`.
2. Всё — CLO остаётся отзывчивым, пока Claude работает.

## Python-листенер (запасной, блокирующий на CLO 7)

1. В CLO: Main Menu → **Edit → Python Script → Run Python Script** →
   `clo_addon/clo_mcp_listener.py`.
2. На CLO 7 он напишет `blocking main-thread mode` — UI CLO занят, пока идёт
   работа; чтобы вернуть управление, попроси Claude вызвать `clo_shutdown`.

## Подключение MCP-сервера (на хосте)

```bash
git clone https://github.com/Lilbonner/clo3d-mcp.git
cd clo3d-mcp && pip install -e .
```

**Claude Code:** `claude mcp add clo3d -- python -m clo_mcp.server`

**Claude Desktop** (`claude_desktop_config.json`):
```json
{ "mcpServers": { "clo3d": { "command": "python", "args": ["-m", "clo_mcp.server"] } } }
```

Проверка: скажи Claude **«вызови clo_ping»** → `CLO listener is up.` значит вся
цепочка работает. Дальше обычным языком: *«импортируй C:/work/dress.zprj и
просимулируй 80 кадров»*.

## Инструменты

| Инструмент | Что делает |
|---|---|
| `clo_ping` | проверить, что листенер доступен |
| `import_project(path)` | открыть `.zprj` (заменяет сцену) / `.avt` (добавляет аватара) / `.zpac` (заменяет одежду, аватар остаётся) |
| `pattern_count()` | число лекал в сцене |
| `simulate(frames)` | прогнать симуляцию ткани |
| `add_fabric` / `assign_fabric` / `set_fabric_color` | работа с тканями |
| `copy_colorway` / `set_colorway` | работа с colorway |
| `render_image()` | рендер, возвращает пути PNG |
| `export_zprj(path)` | сохранить сцену в `.zprj` |
| `clo_shutdown` | остановить листенер (в блокирующем режиме вернёт UI) |

Совет: чтобы одеть аватара — сначала импортируй `.avt`, затем `.zpac`-гармент
с точками расстановки (например, библиотечный `Male_T-shirt.zpac` из ассетов
CLO), затем `simulate`. (`auto_hang` в C++ API SDK v4.3.4 не экспонирован.)

## Настройка

Одинаковые значения с обеих сторон (env хоста и CLO):
`CLO_MCP_HOST` (по умолчанию `127.0.0.1`), `CLO_MCP_PORT` (`5005`),
`CLO_MCP_TIMEOUT` (`600` сек — симуляция и рендер бывают долгими).

## Траблшутинг

| Симптом | Причина / решение |
|---|---|
| `Cannot reach CLO listener` | CLO не открыт, листенер не запущен (окно статуса: `○ Stopped`?) или порт не совпадает. |
| Пункта меню плагина нет | DLL не в `…\Assets\Preferences\API_Plug_in\`, либо собрана Debug, либо CLO < 7.3. |
| Клик по меню «ничего не делает» | Смотри `clo_mcp_plugin.log` — он показывает каждый вызов CLO в плагин. |
| UI CLO не реагирует | Python-режим: ожидаемо, вызови `clo_shutdown`. Плагин: идёт долгая команда (simulate/render). |
| Команда зависает | Долгая симуляция/рендер — увеличь `CLO_MCP_TIMEOUT`. |

## Статус

Проверено end-to-end на CLO 7.3.240 с нативным плагином: ping → импорт →
pattern_count → simulate → экспорт (реальный `.zprj` на диске) → рендер
(реальный PNG), включая добавление аватара и его одевание. Лог валидации — в
`AUDIT.md`, внутренности плагина — в `clo_plugin/README.md`.
