---
name: clo-patterns
description: Build a garment from pattern pieces (лекала) in CLO 3D via the clo3d MCP tools — import DXF, probe outlines, arrange pieces on the avatar, sew, simulate, render. Use when the user asks to import patterns, sew pieces (сшить детали), arrange/dress a garment, or simulate cloth in CLO.
---

# Building garments from лекала in CLO via MCP

## Prerequisites

1. CLO must be running with the MCP listener up: `clo_ping` → "CLO listener is up."
   If not: CLO menu **Settings → Plug-in → MCP Listener (start / stop)** — the first
   click auto-starts the listener on 127.0.0.1:5005. Plugin debug log:
   `C:\Users\Public\Documents\CLO\clo_mcp_plugin.log`.
2. **Commands run on CLO's main thread.** During `simulate` / `import_project` /
   `render_image` the CLO window stops responding — this is normal, NOT a crash.
   Warn the user not to close the window; Windows will offer to kill it (AppHangB1).

## CRITICAL: checkpoint before sewing

There is **no remove_seam and no undo** over MCP — a wrong seam is permanent for the
session. Before any sewing experiment, save a checkpoint:

```
export_zprj  →  e.g. C:\projects\mcps\clo3d\out\checkpoint-before-sew.zprj
```

If sewing goes wrong, `import_project` the checkpoint back.

## Pipeline

### 1. Import (if starting from DXF)

`import_project` imports AAMA/ASTM DXF (e.g. exported from Grafis v11 with Collating
on). It keeps the avatar and **replaces** existing garment patterns. Pieces land flat,
un-arranged, un-sewn.

### 2. Inventory the pieces

- `pattern_count`, then `pattern_info(i)` for each — returns only bbox, id, name
  (NO outline geometry).
- Names from Russian DXFs arrive cp1251-mojibaked (`ïåðåä` = перед). Decode:
  `s.encode('latin1').decode('cp1251')`. Typical names: перед=front, спинка=back,
  рукав=sleeve, манжета=cuff, стойка=collar stand, воротник=collar, планка=placket,
  кокетка=yoke, бейка=binding.
- Pieces lying flat on the 2D table have z≈const (e.g. 200) and depth≈0 in their bbox;
  arranged pieces have real 3D positions.

### 3. Probe outlines

Run `python scripts/probe_outlines.py` — dumps every outline line length for all
patterns in seconds by talking to the listener directly (much faster than per-line MCP
calls). Gotcha: out-of-range `line_length` returns **0**, not an error — the real line
count is the index of the first 0.

### 4. Match seam lines by length

Edges that sew together have (nearly) equal lengths:
- exact match (±0.1mm) → same seam, high confidence (side seams, underarm);
- ±1–10mm → possible ease (посадка), medium confidence (yoke↔shoulder);
- a sleeve cap chopped into ~12 tiny DXF segments **cannot** be sewn — `add_seam`
  is strictly one line to one line. Skip it and tell the user.

### 5. Arrange on the avatar

`arrangement_list` → indices of body points; `set_arrangement(pattern, point)`.
Points observed on the default female avatar (verify with `arrangement_list`, indices
may differ per avatar): Body_Front_2_L=51 / R=52, Body_Back_2_L=34 / R=35,
Arm_Outside_2_L=20 / R=21, Neck_Back=76. Avatar's left ≈ +X in world coords.
Fine-tune with `set_arrangement_position(pattern, x, y, offset)`.

**Render and inspect BEFORE simulating** — pieces must wrap around the body in
plausible places.

### 6. Sew

`add_seam(pattern_a, line_a, pattern_b, line_b, dir_a, dir_b)`. Check `seam_count`
before/after (seams group, so +N calls may add <N groups).

The big unsolved problem: **stitch direction is guesswork** — `pattern_info` gives no
vertex data, so `dir_a`/`dir_b` cannot be computed. Wrong direction = twisted seam =
crumpled ball after simulate. Mitigations:
- sew the few high-confidence structural seams only, not everything;
- checkpoint first (see above);
- simulate in short bursts (30–60 frames), render, and stop early if it crumples.

A garment held only by side seams slides off the avatar — it needs shoulder/yoke
seams to hang.

### 7. Simulate & verify

`simulate(frames=60)` → `render_image` → Read the PNG (path is returned, e.g.
`...\AppData\Local\CLO\CLO Standalone OnlineAuth\<pid>\output_Default Colorway_1.png`).
Judge: pieces draped on body = good; ball of cloth on the floor/chest = bad seams →
restore checkpoint.

## Known limitations (as of plugin v. 2026-06)

- no `remove_seam` / undo;
- `pattern_info` lacks outline vertices → directions and edge semantics unknowable;
- no multi-segment (group) sewing;
- no command to draft pattern geometry from scratch — лекала must come from DXF.

If the user asks for one of these, the honest answer is "needs a new plugin command"
(C++ side: `clo_plugin/clo_api_clo.cpp` + listener + `clo_mcp/server.py` + protocol.md).
