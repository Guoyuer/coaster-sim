#!/usr/bin/env python3
"""Generate the world-longest scenic Yarlung coaster track CSV."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw

from yarlung_track_lib import (
    Heightfield,
    TrackPoint,
    closed_polyline_length,
    load_heightfield,
    moving_average_points,
    polyline_length,
    resample_polyline,
    write_track_csv,
)


SECTION_COLORS = {
    "Station": (255, 255, 255),
    "Lift": (255, 210, 64),
    "Outbound": (40, 180, 255),
    "Turnaround": (255, 120, 40),
    "Return": (80, 230, 120),
    "Launch": (255, 70, 180),
    "Brake": (255, 50, 50),
}

GRAVITY_MPS2 = 9.80665


def extract_thalweg(heightfield: Heightfield, step_x: int = 5, search_radius: int = 70) -> list[tuple[float, float, float]]:
    width = heightfield.width
    height = heightfield.height
    start_x = max(4, int(width * 0.10))
    end_x = min(width - 5, int(width * 0.90))

    first_x = (start_x + end_x) // 2
    previous_y = min(
        range(int(height * 0.12), int(height * 0.88)),
        key=lambda y: heightfield.heights_cm[y * width + first_x],
    )

    columns = list(range(first_x, end_x, step_x))
    forward = trace_columns(heightfield, columns, previous_y, search_radius)
    columns = list(range(first_x - step_x, start_x, -step_x))
    backward = trace_columns(heightfield, columns, previous_y, search_radius)
    grid_path = list(reversed(backward)) + forward
    world_path = []
    for gx, gy in grid_path:
        x, y = heightfield.grid_to_world(gx, gy)
        world_path.append((x, y, heightfield.sample_cm(x, y)))
    return moving_average_points(world_path, radius=5, passes=3)


def trace_columns(
    heightfield: Heightfield,
    columns: list[int],
    start_y: int,
    search_radius: int,
) -> list[tuple[float, float]]:
    width = heightfield.width
    height = heightfield.height
    previous_y = start_y
    path = []
    for x in columns:
        y_min = max(2, previous_y - search_radius)
        y_max = min(height - 3, previous_y + search_radius)
        best_y = min(
            range(y_min, y_max + 1),
            key=lambda y: heightfield.heights_cm[y * width + x] + abs(y - previous_y) * 45.0,
        )
        previous_y = best_y
        path.append((float(x), float(best_y)))
    return path


def tangent_normal(points: list[tuple[float, float, float]], index: int) -> tuple[float, float]:
    a = points[max(0, index - 1)]
    b = points[min(len(points) - 1, index + 1)]
    dx = b[0] - a[0]
    dy = b[1] - a[1]
    length = math.hypot(dx, dy)
    if length < 1.0:
        return 0.0, 1.0
    return -dy / length, dx / length


def section_for(route: str, ratio: float) -> str:
    if route == "outbound":
        if ratio < 0.04:
            return "Station"
        if ratio < 0.16:
            return "Lift"
        if ratio > 0.94:
            return "Turnaround"
        return "Outbound"
    if ratio < 0.12:
        return "Turnaround"
    if ratio < 0.72:
        return "Return"
    if ratio < 0.86:
        return "Launch"
    if ratio < 0.96:
        return "Brake"
    return "Station"


def build_out_and_back(
    heightfield: Heightfield,
    thalweg: list[tuple[float, float, float]],
    target_length_m: float,
    spacing_m: float,
    clearance_m: float,
    outbound_offset_m: float,
    return_offset_m: float,
    return_extra_height_m: float,
    design_speed_mps: float,
    max_bank_deg: float,
) -> list[TrackPoint]:
    thalweg_resampled = resample_polyline(thalweg, spacing_m * 100.0)
    outbound_length_cm = target_length_m * 100.0 * 0.5
    total_thalweg = polyline_length(thalweg_resampled)
    start_distance = max(0.0, (total_thalweg - outbound_length_cm) * 0.46)
    outbound_center = cut_polyline(thalweg_resampled, start_distance, outbound_length_cm)
    outbound_center = resample_polyline(outbound_center, spacing_m * 100.0)

    points: list[TrackPoint] = []
    station_z = None
    for index, center in enumerate(outbound_center):
        ratio = index / max(1, len(outbound_center) - 1)
        nx, ny = tangent_normal(outbound_center, index)
        offset_cm = outbound_offset_m * 100.0
        x = center[0] + nx * offset_cm
        y = center[1] + ny * offset_cm
        terrain_z = heightfield.sample_cm(x, y)
        descent_drop = 8500.0 * ratio
        airtime = math.sin(ratio * math.pi * 8.0) * 900.0 * (0.25 + 0.75 * ratio)
        if station_z is None:
            station_z = terrain_z + clearance_m * 100.0 + 3200.0
        design_z = station_z - descent_drop + airtime
        z = max(terrain_z + clearance_m * 100.0, design_z)
        section = section_for("outbound", ratio)
        roll = 0.0
        points.append(TrackPoint(len(points), x, y, z, roll, section, terrain_z))

    return_center = list(reversed(outbound_center))
    for index, center in enumerate(return_center):
        ratio = index / max(1, len(return_center) - 1)
        nx, ny = tangent_normal(return_center, index)
        offset_cm = return_offset_m * 100.0
        x = center[0] - nx * offset_cm
        y = center[1] - ny * offset_cm
        terrain_z = heightfield.sample_cm(x, y)
        climb = 6500.0 * ratio
        wave = math.sin(ratio * math.pi * 5.0 + 0.3) * 520.0
        design_z = (station_z or terrain_z) - 7600.0 + climb + wave + return_extra_height_m * 100.0
        if ratio > 0.82 and station_z is not None:
            blend = (ratio - 0.82) / 0.18
            design_z = design_z * (1.0 - blend) + station_z * blend
        z = max(terrain_z + (clearance_m + 6.0) * 100.0, design_z)
        section = section_for("return", ratio)
        roll = 0.0
        points.append(TrackPoint(len(points), x, y, z, roll, section, terrain_z))

    smooth_z(points, radius=2, passes=2, heightfield=heightfield, clearance_cm=clearance_m * 100.0)
    apply_curvature_banking(points, design_speed_mps=design_speed_mps, max_bank_deg=max_bank_deg)
    smooth_roll(points, radius=2, passes=2)
    for idx, point in enumerate(points):
        point.idx = idx
    return points


def cut_polyline(
    points: list[tuple[float, float, float]],
    start_cm: float,
    length_cm: float,
) -> list[tuple[float, float, float]]:
    distances = [0.0]
    for a, b in zip(points, points[1:]):
        distances.append(distances[-1] + math.dist(a, b))
    end_cm = min(distances[-1], start_cm + length_cm)
    result = []
    for point, distance_cm in zip(points, distances):
        if start_cm <= distance_cm <= end_cm:
            result.append(point)
    if len(result) < 2:
        return points
    return result


def smooth_z(points: list[TrackPoint], radius: int, passes: int, heightfield: Heightfield, clearance_cm: float) -> None:
    for _ in range(passes):
        z_values = [point.z for point in points]
        for index, point in enumerate(points):
            start = max(0, index - radius)
            end = min(len(points), index + radius + 1)
            averaged = sum(z_values[start:end]) / (end - start)
            point.terrain_z = heightfield.sample_cm(point.x, point.y)
            point.z = max(point.terrain_z + clearance_cm, averaged)


def smooth_closed_xy(
    points: list[TrackPoint],
    radius: int,
    passes: int,
    heightfield: Heightfield,
    clearance_cm: float,
) -> None:
    count = len(points)
    for _ in range(passes):
        old = [(point.x, point.y, point.z) for point in points]
        for index, point in enumerate(points):
            xs = []
            ys = []
            zs = []
            for offset in range(-radius, radius + 1):
                sample = old[(index + offset) % count]
                xs.append(sample[0])
                ys.append(sample[1])
                zs.append(sample[2])
            point.x = sum(xs) / len(xs)
            point.y = sum(ys) / len(ys)
            point.terrain_z = heightfield.sample_cm(point.x, point.y)
            point.z = max(point.terrain_z + clearance_cm, sum(zs) / len(zs))


def signed_curvature_xy(points: list[TrackPoint], index: int) -> float:
    count = len(points)
    previous = points[(index - 1) % count]
    current = points[index]
    next_point = points[(index + 1) % count]
    ax, ay = current.x - previous.x, current.y - previous.y
    bx, by = next_point.x - current.x, next_point.y - current.y
    len_a = math.hypot(ax, ay)
    len_b = math.hypot(bx, by)
    if len_a < 1.0 or len_b < 1.0:
        return 0.0
    ux, uy = ax / len_a, ay / len_a
    vx, vy = bx / len_b, by / len_b
    dot = max(-1.0, min(1.0, ux * vx + uy * vy))
    angle = math.acos(dot)
    cross = ux * vy - uy * vx
    average_length_m = (len_a + len_b) * 0.5 / 100.0
    if average_length_m <= 0.01:
        return 0.0
    return math.copysign(angle / average_length_m, cross)


def apply_curvature_banking(points: list[TrackPoint], design_speed_mps: float, max_bank_deg: float) -> None:
    max_bank_rad = math.radians(max_bank_deg)
    for index, point in enumerate(points):
        curvature = signed_curvature_xy(points, index)
        bank_rad = math.atan((design_speed_mps * design_speed_mps * curvature) / GRAVITY_MPS2)
        bank_rad = max(-max_bank_rad, min(max_bank_rad, bank_rad))
        point.roll_deg = math.degrees(bank_rad)


def smooth_roll(points: list[TrackPoint], radius: int, passes: int) -> None:
    count = len(points)
    for _ in range(passes):
        old = [point.roll_deg for point in points]
        for index, point in enumerate(points):
            values = [old[(index + offset) % count] for offset in range(-radius, radius + 1)]
            point.roll_deg = sum(values) / len(values)


def update_manifest(
    root: Path,
    track_path: Path,
    points: list[TrackPoint],
    design_speed_mps: float,
    max_bank_deg: float,
) -> dict[str, object]:
    manifest_path = root / "Content/Generated/YarlungLandscape/manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    xyz = [(point.x, point.y, point.z) for point in points]
    length_cm = closed_polyline_length(xyz)
    clearances = [(point.z - point.terrain_z) / 100.0 for point in points]
    section_distances: dict[str, float] = {}
    for point_a, point_b in zip(points, points[1:] + points[:1]):
        section_distances[point_a.section] = section_distances.get(point_a.section, 0.0) + math.dist(
            (point_a.x, point_a.y, point_a.z),
            (point_b.x, point_b.y, point_b.z),
        ) / 100.0
    manifest["track"] = {
        "path": str(track_path).replace("\\", "/"),
        "length_m": round(length_cm / 100.0, 2),
        "control_point_count": len(points),
        "out_back_split_m": round(length_cm / 200.0, 2),
        "min_clearance_m": round(min(clearances), 2),
        "section_distances_m": {key: round(value, 2) for key, value in section_distances.items()},
        "banking": {
            "roll_source": "curvature-derived",
            "design_speed_mps": design_speed_mps,
            "max_bank_deg": max_bank_deg,
        },
        "generator": "scripts/generate-yarlung-track.py",
        "note": "Generated scenic out-and-back track with curvature-derived roll_deg for P3 comfort validation.",
    }
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    return manifest


def write_overlay(
    root: Path,
    heightfield: Heightfield,
    thalweg: list[tuple[float, float, float]],
    points: list[TrackPoint],
    output: Path,
) -> None:
    hillshade = root / "Content/Generated/YarlungLandscape/YarlungTsangpo_hillshade.png"
    image = Image.open(hillshade).convert("RGB")
    draw = ImageDraw.Draw(image)

    def to_pixel(x: float, y: float) -> tuple[int, int]:
        gx, gy = heightfield.world_to_grid(x, y)
        return int(round(gx)), int(round(gy))

    thalweg_pixels = [to_pixel(point[0], point[1]) for point in thalweg]
    if len(thalweg_pixels) > 1:
        draw.line(thalweg_pixels, fill=(255, 255, 255), width=2)

    for section, color in SECTION_COLORS.items():
        section_pixels = [to_pixel(point.x, point.y) for point in points if point.section == section]
        if len(section_pixels) > 1:
            draw.line(section_pixels, fill=color, width=4)
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target-length-m", type=float, default=5000.0)
    parser.add_argument("--spacing-m", type=float, default=55.0)
    parser.add_argument("--clearance-m", type=float, default=24.0)
    parser.add_argument("--outbound-offset-m", type=float, default=72.0)
    parser.add_argument("--return-offset-m", type=float, default=148.0)
    parser.add_argument("--return-extra-height-m", type=float, default=32.0)
    parser.add_argument("--design-speed-mps", type=float, default=22.0)
    parser.add_argument("--max-bank-deg", type=float, default=70.0)
    parser.add_argument("--out", default="Content/Generated/YarlungLandscape/YarlungTrack.csv")
    parser.add_argument("--overlay", default="Saved/Diagnostics/yarlung-track-overlay.png")
    args = parser.parse_args()

    root = Path.cwd()
    heightfield = load_heightfield(root)
    thalweg = extract_thalweg(heightfield)
    points = build_out_and_back(
        heightfield,
        thalweg,
        target_length_m=args.target_length_m,
        spacing_m=args.spacing_m,
        clearance_m=args.clearance_m,
        outbound_offset_m=args.outbound_offset_m,
        return_offset_m=args.return_offset_m,
        return_extra_height_m=args.return_extra_height_m,
        design_speed_mps=args.design_speed_mps,
        max_bank_deg=args.max_bank_deg,
    )
    track_path = Path(args.out)
    write_track_csv(track_path, points)
    manifest = update_manifest(root, track_path, points, args.design_speed_mps, args.max_bank_deg)
    overlay = Path(args.overlay)
    write_overlay(root, heightfield, thalweg, points, overlay)
    track = manifest["track"]
    print(
        f"generated {track_path} points={len(points)} length={track['length_m']}m "
        f"min_clearance={track['min_clearance_m']}m overlay={overlay}"
    )


if __name__ == "__main__":
    main()
