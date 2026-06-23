# Staircase Decisive-Cut Review — Soften + Full-Density Scatter (binary gate)

- Date: 2026-06-19
- Reviewer: independent external reviewer / human judge
- Review target: the "one decisive cut" for the staircase per the 2026-06-19 plan — commits `e6c8298` (replace cliff fascia with terrain softening + regen track) and `0cdf665` (add `AYarlungSceneryActor` full-density scatter). Secondary: `1d3bbbe`/`409aa7c` (river shading/opacity) reviewed in passing.
- Reviewed artifacts (read/run directly): both diffs; `Source/CoasterSim/YarlungSceneryActor.cpp` (mesh source + placement); `scripts/generate-yarlung-landscape-assets.py` (softening); `Saved/Diagnostics/yarlung-map-inspect.txt` (regenerated); **re-ran `scripts/verify-track-clearance.py` myself**; `Saved/OffscreenShots/c1-full-scatter-v1.png` (read); `photoreal-progress.md`; `manifest.json`.
- Verdict: **Infrastructure PASS (honest) / Visual gate FAIL — but the decisive bet was never actually run.** The softening+scatter pipeline is correct and ride-safe; the staircase is NOT suppressed. Crucially, the "real vegetation" bet was tested only with **boulder proxies** (no real tree/shrub assets exist locally), so technique failure vs asset wall cannot be separated. **No 注水 — Codex reported this straight.** The real decision (acquire assets vs fail-exit) is escalated to the user.

## What genuinely PASSED (independently verified)

1. **Fascia fully retired.** `YarlungCliffActor.cpp/.h` deleted (321+49 lines), `M_YarlungCliffRock.uasset` deleted, commandlet placement removed. The view-dependent skin is gone, as directed. inspect confirms no cliff actor.
2. **Softening pipeline ran in the correct order — ride not broken.** Terrain `.r16` regenerated → `YarlungTrack.csv` regenerated against it (94 rows changed) → clearance re-verified. **I re-ran `verify-track-clearance.py` myself: `violations=0`, `min_clearance=42.40m`, `min_radius=19.7m`, lat 1.01 / vert −0.89/3.07 / long 1.06, `stall=0`** — all in envelope, exactly matching Codex's reported numbers (not inflated). Softening did not desync the track or break 不穿山/comfort.
3. **Scatter infrastructure is real, placement is real.** New `AYarlungSceneryActor` samples the real `.r16` heightmap for height/normal and places two HISM groups (`RockOutcrops=2007`, `UnderstoryClumps=11000` = 13007 instances, ≥4000 gate). inspect: unique `YarlungForestRockScenery`, `actor_count=4`, no cliff actor. The placement is DEM-sourced, not arbitrary.
4. **Softening kept the gorge, didn't bulldoze it.** Manifest reports `affected_samples=45764, max_lowered≈308m, mean≈78m`; despite the large max, the screenshot still shows a recognizable deep canyon — the softening lowered near-vertical wall tops toward a slope rather than flattening the valley. Consistent with "naturalize, don't carve to track."

## What FAILED — and the honest reason

- **The staircase still dominates.** `c1-full-scatter-v1.png` (read): the right cliff still shows clear heightfield terracing at grazing angle; softening improved the data profile but did not remove the faceting, confirming the project's own conclusion that this is a **heightfield representation ceiling**, not a tunable bug. The discarded `lod0` experiment (forced LOD0 looked worse) corroborates it.
- **The scatter reads as noise, not forest.** Both `RockOutcrops` and `UnderstoryClumps` use the **same `boulder_01_1k` proxy mesh** tinted with `M_CoasterTint` — i.e. the 11000 "understory" instances are tinted boulders. In-frame they read as sparse dark blobs/dots on the slopes, not Linzhi dense forest. So the cut delivered full *instance count* but not *real vegetation*.
- **Therefore the decisive bet ("满密度真实散布") was not truly executed.** The user's own framing was "成败看下一刀真实散布 / 真实植被". What ran was full-density **proxy** scatter. It failed to read as forest — but that failure is at least as much an **asset wall** (no real tree/shrub/cliff-rock assets in-project) as a technique verdict. The two cannot be separated with current assets.

## Honesty assessment (the point of this role)

This was a high-temptation moment to inflate — 13007 instances + "forest scatter" could have been spun as "C progressing." Codex did the opposite: scored **D1=3, mean 2.0, C NOT Done**, wrote that "绿色 boulder proxy 读成暗块/噪点而非林芝密林", said the staircase still dominates, and **surfaced a NEEDS-HUMAN asset gate** rather than pass off proxies as vegetation. No 谎报, no 遮丑-and-claim-done. Reviewer-confirmed honest.

## The real fork (escalated to user)

The cheap-to-test version (proxy scatter) failed and reads as noise; the staircase is largely on **bare upper rock** that even a perfect forest may not cover; and the true bet (real dense vegetation) needs assets the project doesn't have. So:
- **Option A — acquire real vegetation/rock assets** (Fab/Megascans/CC0) and actually run the decisive scatter. The bet is genuinely untested; Yarlung walls are really forested, so this is legitimate content, not masking. Cost: asset acquisition/integration, and bare upper-rock faces may still staircase.
- **Option B — take the planned fail-exit now**: stop cliff iteration, move to D/E (water material / train-car / imaging), and record near-cliff 真根治 = **Nanite / mesh terrain (not Landscape)** as the deferred correct fix.

## Secondary (reviewed in passing, not the focus this round)

- `409aa7c` "Use river vertex alpha for water opacity" — addresses my prior non-blocking note (vertex alpha was dead); directionally correct. `1d3bbbe` "river energy shading" — D-axis polish. Both given a light pass only; flag for a fuller look if D becomes the active track.

## Agent Disposition

- Status: **Open (C blocked on assets / decision)** — infrastructure (softening + scatter + fascia removal) PASS and honest; visual gate FAIL; decisive real-vegetation bet untested for lack of assets.
- Next: user picks Option A (assets) or Option B (fail-exit to D/E). Do NOT mark C Done; do NOT iterate more heightfield/LOD (ceiling confirmed); do NOT pass proxy scatter off as forest.
