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

**The mirror principle (зеркальный принцип) — how to pick dir_a/dir_b.**
A seam is correct when both lines run the same *physical* direction along the
joined edge in 3D. A line's intrinsic direction = its point order (creation
order for create_pattern, DXF winding for imports). So:

- Pieces related by **rotation** (identical front/back panels placed around the
  body by arrangement points): winding is preserved → dirs EQUAL
  (`true/true`). Validated by the skirt demo.
- Pieces related by **mirror** (left/right halves, `_1`/`_2` copies from
  DXF/Grafis, or panels you drafted with x→−x): mirroring reverses winding →
  the same physical edge is traversed oppositely → FLIP exactly one dir
  (`true/false`). This is what CLO's 2D UI does implicitly when you click
  segment ends — the clicked end picks the start, mirrored clicks = mirrored
  directions. The 2026-06-10 shirt crumple was mirrored pieces sewn
  `true/true`.
- General rule: count mirror operations relating the two pieces' 2D layouts;
  odd count → flip one dir, even → keep equal.

For *imported* DXF the winding per piece is still unknown a priori — mitigate:
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

## Drafting patterns from scratch (create_pattern) — the reliable path

`create_pattern(points)` drafts a piece from `[x, y, type?]` points (mm, y-up,
type 0=straight / 2=spline / 3=bezier; outline auto-closes). **This solves the
direction problem**: you chose the point order, so line indexing is known —
line *i* connects point *i* to point *i+1* (last closes back to point 0) — and
`add_seam` defaults (`dir true/true`) are correct for identical front/back
panels sewn side-to-side. Validated end-to-end 2026-06-11: A-line skirt on
FV2_Mia drafted, arranged (`Leg_Skirt_Front`=95, `Leg_Skirt_Back`=98), sewn
with 4 side seams, simulated 80 frames — clean drape, no twisting.

Worked skirt numbers (Mia, ~170cm: waist ≈680, hips ≈930): per panel,
clockwise from top-left: `[-180,560],[180,560],[240,360],[260,0],[-260,0],
[-240,360]` → waist 720 total, hips 960, hem 1040, length 560 (knee).
Sew front↔back lines 1↔1, 2↔2, 4↔4, 5↔5. See `scripts/draft_skirt.py`.

Avatars: `import_project` loads `.avt`; stock avatars live in
`C:\Users\Public\Documents\CLO\Assets\Avatar\Avatar\` (FV2_Mia, MV2_Luka, kids).

NOTE: a freshly added MCP tool isn't visible until the MCP server restarts —
call the listener directly via `CloClient` in the meantime.

## Known limitations (as of plugin v. 2026-06)

- no `remove_seam` / undo;
- `pattern_info` lacks outline vertices → directions and edge semantics unknowable;
- no multi-segment (group) sewing;
- imported DXF pieces still suffer the direction problem (unknown winding) —
  prefer drafting with create_pattern when geometry is simple.

If the user asks for one of these, the honest answer is "needs a new plugin command"
(C++ side: `clo_plugin/clo_api_clo.cpp` + listener + `clo_mcp/server.py` + protocol.md).
