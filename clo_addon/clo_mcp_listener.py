"""Run this INSIDE CLO: Main Menu -> Edit -> Python Script -> Run Python Script.

It opens a TCP listener and lets the external MCP server drive CLO. The design
exists to respect one hard constraint:

    CLO runs Python on its main UI thread, and CLO API calls are only safe on
    that thread. A blocking socket loop on the main thread would freeze CLO.

So we split the work:

    * a BACKGROUND thread owns the socket and may block on accept()/recv() —
      it only parses bytes and parks each request on a thread-safe queue;
    * a Qt TIMER on the MAIN thread drains that queue, runs the CLO API handler,
      and hands the result back for the network thread to send.

This is the same split Blender-MCP uses (modal timer + queue). If your CLO build
exposes Qt to Python (it almost certainly does — CLO is a Qt app), the timer
path is used. If not, we fall back to running handlers on the network thread and
print a loud warning: CLO API calls from a non-main thread may crash.

Lines marked `# VERIFY` are the two things to confirm against your CLO build:
how the *_api handles are obtained, and that Qt is importable here.
"""

import json
import os
import queue
import socket
import threading
import traceback

HOST = os.environ.get("CLO_MCP_HOST", "127.0.0.1")
PORT = int(os.environ.get("CLO_MCP_PORT", "5005"))

# --- queue of (request_dict, reply_queue) pending main-thread execution ------
_work = queue.Queue()
_stop = threading.Event()


# === CLO API access ==========================================================
# VERIFY: confirm how CLO exposes the API objects to the Python editor. The docs
# reference them as import_api / export_api / fabric_api / pattern_api /
# utility_api. They may be importable, or injected as globals. Adjust this one
# function and nothing else downstream changes.
def _apis():
    # Example shapes to try in your build — keep the one that resolves:
    #   import CLOAPI; return CLOAPI.import_api, CLOAPI.export_api, ...
    #   return import_api, export_api, fabric_api, pattern_api, utility_api
    g = globals()
    try:
        return (g["import_api"], g["export_api"], g["fabric_api"],
                g["pattern_api"], g["utility_api"])
    except KeyError as exc:  # pragma: no cover - environment dependent
        raise RuntimeError(
            "CLO API handles not found. Edit _apis() in clo_mcp_listener.py to "
            "match how your CLO build exposes import_api/export_api/etc."
        ) from exc


# === command handlers (run on the MAIN thread) ===============================
def _handle(command, params):
    import_api, export_api, fabric_api, pattern_api, utility_api = _apis()

    if command == "ping":
        return {"clo": True}

    if command == "import_project":
        import_api.ImportFile(params["path"])
        return {}

    if command == "import_avatar":
        import_api.ImportAvatar(params["path"], params.get("options", 0))
        return {}

    if command == "auto_hang":
        utility_api.AutoHang(params["garment"], params["hanger"],
                             1 if params.get("bottom") else 0, None)
        return {}

    if command == "simulate":
        utility_api.Simulate(int(params["frames"]))
        return {}

    if command == "add_fabric":
        idx = fabric_api.AddFabric(params["path"])
        return {"fabric_index": idx}

    if command == "assign_fabric":
        fabric_api.AssignFabricToPattern(params["fabric_index"], params["pattern_index"])
        return {}

    if command == "set_fabric_color":
        fabric_api.SetFabricPBRMaterialBaseColor(
            params["colorway"], params["fabric_index"], params["face"],
            params["r"], params["g"], params["b"], params["a"])
        return {}

    if command == "copy_colorway":
        new_index = utility_api.CopyColorway(params["index"])
        utility_api.UpdateColorways(True)
        return {"new_index": new_index}

    if command == "set_colorway":
        utility_api.SetCurrentColorwayIndex(params["index"])
        if params.get("name"):
            utility_api.SetColorwayName(params["index"], params["name"])
        return {}

    if command == "render_image":
        paths = export_api.ExportRenderingImage(True)
        return {"paths": list(paths) if paths else []}

    if command == "export_zprj":
        export_api.ExportZPrj(params["path"])
        return {}

    if command == "pattern_count":
        return {"count": pattern_api.GetPatternCount()}

    raise ValueError(f"unknown command: {command}")


