# Fail-Close Setup Scan

- Date: 2026-06-21
- Reviewer: Codex
- Review target: Yarlung water/material/config/test pipeline after the "no visible water" bug
- Reviewed artifacts: `scripts/import-yarlung-landscape.ps1`, `scripts/create-coaster-materials.py`, `scripts/test-yarlung.ps1`, `Source/CoasterSim/YarlungAssetConfig.cpp`, `Source/CoasterSim/YarlungTerrainProfile.cpp`, `CoasterSim.uproject`, `Source/CoasterSim/Tests/YarlungCorridorProfileTests.cpp`
- Verdict: COMMENT
- Related phase/task: D water/track and cross-cutting repo setup gates

## Summary

The water visibility bug was not just a single rendering issue. It exposed a partially fail-open project setup: Unreal Python was invoked through multiple command-line shapes, config loaders could continue with empty/default values after setup errors, material generation used noisy missing-asset probes, stale plugin metadata remained after code cleanup, and Yarlung automation tests were not wrapped in a first-class repo gate.

This pass tightened the default path so missing config, broken material generation, stale Water plugin assumptions, and red Yarlung tests fail close instead of silently producing misleading screenshots.

## Blocking Issues

1. None remaining from this scan. The current source/build/test path is green.

## Non-Blocking Suggestions

1. Extend config validation from required top-level objects to required scalar fields with explicit field names.

## Required Next Action

Continue AAA visual iteration, but keep `scripts/test-yarlung.ps1` as the setup/test gate before commits that touch terrain, water, config, or generated map behavior.

## Agent Disposition

- Status: Done
- Progress link: `docs/plans/photoreal-progress.md`
- Follow-up evidence: C++ build PASS; `scripts/import-yarlung-landscape.ps1` material+verify PASS; `scripts/test-yarlung.ps1 -Build` PASS with 10 Yarlung automation tests; generated UE Water/material assets regenerated.

## Follow-Up Hardening

- `CoasterTrackComponent` now fails generated CSV load on unknown section names or invalid generated section/roll sample ranges, instead of silently falling back to `Coast`, zero bank, or zero terrain height.
- `YarlungLandscapeImportCommandlet` now fails map import when required world actors or UE Water fail to spawn.
- `YarlungWaterBuilder` now requires WaterZone, river material, and surface material; missing water materials no longer fall through to `AWaterBodyRiver` defaults.
- `inspect-yarlung-map.py` now uses `UnrealEditorSubsystem` / `EditorActorSubsystem`; map verify reaches `Success - 0 error(s), 0 warning(s)`.
- `import-yarlung-landscape.ps1` now requires Unreal commandlets/Python scripts to report `Success - 0 error(s)` in addition to process exit code 0.
- `create-coaster-materials.py` now uses `set_base_material_usage(...)` instead of deprecated material usage APIs, reuses existing material assets instead of deleting/recreating them, and reaches material generation `Success - 0 error(s), 0 warning(s)`.
- `inspect-yarlung-assets.py` now checks `does_asset_exist(...)` before loading optional assets, so missing local-only assets are reported as diagnostics instead of UE Error log noise.
