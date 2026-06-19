#!/usr/bin/env python3
"""Verify generated Yarlung track clearance and comfort gates."""

from __future__ import annotations

import argparse
import csv
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from yarlung_track_lib import (
    TrackPoint,
    catmull_rom,
    load_heightfield,
    read_track_csv,
)


GRAVITY_MPS2 = 9.80665


def sample_track(points: list[TrackPoint], samples_per_segment: int) -> list[tuple[float, float, float, str]]:
    base = [(point.x, point.y, point.z) for point in points]
    samples = []
    count = len(base)
    for index in range(count):
        p0 = base[(index - 1) % count]
        p1 = base[index]
        p2 = base[(index + 1) % count]
        p3 = base[(index + 2) % count]
        section = points[index].section
        for step in range(samples_per_segment):
            sample = catmull_rom(p0, p1, p2, p3, step / samples_per_segment)
            samples.append((sample[0], sample[1], sample[2], section))
    return samples


def circumradius_xy(
    a: tuple[float, float, float, str],
    b: tuple[float, float, float, str],
    c: tuple[float, float, float, str],
) -> float:
    ax, ay = a[0], a[1]
    bx, by = b[0], b[1]
    cx, cy = c[0], c[1]
    ab = math.hypot(ax - bx, ay - by)
    bc = math.hypot(bx - cx, by - cy)
    ca = math.hypot(cx - ax, cy - ay)
    area2 = abs((bx - ax) * (cy - ay) - (by - ay) * (cx - ax))
    if area2 < 1.0:
        return 1.0e9
    return (ab * bc * ca) / (2.0 * area2)


def segment_distance_m(a: tuple[float, float, float, str], b: tuple[float, float, float, str]) -> float:
    return math.sqrt((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2) / 100.0


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="ascii") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "sample",
                "distance_m",
                "x_cm",
                "y_cm",
                "track_z_cm",
                "terrain_z_cm",
                "clearance_m",
                "grade_pct",
                "radius_m",
                "est_lat_g",
                "est_long_g",
                "section",
                "violation",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)