def _dispatch(request):
    """Build a response dict for one request, catching all CLO errors."""
    rid = request.get("id")
    try:
        result = _handle(request["command"], request.get("params", {}))
        return {"id": rid, "ok": True, "result": result}
    except Exception as exc:  # noqa: BLE001 - errors travel over the wire, not up
        return {"id": rid, "ok": False, "error": f"{type(exc).__name__}: {exc}"}


# === networking (BACKGROUND thread; blocking is fine here) ===================
def _serve(srv):
    print(f"[clo-mcp] listening on {HOST}:{PORT}")
    while not _stop.is_set():
        try:
            conn, _ = srv.accept()
        except socket.timeout:
            continue
        except OSError:
            break
        threading.Thread(target=_client, args=(conn,), daemon=True).start()
    try:
        srv.close()
    except OSError:
        pass
    print("[clo-mcp] listener stopped")


def _client(conn):
    with conn:
        conn.settimeout(30.0)  # a stalled client must not pin a thread forever
        data = b""
        try:
            while b"\n" not in data:
                chunk = conn.recv(4096)
                if not chunk:
                    return
                data += chunk
        except socket.timeout:
            return
        line = data.split(b"\n", 1)[0]
        try:
            request = json.loads(line)
        except json.JSONDecodeError:
            conn.sendall(b'{"ok":false,"error":"bad JSON"}\n')
            return

        reply = queue.Queue(maxsize=1)
        _work.put((request, reply))
        response = reply.get()  # blocks this client thread until main thread runs it
        conn.sendall((json.dumps(response) + "\n").encode("utf-8"))


# === main-thread pump ========================================================
def _drain_once():
    """Run all currently-queued requests on the main thread."""
    while True:
        try:
            request, reply = _work.get_nowait()
        except queue.Empty:
            return
        reply.put(_dispatch(request))


def _install_qt_pump():
    """Drive _drain_once from a Qt timer on the main thread. Returns the timer
    so it isn't garbage-collected, or None if Qt is unavailable."""
    for mod in ("PySide6", "PySide2", "PyQt5"):  # VERIFY: which ships with your CLO
        try:
            qtcore = __import__(f"{mod}.QtCore", fromlist=["QtCore"])
        except ImportError:
            continue
        timer = qtcore.QTimer()
        timer.setInterval(50)
        timer.timeout.connect(_drain_once)
        timer.start()
        print(f"[clo-mcp] main-thread pump via {mod}.QtCore.QTimer")
        return timer
    return None


def stop():
    """Stop a running listener. Safe to call when nothing is running."""
    import builtins
    _stop.set()
    prev = getattr(builtins, "_CLO_MCP", None)
    if prev:
        srv = prev.get("srv")
        if srv is not None:
            try:
                srv.close()
            except OSError:
                pass
        timer = prev.get("timer")
        if timer is not None:
            try:
                timer.stop()
            except Exception:  # noqa: BLE001
                pass
        builtins._CLO_MCP = None
    print("[clo-mcp] stopped")


def start():
    import builtins
    # Re-run safety: an earlier Run may have left a listener bound to the port.
    if getattr(builtins, "_CLO_MCP", None):
        stop()
    _stop.clear()

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((HOST, PORT))  # bind on the main thread so a port clash fails loudly
    srv.listen(8)
    srv.settimeout(0.5)

    net = threading.Thread(target=_serve, args=(srv,), daemon=True)
    net.start()

    timer = _install_qt_pump()
    if timer is None:
        print("[clo-mcp] WARNING: no Qt timer available — running handlers on the "
              "network thread. CLO API calls may be unsafe off the main thread.")
        # Fallback: drain on a background thread. Risky but better than nothing.
        threading.Thread(target=_fallback_pump, daemon=True).start()

    # Persist references on builtins so they outlive this script's module scope.
    # CLO may discard the module globals once Run Python Script returns, which
    # would otherwise garbage-collect the timer/socket and kill the listener.
    builtins._CLO_MCP = {"srv": srv, "timer": timer, "net": net}
    return timer


def _fallback_pump():
    while not _stop.is_set():
        try:
            request, reply = _work.get(timeout=0.2)
        except queue.Empty:
            continue
        reply.put(_dispatch(request))


try:
    start()
except Exception:  # pragma: no cover
    traceback.print_exc()
