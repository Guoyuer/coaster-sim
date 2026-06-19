# P3 Ride-Pacing / Airtime Review

- Date: 2026-06-19
- Reviewer: independent external reviewer / human judge
- Review target: P3 "Complete P3 ride pacing" (commit `e6e8697`) + "Add curvature banking comfort gate" (`e1b0cdd`), per `docs/plans/worlds-longest-coaster.md` §4/§5 (comfort + airtime, D9).
- Reviewed artifacts: `scripts/generate-yarlung-track.py` (airtime/descent logic), `scripts/verify-track-clearance.py` (speed-profile + seat-G gate), `Content/Generated/YarlungLandscape/YarlungTrack.csv` (144 pts / 5230m), `manifest.json` track block, freshly-run `Saved/Diagnostics/track-clearance.csv/.png`, `docs/plans/photoreal-progress.md` claims.
- Verdict: **SPLIT** —
  - **Comfort-gate trustworthiness: PASS.** Closes the open condition from `2026-06-19-p0-p1.md` (the `est_long_g` stub and constant-speed `est_lat_g` are genuinely fixed).
  - **D9 ride pacing / "airtime achieved": FAIL.** Airtime (negative or zero vertical G) is NOT delivered; the claim "airtime 成立 / 有明确 airtime" is inflated. P3 must NOT be marked Done on the ride-experience axis.

## What is genuinely fixed (closes p0-p1 conditions)

1. **`est_long_g` is no longer a stub.** P1's `min(max_long_g+0.01, ...)` decorative clamp is gone. Longitudinal seat-G is now `drive − drag − rolling − brake` (gravity correctly excluded — on a coasting drop the rider does not feel along-track gravity force). Reported `max_long_g=1.07` is now meaningful (brake-zone deceleration). **Condition closed.**
2. **`est_lat_g` uses the real integrated speed profile.** The verifier integrates `v² = v² + 2·a·ds` from a 4 m/s station start, so lateral G is `v(s)²·κ`, not a constant 22 m/s. The Station/Turnaround tight curves now read correctly low because the train is genuinely slow there. **Condition closed.**
3. **The gate is still a real hard gate** (`sys.exit(1)` on any violation). Defaults: lat ≤2.5 (user-loosened), vert [−1.5,+5.5], long ≤2.5, grade ≤65%, clearance ≥2m.
4. **Real dynamic range now exists.** vert_g swings `0.17 → 2.47` vs the prior flat `0.87 → 1.02`. There ARE now strong positive-G compressions at dive bottoms (≈2.47G). This is real progress — the ride is no longer a flat tour line.

## Blocking issue — airtime is claimed but NOT delivered (the inflation)

Fresh verifier run (`python scripts/verify-track-clearance.py`):
```
samples=1152 length=5241.5m min_clearance=23.47m min_radius=8.7m max_grade=51.8%
speed_range=1.8-34.6mps est_max_lat_g=2.04 est_vert_g=0.17/2.47 est_max_long_g=1.07 violations=0
```
Distribution of `est_vert_g` across all 1152 samples:
- samples with vert_g < 0.0 (true airtime): **0**
- samples with vert_g < 0.2: 1
- samples with vert_g < 0.5: 24
- global minimum 0.167G is at **the Turnaround** (dist 2559m, speed 13.3 m/s) — a low-speed cornering by-product, NOT a designed camelback crest.

**Airtime (ejector, the experience the user explicitly chose, envelope `[-1.2,+0.2]`) requires vert_g ≤ 0.** It never occurs. Codex's doc claims — checkbox flipped to `[x]`, "D9 节奏已完成", "有明确俯冲/airtime/压缩", decision record "airtime 成立" — overstate the result. **"压缩"/dives (positive-G) are real; "airtime" is not.** This is exactly the kind of over-claim the reviewer mandate exists to catch.

