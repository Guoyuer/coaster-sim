#!/usr/bin/env python3
"""Generate landscape-ready Yarlung Tsangpo canyon assets.

The raw heightmap is little-endian uint16 `.r16`, which Unreal Landscape can
import directly. TGA macro textures are imported by Unreal and drive the
landscape's broad color/roughness zones; PPM previews are dependency-free
sanity checks for shape, river width, forest massing, snow, and rock zones.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path


SIZE = 1009
MIN_X = -4200.0
MAX_X = 12200.0
MIN_Y = -10500.0
MAX_Y = 8200.0
RIVER_Z = 18.0
HEIGHT_MIN = -360.0
HEIGHT_MAX = 3900.0


def smooth01(value: float) -> float:
    t = min(1.0, max(0.0, value))
    return t * t * (3.0 - 2.0 * t)


def hash2(ix: int, iy: int, salt: int = 0) -> float:
    value = math.sin(ix * 127.1 + iy * 311.7 + salt * 74.7) * 43758.5453123
    return value - math.floor(value)


def value_noise(x: float, y: float, cell_size: float, salt: int = 0) -> float:
    gx = x / cell_size
    gy = y / cell_size
    x0 = math.floor(gx)
    y0 = math.floor(gy)
    tx = smooth01(gx - x0)
    ty = smooth01(gy - y0)
    a = hash2(x0, y0, salt)
    b = hash2(x0 + 1, y0, salt)
    c = hash2(x0, y0 + 1, salt)
    d = hash2(x0 + 1, y0 + 1, salt)
    return lerp(lerp(a, b, tx), lerp(c, d, tx), ty)


def river_center_y(x: float) -> float:
    return -1150.0 + 1050.0 * math.sin(x * 0.00048 + 0.7) + 420.0 * math.sin(x * 0.00115 - 0.6)


def terrain_height(x: float, y: float) -> float:
    river_y = river_center_y(x)
    lateral = abs(y - river_y)
    wide_valley = smooth01((lateral - 1150.0) / 9400.0)
    outer_mountain = smooth01((lateral - 5800.0) / 7600.0)
    cliff_band = smooth01((lateral - 3100.0) / 4200.0)
    long_ridge = 155.0 * math.sin(x * 0.00075 + y * 0.00018)
    fold_noise = 82.0 * math.sin(x * 0.0018 - y * 0.00072) + 46.0 * math.sin(x * 0.0034 + y * 0.0011)
    terraces = 58.0 * math.sin((lateral - 1200.0) * 0.0018 + x * 0.00042)

    height = (
        RIVER_Z
        - 22.0
        + wide_valley * 520.0
        + cliff_band * 1420.0
        + outer_mountain * 1900.0
        + long_ridge
        + fold_noise
        + terraces
    )
    if lateral < 980.0:
        channel = smooth01(lateral / 980.0)
        height = (RIVER_Z - 46.0) * (1.0 - channel) + (RIVER_Z + 34.0) * channel
    return height


def forest_amount(x: float, y: float, height: float) -> float:
    lateral = abs(y - river_center_y(x))
    forest_band = smooth01((lateral - 1700.0) / 1700.0) * (1.0 - smooth01((lateral - 9100.0) / 1900.0))
    forest_height = 1.0 - smooth01((height - 2150.0) / 760.0)
    patch_noise = 0.58 * value_noise(x, y, 1650.0, 2) + 0.42 * value_noise(x, y, 760.0, 7)
    return max(0.0, min(1.0, forest_band * forest_height * smooth01((patch_noise - 0.18) / 0.42)))


def masks(x: float, y: float, height: float) -> tuple[int, int, int, int]:
    lateral = abs(y - river_center_y(x))
    river = int(max(0.0, 1.0 - lateral / 1150.0) * 255.0)
    snow = int(smooth01((height - 2860.0) / 520.0) * 255.0)
    forest = int(forest_amount(x, y, height) * 255.0)
    rock = int(max(0.0, min(1.0, 0.24 + smooth01((height - 950.0) / 2150.0) - forest / 390.0)) * 255.0)
    return rock, forest, snow, river


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def rgb_for_preview(x: float, y: float, height: float) -> tuple[int, int, int]:
    rock, forest, snow, river = masks(x, y, height)
    if river > 8:
        return 12, min(255, 88 + river // 3), min(255, 102 + river // 3)
    if snow > 24:
        v = min(255, 158 + snow // 5)
        return v, min(255, v + 7), min(255, v + 5)
    if forest > 18:
        canopy = min(118, max(48, forest // 2 + 36))
        return 20, canopy, 34
    if abs(y - river_center_y(x)) < 2350.0:
        return 54, 76, 68
    shade = int(max(0.0, min(1.0, (height - 250.0) / 2900.0)) * 70.0)
    grain = int(12.0 * math.sin(x * 0.013 + y * 0.017) + 8.0 * math.sin(x * 0.029 - y * 0.011))
    return (
        max(0, min(255, 55 + shade // 2 + grain)),
        max(0, min(255, 59 + shade // 2 + grain)),
        max(0, min(255, 50 + shade // 3 + grain // 2)),
    )


def noise01(x: float, y: float) -> float:
    value = 0.55 * value_noise(x, y, 2200.0, 11) + 0.30 * value_noise(x, y, 980.0, 19) + 0.15 * value_noise(x, y, 420.0, 31)
    return max(0.0, min(1.0, value))


def blend_rgb(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    clamped = max(0.0, min(1.0, t))
    return (
        int(round(lerp(a[0], b[0], clamped))),
        int(round(lerp(a[1], b[1], clamped))),
        int(round(lerp(a[2], b[2], clamped))),
    )


def macro_albedo(x: float, y: float, height: float) -> tuple[int, int, int]:
    rock, forest, snow, river = masks(x, y, height)
    lateral = abs(y - river_center_y(x))
    n = noise01(x, y)

    alluvial = smooth01((2350.0 - lateral) / 1200.0) * (1.0 - river / 255.0)
    slope_wetness = 1.0 - smooth01((height - 1350.0) / 2100.0)
    base = blend_rgb((23, 71, 46), (52, 101, 63), n)

    rock_color = blend_rgb((58, 75, 72), (125, 139, 135), smooth01((height - 650.0) / 2550.0))
    forest_color = blend_rgb((12, 67, 31), (37, 111, 48), n)
    alluvial_color = blend_rgb((64, 97, 90), (112, 134, 119), n)
    snow_color = blend_rgb((184, 194, 196), (231, 239, 238), snow / 255.0)
    river_color = blend_rgb((43, 141, 148), (180, 216, 204), 0.38 + river / 510.0)

    color = blend_rgb(base, rock_color, rock / 430.0)
    color = blend_rgb(color, alluvial_color, alluvial * 0.58)
    color = blend_rgb(color, forest_color, forest / 255.0)
    color = blend_rgb(color, snow_color, snow / 255.0)
    color = blend_rgb(color, river_color, river / 255.0)

    humid_shade = 0.92 + 0.10 * slope_wetness + 0.05 * n
    return tuple(max(0, min(255, int(channel * humid_shade))) for channel in color)


def macro_roughness(x: float, y: float, height: float) -> tuple[int, int, int]:
    rock, forest, snow, river = masks(x, y, height)
    lateral = abs(y - river_center_y(x))
    alluvial = smooth01((2350.0 - lateral) / 1200.0) * (1.0 - river / 255.0)
    value = 204
    value = int(lerp(value, 174, rock / 255.0))
    value = int(lerp(value, 226, forest / 255.0))
    value = int(lerp(value, 128, alluvial))
    value = int(lerp(value, 92, river / 255.0))
    value = int(lerp(value, 188, snow / 255.0))
    value = max(42, min(235, value))
    return value, value, value


def write_ppm(path: Path, pixels: list[tuple[int, int, int]]) -> None:
    with path.open("wb") as f:
        f.write(f"P6\n{SIZE} {SIZE}\n255\n".encode("ascii"))
        for r, g, b in pixels:
            f.write(bytes((r, g, b)))


def write_tga(path: Path, pixels: list[tuple[int, int, int]], width: int, height: int) -> None:
    with path.open("wb") as f:
        f.write(struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, width, height, 24, 0x20))
        for r, g, b in pixels:
            f.write(bytes((b, g, r)))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="Content/Generated/YarlungLandscape")
    parser.add_argument("--texture-out", default="SourceAssets/Generated/YarlungLandscape")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    texture_out_dir = Path(args.texture_out)
    texture_out_dir.mkdir(parents=True, exist_ok=True)

    height_values: list[int] = []
    preview: list[tuple[int, int, int]] = []
    mask_preview: list[tuple[int, int, int]] = []
    macro_pixels: list[tuple[int, int, int]] = []
    macro_masks: list[tuple[int, int, int]] = []
    roughness_pixels: list[tuple[int, int, int]] = []
    stats = {"min_cm": 999999.0, "max_cm": -999999.0}

    for y_index in range(SIZE):
        v = y_index / (SIZE - 1)
        y = lerp(MIN_Y, MAX_Y, v)
        for x_index in range(SIZE):
            u = x_index / (SIZE - 1)
            x = lerp(MIN_X, MAX_X, u)
            height = terrain_height(x, y)
            stats["min_cm"] = min(stats["min_cm"], height)
            stats["max_cm"] = max(stats["max_cm"], height)
            encoded = int(max(0.0, min(1.0, (height - HEIGHT_MIN) / (HEIGHT_MAX - HEIGHT_MIN))) * 65535.0)
            height_values.append(encoded)
            preview.append(rgb_for_preview(x, y, height))
            rock, forest, snow, river = masks(x, y, height)
            mask_preview.append((river, forest, max(rock, snow)))
            macro_pixels.append(macro_albedo(x, y, height))
            macro_masks.append((river, forest, max(rock, snow)))
            roughness_pixels.append(macro_roughness(x, y, height))

    with (out_dir / "YarlungTsangpo_1009.r16").open("wb") as f:
        f.write(struct.pack("<" + "H" * len(height_values), *height_values))

    write_ppm(out_dir / "YarlungTsangpo_preview.ppm", preview)
    write_ppm(out_dir / "YarlungTsangpo_masks.ppm", mask_preview)
    write_tga(texture_out_dir / "YarlungTsangpo_macro_albedo.tga", macro_pixels, SIZE, SIZE)
    write_tga(texture_out_dir / "YarlungTsangpo_macro_masks.tga", macro_masks, SIZE, SIZE)
    write_tga(texture_out_dir / "YarlungTsangpo_macro_roughness.tga", roughness_pixels, SIZE, SIZE)

    manifest = {
        "name": "Yarlung Tsangpo cinematic canyon landscape",
        "heightmap": "YarlungTsangpo_1009.r16",
        "preview": "YarlungTsangpo_preview.ppm",
        "masks_preview": "YarlungTsangpo_masks.ppm",
        "macro_albedo": str(texture_out_dir / "YarlungTsangpo_macro_albedo.tga"),
        "macro_masks": str(texture_out_dir / "YarlungTsangpo_macro_masks.tga"),
        "macro_roughness": str(texture_out_dir / "YarlungTsangpo_macro_roughness.tga"),
        "resolution": [SIZE, SIZE],
        "format": "little-endian uint16 raw",
        "world_bounds_cm": {"min_x": MIN_X, "max_x": MAX_X, "min_y": MIN_Y, "max_y": MAX_Y},
        "height_range_cm": {"encoded_min": HEIGHT_MIN, "encoded_max": HEIGHT_MAX, **stats},
        "unreal_import": {
            "landscape_section_size": 63,
            "sections_per_component": 1,
            "component_count": "16 x 16",
            "xy_scale_cm": round((MAX_X - MIN_X) / (SIZE - 1), 3),
            "z_scale_note": "Set Z scale so the encoded 3950 cm range maps to the desired mountain relief.",
            "material_layers": ["River", "AlluvialSand", "DarkForest", "WeatheredRock", "Snow"],
            "nanite_followup": "Import high-poly rock and vegetation StaticMesh assets with bBuildNanite enabled; these generated masks define placement/material zones.",
        },
    }
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    (out_dir / "README.md").write_text(
        "# Yarlung Tsangpo Landscape Assets\n\n"
        "- `YarlungTsangpo_1009.r16`: Unreal Landscape heightmap import source.\n"
        "- `YarlungTsangpo_preview.ppm`: dependency-free color preview of the terrain.\n"
        "- `YarlungTsangpo_masks.ppm`: RGB mask preview where red=river, green=forest, blue=rock/snow.\n"
        "- `SourceAssets/Generated/YarlungLandscape/YarlungTsangpo_macro_albedo.tga`: UE-imported macro color map.\n"
        "- `SourceAssets/Generated/YarlungLandscape/YarlungTsangpo_macro_masks.tga`: UE-imported river/forest/rock-snow mask.\n"
        "- `SourceAssets/Generated/YarlungLandscape/YarlungTsangpo_macro_roughness.tga`: UE-imported roughness macro map.\n"
        "- `manifest.json`: import scale and layer notes.\n",
        encoding="utf-8",
    )
    print(f"Generated {out_dir / 'YarlungTsangpo_1009.r16'} and macro textures ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