def write_plot(path: Path, rows: list[dict[str, object]], title: str) -> None:
    width, height = 1400, 760
    left, right, top, bottom = 82, 40, 60, 96
    plot_w = width - left - right
    plot_h = height - top - bottom
    image = Image.new("RGB", (width, height), (248, 248, 246))
    draw = ImageDraw.Draw(image)
    try:
        font = ImageFont.truetype("arial.ttf", 16)
        small = ImageFont.truetype("arial.ttf", 12)
    except OSError:
        font = small = None

    distances = [float(row["distance_m"]) for row in rows]
    clearances = [float(row["clearance_m"]) for row in rows]
    max_d = max(distances) if distances else 1.0
    min_c = min(clearances) if clearances else 0.0
    max_c = max(clearances) if clearances else 1.0
    lo = min(-5.0, min_c - 2.0)
    hi = max(60.0, max_c + 4.0)

    draw.text((left, 20), title, fill=(20, 20, 20), font=font)
    draw.line((left, height - bottom, width - right, height - bottom), fill=(30, 30, 30), width=2)
    draw.line((left, height - bottom, left, top), fill=(30, 30, 30), width=2)

    for index in range(7):
        value = lo + (hi - lo) * index / 6
        y = height - bottom - (value - lo) / (hi - lo) * plot_h
        draw.line((left, y, width - right, y), fill=(224, 224, 224))
        draw.text((8, y - 8), f"{value:.0f}m", fill=(55, 55, 55), font=small)

    points = []
    for row in rows:
        x = left + float(row["distance_m"]) / max_d * plot_w
        y = height - bottom - (float(row["clearance_m"]) - lo) / (hi - lo) * plot_h
        points.append((x, y))
    if len(points) > 1:
        draw.line(points, fill=(30, 92, 155), width=3)

    for row, point in zip(rows, points):
        if row["violation"]:
            x, y = point
            draw.ellipse((x - 4, y - 4, x + 4, y + 4), fill=(210, 40, 35))

    floor_y = height - bottom - (2.0 - lo) / (hi - lo) * plot_h
    draw.line((left, floor_y, width - right, floor_y), fill=(210, 40, 35), width=2)
    draw.text((left + 6, floor_y - 18), "2m clearance floor", fill=(160, 30, 30), font=small)
    draw.text((left, height - 28), "blue=clearance along generated track; red dots=gate violations", fill=(70, 70, 70), font=small)
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--track", default="Content/Generated/YarlungLandscape/YarlungTrack.csv")
    parser.add_argument("--out-csv", default="Saved/Diagnostics/track-clearance.csv")
    parser.add_argument("--out-png", default="Saved/Diagnostics/track-clearance.png")
    parser.add_argument("--samples-per-segment", type=int, default=8)
    parser.add_argument("--clearance-floor-m", type=float, default=2.0)
    parser.add_argument("--design-speed-mps", type=float, default=22.0)
    parser.add_argument("--max-lat-g", type=float, default=3.8)
    parser.add_argument("--max-long-g", type=float, default=2.5)
    parser.add_argument("--max-grade-pct", type=float, default=65.0)
    args = parser.parse_args()

    root = Path.cwd()
    heightfield = load_heightfield(root)
    track = read_track_csv(Path(args.track))
    if len(track) < 4:
        print("track must have at least 4 points", file=sys.stderr)
        sys.exit(2)

    samples = sample_track(track, args.samples_per_segment)
    rows: list[dict[str, object]] = []
    distance_m = 0.0
    violations = 0
    min_clearance = 1.0e9
    min_radius = 1.0e9
    max_grade = 0.0
    max_lat_g = 0.0
    max_long_g = 0.0

    for index, sample in enumerate(samples):
        previous = samples[index - 1]
        next_sample = samples[(index + 1) % len(samples)]
        if index > 0:
            distance_m += segment_distance_m(previous, sample)
        terrain_z = heightfield.sample_cm(sample[0], sample[1])
        clearance_m = (sample[2] - terrain_z) / 100.0
        horizontal_m = max(0.1, math.hypot(next_sample[0] - previous[0], next_sample[1] - previous[1]) / 100.0)
        grade_pct = abs((next_sample[2] - previous[2]) / 100.0 / horizontal_m) * 100.0
        radius_m = circumradius_xy(previous, sample, next_sample) / 100.0
        lat_g = 0.0 if radius_m > 1.0e7 else args.design_speed_mps * args.design_speed_mps / max(radius_m, 1.0) / GRAVITY_MPS2
        long_g = min(args.max_long_g + 0.01, abs(math.atan(grade_pct / 100.0)) * args.design_speed_mps / 10.0)
        violation_reasons = []
        if clearance_m < args.clearance_floor_m:
            violation_reasons.append("clearance")
        if grade_pct > args.max_grade_pct:
            violation_reasons.append("grade")
        if lat_g > args.max_lat_g:
            violation_reasons.append("lat_g")
        if long_g > args.max_long_g:
            violation_reasons.append("long_g")

        min_clearance = min(min_clearance, clearance_m)
        min_radius = min(min_radius, radius_m)
        max_grade = max(max_grade, grade_pct)
        max_lat_g = max(max_lat_g, lat_g)
        max_long_g = max(max_long_g, long_g)
        if violation_reasons:
            violations += 1

        rows.append(
            {
                "sample": index,
                "distance_m": f"{distance_m:.3f}",
                "x_cm": f"{sample[0]:.3f}",
                "y_cm": f"{sample[1]:.3f}",
                "track_z_cm": f"{sample[2]:.3f}",
                "terrain_z_cm": f"{terrain_z:.3f}",
                "clearance_m": f"{clearance_m:.3f}",
                "grade_pct": f"{grade_pct:.3f}",
                "radius_m": f"{radius_m:.3f}",
                "est_lat_g": f"{lat_g:.3f}",
                "est_long_g": f"{long_g:.3f}",
                "section": sample[3],
                "violation": "|".join(violation_reasons),
            }
        )

    write_csv(Path(args.out_csv), rows)
    write_plot(
        Path(args.out_png),
        rows,
        (
            f"Yarlung track clearance: min={min_clearance:.2f}m "
            f"latG={max_lat_g:.2f} grade={max_grade:.1f}%"
        ),
    )
    print(
        f"samples={len(samples)} length={distance_m:.1f}m min_clearance={min_clearance:.2f}m "
        f"min_radius={min_radius:.1f}m max_grade={max_grade:.1f}% "
        f"est_max_lat_g={max_lat_g:.2f} est_max_long_g={max_long_g:.2f} "
        f"violations={violations} csv={args.out_csv} plot={args.out_png}"
    )
    if violations:
        sys.exit(1)


if __name__ == "__main__":
    main()
