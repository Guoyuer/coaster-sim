#!/usr/bin/env python3
"""Verify generated Yarlung track clearance and comfort gates."""

from __future__ import annotations

import argparse
import csv
import math
import sys
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from yarlung_track_lib import (
    TrackPoint,
    catmull_rom,
    load_heightfield,
    read_track_csv,
)


GRAVITY_MPS2 = 9.80665


@dataclass(frozen=True)
class TrackSample:
    x: float
    y: float
    z: float
    section: str
    roll_deg: float


def sample_track(points: list[TrackPoint], samples_per_segment: int) -> list[TrackSample]:
    base = [(point.x, point.y, point.z) for point in points]
    samples = []
    count = len(base)
    for index in range(count):
        p0 = base[(index - 1) % count]
        p1 = base[index]
        p2 = base[(index + 1) % count]
        p3 = base[(index + 2) % count]
        section = points[index].section
        roll_a = points[index].roll_deg
        roll_b = points[(index + 1) % count].roll_deg
        for step in range(samples_per_segment):
            t = step / samples_per_segment
            sample = catmull_rom(p0, p1, p2, p3, t)
            samples.append(TrackSample(sample[0], sample[1], sample[2], section, lerp(roll_a, roll_b, t)))
    return samples


def circumradius_xy(
    a: TrackSample,
    b: TrackSample,
    c: TrackSample,
) -> float:
    ax, ay = a.x, a.y
    bx, by = b.x, b.y
    cx, cy = c.x, c.y
    ab = math.hypot(ax - bx, ay - by)
    bc = math.hypot(bx - cx, by - cy)
    ca = math.hypot(cx - ax, cy - ay)
    area2 = abs((bx - ax) * (cy - ay) - (by - ay) * (cx - ax))
    if area2 < 1.0:
        return 1.0e9
    return (ab * bc * ca) / (2.0 * area2)


