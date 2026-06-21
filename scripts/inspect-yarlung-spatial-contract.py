#!/usr/bin/env python3
"""Validate high-level Yarlung spatial contracts that actor existence cannot prove."""

from __future__ import annotations

import argparse
import csv
import math
import sys
from pathlib import Path

from yarlung_track_lib import Heightfield, load_heightfield


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


def camera_position(
    track_position: tuple[float, float, float],
    forward: tuple[float, float, float],
    camera_back_cm: float,
    camera_up_cm: float,
) -> tuple[float, float, float]:
    world_up = (0.0, 0.0, 1.0)
    right = normalize(cross(world_up, forward), (0.0, 1.0, 0.0))
    up = normalize(cross(forward, right), world_up)
    return (
        track_position[0] - forward[0] * camera_back_cm + up[0] * camera_up_cm,
        track_position[1] - forward[1] * camera_back_cm + up[1] * camera_up_cm,
        track_position[2] - forward[2] * camera_back_cm + up[2] * camera_up_cm,
    )


def terrain_occludes(
    heightfield: Heightfield,
    start: tuple[float, float, float],
    end: tuple[float, float, float],
    clearance_cm: float,
    samples: int,
) -> bool:
    for index in range(1, samples):
        t = index / samples
        x = start[0] + (end[0] - start[0]) * t
        y = start[1] + (end[1] - start[1]) * t
        z = start[2] + (end[2] - start[2]) * t
        if heightfield.sample_cm(x, y) + clearance_cm > z:
            return True
    return False


def river_visibility(
    heightfield: Heightfield,
    river: list[dict[str, float]],
    position: tuple[float, float, float],
    camera_forward: tuple[float, float, float],
    camera_right: tuple[float, float, float],
    camera_up: tuple[float, float, float],
    half_h: float,
    half_v: float,
    max_distance_cm: float,
    occlusion_clearance_cm: float,
    occlusion_samples: int,
) -> tuple[dict[str, float], float, float, float, float, bool, int]:
    best: tuple[float, dict[str, float], float, float, float, float, bool] | None = None
    visible_count = 0
    for row in river:
        target = (row["x"], row["y"], row["z"] + 220.0)
        to_river = (target[0] - position[0], target[1] - position[1], target[2] - position[2])
        xy_distance = math.hypot(to_river[0], to_river[1])
        if xy_distance > max_distance_cm:
            continue
        forward_depth = dot(to_river, camera_forward)
        horizontal_angle = math.degrees(math.atan2(dot(to_river, camera_right), max(1.0, forward_depth)))
        vertical_angle = math.degrees(math.atan2(dot(to_river, camera_up), max(1.0, forward_depth)))
        in_fov = (
            forward_depth > 0.0
            and abs(math.radians(horizontal_angle)) <= half_h
            and abs(math.radians(vertical_angle)) <= half_v
        )
        occluded = in_fov and terrain_occludes(
            heightfield,
            position,
            target,
            occlusion_clearance_cm,
            occlusion_samples,
        )
        visible = in_fov and not occluded
        visible_count += int(visible)
        horizontal_overflow = max(0.0, abs(math.radians(horizontal_angle)) - half_h)
        vertical_overflow = max(0.0, abs(math.radians(vertical_angle)) - half_v)
        behind_penalty = 20.0 if forward_depth <= 0.0 else 0.0
        score = behind_penalty + horizontal_overflow + vertical_overflow
        if visible:
            # Prefer river that is actually ahead, not a near-edge sliver.
            score -= min(forward_depth / 100000.0, 1.0)
        if best is None or score < best[0]:
            dz_m = (position[2] - target[2]) / 100.0
            best = (score, row, xy_distance / 100.0, dz_m, horizontal_angle, vertical_angle, visible)
    if best is None:
        row = nearest_river(river, position)
        target = (row["x"], row["y"], row["z"] + 220.0)
        to_river = (target[0] - position[0], target[1] - position[1], target[2] - position[2])
        return row, math.hypot(to_river[0], to_river[1]) / 100.0, (position[2] - target[2]) / 100.0, 180.0, 180.0, False, 0
    return best[1], best[2], best[3], best[4], best[5], best[6], visible_count


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--track", default="Content/Generated/YarlungLandscape/YarlungTrack.csv")
    parser.add_argument("--river", default="Content/Generated/YarlungLandscape/YarlungRiver.csv")
    parser.add_argument("--times", default="45,57,60,66,237")
    parser.add_argument("--start-ratio", type=float, default=0.34)
    parser.add_argument("--speed-mps", type=float, default=18.0)
    parser.add_argument("--camera-pitch-deg", type=float, default=-14.0)
    parser.add_argument("--camera-back-cm", type=float, default=146.0)
    parser.add_argument("--camera-up-cm", type=float, default=372.0)
    parser.add_argument("--horizontal-fov-deg", type=float, default=86.0)
    parser.add_argument("--vertical-fov-deg", type=float, default=52.0)
    parser.add_argument("--max-river-target-distance-m", type=float, default=1800.0)
    parser.add_argument("--occlusion-clearance-cm", type=float, default=180.0)
    parser.add_argument("--occlusion-samples", type=int, default=24)
    parser.add_argument("--min-visible-samples", type=int, default=1)
    parser.add_argument("--out-csv", default="Saved/Diagnostics/yarlung-spatial-contract.csv")
    args = parser.parse_args()

    track = read_track(Path(args.track))
    river = read_river(Path(args.river))
    heightfield = load_heightfield(Path.cwd())
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
        track_pos, forward, section = sample_track(segments, total_length, distance_cm)
        pos = camera_position(track_pos, forward, args.camera_back_cm, args.camera_up_cm)
        camera_forward, camera_right, camera_up = camera_axes(forward, args.camera_pitch_deg)
        nearest_row = nearest_river(river, pos)
        nearest_xy_distance_m = math.hypot(nearest_row["x"] - pos[0], nearest_row["y"] - pos[1]) / 100.0
        river_row, xy_distance_m, dz_m, horizontal_angle, vertical_angle, visible, visible_river_samples = river_visibility(
            heightfield,
            river,
            pos,
            camera_forward,
            camera_right,
            camera_up,
            half_h,
            half_v,
            args.max_river_target_distance_m * 100.0,
            args.occlusion_clearance_cm,
            args.occlusion_samples,
        )
        visible_count += int(visible)
        rows.append(
            {
                "time_s": seconds,
                "section": section,
                "target_river_x": round(river_row["x"], 2),
                "target_river_y": round(river_row["y"], 2),
                "nearest_xy_distance_m": round(nearest_xy_distance_m, 2),
                "xy_distance_m": round(xy_distance_m, 2),
                "track_above_water_m": round(dz_m, 2),
                "horizontal_angle_deg": round(horizontal_angle, 2),
                "vertical_angle_deg": round(vertical_angle, 2),
                "visible_river_samples": visible_river_samples,
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
