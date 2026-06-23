# Real-Data Anchors + Vertex-Color Water Review

- Date: 2026-06-19
- Reviewer: independent external reviewer / human judge
- Review target: two follow-up commits —
  - `ceabc87` "Anchor ride scenery to generated terrain data" (acts on the non-blocking Finding from `river-actor-migration`).
  - `472f164` "Improve Yarlung river vertex-color water" (D/P4 water material groundwork v1).
- Reviewed artifacts (all read directly): full diffs of both commits; `CoasterRideActor.cpp`, `CoasterTrackComponent.cpp/.h` (anchor + new `GetGeneratedTerrainZAtDistance`); `YarlungRiverActor.cpp` (water mesh), `create-coaster-materials.py` (vertex-color material); `Saved/Diagnostics/yarlung-map-inspect.txt` (regenerated); `Saved/OffscreenShots/p4-real-anchors-v1.png` + `p4-water-vertex-v1.png` (read); `photoreal-progress.md`.
- Verdict: **PASS (reviewer-confirmed) — both commits are honest and correct for what they claim.** `ceabc87` genuinely resolves the half of my Finding it scopes (supports + fog → real DEM) and correctly defers the rest. `472f164` is honest "groundwork v1" with D3 explicitly left open. One non-blocking note + one strategic observation below.

## ceabc87 — anchors (directly addresses my prior Finding)

1. **Supports now plant on real DEM terrain, not the synthetic 2652m river.** `UCoasterTrackComponent` now stores `terrain_z` (CSV column 6) and exposes `GetGeneratedTerrainZAtDistance()`; support feet use `terrain_z − 35` instead of the old constant `RiverZCm − 35`. I verified the new interpolator is **correct, not just present**: its guard needs `RollSampleDistances.Num() == TerrainZ.Num() + 1`; `RollSampleDistances` is built as `Points.Num()+1` (one per point + a trailing `SplineLength`) and `TerrainZ` as `Points.Num()`, so `199 == 198+1` holds — the function interpolates real per-distance terrain and does **not** fall through to the `0.0f` sea-level error path. `p4-real-anchors-v1.png` (read) confirms supports descend to the valley floor with no regression / no giant legs.
2. **Valley fog re-anchored to the real river surface.** Fog Z changed from synthetic `RiverZCm+70` (2652.7m) to the **average of the real `YarlungRiver.csv` Z + 70**, loaded at runtime (`Anchored valley fog ... 267655.0cm`; inspect `fog_component=ValleyFog relative_z_cm=267725.0` = 2677.25m). The ~24.5m correction onto the real DEM water plane is consistent with the river CSV.
3. **Honest scoping — no over-claim.** The note explicitly says this fixes **"一半" (half)** of the Finding and lists the limitation that `YarlungRiverCenterY/YarlungLandscapeHeight` still serve `BoulderOutcrops` (currently `instances=0`), with full scatter migration / synthetic-terrain retirement deferred to A3/C — "不在本小步冒称完成". Independently confirmed: those synthetic functions do still exist and are still referenced only at the boulder-placement site (line 477-478). Correctly deferred, not hidden.

## 472f164 — vertex-color water (groundwork v1)

1. **Material change is sound.** `M_YarlungRiverWater/Foam` switched from a fixed BaseColor parameter to a `VertexColor`-driven BaseColor, so the per-vertex colors the mesh already computes now actually drive the surface (previously they were overridden by the constant param). Mesh refined: water cross-samples 5→11 with bank noise / center color band / composite ripple; foam 4→7 lanes with edge-weighted width/color/alpha. Build/import/inspect PASS.
2. **No over-claim.** Note states D3 stays **not Done**: still a translucent procedural ribbon, missing real flow normal / depth-shore fade / reflection-refraction, with the near-cliff Landscape staircase still the dominant short-board. Matches what I see.
3. **Non-blocking note: the per-vertex water Alpha is currently dead.** The mesh computes a shore `Alpha` (lerp 0.76→0.24 at banks) into vertex-color alpha, but the material drives Opacity from a constant scalar parameter (0.68), not from vertex alpha — so the intended shore alpha-fade is not actually rendered. Codex did not claim depth/shore fade works (explicitly lists it as TODO), so this is not an over-claim — just flagging that the vertex alpha is unused until Opacity is wired to it (or to a depth fade).

## Strategic observation (honest takeaway, not a blocker)

Across `p4-river-actor-v1 → v2 → real-anchors-v1 → water-vertex-v1`, the in-engine frames are **visually near-indistinguishable**. The water work is real in code, but the photoreal payoff is not yet visible because every frame is dominated by the **greybox macro terrain + the right-cliff Landscape staircase** — both stage C. Water cannot read as photoreal while sitting inside greybox cliffs, so further water-vertex micro-iteration is hitting diminishing returns. Codex's own "下一步" already says this ("若收益仍低则回 C"); I'm reinforcing it: **the highest-value next move is stage C (terrain materials + near-cliff staircase via corridor detail-normal → Nanite), not more water tweaks.**

## What is NOT closed (correctly deferred)

- **D3 photoreal water** — flow normal, depth/shore fade (wire vertex alpha or depth), reflection/refraction. Still v1; NOT Done.
- **Synthetic terrain/centerline retirement for boulders (A3) + full scatter migration** — `YarlungRiverCenterY/YarlungLandscapeHeight` still serve `BoulderOutcrops`. Deferred to A3/C.
- **Near-cliff Landscape staircase + terrain materials** — stage C; the dominant visual short-board.

## Agent Disposition

- Status: **Closed** — anchors correctly fixed to real DEM (half-Finding resolved + remainder honestly deferred); water material groundwork is honest v1.
- Next (recommended): pivot to **stage C** (terrain materials + staircase) as the dominant short-board, rather than further water-vertex iteration; wire water vertex-alpha → opacity/depth-fade when D3 material work resumes.
