# Yarlung AAA Plan

## Goal

Build a first-person on-rails scenic coaster through a Yarlung Tsangpo / Linzhi
style canyon. A still frame should read as a real photo or film frame, not as a
prototype.

Visual anchors:

- Wet deep-green forest mass.
- Milky turquoise fast river in a deep canyon.
- Cold gray / gray-green wet rock walls.
- Blue sky, cloud bands, light mist, far snow-mountain silhouette.
- First-person coaster foreground: track, supports, and eventually car/cockpit.

## Current Architecture

- UE 5.8 project, default map:
  `/Game/Generated/YarlungLandscape/YarlungLandscape_Level`.
- Generated map and generated assets are rebuilt by commandlets. Do not
  hand-place persistent level actors; change the generator/config and rebuild.
- World scope is the first-person camera corridor, not a square full map.
- Track is a generated 5 km scenic loop. It may cross the river, climb high,
  dive, overbank, and reveal the valley; it is not constrained to follow the
  river.
- Terrain is a generated corridor StaticMesh base. It should become a hidden
  support layer in hero frames, not the main visible mountain surface.
- Water is a single explicit `SM_YarlungRiverSurface`; old UE WaterZone /
  WaterBodyRiver and old procedural river actors are not live.
- Scenery assets are configured in `Config/yarlung-assets.json`; missing paths or
  invalid contracts should fail loudly.
- Large local Fab/Megascans/PN assets may stay local and do not need to go to
  GitHub unless intentionally tracked.

## Active Pipeline

Use these commands instead of hand-splicing UE command lines:

```powershell
.\scripts\yarlung-agent-status.ps1
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Standard -Build -NamePrefix "iter-name"
```

Mode choice:

- `Actor`: scenery placement, lighting, camera, water actor/map changes.
- `Material`: generated material graph/parameter changes.
- `Terrain`: corridor terrain geometry, vertex colors, river surface geometry.
- `Full`: source asset, generated track, terrain, materials, and map all need a
  refresh.
- `ScreenshotOnly`: inspect current built map only; not valid after code/config
  changes that require regeneration.

Every visual iteration must open the contact sheet image before judging. Script
metrics are triage only.

## Current AAA Diagnosis

- The biggest blocker is not one more scalar parameter.
- The frame still reads as smooth corridor terrain with assets sitting on top.
- Belts/scatter are useful background fill, but they do not replace authored
  first-person composition.
- Rock-wall segments now provide cleaner geologic massing, but they are still
  overlapping instances rather than continuous authored cliff forms.
- Water has a cleaner Single Layer Water direction and better shore contact, but
  still lacks photo water: flow normals, reflection/depth variation, whitewater,
  wet shore breakup.
- Forest reads as individual assets in places, not as far canopy mass + mid
  whole trees + near understory.
- Foreground coaster hardware is still too proxy-like for AAA.

## Highest-Return Work Order

1. Systemic ground coverage: forest-floor, scree/talus, wet-rock shore, and
   canopy mass that hide exposed corridor terrain in hero frames.
2. Authored set-piece placement: named first-person ranges with explicit valley
   reveal, no-spawn sightlines, cliff anchors, river windows, and canopy blocks.
3. Continuous cliff/wall generation: group-level wall spline/planes or generated
   meshes that establish silhouettes before adding smaller rocks.
4. Photo water pass: flow normal, depth/shore fade, foam/whitewater breakup,
   reflection variation, and wet bank transition.
5. Foreground coaster pass: rail profile/material, support style, car nose /
   cockpit anchor, motion blur tuning.
6. Route/camera composition pass: high viewpoints, river crossings, overbanks,
   dives, and valley reveals should drive track choices.

Do not spend another iteration merely increasing HISM counts unless it directly
serves one of the above systems.

## Fail-Close Rules

- No silent fallback asset paths.
- No `assets.local.json` split for required project config.
- No old procedural canyon wall, old procedural river, UE WaterZone/WaterBodyRiver,
  PolyHaven live path, square full-map fallback, or old short-loop track fallback.
- If a visual result is unexpectedly bad, first test for pipeline/setup bugs:
  material wiring, actor visibility, asset load, height model, exposure, capture
  time, generation mode, and stale map.
- If a direction is rejected, record whether it failed because of `BUG`,
  `CONTENT-LIMIT`, or `DIRECTION-LIMIT`.
