# Coaster Sim Context

## Visual Target

The canyon setting is the Yarlung Tsangpo Grand Canyon / Nyingchi region, not an American Southwest red-rock canyon.

Use these visual anchors:

- Broad, humid Himalayan canyon scale.
- Dense deep-green vegetation on slopes and valley shelves.
- Milky white, turquoise, or blue-green turbulent river water.
- Cool gray and gray-green rock, wet stone, alluvial banks, and cliff faces.
- Cloud bands, haze, and distant snow mountains around Namcha Barwa-style terrain.
- Clear daytime lighting with blue sky and white clouds unless a task explicitly asks for different weather.

Avoid these as default canyon cues:

- Dominant red/orange desert rock.
- Dry badlands color palettes.
- Flat single-color slopes.
- Ad hoc visual overlays that hide the real terrain/material problem.

## Visual Iteration Rule

If a visual result looks poor or unrealistic, first reassess whether the current approach has enough upside before doing more small tweaks. Prefer high-ceiling methods that can approach reality:

- DEM/r16-derived terrain data feeding the generated corridor StaticMesh.
- Proper terrain material layers, masks, satellite/albedo references, and imported assets.
- Nanite rock and vegetation assets where useful.
- Root-cause diagnosis of the actual rendered actor/material/texture.

Do not keep iterating low-ceiling fixes such as superficial color nudges, temporary cover meshes, or one-off geometry patches when the underlying asset/material pipeline is wrong.

## 3A Visual Pipeline

The target quality bar is photo-real / 3A-style, not prototype-good. The visual pipeline should be built around high-ceiling production methods:

- The visible terrain is the generated `SM_YarlungCorridorTerrain` StaticMesh, not UE `ALandscape`. DEM/r16 data remains the offline source for terrain scale, silhouettes, river masks, forest, rock, alluvial bank, snow, and wetness zones.
- The terrain material must be layered. Macro color/coverage decides what the mountain looks like from far away; scanned PBR textures provide close-range detail normals, roughness, AO, and breakup.
- Do not use a single PBR photo texture as the whole mountain base color. It creates visible tiling, wrong regional color, and low-end “texture test” visuals.
- Use Nanite-ready rock assets for cliff faces, boulders, outcrops, and riverbank detail. Scatter them with slope/height/river masks rather than hand-placed ad hoc props.
- Use foliage/PCG-style vegetation for dense Nyingchi/Yarlung greenery. Basic engine cones, cubes, or spheres are not acceptable vegetation placeholders for final visual direction.
- Use Lumen/sky atmosphere/volumetric fog/daylight setup as a first-class visual layer: blue daytime sky, white clouds, humid haze, snow mountains, and river mist.
- Use cinematic camera validation. Every visual iteration should be checked from multiple ride positions, including first-person views that show the train, track, supports, river, canyon width, and distant terrain.

Current implementation decisions:

- CC0 scanned PBR assets are allowed and preferred for material detail, but they must be used as part of a layered material stack.
- Temporary geometry that reduces realism should be cut quickly, even if it makes the scene sparser temporarily.
- The correct next asset steps are Nanite rock/vegetation scatter and corridor terrain masks, not more hand-authored basic-shape clutter.
