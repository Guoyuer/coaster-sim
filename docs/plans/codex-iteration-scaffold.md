# Codex Unattended Iteration Scaffold

This repo is expected to be iterated by Codex end to end. The target is not
"better than the prototype"; it is the first-person reference look in
`docs/refs/local/02_fp_yarlung_coaster_canyon.png` and
`docs/refs/local/03_fp_mountain_coaster_valley.png`, with palette/atmosphere
anchored by `docs/refs/local/01_yarlung_valley_river_blossom.jpeg`.

The harness goal is simple: a fresh Codex run should be able to discover state,
pick the cheapest valid loop, run it, leave evidence, and hand off without
guessing.

## Start Here

Every unattended session starts with:

```powershell
.\scripts\yarlung-agent-status.ps1
```

This prints the current branch, HEAD, dirty files, latest iteration manifests,
the top of `photoreal-progress.md`, and the recommended next command. Use
`-Json` when another script or agent needs machine-readable state.

The dirty list is grouped. Treat the groups differently:

- `source/docs/config`: normal implementation surface; stage only intentional
  files for the current task.
- `generated tracked outputs`: UE/generated assets under `Content/Generated/`
  or `SourceAssets/Generated/`; commit them only when the producing source,
  commandlet, material script, or explicit regenerated evidence is part of the
  same task.
- `local-only refs/clutter`: reference images and local-only files. Do not
  commit them.
- `other`: inspect manually before proceeding.

## Default Entry Point

Use `scripts/iterate-yarlung.ps1` for normal visual iterations:

```powershell
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Standard -Build -NamePrefix "iter-name"
```

The script runs the selected import loop, verifies the generated map, captures
first-person screenshots, analyzes them, writes a contact sheet, and records a
run manifest plus handoff note:

- `Saved\Diagnostics\<name>.png`
- `Saved\Diagnostics\<name>.csv`
- `Saved\Diagnostics\<name>.json`
- `Saved\Diagnostics\<name>-run.json`
- `Saved\Diagnostics\<name>-handoff.md`

Always open the contact sheet as an image before judging the result.

For code-only smoke runs that should verify Actor/Material import without
committing a regenerated binary map, add `-SkipCapture -RestoreGeneratedMap`:

```powershell
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Quick -Build -SkipCapture -RestoreGeneratedMap -NamePrefix "smoke-name"
```

`-RestoreGeneratedMap` only restores
`Content/Generated/YarlungLandscape/YarlungLandscape_Level.umap` when that file
was clean before the run. If the map was already dirty, the script leaves it
alone and records the skip reason in the manifest/handoff.

## Modes

| Mode | Use When | Terrain Mesh Rebuild |
|---|---|---|
| `Actor` | C++ actor placement, scenery actors, water actor, lighting, camera, postprocess | No |
| `Material` | Generated material graph or parameter changes | No |
| `Terrain` | Corridor terrain geometry, vertex color, displacement, Nanite mesh generation | Yes |
| `Full` | DEM/source asset generation, generated track, materials, model imports, and map need refresh | Yes |
| `ScreenshotOnly` | Inspect the current built map without importing | No |

Rules:
- Do not use `Terrain` or `Full` for actor-only work.
- Do not use `ScreenshotOnly` as validation after code changes that require a
  generated map update.
- If `Actor` or `Material` reports a missing corridor terrain asset, run
  `Terrain` once to rebuild the base mesh.

## Presets

Presets live in `Config/yarlung-iteration.json` so future Codex runs use the
same capture surface.

| Preset | Use When |
|---|---|
| `Quick` | Script/harness smoke. Not visual acceptance. |
| `Standard` | Default visual iteration. |
| `Route` | Track, camera, scenery visibility, or route-wide changes. |
| `Hero` | Pre-commit or reviewer-facing evidence. |
| `Final` | Stage exit validation only. Slow on purpose. |

