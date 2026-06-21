#!/usr/bin/env python3
"""Validate high-level Yarlung spatial contracts that actor existence cannot prove."""

from __future__ import annotations

import argparse
import csv
import math
import sys
from pathlib import Path


def read_track(path: Path) -> list[dict[str, object]]:
    with path.open(newline="", encoding="utf-8") as handle:
        return [
            {
                "x": float(row["x"]),
                "y": float(row["y"]),
                "z": float(row["z"]),
                "section": row["section"],
            }
            for row in csv.DictReader(handle)
        ]


def read_river(path: Path) -> list[dict[str, float]]:
    with path.open(newline="", encoding="utf-8") as handle:
        return [
            {
                "x": float(row["x"]),
                "y": float(row["y"]),
                "z": float(row["z"]),
                "half_width_cm": float(row["half_width_cm"]),
            }
            for row in csv.DictReader(handle)
        ]


def distance(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3)))


def normalize(v: tuple[float, float, float], fallback: tuple[float, float, float]) -> tuple[float, float, float]:
    length = math.sqrt(sum(x * x for x in v))
    if length < 1.0e-6:
        return fallback
    return tuple(x / length for x in v)


def dot(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def track_segments(track: list[dict[str, object]]) -> tuple[list[tuple[float, float, dict[str, object], dict[str, object]]], float]:
    segments: list[tuple[float, float, dict[str, object], dict[str, object]]] = []
    total = 0.0
    for a, b in zip(track, track[1:]):
        seg_len = distance((a["x"], a["y"], a["z"]), (b["x"], b["y"], b["z"]))  # type: ignore[arg-type]
        segments.append((total, seg_len, a, b))
        total += seg_len
    return segments, total


def sample_track(
    segments: list[tuple[float, float, dict[str, object], dict[str, object]]],
    total_length: float,
    distance_cm: float,
) -> tuple[tuple[float, float, float], tuple[float, float, float], str]:
    d = distance_cm % total_length
    for start, seg_len, a, b in segments:
        if start + seg_len >= d:
            t = 0.0 if seg_len <= 0.0 else (d - start) / seg_len
            pos = tuple(float(a[k]) + (float(b[k]) - float(a[k])) * t for k in ("x", "y", "z"))
            forward = normalize(
                tuple(float(b[k]) - float(a[k]) for k in ("x", "y", "z")),
                (1.0, 0.0, 0.0),
            )
            return pos, forward, str(a["section"])
    last = segments[-1][3]
    return (float(last["x"]), float(last["y"]), float(last["z"])), (1.0, 0.0, 0.0), str(last["section"])


def nearest_river(river: list[dict[str, float]], position: tuple[float, float, float]) -> dict[str, float]:
    return min(river, key=lambda row: (row["x"] - position[0]) ** 2 + (row["y"] - position[1]) ** 2)


def camera_axes(forward: tuple[float, float, float], pitch_deg: float) -> tuple[tuple[float, float, float], tuple[float, float, float], tuple[float, float, float]]:
    world_up = (0.0, 0.0, 1.0)
    right = normalize(cross(world_up, forward), (0.0, 1.0, 0.0))
    up = normalize(cross(forward, right), world_up)
    pitch = math.radians(pitch_deg)
    camera_forward = normalize(
        (
            forward[0] * math.cos(pitch) + up[0] * math.sin(pitch),
            forward[1] * math.cos(pitch) + up[1] * math.sin(pitch),
            forward[2] * math.cos(pitch) + up[2] * math.sin(pitch),
        ),
        forward,
    )
    camera_up = normalize(cross(camera_forward, right), up)
    return camera_forward, right, camera_up


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--track", default="Content/Generated/YarlungLandscape/YarlungTrack.csv")
    parser.add_argument("--river", default="Content/Generated/YarlungLandscape/YarlungRiver.csv")
    parser.add_argument("--times", default="45,57,60,66,237")
    parser.add_argument("--start-ratio", type=float, default=0.34)
    parser.add_argument("--speed-mps", type=float, default=18.0)
    parser.add_argument("--camera-pitch-deg", type=float, default=-7.5)
    parser.add_argument("--horizontal-fov-deg", type=float, default=86.0)
    parser.add_argument("--vertical-fov-deg", type=float, default=52.0)
    parser.add_argument("--min-visible-samples", type=int, default=1)
    parser.add_argument("--out-csv", default="Saved/Diagnostics/yarlung-spatial-contract.csv")
    args = parser.parse_args()

    track = read_track(Path(args.track))
    river = read_river(Path(args.river))
    segments, total_length = track_segments(track)
    if not segments or not river:
        print("missing track or river samples", file=sys.stderr)
        return 2

    times = [float(part) for part in args.times.replace("+", ",").split(",") if part.strip()]
    half_h = math.radians(args.horizontal_fov_deg * 0.5)
    half_v = math.radians(args.vertical_fov_deg * 0.5)
    visible_count = 0
    rows: list[dict[str, object]] = []

    for seconds in times:
        distance_cm = total_length * args.start_ratio + args.speed_mps * 100.0 * seconds
        pos, forward, section = sample_track(segments, total_length, distance_cm)
        river_row = nearest_river(river, pos)
        target = (
            river_row["x"],
            river_row["y"],
            river_row["z"] + 220.0,
        )
        to_river = (target[0] - pos[0], target[1] - pos[1], target[2] - pos[2])
        camera_forward, camera_right, camera_up = camera_axes(forward, args.camera_pitch_deg)
        forward_depth = dot(to_river, camera_forward)
        horizontal_angle = math.degrees(math.atan2(dot(to_river, camera_right), max(1.0, forward_depth)))
        vertical_angle = math.degrees(math.atan2(dot(to_river, camera_up), max(1.0, forward_depth)))
        xy_distance_m = math.hypot(to_river[0], to_river[1]) / 100.0
        dz_m = (pos[2] - target[2]) / 100.0
        visible = forward_depth > 0.0 and abs(math.radians(horizontal_angle)) <= half_h and abs(math.radians(vertical_angle)) <= half_v
        visible_count += int(visible)
        rows.append(
            {
                "time_s": seconds,
                "section": section,
                "xy_distance_m": round(xy_distance_m, 2),
                "track_above_water_m": round(dz_m, 2),
                "horizontal_angle_deg": round(horizontal_angle, 2),
                "vertical_angle_deg": round(vertical_angle, 2),
                "in_camera_fov": visible,
            }
        )

    out = Path(args.out_csv)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(f"spatial_contract visible_samples={visible_count}/{len(rows)} csv={out}")
    if visible_count < args.min_visible_samples:
        print("spatial contract failed: no sampled first-person frame can see the generated river", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