def segment_distance_m(a: TrackSample, b: TrackSample) -> float:
    return math.sqrt((a.x - b.x) ** 2 + (a.y - b.y) ** 2 + (a.z - b.z) ** 2) / 100.0


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def vec_sub(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return a[0] - b[0], a[1] - b[1], a[2] - b[2]


def vec_add(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return a[0] + b[0], a[1] + b[1], a[2] + b[2]


def vec_scale(a: tuple[float, float, float], scalar: float) -> tuple[float, float, float]:
    return a[0] * scalar, a[1] * scalar, a[2] * scalar


def dot(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def length(a: tuple[float, float, float]) -> float:
    return math.sqrt(dot(a, a))


def normalize(a: tuple[float, float, float], fallback: tuple[float, float, float]) -> tuple[float, float, float]:
    magnitude = length(a)
    if magnitude < 1.0e-6:
        return fallback
    return a[0] / magnitude, a[1] / magnitude, a[2] / magnitude


def sample_pos_m(sample: TrackSample) -> tuple[float, float, float]:
    return sample.x / 100.0, sample.y / 100.0, sample.z / 100.0


def rotate_about_axis(
    vector: tuple[float, float, float],
    axis: tuple[float, float, float],
    angle_rad: float,
) -> tuple[float, float, float]:
    cos_a = math.cos(angle_rad)
    sin_a = math.sin(angle_rad)
    return vec_add(
        vec_add(vec_scale(vector, cos_a), vec_scale(cross(axis, vector), sin_a)),
        vec_scale(axis, dot(axis, vector) * (1.0 - cos_a)),
    )


def frame_axes(
    previous: TrackSample,
    sample: TrackSample,
    next_sample: TrackSample,
) -> tuple[tuple[float, float, float], tuple[float, float, float], tuple[float, float, float]]:
    forward = normalize(vec_sub(sample_pos_m(next_sample), sample_pos_m(previous)), (1.0, 0.0, 0.0))
    world_up = (0.0, 0.0, 1.0)
    right = normalize(cross(world_up, forward), (0.0, 1.0, 0.0))
    up = normalize(cross(forward, right), world_up)
    bank_rad = math.radians(sample.roll_deg)
    return (
        forward,
        normalize(rotate_about_axis(right, forward, bank_rad), right),
        normalize(rotate_about_axis(up, forward, bank_rad), up),
    )


def curvature_accel_mps2(
    previous: TrackSample,
    sample: TrackSample,
    next_sample: TrackSample,
    speed_mps: float,
) -> tuple[float, float, float]:
    prev_delta = vec_sub(sample_pos_m(sample), sample_pos_m(previous))
    next_delta = vec_sub(sample_pos_m(next_sample), sample_pos_m(sample))
    prev_forward = normalize(prev_delta, (1.0, 0.0, 0.0))
    next_forward = normalize(next_delta, prev_forward)
    average_ds = max(0.1, (length(prev_delta) + length(next_delta)) * 0.5)
    curvature_vector = vec_scale(vec_sub(next_forward, prev_forward), 1.0 / average_ds)
    return vec_scale(curvature_vector, speed_mps * speed_mps)


def ride_accel_components(section: str, speed_mps: float, forward: tuple[float, float, float]) -> tuple[float, float, float, float, float]:
    drive = 0.0
    brake = 0.0
    if section == "Lift":
        drive = clamp((12.0 - speed_mps) * 2.0, 0.0, 9.8)
    elif section == "Launch":
        drive = 7.2 if speed_mps < 34.0 else 0.0
    elif section == "Brake":
        brake = min((speed_mps - 8.0) * 1.1, 8.5) if speed_mps > 8.0 else 0.0
    elif section == "Station":
        drive = clamp((4.0 - speed_mps) * 1.0, -2.0, 1.8)

    gravity = dot((0.0, 0.0, -GRAVITY_MPS2), forward)
    drag = 0.0015 * speed_mps * speed_mps
    rolling = 0.18
    net = drive + gravity - drag - rolling - brake
    longitudinal_seat = drive - drag - rolling - brake
    return net, longitudinal_seat, drive, drag, brake


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
                "speed_mps",
                "est_lat_g",
                "est_vert_g",
                "est_long_g",
                "roll_deg",
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
    parser.add_argument("--initial-speed-mps", type=float, default=4.0)
    parser.add_argument("--min-speed-mps", type=float, default=1.8)
    parser.add_argument("--max-speed-mps", type=float, default=56.0)
    parser.add_argument("--max-lat-g", type=float, default=2.5)
    parser.add_argument("--min-vert-g", type=float, default=-1.5)
    parser.add_argument("--max-vert-g", type=float, default=5.5)
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
    min_vert_g = 1.0e9
    max_vert_g = -1.0e9
    max_long_g = 0.0
    min_speed = 1.0e9
    max_speed = 0.0
    speed_mps = clamp(args.initial_speed_mps, args.min_speed_mps, args.max_speed_mps)

    for index, sample in enumerate(samples):
        previous = samples[index - 1]
        next_sample = samples[(index + 1) % len(samples)]
        if index > 0:
            distance_m += segment_distance_m(previous, sample)
        forward, right, up = frame_axes(previous, sample, next_sample)
        terrain_z = heightfield.sample_cm(sample.x, sample.y)
        clearance_m = (sample.z - terrain_z) / 100.0
        horizontal_m = max(0.1, math.hypot(next_sample.x - previous.x, next_sample.y - previous.y) / 100.0)
        grade_pct = abs((next_sample.z - previous.z) / 100.0 / horizontal_m) * 100.0
        radius_m = circumradius_xy(previous, sample, next_sample) / 100.0
        curve_accel = curvature_accel_mps2(previous, sample, next_sample, speed_mps)
        seat_force = vec_add(curve_accel, (0.0, 0.0, GRAVITY_MPS2))
        lat_g = dot(seat_force, right) / GRAVITY_MPS2
        vert_g = dot(seat_force, up) / GRAVITY_MPS2
        net_accel, long_accel, _drive, _drag, _brake = ride_accel_components(sample.section, speed_mps, forward)
        long_g = long_accel / GRAVITY_MPS2
        violation_reasons = []
        if clearance_m < args.clearance_floor_m:
            violation_reasons.append("clearance")
        if grade_pct > args.max_grade_pct:
            violation_reasons.append("grade")
        if abs(lat_g) > args.max_lat_g:
            violation_reasons.append("lat_g")
        if vert_g < args.min_vert_g or vert_g > args.max_vert_g:
            violation_reasons.append("vert_g")
        if abs(long_g) > args.max_long_g:
            violation_reasons.append("long_g")

        min_clearance = min(min_clearance, clearance_m)
        min_radius = min(min_radius, radius_m)
        max_grade = max(max_grade, grade_pct)
        max_lat_g = max(max_lat_g, abs(lat_g))
        min_vert_g = min(min_vert_g, vert_g)
        max_vert_g = max(max_vert_g, vert_g)
        max_long_g = max(max_long_g, abs(long_g))
        min_speed = min(min_speed, speed_mps)
        max_speed = max(max_speed, speed_mps)
        if violation_reasons:
            violations += 1

        rows.append(
            {
                "sample": index,
                "distance_m": f"{distance_m:.3f}",
                "x_cm": f"{sample.x:.3f}",
                "y_cm": f"{sample.y:.3f}",
                "track_z_cm": f"{sample.z:.3f}",
                "terrain_z_cm": f"{terrain_z:.3f}",
                "clearance_m": f"{clearance_m:.3f}",
                "grade_pct": f"{grade_pct:.3f}",
                "radius_m": f"{radius_m:.3f}",
                "speed_mps": f"{speed_mps:.3f}",
                "est_lat_g": f"{lat_g:.3f}",
                "est_vert_g": f"{vert_g:.3f}",
                "est_long_g": f"{long_g:.3f}",
                "roll_deg": f"{sample.roll_deg:.3f}",
                "section": sample.section,
                "violation": "|".join(violation_reasons),
            }
        )

        ds = segment_distance_m(sample, next_sample)
        speed_mps = math.sqrt(max(args.min_speed_mps * args.min_speed_mps, speed_mps * speed_mps + 2.0 * net_accel * ds))
        speed_mps = clamp(speed_mps, args.min_speed_mps, args.max_speed_mps)

    write_csv(Path(args.out_csv), rows)
    write_plot(
        Path(args.out_png),
        rows,
        (
            f"Yarlung track clearance: min={min_clearance:.2f}m "
            f"latG={max_lat_g:.2f} speed={min_speed:.1f}-{max_speed:.1f}m/s"
        ),
    )
    print(
        f"samples={len(samples)} length={distance_m:.1f}m min_clearance={min_clearance:.2f}m "
        f"min_radius={min_radius:.1f}m max_grade={max_grade:.1f}% "
        f"speed_range={min_speed:.1f}-{max_speed:.1f}mps "
        f"est_max_lat_g={max_lat_g:.2f} est_vert_g={min_vert_g:.2f}/{max_vert_g:.2f} est_max_long_g={max_long_g:.2f} "
        f"violations={violations} csv={args.out_csv} plot={args.out_png}"
    )
    if violations:
        sys.exit(1)


if __name__ == "__main__":
    main()
