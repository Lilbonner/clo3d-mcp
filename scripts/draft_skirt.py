"""Draft two A-line skirt panels (front + back) via the listener's create_pattern."""
import sys

sys.path.insert(0, r"C:\projects\mcps\clo3d")

from clo_mcp.clo_client import CloClient

# Waist-to-knee panel, mm, y-up. Clockwise from top-left:
# L0 waist (360), L1 waist->hip (right), L2 hip->hem (right),
# L3 hem (520), L4 hem->hip (left), L5 hip->waist (left).
PANEL = [
    [-180, 560], [180, 560],   # waist edge
    [240, 360],                # right hip
    [260, 0],                  # right hem corner
    [-260, 0],                 # left hem corner
    [-240, 360],               # left hip
]

clo = CloClient()
for name in ("front", "back"):
    r = clo.call("create_pattern", {"points": PANEL})
    print(f"{name}: pattern_index={r.get('pattern_index')}")
