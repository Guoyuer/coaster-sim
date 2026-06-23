# AGENTS.md - Yarlung AAA Operating Guide

Your job in this repo is to keep pushing the first-person Yarlung scenic coaster
toward photo/film-level AAA quality.

## Core Target

- First-person on-rails coaster only. No free-camera deliverable.
- Yarlung/Linzhi look: deep green forest, milky turquoise river, wet gray-green
  rock, blue sky/cloud bands/mist, far snow-mountain silhouette.
- A still frame should read as a real photo or film frame, not as "better than
  the prototype."

## Live Docs

Read only these by default:

| File | Purpose |
|---|---|
| `docs/plans/photoreal-progress.md` | Current status, latest evidence, next task |
| `docs/plans/photoreal-plan.md` | Active product, visual, and architecture plan |
| `docs/specs/photoreal-acceptance.md` | Scoring and done definition |
| `docs/refs/README.md` | Reference image notes |

Everything under `docs/archive/` and `docs/reviews/archive/` is historical. Do
not read it during normal iteration unless a live doc points to a specific file
or you are explicitly doing archaeology.

## Commands

Engine:

- Editor: `C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe`
- Build: `C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat`
- Project: `CoasterSim.uproject`
- Default map: `/Game/Generated/YarlungLandscape/YarlungLandscape_Level`

Build C++:

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" CoasterSimEditor Win64 Development "-Project=$PWD\CoasterSim.uproject" -WaitMutex -NoHotReloadFromIDE
```

Default iteration:

```powershell
.\scripts\yarlung-agent-status.ps1
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Standard -Build -NamePrefix "iter-name"
```

Full asset/map rebuild:

```powershell
.\scripts\import-yarlung-corridor.ps1 -Build
```

Mode choice:

- `Actor`: scenery placement, lighting, camera, postprocess, map actor rebuild.
- `Material`: generated material changes.
- `Terrain`: corridor terrain or river-surface geometry/vertex changes.
- `Full`: source assets, generated track, terrain, materials, and map.
- `ScreenshotOnly`: inspect current built map only.

## Generated Map Rule

The `.umap` is generated. Persistent level changes must be made in generator
code/config and rebuilt. Do not hand-place durable actors in the editor and call
that done.

## Iteration Loop

1. Read `docs/plans/photoreal-progress.md`.
2. Pick the next task from `docs/plans/photoreal-plan.md`.
3. Make one scoped systemic change.
4. Run the cheapest correct build/import loop.
5. Open the generated contact sheet image and judge visually.
6. Update `photoreal-progress.md` concisely.
7. Commit/push intentional files only.

If the visual result is bad, first ask whether this is a pipeline/setup bug:
stale map, wrong mode, missing asset, material wiring, exposure, actor visibility,
height model, water contact, or capture time. Do not reject a high-ceiling
direction until those are ruled out.

## Hard Rules

- No silent fallbacks for required assets/config.
- No `assets.local.json` for project-critical config.
- No old procedural canyon wall, old procedural river actor, UE
  WaterZone/WaterBodyRiver, PolyHaven live path, square full-map fallback, or old
  short-loop track fallback.
- No one-off visual patch lists when a systemic placement/material/composition
  method is needed.
- Do not add permanent scenery to `ACoasterRideActor`; scenery belongs in the
  generated map/scenery pipeline.
- Do not judge by metrics alone. Always open screenshots/contact sheets.
- Do not keep live docs as transcripts. Archive stale detail.

## Human Gates

Mark `NEEDS-HUMAN` in `photoreal-progress.md` instead of guessing when work
requires:

- paid/login-gated assets,
- subjective selection of final hero/reference shots,
- uncertain external DEM/source-data alignment.

## Commit Convention

One coherent task per commit. Use:

```text
Co-Authored-By: Claude <noreply@anthropic.com>
```
