"""The MCP server. Exposes CLO operations as tools over stdio.

Each tool is a thin wrapper that forwards to the listener inside CLO via
CloClient and returns a short human-readable string. Validation that can be done
without CLO (path exists, ints in range) happens here so failures are fast and
legible; everything else is reported by CLO.
"""

from __future__ import annotations

import os

from mcp.server.fastmcp import FastMCP

from clo_mcp.clo_client import CloClient, CloError, CloUnavailable

mcp = FastMCP("clo3d")
clo = CloClient()


def _run(command: str, params: dict | None = None) -> dict:
    try:
        return clo.call(command, params)
    except CloUnavailable as exc:
        raise RuntimeError(str(exc)) from exc
    except CloError as exc:
        raise RuntimeError(f"CLO rejected {command}: {exc}") from exc


def _require_file(path: str) -> str:
    path = os.path.expanduser(path)
    if not os.path.isfile(path):
        raise RuntimeError(f"File not found on the machine running CLO: {path}")
    return path


# --- connectivity -----------------------------------------------------------

@mcp.tool()
def clo_ping() -> str:
    """Check that CLO is open and the listener is reachable."""
    _run("ping")
    return "CLO listener is up."


# --- import -----------------------------------------------------------------

@mcp.tool()
def import_project(path: str) -> str:
    """Open a CLO project or garment file (.zprj/.zpac/.avt) into the scene."""
    _run("import_project", {"path": _require_file(path)})
    return f"Imported {path}."


@mcp.tool()
def import_avatar(path: str, options: int = 0) -> str:
    """Import an avatar file into the scene."""
    _run("import_avatar", {"path": _require_file(path), "options": options})
    return f"Imported avatar {path}."


# --- simulation -------------------------------------------------------------

@mcp.tool()
def auto_hang(garment: str, hanger: str, bottom: bool = False) -> str:
    """Auto-hang a garment on a hanger. bottom=True uses bottom hangers."""
    _run(
        "auto_hang",
        {"garment": _require_file(garment), "hanger": _require_file(hanger), "bottom": bottom},
    )
    return "Auto-hang done."


@mcp.tool()
def simulate(frames: int = 60) -> str:
    """Run cloth simulation for the given number of frames."""
    if frames <= 0:
        raise RuntimeError("frames must be a positive integer")
    _run("simulate", {"frames": frames})
    return f"Simulated {frames} frames."


# --- fabric -----------------------------------------------------------------

@mcp.tool()
def add_fabric(path: str) -> str:
    """Add a .zfab fabric to the project. Returns its fabric index."""
    result = _run("add_fabric", {"path": _require_file(path)})
    return f"Added fabric at index {result.get('fabric_index')}."


@mcp.tool()
def assign_fabric(fabric_index: int, pattern_index: int) -> str:
    """Assign an existing fabric to a pattern piece."""
    _run("assign_fabric", {"fabric_index": fabric_index, "pattern_index": pattern_index})
    return f"Assigned fabric {fabric_index} to pattern {pattern_index}."


@mcp.tool()
def set_fabric_color(
    fabric_index: int,
    r: float,
    g: float,
    b: float,
    a: float = 1.0,
    colorway: int = 0,
    face: int = 0,
) -> str:
    """Set a fabric's PBR base color (0..1 floats) for a colorway/face."""
    _run(
        "set_fabric_color",
        {"colorway": colorway, "fabric_index": fabric_index, "face": face,
         "r": r, "g": g, "b": b, "a": a},
    )
    return f"Set color of fabric {fabric_index}."


# --- colorway ---------------------------------------------------------------

@mcp.tool()
def copy_colorway(index: int = 0) -> str:
    """Duplicate a colorway. Returns the new colorway index."""
    result = _run("copy_colorway", {"index": index})
    return f"New colorway index {result.get('new_index')}."


@mcp.tool()
def set_colorway(index: int, name: str | None = None) -> str:
    """Make a colorway current, optionally renaming it."""
    _run("set_colorway", {"index": index, "name": name})
    return f"Active colorway is now {index}."


# --- render / export --------------------------------------------------------

@mcp.tool()
def render_image() -> str:
    """Render the current scene to image(s). Returns the saved file path(s)."""
    result = _run("render_image")
    paths = result.get("paths", [])
    return "Rendered: " + (", ".join(paths) if paths else "(no paths returned)")


@mcp.tool()
def export_zprj(path: str) -> str:
    """Save the current scene as a .zprj project file."""
    _run("export_zprj", {"path": os.path.expanduser(path)})
    return f"Saved project to {path}."


@mcp.tool()
def pattern_count() -> str:
    """Return the number of pattern pieces in the current scene."""
    result = _run("pattern_count")
    return f"{result.get('count')} pattern pieces."


def main() -> None:
    mcp.run()


if __name__ == "__main__":
    main()
