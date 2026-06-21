# Codex Iteration Scaffold

This repo is expected to be iterated by Codex end to end. Use this page to pick
the cheapest valid loop for a change before editing visuals.

## Default Entry Point

Use `scripts/iterate-yarlung.ps1` for normal visual iterations:

```powershell
.\scripts\iterate-yarlung.ps1 -Mode Actor -Build -NamePrefix "iter-name" -Times 30,90,150 -ResX 1280 -ResY 720
```

The script runs the selected import loop, verifies the generated map, captures
first-person screenshots, analyzes them, writes a contact sheet, and records a
run manifest at `Saved\Diagnostics\<name>-run.json`.

Always open the contact sheet as an image before judging the result.

## Modes

| Mode | Use When | Terrain Mesh Rebuild |
|---|---|---|
| `Actor` | C++ actor placement, scenery actors, water actor, lighting, camera, postprocess | No |
| `Material` | Generated material graph or parameter changes | No |
| `Terrain` | Corridor terrain geometry, vertex color, displacement, Nanite mesh generation | Yes |
| `Full` | DEM/source asset generation, materials, model imports, and map need refresh | Yes |
| `ScreenshotOnly` | Inspect the current built map without importing | No |

Rules:
- Do not use `Terrain` or `Full` for actor-only work.
- Do not use `ScreenshotOnly` as validation after code changes that require a
  generated map update.
- If `Actor` or `Material` reports a missing corridor terrain asset, run
  `Terrain` once to rebuild the base mesh.

## Outputs To Record

Each iteration should put these in `docs/plans/photoreal-progress.md`:

- command line and mode
- `Saved\Diagnostics\<name>.png` contact sheet
- `Saved\Diagnostics\<name>-run.json` manifest
- worst-frame metrics from script output
- visual verdict after opening the contact sheet
- next concrete step

## Fast Loop Rationale

`SM_YarlungCorridorTerrain` is the slow artifact. Rebuilding it is only required
when terrain source data, terrain geometry, vertex colors, or terrain material
bindings change. Actor-only work can reuse the existing mesh via
`-SkipTerrainMeshBuild`; this keeps iteration focused on first-person visuals
instead of repeatedly baking unchanged geometry.