Prefer `Standard` unless the change clearly needs a wider route or final-grade
survey. Do not burn `Final` on ordinary iteration.

## Outputs To Record

Each iteration should put these in `docs/plans/photoreal-progress.md`:

- command line, mode, and preset
- `Saved\Diagnostics\<name>.png` contact sheet
- `Saved\Diagnostics\<name>-run.json` manifest
- `Saved\Diagnostics\<name>-handoff.md` handoff note
- worst-frame metrics from script output
- `RiskGate=OK|WARN|FAIL|UNKNOWN`
- visual verdict after opening the contact sheet
- next concrete step

Keep `photoreal-progress.md` as a current dashboard, not a full transcript. Move
long historical scoring logs to `docs/plans/archive/` or the relevant
`docs/reviews/` file, then link them from the dashboard.

## Risk Gate

`iterate-yarlung.ps1` computes a cheap risk gate from the screenshot metrics.
It is a triage signal, not a replacement for looking at the image.

- `OK`: metrics are not obviously greybox-risky.
- `WARN`: likely visual problem; inspect before investing more.
- `FAIL`: likely still reads as greybox or low-quality terrain.
- `UNKNOWN`: no capture/analysis happened.

For the current project state, `FAIL` is expected. Do not hide it. Use it to
select the next high-ceiling change.

## Unattended Run Protocol

1. Run `.\scripts\yarlung-agent-status.ps1`.
2. Read `docs/plans/photoreal-progress.md` and any current reviewer note.
3. Make one scoped change.
4. Run `iterate-yarlung.ps1` with the cheapest correct mode and preset.
5. Open the contact sheet as an image.
6. Update `photoreal-progress.md` with the run evidence and verdict.
7. Commit/push only intentional files. Preserve unrelated dirty files.

Stop instead of continuing when:

- build/import/inspect fails
- contact sheet is missing or unreadable
- run manifest is missing
- the next change would require paid/login-gated assets
- dirty files make the intended commit ambiguous
- a `NEEDS-HUMAN` review block is unresolved

## Generated Asset Policy

Tracked generated assets are part of the runnable UE project, but they are not
free-for-all commit ballast. The intended tracked outputs are declared in
`Config/yarlung-iteration.json` under `generated_asset_policy`.

Commit generated outputs when:

- the task changed the generator, commandlet, material script, source model, or
  config that produced them
- the iteration evidence says the assets were regenerated intentionally
- the generated file is needed for another machine to run the same map

Do not commit generated outputs when:

- UE merely touched binary asset metadata during an unrelated smoke
- the output belongs to a failed visual experiment that was not accepted
- the status script still shows unrelated source/doc dirty files mixed into the
  same commit

For unrelated smoke validation, prefer `-RestoreGeneratedMap`. The run manifest
records `generated_map.restore_requested`, `dirty_before`, `dirty_after`, and
`restored` so the handoff explains why the tracked `.umap` did or did not stay
dirty.

## Fast Loop Rationale

`SM_YarlungCorridorTerrain` is the slow artifact. Rebuilding it is only required
when terrain source data, terrain geometry, vertex colors, or terrain material
bindings change. `YarlungTrack.csv` is now a first-class generated artifact in
the Full pipeline: source terrain/river changes must regenerate and verify the
track before map import. Actor-only work can reuse the existing mesh and track
via `-SkipTerrainMeshBuild` / `-SkipTrackGeneration`; this keeps iteration
focused on first-person visuals instead of repeatedly baking unchanged geometry.

## Current High-Ceiling Bias

The project should move toward real assetized scenery and believable camera
rendering, not more low-ceiling heightfield tweaks. In practical terms:

- use `Actor` mode for canyon-wall/forest/water/atmosphere/camera iterations
  while the terrain mesh is unchanged
- use `Material` mode for material graph or opacity/roughness/normal changes
- use `Terrain` only when the corridor mesh itself changes
- compare every meaningful visual run against L1-L3 local references
