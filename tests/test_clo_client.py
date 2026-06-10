"""Round-trip tests for CloClient against a fake one-shot socket server.

These exercise the wire format end to end without needing CLO or the `mcp`
package — only `clo_mcp.clo_client`, which depends on the stdlib alone.
"""

import json
import socket
import threading

from clo_mcp.clo_client import CloClient, CloError


def _bound_server():
    """A listening socket on an ephemeral port (bound before any client connects)."""
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    srv.listen(1)
    return srv, srv.getsockname()[1]


def _serve_once(srv, response):
    """Accept one client, read one request line, echo its id back in `response`."""
    conn, _ = srv.accept()
    with conn:
        data = b""
        while b"\n" not in data:
            chunk = conn.recv(4096)
            if not chunk:
                break
            data += chunk
        request = json.loads(data.split(b"\n", 1)[0])
        reply = dict(response, id=request["id"])
        conn.sendall((json.dumps(reply) + "\n").encode("utf-8"))
    srv.close()


def test_call_returns_result():
    srv, port = _bound_server()
    threading.Thread(
        target=_serve_once,
        args=(srv, {"ok": True, "result": {"fabric_index": 3}}),
        daemon=True,
    ).start()

    result = CloClient(port=port, timeout=5.0).call("add_fabric", {"path": "x.zfab"})
    assert result == {"fabric_index": 3}


def test_call_raises_clo_error_on_failure():
    srv, port = _bound_server()
    threading.Thread(
        target=_serve_once,
        args=(srv, {"ok": False, "error": "AddFabric: file not found"}),
        daemon=True,
    ).start()

    try:
        CloClient(port=port, timeout=5.0).call("add_fabric", {"path": "missing.zfab"})
    except CloError as exc:
        assert "file not found" in str(exc)
    else:  # pragma: no cover
        raise AssertionError("expected CloError")


def test_add_seam_params_cross_the_wire():
    """The sewing command's params must arrive at the listener verbatim."""
    srv, port = _bound_server()
    seen = {}

    def serve():
        conn, _ = srv.accept()
        with conn:
            data = b""
            while b"\n" not in data:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                data += chunk
            request = json.loads(data.split(b"\n", 1)[0])
            seen.update(request)
            reply = {"ok": True, "result": {}, "id": request["id"]}
            conn.sendall((json.dumps(reply) + "\n").encode("utf-8"))
        srv.close()

    threading.Thread(target=serve, daemon=True).start()

    params = {"pattern_a": 0, "line_a": 2, "pattern_b": 1, "line_b": 2,
              "dir_a": True, "dir_b": False}
    CloClient(port=port, timeout=5.0).call("add_seam", params)
    assert seen["command"] == "add_seam"
    assert seen["params"] == params


def test_create_pattern_points_cross_the_wire():
    """create_pattern's nested point list must arrive at the listener verbatim."""
    srv, port = _bound_server()
    seen = {}

    def serve():
        conn, _ = srv.accept()
        with conn:
            data = b""
            while b"\n" not in data:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                data += chunk
            request = json.loads(data.split(b"\n", 1)[0])
            seen.update(request)
            reply = {"ok": True, "result": {"pattern_index": 5}, "id": request["id"]}
            conn.sendall((json.dumps(reply) + "\n").encode("utf-8"))
        srv.close()

    threading.Thread(target=serve, daemon=True).start()

    params = {"points": [[0, 0], [200.5, 0, 0], [200.5, 300, 2], [0, 300, 3]]}
    result = CloClient(port=port, timeout=5.0).call("create_pattern", params)
    assert seen["command"] == "create_pattern"
    assert seen["params"] == params
    assert result == {"pattern_index": 5}


if __name__ == "__main__":  # allow `python tests/test_clo_client.py` without pytest
    test_call_returns_result()
    test_call_raises_clo_error_on_failure()
    test_add_seam_params_cross_the_wire()
    test_create_pattern_points_cross_the_wire()
    print("ok")
