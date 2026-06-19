# Stage C v1 Review — Layered Detail Material + Corridor Cliff Mesh

- Date: 2026-06-19
- Reviewer: independent external reviewer / human judge
- Review target: two stage-C commits —
  - `e8007a3` "Add layered Yarlung landscape detail material" (C1 material groundwork).
  - `581ce5e` "Add generated Yarlung cliff mesh scenery" (C1 corridor cliff-mesh takeover v1).
- Reviewed artifacts (all read directly): full diffs; `create-coaster-materials.py` (detail-material + cliff-rock material); `Source/CoasterSim/YarlungCliffActor.cpp/.h` (full); `YarlungLandscapeImportCommandlet.cpp`; `scripts/inspect-yarlung-map.py`; `Saved/Diagnostics/yarlung-map-inspect.txt` (regenerated); `Saved/OffscreenShots/c1-landscape-detail-v2.png` + `c1-cliff-fascia-v1.png` (read); `photoreal-progress.md`.
- Verdict: **PASS (conditional) — both commits are honest; the cliff mesh uses the *sanctioned* approach (real-DEM corridor takeover), NOT cosmetic masking in spirit. But its v1 execution is a view-dependent skin, which is the boundary with 禁区 ②. Proceed toward replacement-fidelity OR take the user's stated D/E exit — do NOT iterate toward thicker occlusion.** C correctly NOT marked Done.

## e8007a3 — layered detail material (clean PASS, honest)

1. **Real material work, not a knob-twiddle disguised as progress.** `create_landscape_material()` now blends two PolyHaven PBR sets (`AerialGrassRock`/`LeafyGrass`) by the macro forest mask, wiring detail BaseColor/Normal/Roughness/AO on an independent detail-coordinate scale (`mapping_scale=140`) so micro-detail isn't bound to the whole-map macro UV. Build/import/inspect PASS; Landscape still fully on `M_YarlungLandscapeGround`, no actor regression.
2. **No 禁区 touch.** Confirmed by diff scan: nothing touches SkyAtmosphere scattering (禁区 ①), and there is no post-process / color-grade / tonemap / saturation change (would be cosmetic masking). Pure surface PBR.
3. **Honest negative result.** Codex states plainly that the near-cliff regular staircase is "几乎不动" and that this **proves detail-normal-only cannot fix the geometry staircase** — root cause is heightfield silhouette, not material. Correct conclusion, correctly leads to the mesh-takeover next step. Good groundwork; C not claimed Done.

## 581ce5e — corridor cliff mesh (the decisive review)

### Why it is NOT 禁区 ② in spirit (the saving facts)
- **It samples the real `.r16` DEM.** `SampleHeightCm()` bilinearly reads the actual heightmap at each vertex XY, so the cliff's *shape is faithful real terrain* — not an arbitrary wall/flat/decal. This is the single fact that separates a legitimate mesh takeover from cosmetic masking, and it holds.
- **It is the explicitly sanctioned stage-C path.** The decision record / 禁区 ③ named "Nanite cliff mesh 走廊级接管" as the correct answer once the cheap plays are spent; e8007a3 spent the cheap detail-normal play and proved it insufficient first. Order respected.
- **No deception.** 禁区 ② is fundamentally about *pretending the staircase is fixed by hiding it*. Codex does the opposite: calls it "一张偏大的岩壁表皮 (an oversized rock skin)", admits the mesh↔Landscape seam, says **"C 不能标 Done"**, and even pre-offers the exit (if it stays fake, skip the staircase line, go to D/E). Transparent, not concealed.
- Screenshot (`c1-cliff-fascia-v1.png`, read) confirms it doesn't even fully hide the staircase — facets are still visible at the waterline base. It is a partial corridor cover, not a sealed curtain.

### Why it's only conditional (the v1-skin concern — the boundary with 禁区 ②)
The mesh is built as a **view-dependent skin**, not a true geometric replacement, via three shortcuts I verified in code:
- `SurfaceLiftCm = 850` — the whole surface floats 8.5m proud of the real Landscape, so it sits *in front of* the staircase rather than *being* the corrected surface.
- `TrackSidePullCm = 5200` — vertices are pulled 52m toward the track from where their height was sampled, skewing the face toward the camera to fill the sightline.
- **Uniform per-row normals** — every vertex in an along-row is assigned one `(-Right*0.86 + Up*0.34)` track-facing normal, not real surface normals. The cliff lights as a billboard facing the track, not as 3D rock.

Net: it occludes the bad render from the *track viewpoint* rather than replacing the terrain. That is exactly the gravitational pull toward 禁区 ②. It does not cross the line now (real-DEM-sourced + honest + partial), but **the fix direction must be toward replacement fidelity** — sit closer to flush, compute real normals from the mesh, taper the seam, and ideally hide/cut the underlying Landscape strip so there is no curtain-over-staircase — **NOT** more lift / decorative blobs / opacity to bury more of the staircase. The latter would convert this into masking.

### Recommendation (endorses Codex's own gate)
One more iteration **only if** it buys real fidelity (real normals + seam taper + flushness make it read as geometry, not a skin). If that iteration still reads as "a different kind of fake," take the **user's stated exit**: accept that the heightfield staircase can't be cheaply killed in this corridor, stop polishing a skin, and move to **D/E (water material / vegetation / imaging)** where the marginal photoreal return is higher. Do not sink rounds into a view-dependent fascia.

## What is NOT closed (correctly deferred / open)

- **C / D1 / D5 photoreal cliff** — NOT Done. Current cliff is a v1 real-DEM-sourced but view-dependent skin (seams, uniform normals, floats proud). Needs real normals + flushness + seam handling to count as a true takeover, or an explicit decision to skip per the exit above.
- **Boulder synthetic terrain retirement (A3)** — still pending from the prior review (`YarlungRiverCenterY/YarlungLandscapeHeight`, boulder instances=0).
- **D water flow normal / depth fade**, vegetation, imaging — D/E.

## Agent Disposition

- Status: **Open (C in progress)** — detail material is honest groundwork (PASS); cliff mesh is the sanctioned approach at honest v1 (conditional PASS), not masking-in-spirit, but executed as a view-dependent skin.
- Next: push cliff toward replacement-fidelity (real normals, flushness, seam/Landscape-strip handling) for ONE gated iteration; if it still reads fake, take the D/E exit rather than thicken the occlusion. Do not mark C Done until the cliff reads as real geometry from off-track angles too.
