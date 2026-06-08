"""Thin TCP/JSON client that talks to the listener running inside CLO.

One short-lived connection per command keeps state simple and survives CLO
restarts: if the listener went away, the next call just fails cleanly instead of
operating on a stale socket. See protocol.md for the wire format.
"""

from __future__ import annotations

import json
import os
import socket
import uuid

DEFAULT_HOST = os.environ.get("CLO_MCP_HOST", "127.0.0.1")
DEFAULT_PORT = int(os.environ.get("CLO_MCP_PORT", "5005"))
# simulation / render can be slow; generous by design.
DEFAULT_TIMEOUT = float(os.environ.get("CLO_MCP_TIMEOUT", "600"))


class CloError(RuntimeError):
    """A command reached CLO but the CLO API call failed."""


class CloUnavailable(RuntimeError):
    """The listener inside CLO could not be reached."""


class CloClient:
    def __init__(
        self,
        host: str = DEFAULT_HOST,
        port: int = DEFAULT_PORT,
        timeout: float = DEFAULT_TIMEOUT,
    ) -> None:
        self.host = host
        self.port = port
        self.timeout = timeout

    def call(self, command: str, params: dict | None = None) -> dict:
        """Send one command, block for its response, return ``result``.

        Raises CloUnavailable if CLO/the listener is not reachable, CloError if
        the CLO API call itself failed.
        """
        request = {
            "id": uuid.uuid4().hex,
            "command": command,
            "params": params or {},
        }
        payload = (json.dumps(request) + "\n").encode("utf-8")

        try:
            with socket.create_connection((self.host, self.port), timeout=5.0) as sock:
                sock.settimeout(self.timeout)
                sock.sendall(payload)
                raw = _recv_line(sock)
        except (ConnectionRefusedError, OSError) as exc:
            raise CloUnavailable(
                f"Cannot reach CLO listener at {self.host}:{self.port}. "
                f"Is CLO open and clo_mcp_listener.py running? ({exc})"
            ) from exc

        response = json.loads(raw)
        if not response.get("ok"):
            raise CloError(response.get("error", "unknown CLO error"))
        return response.get("result", {})


def _recv_line(sock: socket.socket) -> bytes:
    """Read until the first newline (one response per line)."""
    chunks: list[bytes] = []
    while True:
        chunk = sock.recv(4096)
        if not chunk:
            break
        chunks.append(chunk)
        if b"\n" in chunk:
            break
    data = b"".join(chunks)
    if not data:
        raise CloUnavailable("CLO listener closed the connection without a response")
    return data.split(b"\n", 1)[0]
