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

---

## Инструкция (RU)

MCP-сервер, который позволяет Claude управлять **CLO3D**: импорт проектов,
назначение тканей, симуляция, рендер и экспорт — прямо из чата.

### Требования

- **CLO3D 6.0.374+** с поддержкой Python (Main Menu → Edit → Python Script).
- **Python 3.10+** на той же машине, где запущен CLO.
- MCP-хост: Claude Code (CLI) или Claude Desktop.

### Установка

```bash
git clone https://github.com/Lilbonner/clo3d-mcp.git
cd clo3d-mcp
pip install -e .
```

### Шаг 1. Запустить листенер внутри CLO

1. Открой CLO3D.
2. Main Menu → **Edit → Python Script → Run Python Script**.
3. Выбери файл `clo_addon/clo_mcp_listener.py`.
4. В Log Console появится одно из двух:

   **CLO 7 (Python без Qt) — блокирующий режим (типичный случай):**
   ```
   [clo-mcp] no Qt binding found — blocking main-thread mode.
   [clo-mcp] listening on 127.0.0.1:5005 (blocking main-thread mode)
   ```
   Команды CLO API выполняются на главном потоке (безопасно), но **UI CLO
   «занят», пока листенер работает** — это нормально. Чтобы вернуть управление
   CLO, попроси Claude вызвать `clo_shutdown` (или закрой CLO).

   **Сборка с Qt — отзывчивый режим:**
   ```
   [clo-mcp] main-thread pump via PySide6.QtCore.QTimer
   [clo-mcp] ready (responsive mode)
   ```
   Здесь UI остаётся отзывчивым, листенер крутится в фоне.

> Повторный запуск скрипта безопасен — старый экземпляр останавливается сам.
> Проверить окружение перед запуском можно скриптом `clo_addon/diagnose.py`
> (тем же Run Python Script): он печатает версию Python, доступные API-модули и Qt.

### Шаг 2. Подключить MCP-сервер к хосту

**Claude Code (CLI):**
```bash
claude mcp add clo3d -- python -m clo_mcp.server
```

**Claude Desktop** — добавь в `claude_desktop_config.json`:
```json
{
  "mcpServers": {
    "clo3d": {
      "command": "python",
      "args": ["-m", "clo_mcp.server"]
    }
  }
}
```
После этого перезапусти Claude Desktop.

### Шаг 3. Проверить связь

Спроси у Claude: **«вызови clo_ping»**. Ответ `CLO listener is up.` означает,
что хост ↔ сервер ↔ CLO связаны. Дальше можно командовать обычным языком:

> «импортируй C:/work/dress.zprj и просимулируй 80 кадров»
> «назначь ткань из C:/fabrics/denim.zfab на паттерн 0 и отрендери картинку»

Когда закончишь — попроси «**вызови clo_shutdown**», чтобы остановить листенер и
вернуть управление окну CLO (актуально для блокирующего режима CLO 7).

### Настройка порта (если 5005 занят)

Порт и хост читаются из переменных окружения **на обеих сторонах** — задай
одинаковые значения там, где запускаешь и CLO, и MCP-сервер:

| Переменная | По умолчанию | Назначение |
|---|---|---|
| `CLO_MCP_HOST` | `127.0.0.1` | адрес листенера |
| `CLO_MCP_PORT` | `5005` | порт |
| `CLO_MCP_TIMEOUT` | `600` | таймаут ответа, сек (симуляция/рендер бывают долгими) |

### Тесты

```bash
# через pytest
pip install pytest && pytest -q
# или без него
PYTHONPATH=. python tests/test_clo_client.py   # ожидается: ok
```

### Что проверить при первом запуске в CLO (`# VERIFY`)

1. **Имена API-объектов.** Если `clo_ping` работает, а реальные команды падают с
   ошибкой про «CLO API handles not found» — поправь функцию `_apis()` в
   `clo_addon/clo_mcp_listener.py` под свою сборку CLO (как именно она отдаёт
   `import_api` / `export_api` / `fabric_api` / `pattern_api` / `utility_api`).
2. **Qt-биндинг.** Код перебирает `PySide6 → PySide2 → PyQt5`. Если ни один не
   нашёлся (`WARNING` в логе) — узнай, какой Qt есть в Python твоей CLO.
3. **Живучесть листенера.** Запусти, подожди минуту, снова вызови `clo_ping`.
   Если отвечает — листенер пережил завершение скрипта (см. `AUDIT.md`, H1).

### Траблшутинг

| Симптом | Причина / решение |
|---|---|
| `Cannot reach CLO listener` | CLO не открыт, или листенер не запущен, или порт не совпадает (см. env-переменные). |
| `no Qt binding found` в логе | Норма для CLO 7: листенер ушёл в блокирующий главнопоточный режим. UI CLO «занят» до `clo_shutdown`. |
| UI CLO не реагирует | Ожидаемо в блокирующем режиме — листенер держит главный поток. Вызови `clo_shutdown`. |
| `CLO API modules not importable` | Скрипт запущен не внутри CLO. Запускай через Edit → Python Script → Run. |
| Команда зависает | Долгая симуляция/рендер — увеличь `CLO_MCP_TIMEOUT`. |
