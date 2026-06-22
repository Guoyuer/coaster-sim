#!/usr/bin/env python3
"""Generate landscape-ready Yarlung Tsangpo canyon assets.

The raw heightmap is little-endian uint16 `.r16`, which Unreal Landscape can
import directly. TGA macro textures are imported by Unreal and drive the
landscape's broad color/roughness zones; PPM previews are dependency-free
sanity checks for shape, river width, forest massing, snow, and rock zones.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import struct
import urllib.request
from pathlib import Path

from PIL import Image

from yarlung_config import (
    HEIGHT_MAX,
    HEIGHT_MIN,
    MAX_X,
    MAX_Y,
    MIN_X,
    MIN_Y,
    RIVER_MASK_HALF_WIDTH_CM,
    SIZE,
    river_center_y,
    smooth01,
)


# World georeferencing, encoded height range, and river anchors/mask now come
# from Config/yarlung-terrain.json via yarlung_config (imported above) so the C++
# mesh builder and this asset pipeline share one source of truth.
NATURALIZE_STEEP_TERRAIN = True
DEM_WEST_LON = 94.945
DEM_EAST_LON = 95.015
DEM_SOUTH_LAT = 29.745
DEM_NORTH_LAT = 29.820
DEM_CACHE_DIR = Path("SourceAssets/DEM/CopernicusGLO30")
DEFAULT_TRACK_GUIDE_PATH = Path("Content/Generated/YarlungLandscape/YarlungTrack.csv")
DEM_TILE_URL = (
    "https://copernicus-dem-30m.s3.amazonaws.com/"
    "Copernicus_DSM_COG_10_{lat_prefix}{lat:02d}_00_{lon_prefix}{lon:03d}_00_DEM/"
    "Copernicus_DSM_COG_10_{lat_prefix}{lat:02d}_00_{lon_prefix}{lon:03d}_00_DEM.tif"
)


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


def grid_x_to_world(x_index: float) -> float:
    return lerp(MIN_X, MAX_X, x_index / (SIZE - 1))


def grid_y_to_world(y_index: float) -> float:
    return lerp(MIN_Y, MAX_Y, y_index / (SIZE - 1))


def world_x_to_grid(x: float) -> float:
    return (x - MIN_X) / (MAX_X - MIN_X) * (SIZE - 1)


def build_dem_river_guide(heights: list[float], search_radius: int = 70) -> list[float]:
    first_x = SIZE // 2
    previous_y = min(
        range(int(SIZE * 0.12), int(SIZE * 0.88)),
        key=lambda y: heights[y * SIZE + first_x],
    )

    center_y = [float(previous_y)] * SIZE
    for x in range(first_x, SIZE):
        y_min = max(2, previous_y - search_radius)
        y_max = min(SIZE - 3, previous_y + search_radius)
        previous_y = min(
            range(y_min, y_max + 1),
            key=lambda y: heights[y * SIZE + x] + abs(y - previous_y) * 45.0,
        )
        center_y[x] = float(previous_y)

    previous_y = int(round(center_y[first_x]))
    for x in range(first_x - 1, -1, -1):
        y_min = max(2, previous_y - search_radius)
        y_max = min(SIZE - 3, previous_y + search_radius)
        previous_y = min(
            range(y_min, y_max + 1),
            key=lambda y: heights[y * SIZE + x] + abs(y - previous_y) * 45.0,
        )
        center_y[x] = float(previous_y)

    smoothed = center_y
    for _ in range(3):
        next_y = []
        for x in range(SIZE):
            start = max(0, x - 5)
            end = min(SIZE, x + 6)
            next_y.append(sum(smoothed[start:end]) / (end - start))
        smoothed = next_y
    return [grid_y_to_world(value) for value in smoothed]


def guided_river_center_y(x: float, river_guide: list[float] | None) -> float:
    if not river_guide:
        return river_center_y(x)
    gx = max(0.0, min(SIZE - 1.0, world_x_to_grid(x)))
    x0 = int(math.floor(gx))
    x1 = min(SIZE - 1, x0 + 1)
    return lerp(river_guide[x0], river_guide[x1], gx - x0)


def sample_height_from_grid(heights: list[float], x: float, y: float) -> float:
    gx = max(0.0, min(SIZE - 1.0, world_x_to_grid(x)))
    gy = max(0.0, min(SIZE - 1.0, (y - MIN_Y) / (MAX_Y - MIN_Y) * (SIZE - 1)))
    x0 = int(math.floor(gx))
    y0 = int(math.floor(gy))
    x1 = min(SIZE - 1, x0 + 1)
    y1 = min(SIZE - 1, y0 + 1)
    tx = gx - x0
    ty = gy - y0

    def at(ix: int, iy: int) -> float:
        return heights[iy * SIZE + ix]

    a = lerp(at(x0, y0), at(x1, y0), tx)
    b = lerp(at(x0, y1), at(x1, y1), tx)
    return lerp(a, b, ty)


def write_river_csv(path: Path, heights: list[float], river_guide: list[float] | None) -> None:
    if not river_guide:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    step = 4
    half_width_cm = 22000.0
    with path.open("w", newline="", encoding="ascii") as handle:
        writer = csv.writer(handle)
        writer.writerow(["idx", "x", "y", "z", "half_width_cm", "flow"])
        row_index = 0
        for x_index in range(int(SIZE * 0.08), int(SIZE * 0.92) + 1, step):
            x = grid_x_to_world(float(x_index))
            y = river_guide[x_index]
            z = sample_height_from_grid(heights, x, y) + 180.0
            flow = row_index / max(1.0, ((SIZE * 0.84) / step))
            writer.writerow(
                [
                    row_index,
                    f"{x:.3f}",
                    f"{y:.3f}",
                    f"{z:.3f}",
                    f"{half_width_cm:.3f}",
                    f"{flow:.6f}",
                ]
            )
            row_index += 1


def height01(height: float) -> float:
    return smooth01((height - HEIGHT_MIN) / (HEIGHT_MAX - HEIGHT_MIN))


def copernicus_tile_name(lon: int, lat: int) -> str:
    lat_prefix = "N" if lat >= 0 else "S"
    lon_prefix = "E" if lon >= 0 else "W"
    return f"Copernicus_DSM_COG_10_{lat_prefix}{abs(lat):02d}_00_{lon_prefix}{abs(lon):03d}_00_DEM.tif"


def cubic_bspline_interp(p0: float, p1: float, p2: float, p3: float, t: float) -> float:
    one_minus_t = 1.0 - t
    w0 = one_minus_t * one_minus_t * one_minus_t / 6.0
    w1 = (3.0 * t * t * t - 6.0 * t * t + 4.0) / 6.0
    w2 = (-3.0 * t * t * t + 3.0 * t * t + 3.0 * t + 1.0) / 6.0
    w3 = t * t * t / 6.0
    return p0 * w0 + p1 * w1 + p2 * w2 + p3 * w3


class CopernicusDem:
    def __init__(self, cache_dir: Path) -> None:
        self.cache_dir = cache_dir
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self.tiles: dict[tuple[int, int], Image.Image] = {}

    def tile_path(self, lon: int, lat: int) -> Path:
        return self.cache_dir / copernicus_tile_name(lon, lat)

    def fetch_tile(self, lon: int, lat: int) -> Image.Image:
        key = (lon, lat)
        if key in self.tiles:
            return self.tiles[key]

        path = self.tile_path(lon, lat)
        if not path.exists():
            lat_prefix = "N" if lat >= 0 else "S"
            lon_prefix = "E" if lon >= 0 else "W"
            url = DEM_TILE_URL.format(
                lat_prefix=lat_prefix,
                lat=abs(lat),
                lon_prefix=lon_prefix,
                lon=abs(lon),
            )
            request = urllib.request.Request(url, headers={"User-Agent": "coaster-sim-dem-generator"})
            with urllib.request.urlopen(request, timeout=180) as response:
                path.write_bytes(response.read())

        image = Image.open(path)
        self.tiles[key] = image
        return image

    def sample_cm(self, lon: float, lat: float) -> float:
        tile_lon = math.floor(lon)
        tile_lat = math.floor(lat)
        image = self.fetch_tile(tile_lon, tile_lat)
        width, height = image.size
        x = (lon - tile_lon) * (width - 1)
        y = (tile_lat + 1.0 - lat) * (height - 1)
        base_x = math.floor(x)
        base_y = math.floor(y)
        tx = x - base_x
        ty = y - base_y

        def pixel(px: int, py: int) -> float:
            clamped_x = max(0, min(width - 1, px))
            clamped_y = max(0, min(height - 1, py))
            return float(image.getpixel((clamped_x, clamped_y)))

        rows = []
        neighborhood = []
        for row_offset in (-1, 0, 1, 2):
            row = [pixel(base_x + col_offset, base_y + row_offset) for col_offset in (-1, 0, 1, 2)]
            neighborhood.extend(row)
            rows.append(cubic_bspline_interp(row[0], row[1], row[2], row[3], tx))
        meters = cubic_bspline_interp(rows[0], rows[1], rows[2], rows[3], ty)
        meters = max(min(neighborhood), min(max(neighborhood), meters))
        return meters * 100.0


def world_to_lon_lat(x: float, y: float) -> tuple[float, float]:
    u = (x - MIN_X) / (MAX_X - MIN_X)
    v = (y - MIN_Y) / (MAX_Y - MIN_Y)
    return lerp(DEM_WEST_LON, DEM_EAST_LON, u), lerp(DEM_SOUTH_LAT, DEM_NORTH_LAT, v)


def build_copernicus_height_grid(cache_dir: Path) -> tuple[list[float], dict[str, object]]:
    dem = CopernicusDem(cache_dir)
    values: list[float] = []
    stats = {"min_cm": 999999999.0, "max_cm": -999999999.0}
    for y_index in range(SIZE):
        v = y_index / (SIZE - 1)
        y = lerp(MIN_Y, MAX_Y, v)
        for x_index in range(SIZE):
            u = x_index / (SIZE - 1)
            x = lerp(MIN_X, MAX_X, u)
            lon, lat = world_to_lon_lat(x, y)
            height = dem.sample_cm(lon, lat)
            values.append(height)
            stats["min_cm"] = min(stats["min_cm"], height)
            stats["max_cm"] = max(stats["max_cm"], height)

    metadata = {
        "source": "Copernicus DEM GLO-30 public COG tiles on AWS Open Data",
        "source_url": "https://registry.opendata.aws/copernicus-dem/",
        "tile_url_template": DEM_TILE_URL,
        "bbox_wgs84": {
            "west_lon": DEM_WEST_LON,
            "east_lon": DEM_EAST_LON,
            "south_lat": DEM_SOUTH_LAT,
            "north_lat": DEM_NORTH_LAT,
        },
        "cache_dir": str(cache_dir),
        "raw_height_range_cm": {
            "min": round(stats["min_cm"], 2),
            "max": round(stats["max_cm"], 2),
        },
        "encoded_height_range_cm": {
            "min": HEIGHT_MIN,
            "max": HEIGHT_MAX,
        },
        "note": "Heights are kept in real-world centimeters and clipped only at encode time.",
    }
    return values, metadata


def box_blur_height_grid(values: list[float], radius: int, passes: int) -> list[float]:
    current = list(values)
    width = SIZE
    height = SIZE
    window = radius * 2 + 1

    for _ in range(passes):
        horizontal = [0.0] * len(current)
        for y in range(height):
            row = y * width
            total = 0.0
            for offset in range(-radius, radius + 1):
                total += current[row + min(width - 1, max(0, offset))]
            for x in range(width):
                horizontal[row + x] = total / window
                remove_x = max(0, x - radius)
                add_x = min(width - 1, x + radius + 1)
                total += current[row + add_x] - current[row + remove_x]

        vertical = [0.0] * len(current)
        for x in range(width):
            total = 0.0
            for offset in range(-radius, radius + 1):
                sample_y = min(height - 1, max(0, offset))
                total += horizontal[sample_y * width + x]
            for y in range(height):
                vertical[y * width + x] = total / window
                remove_y = max(0, y - radius)
                add_y = min(height - 1, y + radius + 1)
                total += horizontal[add_y * width + x] - horizontal[remove_y * width + x]

        current = vertical

    return current


def relax_height_slopes(values: list[float], max_slope: float, passes: int) -> list[float]:
    current = list(values)
    dx_cm = (MAX_X - MIN_X) / (SIZE - 1)
    dy_cm = (MAX_Y - MIN_Y) / (SIZE - 1)
    max_x_delta = dx_cm * max_slope
    max_y_delta = dy_cm * max_slope

    for _ in range(passes):
        next_values = list(current)
        for y in range(SIZE):
            row = y * SIZE
            for x in range(SIZE):
                index = row + x
                if x + 1 < SIZE:
                    relax_pair(next_values, current[index], current[index + 1], index, index + 1, max_x_delta)
                if y + 1 < SIZE:
                    relax_pair(next_values, current[index], current[index + SIZE], index, index + SIZE, max_y_delta)
        current = [max(HEIGHT_MIN, min(HEIGHT_MAX, height)) for height in next_values]

    return current


def relax_pair(
    target: list[float],
    height_a: float,
    height_b: float,
    index_a: int,
    index_b: int,
    max_delta: float,
) -> None:
    diff = height_b - height_a
    excess = abs(diff) - max_delta
    if excess <= 0.0:
        return

    direction = 1.0 if diff > 0.0 else -1.0
    correction = excess * 0.32
    target[index_a] += direction * correction
    target[index_b] -= direction * correction


def naturalize_height_grid(heights: list[float]) -> list[float]:
    if not NATURALIZE_STEEP_TERRAIN:
        return heights

    broad = box_blur_height_grid(heights, radius=18, passes=3)
    conditioned: list[float] = []
    for index, height in enumerate(heights):
        x = index % SIZE
        y = index // SIZE
        neighbor_relief = 0.0
        if x > 0:
            neighbor_relief = max(neighbor_relief, abs(height - heights[index - 1]))
        if x < SIZE - 1:
            neighbor_relief = max(neighbor_relief, abs(height - heights[index + 1]))
        if y > 0:
            neighbor_relief = max(neighbor_relief, abs(height - heights[index - SIZE]))
        if y < SIZE - 1:
            neighbor_relief = max(neighbor_relief, abs(height - heights[index + SIZE]))

        steep_weight = smooth01((neighbor_relief - 420.0) / 1800.0)
        blend = 0.18 + 0.62 * steep_weight
        conditioned.append(lerp(height, broad[index], blend))

    return relax_height_slopes(conditioned, max_slope=0.78, passes=8)


def read_outbound_softening_polyline(path: Path) -> list[tuple[float, float]]:
    if not path.exists():
        return []

    points: list[tuple[float, float]] = []
    with path.open("r", newline="", encoding="ascii") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            if row.get("section") == "Outbound":
                points.append((float(row["x"]), float(row["y"])))
    return points


def build_softening_segments(points: list[tuple[float, float]]):
    segments = []
    for start, end in zip(points, points[1:]):
        ax, ay = start
        bx, by = end
        vx = bx - ax
        vy = by - ay
        length_sq = vx * vx + vy * vy
        if length_sq < 1.0:
            continue
        length = math.sqrt(length_sq)
        segments.append(
            {
                "ax": ax,
                "ay": ay,
                "vx": vx,
                "vy": vy,
                "length_sq": length_sq,
                "nx": -vy / length,
                "ny": vx / length,
                "min_x": min(ax, bx),
                "max_x": max(ax, bx),
                "min_y": min(ay, by),
                "max_y": max(ay, by),
            }
        )
    return segments


def nearest_softening_frame(x: float, y: float, segments, search_radius_cm: float):
    best = None
    best_distance_sq = search_radius_cm * search_radius_cm
    for segment in segments:
        if (
            x < segment["min_x"] - search_radius_cm
            or x > segment["max_x"] + search_radius_cm
            or y < segment["min_y"] - search_radius_cm
            or y > segment["max_y"] + search_radius_cm
        ):
            continue

        rel_x = x - segment["ax"]
        rel_y = y - segment["ay"]
        t = max(0.0, min(1.0, (rel_x * segment["vx"] + rel_y * segment["vy"]) / segment["length_sq"]))
        px = segment["ax"] + segment["vx"] * t
        py = segment["ay"] + segment["vy"] * t
        dx = x - px
        dy = y - py
        distance_sq = dx * dx + dy * dy
        if distance_sq < best_distance_sq:
            signed_lateral = dx * segment["nx"] + dy * segment["ny"]
            best_distance_sq = distance_sq
            best = (px, py, signed_lateral, math.sqrt(distance_sq))
    return best


def soften_outbound_cliff_slope(
    heights: list[float],
    track_path: Path,
) -> tuple[list[float], dict[str, object]]:
    points = read_outbound_softening_polyline(track_path)
    if len(points) < 3:
        return heights, {
            "enabled": False,
            "reason": f"missing or insufficient Outbound track guide: {track_path}",
        }

    segments = build_softening_segments(points)
    if not segments:
        return heights, {"enabled": False, "reason": "no valid Outbound softening segments"}

    inner_cm = 5000.0
    outer_cm = 112000.0
    fade_out_start_cm = 88000.0
    max_slope = 0.45
    base_lift_cm = 2400.0
    next_heights = list(heights)
    affected_weights = [0.0] * len(heights)
    affected_count = 0
    total_lowered = 0.0
    max_lowered = 0.0

    for y_index in range(SIZE):
        y = grid_y_to_world(float(y_index))
        for x_index in range(SIZE):
            x = grid_x_to_world(float(x_index))
            frame = nearest_softening_frame(x, y, segments, outer_cm)
            if frame is None:
                continue

            px, py, _signed_lateral, distance_cm = frame
            if distance_cm < inner_cm or distance_cm > outer_cm:
                continue

            index = y_index * SIZE + x_index
            base_height = sample_height_from_grid(heights, px, py)
            target_height = base_height + base_lift_cm + max(0.0, distance_cm - inner_cm) * max_slope
            original_height = heights[index]
            if original_height <= target_height:
                continue

            side_weight = smooth01((distance_cm - inner_cm) / 36000.0)
            side_weight *= 1.0 - smooth01((distance_cm - fade_out_start_cm) / (outer_cm - fade_out_start_cm))
            if side_weight <= 0.0:
                continue

            lowered_height = lerp(original_height, target_height, side_weight)
            lowered = original_height - lowered_height
            next_heights[index] = lowered_height
            affected_weights[index] = side_weight
            affected_count += 1
            total_lowered += lowered
            max_lowered = max(max_lowered, lowered)

    smoothed = next_heights
    for _ in range(2):
        current = list(smoothed)
        for y_index in range(1, SIZE - 1):
            for x_index in range(1, SIZE - 1):
                index = y_index * SIZE + x_index
                weight = affected_weights[index] * 0.22
                if weight <= 0.0:
                    continue
                neighbor_average = (
                    current[index - 1]
                    + current[index + 1]
                    + current[index - SIZE]
                    + current[index + SIZE]
                ) * 0.25
                smoothed[index] = lerp(current[index], neighbor_average, weight)

    return smoothed, {
        "enabled": True,
        "track_guide": str(track_path),
        "source": "previous generated Outbound track guide; track CSV is regenerated after terrain generation",
        "inner_cm": inner_cm,
        "outer_cm": outer_cm,
        "max_slope_rise_per_run": max_slope,
        "base_lift_cm": base_lift_cm,
        "affected_samples": affected_count,
        "max_lowered_cm": round(max_lowered, 3),
        "mean_lowered_cm": round(total_lowered / max(1, affected_count), 3),
        "purpose": "Replace the grazing-angle DEM-derived cliff wall with a corridor-mesh-friendly natural slope instead of a fascia mesh.",
    }


def forest_amount(x: float, y: float, height: float, river_y: float | None = None) -> float:
    center_y = river_center_y(x) if river_y is None else river_y
    lateral = abs(y - center_y)
    range_cm = HEIGHT_MAX - HEIGHT_MIN
    forest_band = smooth01((lateral - 26000.0) / 65000.0) * (1.0 - smooth01((lateral - 520000.0) / 130000.0))
    forest_height = 1.0 - smooth01((height - (HEIGHT_MIN + range_cm * 0.45)) / (range_cm * 0.20))
    patch_noise = 0.58 * value_noise(x, y, 1650.0, 2) + 0.42 * value_noise(x, y, 760.0, 7)
    return max(0.0, min(1.0, forest_band * forest_height * smooth01((patch_noise - 0.18) / 0.42)))


def masks(x: float, y: float, height: float, river_y: float | None = None) -> tuple[int, int, int, int]:
    center_y = river_center_y(x) if river_y is None else river_y
    lateral = abs(y - center_y)
    h01 = height01(height)
    river = int(max(0.0, 1.0 - lateral / RIVER_MASK_HALF_WIDTH_CM) * 255.0)
    snow = int(smooth01((h01 - 0.72) / 0.10) * 255.0)
    forest = int(forest_amount(x, y, height, center_y) * 255.0)
    rock = int(max(0.0, min(1.0, 0.24 + smooth01((h01 - 0.25) / 0.50) - forest / 390.0)) * 255.0)
    return rock, forest, snow, river


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def rgb_for_preview(x: float, y: float, height: float, river_y: float | None = None) -> tuple[int, int, int]:
    center_y = river_center_y(x) if river_y is None else river_y
    rock, forest, snow, river = masks(x, y, height, center_y)
    if river > 8:
        return 12, min(255, 88 + river // 3), min(255, 102 + river // 3)
    if snow > 24:
        v = min(255, 158 + snow // 5)
        return v, min(255, v + 7), min(255, v + 5)
    if forest > 18:
        canopy = min(118, max(48, forest // 2 + 36))
        return 20, canopy, 34
    if abs(y - center_y) < 150000.0:
        return 54, 76, 68
    shade = int(height01(height) * 70.0)
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


def write_ppm(path: Path, pixels: list[tuple[int, int, int]]) -> None:
    with path.open("wb") as f:
        f.write(f"P6\n{SIZE} {SIZE}\n255\n".encode("ascii"))
        for r, g, b in pixels:
            f.write(bytes((r, g, b)))


def write_hillshade(path: Path, heights: list[float]) -> None:
    pixels: list[int] = []
    min_h = min(heights)
    max_h = max(heights)
    for y in range(SIZE):
        for x in range(SIZE):
            left = heights[y * SIZE + max(0, x - 1)]
            right = heights[y * SIZE + min(SIZE - 1, x + 1)]
            down = heights[min(SIZE - 1, y + 1) * SIZE + x]
            up = heights[max(0, y - 1) * SIZE + x]
            slope = abs(right - left) + abs(down - up)
            shade = 0.55 + 0.45 * ((heights[y * SIZE + x] - min_h) / max(max_h - min_h, 1.0))
            edge = min(1.0, slope / 16000.0)
            pixels.append(int(max(0.0, min(1.0, shade * 0.62 + edge * 0.38)) * 255.0))
    image = Image.new("L", (SIZE, SIZE))
    image.putdata(pixels)
    image.save(path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="Content/Generated/YarlungLandscape")
    parser.add_argument("--dem-cache", default=str(DEM_CACHE_DIR))
    parser.add_argument("--track-guide", default=str(DEFAULT_TRACK_GUIDE_PATH))
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = out_dir / "manifest.json"
    previous_manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}

    source_heights, source_metadata = build_copernicus_height_grid(Path(args.dem_cache))
    source_heights = naturalize_height_grid(source_heights)
    source_heights, outbound_softening = soften_outbound_cliff_slope(source_heights, Path(args.track_guide))
    source_metadata["naturalization"] = {
        "enabled": NATURALIZE_STEEP_TERRAIN,
        "purpose": "Preserve broad Yarlung canyon shapes while reshaping near-vertical 30m DEM cliffs into ride-scale natural slopes for first-person photorealism.",
        "broad_blur": "box blur radius=18, passes=3, steep-relief weighted blend",
        "slope_relaxation": "max_slope=0.78, passes=8",
        "outbound_cliff_softening": outbound_softening,
    }

    river_guide = build_dem_river_guide(source_heights)
    if river_guide:
        source_metadata["river_mask"] = {
            "centerline": "DEM thalweg extracted from the naturalized height grid",
            "search_radius_px": 70,
            "smoothing": "moving average radius=5, passes=3",
            "purpose": "Keep macro river/water masks aligned with the valley floor and generated coaster route.",
        }

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
            data_index = y_index * SIZE + x_index
            height = source_heights[data_index]
            river_y = guided_river_center_y(x, river_guide)
            stats["min_cm"] = min(stats["min_cm"], height)
            stats["max_cm"] = max(stats["max_cm"], height)
            encoded = int(max(0.0, min(1.0, (height - HEIGHT_MIN) / (HEIGHT_MAX - HEIGHT_MIN))) * 65535.0)
            height_values.append(encoded)
            preview.append(rgb_for_preview(x, y, height, river_y))
            rock, forest, snow, river = masks(x, y, height, river_y)
            mask_preview.append((river, forest, max(rock, snow)))

    with (out_dir / "YarlungTsangpo_1009.r16").open("wb") as f:
        f.write(struct.pack("<" + "H" * len(height_values), *height_values))

    write_ppm(out_dir / "YarlungTsangpo_preview.ppm", preview)
    write_ppm(out_dir / "YarlungTsangpo_masks.ppm", mask_preview)
    write_river_csv(out_dir / "YarlungRiver.csv", source_heights, river_guide)
    write_hillshade(out_dir / "YarlungTsangpo_hillshade.png", [
        lerp(HEIGHT_MIN, HEIGHT_MAX, value / 65535.0) for value in height_values
    ])

    manifest = {
        "name": "Yarlung Tsangpo cinematic canyon landscape",
        "heightmap": "YarlungTsangpo_1009.r16",
        "river_csv": "YarlungRiver.csv",
        "preview": "YarlungTsangpo_preview.ppm",
        "masks_preview": "YarlungTsangpo_masks.ppm",
        "resolution": [SIZE, SIZE],
        "format": "little-endian uint16 raw",
        "source": "copernicus",
        "dem": source_metadata,
        "world_bounds_cm": {"min_x": MIN_X, "max_x": MAX_X, "min_y": MIN_Y, "max_y": MAX_Y},
        "height_range_cm": {"encoded_min": HEIGHT_MIN, "encoded_max": HEIGHT_MAX, **stats},
        "unreal_import": {
            "landscape_section_size": 63,
            "sections_per_component": 1,
            "component_count": "16 x 16",
            "xy_scale_x_cm": round((MAX_X - MIN_X) / (SIZE - 1), 3),
            "xy_scale_y_cm": round((MAX_Y - MIN_Y) / (SIZE - 1), 3),
            "z_scale": round((HEIGHT_MAX - HEIGHT_MIN) / 512.0, 3),
            "z_scale_note": "Encoded 16-bit heights map to the real-scale 2600m-7300m elevation window.",
            "material_layers": ["River", "AlluvialSand", "DarkForest", "WeatheredRock", "Snow"],
            "nanite_followup": "Import high-poly rock and vegetation StaticMesh assets with bBuildNanite enabled; these generated masks define placement/material zones.",
        },
        "river": {
            "path": "Content/Generated/YarlungLandscape/YarlungRiver.csv",
            "source": "DEM thalweg extracted from naturalized height grid",
            "half_width_cm": 22000.0,
            "water_z_offset_cm": 180.0,
        },
    }
    if "track" in previous_manifest:
        manifest["track"] = previous_manifest["track"]
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    (out_dir / "README.md").write_text(
        "# Yarlung Tsangpo Landscape Assets\n\n"
        "- `YarlungTsangpo_1009.r16`: Unreal Landscape heightmap import source.\n"
        "- `YarlungTsangpo_preview.ppm`: dependency-free color preview of the terrain.\n"
        "- `YarlungTsangpo_hillshade.png`: real-scale DEM hillshade preview for bbox/orientation checks.\n"
        "- `YarlungTsangpo_masks.ppm`: RGB mask preview where red=river, green=forest, blue=rock/snow.\n"
        "- `YarlungRiver.csv`: DEM-thalweg river ribbon samples for the scenery river actor.\n"
        "- `manifest.json`: import scale and layer notes.\n",
        encoding="utf-8",
    )
    print(f"Generated {out_dir / 'YarlungTsangpo_1009.r16'} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
