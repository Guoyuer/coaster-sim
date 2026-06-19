# P4 Scenery Overlay / River-Mask Alignment Review

- Date: 2026-06-19
- Reviewer: independent external reviewer / human judge
- Review target: P4 scenery diagnostic + river-mask alignment — commits `4696897` (overlay diagnostic), `97526f9` (compare vs DEM thalweg), `57dd99d` (align river mask to thalweg).
- Reviewed artifacts: `scripts/diagnose-yarlung-scenery-overlay.py`, `scripts/generate-yarlung-landscape-assets.py` (river-guide change), `Saved/Diagnostics/yarlung-scenery-overlay.png` + `-crop.png` + `.csv` (all read), `Saved/OffscreenShots/p4-river-mask-narrow.png` (read), `manifest.json`.
- Verdict: **PASS (reviewer-confirmed) — closes the long-standing "scenery unverified" Open item at the geometric/alignment level.** The track demonstrably hugs the real DEM valley/river corridor; the future water mask is now aligned to the same real thalweg. The photoreal visual ("贴江如画" in-engine) is correctly NOT claimed and remains for stages C/D/E.

## Why it passes (independently checked)

1. **Overlay is now legible (the original blocker).** All three images were read directly. The hillshade is clear, the cyan river band sits in the valley bottom, and the colored track line is distinguishable per section — a real human/reviewer judgment is now possible, unlike the prior washed-out overlay.
2. **Track genuinely hugs the real river corridor.** CSV spot-check of all 198 control points vs the DEM thalweg: offsets **71–149 m (median 117, p90 148), ZERO fly-aways** (0 points >200 m). Per-section offsets equal the design offsets exactly — Outbound 72 m, Return/Launch/Brake 148 m, Turnaround 147 m, Lift 72 m. The whole 5 km out-and-back stays within ~150 m of the valley bottom and follows the river bends (visible in the crop). This is a riverside route, not a wander.
3. **The validation is NOT circular.** The "DEM thalweg" is computed by `extract_thalweg(heightfield)` from the DEM elevation (valley bottom), independent of the track CSV. The fix moved the **water mask** (macro-mask center offset 670 m → 137 m), and the **DEM-thalweg offset is unchanged** — proving the track was not nudged to fake alignment; the previously-misplaced cyan water was moved onto the real channel. River-mask coverage 89/198 (44.9%) → 187/198 (94.4%).
4. **River mask narrowed honestly.** Half-width 700 m → 260 m so the water no longer floods the whole valley floor/cliffs; the cyan now traces the channel.
5. **No over-claim.** The in-engine offscreen (`p4-river-mask-narrow.png`) is still greybox: green-tinted macro terrain, fake flat water, and the right-cliff Landscape staircase clearly visible. Codex explicitly states the visual is still macro fake-water + staircase and defers real water body/material to C/D. Honest scoping — the geometric alignment is closed; the photoreal visual is not claimed.
6. **Ride physics preserved.** P3 verifier re-run still PASS (`violations=0`) — the river-guide/mask change did not move the track or disturb the powered-coaster result.

## What is NOT closed by this (correctly deferred)

- **Photoreal "贴江如画" in-engine** — real water body + refraction/flow (stage D), terrain materials (stage C), and the near-cliff Landscape staircase (stage C/render) are all still TODO. "Scenery Done" overall is NOT achievable yet; only the **alignment/overlay blocker** and the **geometric river-following** are confirmed.

## Agent Disposition

- Status: **Closed** — scenery overlay/alignment blocker resolved (reviewer-confirmed). The track hugs the real river; water mask aligned to real thalweg.
- Next: stages C/D visual work (real water material, terrain materials, near-cliff staircase), then human judgment of the actual photoreal riverside look.