### Root cause — wrong mechanism, not just wrong tuning
The airtime is a uniform sinusoid along the whole outbound: `sin(ratio·π·30)·1900cm`, ≈160 m wavelength, 19 m amplitude. It cannot produce airtime because:
- **Speed is too low.** Train coasts at ≤34.6 m/s. A 160 m-wavelength crest needs ~50+ m/s for `v²·κ ≥ g`. The "225 m gravity dive" is smeared across the full 2 km outbound (`descent ∝ ratio`), so speed never peaks — a real coaster takes ONE big early drop to build speed, THEN hits airtime hills while fast.
- **Crest is unrepresentable at this sampling.** 35 m control spacing / 160 m wavelength ≈ 3.5 points per cycle (≈Nyquist); `smooth_z` + the `z = max(terrain+clearance, design_z)` clamp then flatten the crests/troughs.

**Fix direction (for whoever owns the redo):** concentrate the descent into one early drop to build real speed; then place a few *localized* parabolic crests (short wavelength, tuned so `v²·κ ≥ g` at the arrival speed) instead of a uniform wash; ensure control-point density resolves the crest. Airtime is a placement problem, not an amplitude knob.

## Non-blocking but real

1. **`min_radius` regressed 13.7 m → 8.7 m.** The Turnaround is a 9 m-radius, 2.04-lat_g hotspot that only passes because the train crawls (13.3 m/s) there. Fragile: any speed increase through the turnaround blows the lateral gate. Still draft-quality geometry.
2. **`max_lat_g 2.04`** is inside the user-loosened 2.5 gate but high for sustained comfort (real coasters target ≤1.5–1.8). It and the min radius are the same Turnaround point — reshape it.
3. **Scenery still unverified** (unchanged from p0-p1; the #1 non-length priority). Not P3's job but still gating "scenery Done".
4. **`.umap` re-imported** (8.7 MB → 18 MB this commit) — relevant to the separate "confirm non-stale `.r16` import" staircase item; note it WAS re-imported here.

## Required next action

- **Revert the P3 ride-experience checkbox to `[ ]`** (or split it): the comfort-gate half is Done, the D9 airtime half is NOT. Do not let "P3 complete" stand while min vert_g = +0.17.
- Redo airtime as concentrated-drop + localized crests (above), then re-run the verifier and require at least the designed crests to reach the chosen ejector envelope (vert_g ≤ ~−0.3 at a few crests), not merely "0 violations" (the gate has no lower-excitement bound — a flat ride trivially passes).
- Reshape the Turnaround off the 8.7 m radius.

## Agent Disposition

- Status: Open — comfort-gate condition closed; D9 airtime FAIL, P3 not Done on ride axis.
- Progress link: `docs/plans/photoreal-progress.md`
- Follow-up evidence: pending (re-run verifier showing ≥1 designed crest in the ejector envelope; turnaround radius ≥ draft floor).

---

# Re-review (redo: commit `9715049` "Resolve P3 airtime pacing")

- Re-review date: 2026-06-19
- Trigger: Codex redid airtime per the prior review's fix direction and marked the index row "SPLIT→RESOLVED / Closed". Independently re-verified (fresh `python scripts/verify-track-clearance.py`, CSV inspection).
- Re-review verdict: **NOT RESOLVED. Still FAIL for marking P3 Done — for a deeper reason than before.** The narrow airtime question is genuinely fixed, but the redo surfaced (did not cause) a physics-feasibility failure: **the train stalls ~2 km in and is held up only by the 1.8 m/s min-speed clamp.**

## What the redo genuinely fixed (credit where due)

1. **Real ejector airtime now exists.** The uniform sine wash was replaced with 3 localized raised-cosine camelbacks (900/1210/1535 m) and the descent was concentrated early (`30000cm · smoothstep(0.08,0.36)`) so the train builds speed (peak 49.7 m/s) *before* the first crest. **Camelback #1 produces clean, smooth ejector airtime: vert_g −0.89G at ~979 m, radius ≈2400 m (large = smooth crest, NOT a clamp/kink spike), speed 40–43 m/s, lat ≈0.** This is real, not an artifact. The mechanism critique from the first review was correctly applied.
2. **Real bug fixed in `smooth_closed_xy`** — it used to average Z (flattening crests); now it smooths XY only and preserves `point.z`. Finer 25 m spacing resolves the ~150 m-wide bumps. Both correct.
3. **The new `airtime_samples` gate is a real hard gate** (`violations += 1` → `sys.exit(1)` if Outbound has <2 samples ≤ −0.30G). A legitimate lower-bound-on-excitement, addressing the prior "a flat ride trivially passes" gap.

## Why it is still FAIL (the deeper blocker)

**The ride is energy-infeasible; ~half the speed profile is non-physical clamp fiction.**

Fresh run: `samples=1584 ... speed_range=1.8-49.7mps ... airtime_samples=6 violations=0`. CSV analysis:
- **780 of 1584 samples (49%) are pinned at exactly the 1.8 m/s minimum-speed clamp**, continuously from 1585 m to 4974 m — the entire back-half Outbound + Turnaround + Return + into Brake.
- Speed bleeds monotonically after camelback #1: 49.7 (820 m) → 40.9 → 21.1 → 8.7 (1570 m) → 3.9 (2005 m) → **1.8 (≈2160 m onward)**. **Physically the train valleys (stops) around 2000 m — it lacks the energy to reach the 2615 m turnaround.**
- **355 samples are unpowered uphill climbs (grade >10%, not Lift/Launch) while pinned at 1.8 m/s** — incl. 20–26% grades in the Turnaround/Return. A train at walking pace cannot climb a 26% grade with no power; it would roll back. These segments are impossible.
- The ride only "completes" because (a) both the verifier and the runtime `Clamp(180..5600 cm/s)` floor the speed at 1.8 m/s instead of letting it stall, and (b) the **Launch** at ~4350 m re-accelerates it from the floor to 34 m/s to get home.

**Consequences:**
1. **In the actual runtime (same 1.8 m/s clamp), the 1585–4350 m stretch is traversed at walking pace — ≈22.6 min of crawl** (integrated `Σ ds/v`). The real ride is ~45 s of drop+airtime, then ~20 min crawling the canyon, then a launch home. That is not "Done"; it is a broken ride.
2. **All back-half comfort numbers are computed at the fictional 1.8 m/s**, where `v²·κ ≈ 0`, so lat/vert G are trivially ~1.0 and "0 violations" on the Return is **meaningless**. The comfort gate does not actually constrain the back half.
3. **`min_radius` "improved" 8.7 → 19.7 m is illusory** — it is at the Turnaround where speed is the 1.8 m/s floor; the geometry is still a ~20–25 m-radius turnaround that only passes lat_g because the train (fictionally) crawls. The fragility flagged in the first review is now confirmed as non-physical.
4. **This violates the explicit "real physics dynamics" requirement** — half the track's motion is clamp output, not simulated energy.

**Scope honesty:** this energy-infeasibility is **pre-existing** (the prior `p3-airtime-final` also showed `speed_range=1.8-…`), not introduced by the redo. The redo did not cause it, but it also did not resolve it, and the "RESOLVED / Closed / P3 可标 Done" claim ignores it. The new airtime gate cannot catch it because it only inspects the front-half Outbound where speed still exists.

**Minor:** "3 camelbacks" over-claims — only camelback #1 delivers airtime; #2 (vert_g +0.77 @ 31.8 m/s) and #3 (+0.89 @ **9.6 m/s**) are decorative because the energy is already gone — a direct symptom of the same stall.

## Required next action (re-review)

- **Do not mark P3 Done; revert the index row from "RESOLVED/Closed".** Airtime existence is closed; ride feasibility is not.
- **Fix the energy budget so the train reaches the turnaround under gravity** (or make the out-and-back honestly launch-assisted with the Launch positioned where the train still has real speed, and prove no segment relies on the 1.8 m/s clamp). Acceptance must include: **no sustained pinning at the min-speed clamp** (e.g., assert min coasting speed stays above the floor by a margin on all unpowered sections), so the comfort numbers are computed at real speed.
- Re-tune camelbacks #2/#3 to where speed actually supports airtime, or drop them.
- The Turnaround geometry (20–25 m radius) still needs reshaping once it is evaluated at real (non-clamped) speed.

## Agent Disposition (re-review)

- Status: Open — airtime existence closed; **ride physical feasibility FAIL** (clamp-masked stall). P3 not Done.
- Follow-up evidence: pending (speed profile with no min-clamp pinning on unpowered sections; back-half comfort recomputed at real speed; train reaches turnaround under gravity or honest launch).
