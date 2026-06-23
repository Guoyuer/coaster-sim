# River-Into-Scenery-Actor Migration Review (D/P4 water v1)

- Date: 2026-06-19
- Reviewer: independent external reviewer / human judge
- Review target: commit `34cafaa` "Move Yarlung river into scenery actor" — pull river rendering out of `ACoasterRideActor` into a dedicated `AYarlungRiverActor` sourced from the DEM thalweg.
- Reviewed artifacts (all read directly): `git show 34cafaa` (full diff), `Source/CoasterSim/CoasterRideActor.cpp` (residual refs), `Source/CoasterSim/YarlungRiverActor.cpp` (new actor, full), `Content/Generated/YarlungLandscape/YarlungRiver.csv` (head), `YarlungTrack.csv` (head, cross-check), `scripts/generate-yarlung-landscape-assets.py:write_river_csv` (generation), `Saved/Diagnostics/yarlung-map-inspect.txt` (in-editor actor/component report), `Saved/OffscreenShots/p4-river-actor-v2.png` (read), `manifest.json`, `photoreal-progress.md`.
- Verdict: **PASS (reviewer-confirmed) on the architecture-migration + DEM-sourced-water-v1 axis.** The river water-rendering responsibility is genuinely and cleanly moved out of the ride actor into a standalone actor that reads the real DEM thalweg; the claims are honestly scoped (D3 explicitly NOT Done; staircase still deferred to C). One non-blocking latent inconsistency recorded below for stage C.

## Why it passes (independently checked)

1. **The migration is real, not cosmetic.** The diff removes the entire old river stack from `ACoasterRideActor`: components `RiverSurface/Rapids/MistBands/RiverRibbonMesh/FoamRibbonMesh` and functions `BuildRiverEffects/BuildRiverSamples/BuildRiverRibbon/BuildFoamRibbon/BuildRapids` (~160 lines) are gone, `RebuildEnvironment()` no longer calls `BuildRiverEffects()`, and the `ProceduralMeshComponent.h` include + `AddDoubleSidedQuad` helper were dropped from the ride actor. This matches the commit claim exactly.
2. **In-editor inspect confirms the runtime result, not just the source.** `yarlung-map-inspect.txt`: level `actor_count=3`; the ride actor (`YarlungCoasterRide`) now exposes only `TrainBody/LeftRail/RightRail/Ties/Supports/BoulderOutcrops` — no river components; and a separate actor `YarlungRiverScenery` (class `YarlungRiverActor`) owns `WaterMesh`/`FoamMesh` with `MID_M_YarlungRiverWater/Foam`. The forbidden-old-river gate is in place. Separation is verified at the asset level.
3. **The new water is sourced from the real DEM thalweg — not the track, not a synthetic curve.** `write_river_csv` emits Y from `river_guide[x_index]` (the naturalized-DEM thalweg also used for the masks) and Z from `sample_height_from_grid(heights, x, y) + 180cm` (DEM valley floor + 1.8m water surface). This is the **same truth source** as terrain and masks, fully **independent of the track centerline** — non-circular. CSV Z ≈ 272650cm (2726m) sits at the DEM valley floor (cross-check: track `terrain_z` ≈ 2729–2740m), as a thalweg should.
4. **`AYarlungRiverActor` is clean and self-contained.** Loads the CSV (header-skipped, ≥4-sample guard, error log on failure), builds a 5-across procedural water ribbon + 4-lane foam from each sample's `Center/HalfWidthCm/Flow`, applies `M_YarlungRiverWater/Foam` with a `M_CoasterTint` fallback. Uses the CSV's real per-sample Z, so the water tracks the actual valley floor.
5. **No over-claim — honest v1 scoping.** The water is plainly a flat transparent band: surface displacement is an 8cm `sin(Flow)` ripple, material is a simple transparent param set (no flow normals, refraction, or real turbulence). Codex states this is "v1 平带/简单透明材质, D3 不能标 Done" — correct, D3 stays open. The in-engine offscreen (`p4-river-actor-v2.png`, read) confirms: the teal water band is now visible hugging the valley floor directly under the track corridor (sourced consistently with track/terrain), while the scene is still greybox (green macro terrain) and the **right-cliff Landscape staircase is fully visible — shown, not hidden.**
6. **No 禁区 violation.** No SkyAtmosphere scattering touched; the staircase is left plainly visible (no color-grade/geometry masking to hide it); no premature Nanite cliff mesh. The "213 samples" claim is accurate (214 CSV lines − 1 header).

## Finding (non-blocking, record for stage C)

- **Two inconsistent "river" notions now coexist.** The new water actor uses the **real DEM thalweg** (Z ≈ 2726m, varying). But `ACoasterRideActor` still retains the **old synthetic river/terrain model** — `YarlungRiverCenterY(X)` (analytic sine centerline, anchor `95543,-142330`) and `RiverZCm = 265200` (2652m) — now used only to place **support footings** (`RiverZCm − 35`) and the **valley fog** plane (`RiverZCm + 70`). That synthetic river sits ~74m **below** the new DEM water surface and follows a different lateral path. This commit did not introduce it and did not claim to fix it (so it is **not** an over-claim), but it is a latent visual-correctness inconsistency: in stage C the support feet / fog should be re-anchored to the real DEM (sampled terrain or the river CSV), or they will visibly disagree with the real water/terrain. Worth tracking so the synthetic `YarlungLandscapeHeight`/`RiverZCm` scaffolding gets retired rather than silently persisting.

## What is NOT closed by this (correctly deferred)

- **D3 photoreal water** — real flow normals / refraction / reflection / turbulent foam, smoothed (non-segmented) banks. Still v1; NOT Done.
- **Near-cliff Landscape staircase** — stage C (render/material; corridor-level detail-normal → Nanite). Still visible, correctly deferred.
- **Support/fog re-anchoring to real DEM** — see Finding above; stage C.

## Agent Disposition

- Status: **Closed** — river water-rendering migration into a standalone DEM-thalweg-sourced scenery actor is reviewer-confirmed; ride actor decoupled from river rendering; claims honestly scoped.
- Next: D/P4 water material work (flow normals/foam rhythm/refraction-reflection, smoothed banks), then stage-C near-cliff staircase and synthetic-anchor retirement.
