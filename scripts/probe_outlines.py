"""One-off: dump outline line lengths for every pattern piece in the scene."""
import sys

sys.path.insert(0, r"C:\projects\mcps\clo3d")

from clo_mcp.clo_client import CloClient, CloError

clo = CloClient()

count = int(clo.call("pattern_count")["count"]) if False else None
# pattern_count result shape unknown here; just probe patterns 0..11
for p in range(12):
    lengths = []
    for i in range(200):
        try:
            r = clo.call("line_length", {"pattern_index": p, "line_index": i})
        except CloError:
            break
        # result dict: find the numeric value whatever the key is
        val = next(v for v in r.values() if isinstance(v, (int, float)))
        lengths.append(round(val, 1))
    print(f"pattern {p}: {len(lengths)} lines")
    print("  " + " ".join(f"{i}:{l}" for i, l in enumerate(lengths)))
