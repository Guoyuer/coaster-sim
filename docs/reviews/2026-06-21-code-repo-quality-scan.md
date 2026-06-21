# Code / Repo Quality Scan — 2026-06-21

- Reviewer: Codex
- Scope: `Source/CoasterSim`, `scripts`, generated-asset worktree behavior
- Context: after the Yarlung pipeline cleanup and before the next visual asset pass

## Verdict

The repo is cleaner than the previous architecture review state. The severe split-brain terrain/track config issue is already fixed, `CoasterRideActor` has been split down, and legacy procedural river/canyon actors are gone.

There are still useful cleanup opportunities, but they should stay separate from visual iteration commits.

## Implemented in this pass

- Removed local ignored `scripts/__pycache__` noise so file scans do not surface stale bytecode.
- Refactored `ACoasterRideActor` command-line ride positioning: `StartRideFromCommandLine()` and batch screenshot positioning now share `ComputeAdvancedTrackRatio()`. This removes duplicated start-distance / speed / wraparound math without changing behavior.
- Validation: `git diff --check` PASS; C++ build PASS; `.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Quick -Build -SkipCapture -NamePrefix ride-start-helper-smoke` PASS. Generated `.umap` dirt from the smoke run was reverted.
- Added `.\scripts\iterate-yarlung.ps1 -RestoreGeneratedMap` for code-only smoke validation. It records generated-map dirty state in the manifest/handoff and restores `Content/Generated/YarlungLandscape/YarlungLandscape_Level.umap` only when it was clean before the run.
- Validation: script parse PASS; `git diff --check` PASS; `.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Quick -Build -SkipCapture -RestoreGeneratedMap -NamePrefix generated-map-restore-smoke` PASS; manifest recorded `dirty_before=false`, `dirty_after=true`, `restored=true`; worktree did not keep `.umap` dirty.
- Consolidated `YarlungSceneryActor` placement gates behind `TryResolvePlacement()`, so scatter rules and canopy belts share terrain bounds, river clearance, authored height, height-range, and slope checks while keeping their visual policy differences local.
- Validation: `git diff --check` PASS; C++ build PASS; `.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Quick -Build -SkipCapture -RestoreGeneratedMap -NamePrefix scenery-placement-helper-smoke` PASS; manifest `Saved\Diagnostics\scenery-placement-helper-smoke-run.json`; generated `.umap` was restored.
- Fixed the UE 5.8 `Rename` deprecation warning in `YarlungLandscapeImportCommandlet.cpp`: generated mesh replacement now explicitly calls `ResetLoaders(MeshPackage)` and uses `REN_AllowPackageLinkerMismatch` instead of deprecated `REN_ForceNoResetLoaders`.
- Validation: `git diff --check` PASS; C++ build PASS with no `Rename` warning; `.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Quick -Build -SkipCapture -RestoreGeneratedMap -NamePrefix rename-warning-fix-smoke` PASS; commandlet reported `Success - 0 error(s), 0 warning(s)`.

## Recommended next cleanup

### 1. Deepen scenery placement — first pass implemented

- Files: `Source/CoasterSim/YarlungSceneryActor.cpp`, `Source/CoasterSim/YarlungAssetConfig.*`
- Problem: `AddScatterRule()` and `AddCanopyBelt()` used to duplicate height sampling, bounds, river clearance, authored profile height, and slope gates. First pass has moved those gates into `TryResolvePlacement()`. Yaw, scale, density, and transform policies are still intentionally separate.
- Next solution: if scenery logic grows again, extract a scenery placement module that owns the shared terrain/river/profile sampling and returns accepted transforms. Keep rock/cliff/canopy style differences as small policy inputs.
- Benefit: locality for placement bugs; one test surface for river clearance and slope gates; easier to add PCG/foliage or whole-tree StaticMesh later.
- Recommendation: Partially done; defer the full module split until the next foliage/PCG change creates pressure for it.

### 2. Make the generated-map dirty contract explicit — implemented

- Files: `scripts/iterate-yarlung.ps1`, `scripts/import-yarlung-landscape.ps1`, `Content/Generated/YarlungLandscape/YarlungLandscape_Level.umap`
- Problem: Actor-mode smoke/import rewrites the tracked `.umap` even for code-only or config-only validation. This is expected but keeps creating dirty generated state that must be manually reverted when the source change is unrelated.
- Solution: implemented as explicit `-RestoreGeneratedMap` on `iterate-yarlung.ps1`. The script reports generated dirt separately and optionally restores the map for smoke-only runs.
- Benefit: cleaner worktree; fewer accidental generated commits; clearer agent handoff.
- Recommendation: Done.

### 3. Fix the UE 5.8 `Rename` deprecation warning — implemented

- Files: `Source/CoasterSim/YarlungLandscapeImportCommandlet.cpp`
- Problem: every build reported `Rename will no longer call ResetLoaders...` at the generated asset replacement path.
- Solution: implemented by explicitly resetting the mesh package loader before moving the old mesh object to transient, then using `REN_AllowPackageLinkerMismatch`.
- Benefit: build logs are quieter; future UE upgrades carry less risk.
- Recommendation: Done.

### 4. Split terrain mesh coloring from map import orchestration

- Files: `Source/CoasterSim/YarlungLandscapeImportCommandlet.cpp`
- Problem: the commandlet is no longer a god object, but terrain color/mask computation still sits beside package save/map actor orchestration.
- Solution: move terrain vertex color and wet-rock/ravine masks behind a small terrain surface module. Keep package creation and actor spawning in the commandlet.
- Benefit: locality for mountain material experiments; safer tests for color/mask changes without touching map import.
- Recommendation: Worth exploring.

### 5. Consolidate Python diagnostic geometry helpers

- Files: `scripts/verify-track-clearance.py`, `scripts/inspect-yarlung-spatial-contract.py`, `scripts/diagnose-yarlung-scenery-overlay.py`
- Problem: small vector math, normalization, and CSV-reading helpers are repeated across diagnostics.
- Solution: extend `scripts/yarlung_track_lib.py` or add a focused `scripts/yarlung_geometry.py` for shared diagnostic math.
- Benefit: less drift across diagnostics; easier to trust spatial contract reports.
- Recommendation: Speculative.

## Not worth doing right now

- Do not split `ACoasterRideActor` further just to make it smaller. After camera/capture/visuals/atmosphere extraction, the remaining actor mostly owns ride simulation and orchestration.
- Do not remove tracked `Content/Generated/*` wholesale. The repo intentionally tracks generated UE assets for reproducible local iteration; the problem is dirty-state handling, not the presence of generated assets.
- Do not spend cleanup time on visual constants until the asset path decision is resolved. That would mix architecture work with failed mountain tuning.
