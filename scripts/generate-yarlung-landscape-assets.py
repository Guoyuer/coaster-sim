#!/usr/bin/env python3
"""Generate landscape-ready Yarlung Tsangpo canyon heightmap assets.

The raw heightmap is little-endian uint16 `.r16`, which Unreal Landscape can
import directly. PPM previews are intentionally dependency-free.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path


SIZE = 505
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
    forest_band = smooth01((lateral - 3300.0) / 1300.0) * (1.0 - smooth01((lateral - 7800.0) / 1500.0))
    forest_height = 1.0 - smooth01((height - 1650.0) / 550.0)
    patch_noise = 0.5 + 0.32 * math.sin(x * 0.0021 + y * 0.0013) + 0.18 * math.sin(x * 0.0053 - y * 0.0031)
    return max(0.0, min(1.0, forest_band * forest_height * smooth01((patch_noise - 0.28) / 0.42)))


def masks(x: float, y: float, height: float) -> tuple[int, int, int, int]:
    lateral = abs(y - river_center_y(x))
    river = int(max(0.0, 1.0 - lateral / 1150.0) * 255.0)
    snow = int(smooth01((height - 2860.0) / 520.0) * 255.0)
    forest = int(forest_amount(x, y, height) * 255.0)
    rock = int(max(0.0, min(1.0, 0.35 + smooth01((height - 650.0) / 1900.0) - forest / 510.0)) * 255.0)
    return rock, forest, snow, river


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def rgb_for_preview(x: float, y: float, height: float) -> tuple[int, int, int]:
    rock, forest, snow, river = masks(x, y, height)
    if river > 8:
        return 0, min(255, 105 + river // 2), min(255, 150 + river // 2)
    if snow > 24:
        v = min(255, 188 + snow // 4)
        return v, min(255, v + 8), min(255, v + 2)
    if forest > 18:
        return 18, max(58, forest), 28
    if abs(y - river_center_y(x)) < 2350.0:
        return 135, 114, 78
    shade = int(max(0.0, min(1.0, (height - 250.0) / 2900.0)) * 70.0)
    return 94 + shade, 80 + shade, 58 + shade


def write_ppm(path: Path, pixels: list[tuple[int, int, int]]) -> None:
    with path.open("wb") as f:
        f.write(f"P6\n{SIZE} {SIZE}\n255\n".encode("ascii"))
        for r, g, b in pixels:
            f.write(bytes((r, g, b)))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="Content/Generated/YarlungLandscape")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    height_values: list[int] = []
    preview: list[tuple[int, int, int]] = []
    mask_preview: list[tuple[int, int, int]] = []
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

    with (out_dir / "YarlungTsangpo_505.r16").open("wb") as f:
        f.write(struct.pack("<" + "H" * len(height_values), *height_values))

    write_ppm(out_dir / "YarlungTsangpo_preview.ppm", preview)
    write_ppm(out_dir / "YarlungTsangpo_masks.ppm", mask_preview)

    manifest = {
        "name": "Yarlung Tsangpo cinematic canyon landscape",
        "heightmap": "YarlungTsangpo_505.r16",
        "preview": "YarlungTsangpo_preview.ppm",
        "masks_preview": "YarlungTsangpo_masks.ppm",
        "resolution": [SIZE, SIZE],
        "format": "little-endian uint16 raw",
        "world_bounds_cm": {"min_x": MIN_X, "max_x": MAX_X, "min_y": MIN_Y, "max_y": MAX_Y},
        "height_range_cm": {"encoded_min": HEIGHT_MIN, "encoded_max": HEIGHT_MAX, **stats},
        "unreal_import": {
            "landscape_section_size": 63,
            "sections_per_component": 1,
            "component_count": "8 x 8",
            "xy_scale_cm": round((MAX_X - MIN_X) / (SIZE - 1), 3),
            "z_scale_note": "Set Z scale so the encoded 3950 cm range maps to the desired mountain relief.",
            "material_layers": ["River", "AlluvialSand", "DarkForest", "WeatheredRock", "Snow"],
            "nanite_followup": "Import high-poly rock and vegetation StaticMesh assets with bBuildNanite enabled; these generated masks define placement/material zones.",
        },
    }
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    (out_dir / "README.md").write_text(
        "# Yarlung Tsangpo Landscape Assets\n\n"
        "- `YarlungTsangpo_505.r16`: Unreal Landscape heightmap import source.\n"
        "- `YarlungTsangpo_preview.ppm`: dependency-free color preview of the terrain.\n"
        "- `YarlungTsangpo_masks.ppm`: RGB mask preview where red=river, green=forest, blue=rock/snow.\n"
        "- `manifest.json`: import scale and layer notes.\n",
        encoding="utf-8",
    )
    print(f"Generated {out_dir / 'YarlungTsangpo_505.r16'} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
