# Nanite Mesh Terrain Takeover Review — C2 v1

- Date: 2026-06-19
- Reviewer: independent external reviewer / human judge
- Review target: commits `a8c350a` (whole-loop staircase-risk diagnostic) and `f6b9c24` (move visible terrain to a Nanite StaticMesh, hide the Landscape).
- Reviewed artifacts (read/run directly): both diffs; `YarlungLandscapeImportCommandlet.cpp` (+297, mesh build); `YarlungMeshTerrainActor.*`; `scripts/diagnose-yarlung-staircase-risk.py`; `Saved/Diagnostics/yarlung-map-inspect.txt` (regenerated); `git lfs ls-files` + disk size of `SM_YarlungMeshTerrain.uasset`; `Saved/OffscreenShots/c2-nanite-terrain-v5-customnormals.png` (read); `photoreal-progress.md`.
- Verdict: **Infrastructure PASS (honest, genuine true-replacement) / staircase hard-gate still FAIL — and the core directive ("脱离DEM" / decouple) was NOT actually executed, which is precisely why it still facets.** Real structural progress; the specific Landscape stair-step artifact is gone. But Codex stayed DEM-faithful (meshed the 30m heightfield) instead of decoupling and adding sub-30m detail, so the resolution ceiling persists as large-triangle faceting. C correctly NOT Done.

## What genuinely PASSED (independently verified)

1. **It is a real, large Nanite mesh — not a stub.** `git lfs ls-files` shows `SM_YarlungMeshTerrain.uasset` LFS-tracked; working-tree file is **208 MB** on disk. Built from the post-clearance-cut heightfield as 2017×2017 (4.07M verts / 8.13M tris), cubic B-spline sampled + custom central-difference normals, Nanite enabled. Real geometry, real pipeline (`CoasterSim.Build.cs` +3 for mesh deps), UE build + import + inspect PASS.
2. **True replacement, NOT a double-layer skin (gate #2 PASS).** inspect confirms `landscape=YarlungTsangpoLandscape ... hidden=True` and a visible `YarlungMeshTerrainScenery`/`StaticMeshComponent` carrying the mesh. The old Landscape staircase render path is **structurally bypassed** — the visible surface is the single Nanite mesh, the Landscape is hidden. This directly satisfies the 禁区②/双层 guard from the directive: it replaces, it doesn't drape. The screenshot shows one coherent surface, no double-surface/z-fight.
3. **Whole-loop diagnostic is good practice.** `diagnose-yarlung-staircase-risk.py` samples first-person corridor cross-sections along the *entire* loop and ranks staircase risk by 1st/2nd-order slope into `yarlung-staircase-risk.csv` — guards against "fixed one hero frame, ignored the rest." Honest caveat noted by Codex: it scores *data-profile* risk, not render, and doesn't replace screenshots.
4. **Honest, no 注水.** Codex states plainly the Landscape LOD path is removed **but** "30m heightfield 上采样成单张 Nanite mesh 后仍有大三角/低模山体折面", the no-staircase/no-facet hard gate is **still Open**, and **C cannot be Done**. It also *retracted* failed experiments rather than commit them (v4 smooth-normals washed the material white; earlier `c2-corridor-terrain-*` procedural overlays looked worse/patchwork). Transparent process.

## What FAILED — and why (the key finding)

- **The staircase hard-gate is not met.** `c2-nanite-terrain-v5-customnormals.png` (read): the regular horizontal stair-steps are **gone**, but the mountain now reads as **large flat triangular facets** (angular low-poly planes on peaks and the right wall). The user's gate is "全第一人称无台阶**且无折面**" — faceting remains, so FAIL. The artifact changed form (stairs → facets) but the root cause is unchanged.
- **Root cause of the remaining facet = the directive was not followed.** The user's instruction was **"可以脱离DEM,要解决台阶"** — *decouple from the DEM* and author free mesh detail. Codex instead did a **DEM-faithful** conversion: it meshed the same 30m heightfield. A faithful mesh of 30m data inherits the 30m resolution poverty — it cannot show rock detail finer than its source, so big triangles are inevitable. **Decoupling was the whole point, and it is the missing step.** Codex itself now converges on this — its "next step" lists "Nanite displacement / 岩壁资产化 / 把近景崖改为真正 author rock asset" — i.e. add detail not present in the DEM.

## Assessment of the divergence

This is honest, useful infrastructure progress (the Nanite pipeline + Landscape-hidden true-replacement is necessary plumbing and is done well), but it is a **detour, not the directed cut**. It validates the directive by negative result: meshing the DEM faithfully still facets — confirming that escaping the 30m ceiling requires *inventing* sub-30m detail, which is exactly "脱离DEM". Not a wasted commit (keep the mesh pipeline + Landscape-hidden), but the central instruction remains undone.

## Recommended next cut (sharpening the original directive)

Keep what works — the Nanite StaticMesh terrain + hidden Landscape — and now **actually decouple**:
- Macro shape from the DEM is fine for **mid/far** terrain (real Yarlung scale, reads correct).
- For the **near corridor walls (the grazed hero faces)**, inject **invented sub-30m rock geometry** the DEM does not contain — procedural high-frequency displacement / rock strata / overhang on the mesh (or a real rock-cliff asset + Nanite displacement), concentrated where `yarlung-staircase-risk.csv` flags risk. This is the "脱离DEM" step applied to the mesh.
- Pair with a real rock material/normal so it doesn't repeat the dark proxy-tint look (material-honesty gate still applies).

## Open / watch items

- **Staircase/facet hard-gate: still Open** — must read with no stairs *and* no facets in first-person along the whole loop (use the risk CSV + off-track screenshot, both still required for a PASS).
- **Single 208 MB whole-world mesh** — works under Nanite, but watch memory/iteration cost; Codex's noted "chunked / higher-density corridor mesh" is the sensible evolution (dense where grazed, coarse far away).
- **Material** still dark vertex-color v1; proxy scatter + support-line noise visible — all correctly NOT claimed Done.

## Agent Disposition

- Status: **Open (C2 in progress)** — Nanite true-replacement infrastructure PASS and honest; staircase/facet gate still FAIL because the DEM was not decoupled.
- Next: the directed step is still pending — inject invented sub-30m detail on the near corridor walls (decouple from DEM), keep mesh+hidden-Landscape, real rock material. Do NOT return to Landscape LOD tuning; do NOT pass faceted DEM-mesh off as Done.
