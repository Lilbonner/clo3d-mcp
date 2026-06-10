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


@mcp.tool()
def clo_shutdown() -> str:
    """Stop the in-CLO listener and hand control back to CLO's UI.

    Needed for the blocking main-thread mode CLO uses when no Qt binding is
    available: the listener occupies CLO until this is called.
    """
    try:
        _run("shutdown")
    except RuntimeError:
        # The listener may close the socket as it stops before we read a reply;
        # treat an unreachable listener here as already-stopped.
        pass
    return "Listener shutting down; CLO is interactive again."


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
    result = _run("export_zprj", {"path": os.path.expanduser(path)})
    return f"Saved project to {result.get('path', path)}."


@mcp.tool()
def pattern_count() -> str:
    """Return the number of pattern pieces in the current scene."""
    result = _run("pattern_count")
    return f"{result.get('count')} pattern pieces."


# --- pattern introspection / arrangement / sewing ----------------------------

@mcp.tool()
def pattern_info(pattern_index: int) -> str:
    """Get a pattern piece's information as JSON (outline lines, etc.).

    Use this to discover line indices before sewing with add_seam.
    """
    result = _run("pattern_info", {"pattern_index": pattern_index})
    return result.get("info", "")


@mcp.tool()
def line_length(pattern_index: int, line_index: int) -> str:
    """Length of one outline line of a pattern piece (to match seam lengths)."""
    result = _run("line_length", {"pattern_index": pattern_index, "line_index": line_index})
    return f"Line {line_index} of pattern {pattern_index} is {result.get('length')} long."


@mcp.tool()
def arrangement_list() -> str:
    """List the avatar's arrangement points (name, type, offset, orientation).

    Use an entry's index with set_arrangement to place a pattern on the body.
    """
    result = _run("arrangement_list")
    arrangements = result.get("arrangements", [])
    if not arrangements:
        return "No arrangement points (is an avatar loaded?)."
    lines = [f"[{i}] " + ", ".join(f"{k}={v}" for k, v in sorted(entry.items()))
             for i, entry in enumerate(arrangements)]
    return "\n".join(lines)


@mcp.tool()
def set_arrangement(pattern_index: int, arrangement_index: int) -> str:
    """Place a pattern piece on an avatar arrangement point (see arrangement_list)."""
    _run("set_arrangement",
         {"pattern_index": pattern_index, "arrangement_index": arrangement_index})
    return f"Pattern {pattern_index} arranged at point {arrangement_index}."


@mcp.tool()
def set_arrangement_position(pattern_index: int, x: int, y: int, offset: int) -> str:
    """Fine-tune a pattern's arrangement position (x, y, offset from body)."""
    _run("set_arrangement_position",
         {"pattern_index": pattern_index, "x": x, "y": y, "offset": offset})
    return f"Pattern {pattern_index} position set to ({x}, {y}) offset {offset}."


@mcp.tool()
def add_seam(
    pattern_a: int,
    line_a: int,
    pattern_b: int,
    line_b: int,
    dir_a: bool = True,
    dir_b: bool = True,
) -> str:
    """Sew outline line_a of pattern_a to outline line_b of pattern_b.

    dir_a/dir_b set each line's stitching direction (True = forward); flip one
    when the seam comes out twisted. Line indices come from pattern_info.
    """
    _run("add_seam", {"pattern_a": pattern_a, "line_a": line_a,
                      "pattern_b": pattern_b, "line_b": line_b,
                      "dir_a": dir_a, "dir_b": dir_b})
    return f"Sewed pattern {pattern_a} line {line_a} to pattern {pattern_b} line {line_b}."


@mcp.tool()
def seam_count() -> str:
    """Number of seamline pair groups (sewings) in the scene."""
    result = _run("seam_count")
    return f"{result.get('count')} seamline pair groups."


# --- pattern drafting ---------------------------------------------------------

@mcp.tool()
def create_pattern(points: list[list[float]]) -> str:
    """Draft a new pattern piece from 2D outline points (units: mm).

    Each point is [x, y] or [x, y, vertex_type]: 0 = straight (default),
    2 = spline curve, 3 = bezier curve. The outline closes automatically
    (last point connects back to the first). Returns the new pattern index —
    use it with set_arrangement / add_seam / assign_fabric.
    """
    result = _run("create_pattern", {"points": points})
    return f"Created pattern {result.get('pattern_index')} from {len(points)} points."


def main() -> None:
    mcp.run()


if __name__ == "__main__":
    main()
