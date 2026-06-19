#!/usr/bin/env python3
"""Shared helpers for generated Yarlung coaster track tools."""

from __future__ import annotations

import csv
import json
import math
import struct
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Heightfield:
    heights_cm: list[float]
    width: int
    height: int
    min_x: float
    max_x: float
    min_y: float
    max_y: float
    manifest: dict[str, object]

    @property
    def x_spacing_cm(self) -> float:
        return (self.max_x - self.min_x) / (self.width - 1)

    @property
    def y_spacing_cm(self) -> float:
        return (self.max_y - self.min_y) / (self.height - 1)

    def world_to_grid(self, x_cm: float, y_cm: float) -> tuple[float, float]:
        gx = (x_cm - self.min_x) / (self.max_x - self.min_x) * (self.width - 1)
        gy = (y_cm - self.min_y) / (self.max_y - self.min_y) * (self.height - 1)
        return gx, gy

    def grid_to_world(self, gx: float, gy: float) -> tuple[float, float]:
        x = self.min_x + gx / (self.width - 1) * (self.max_x - self.min_x)
        y = self.min_y + gy / (self.height - 1) * (self.max_y - self.min_y)
        return x, y

    def sample_cm(self, x_cm: float, y_cm: float) -> float:
        gx, gy = self.world_to_grid(x_cm, y_cm)
        gx = max(0.0, min(self.width - 1.0, gx))
        gy = max(0.0, min(self.height - 1.0, gy))
        x0 = int(math.floor(gx))
        y0 = int(math.floor(gy))
        x1 = min(self.width - 1, x0 + 1)
        y1 = min(self.height - 1, y0 + 1)
        tx = gx - x0
        ty = gy - y0

        def at(ix: int, iy: int) -> float:
            return self.heights_cm[iy * self.width + ix]

        a = lerp(at(x0, y0), at(x1, y0), tx)
        b = lerp(at(x0, y1), at(x1, y1), tx)
        return lerp(a, b, ty)


@dataclass
class TrackPoint:
    idx: int
    x: float
    y: float
    z: float
    roll_deg: float
    section: str
    terrain_z: float


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def load_heightfield(root: Path) -> Heightfield:
    manifest_path = root / "Content/Generated/YarlungLandscape/manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    width, height = (int(value) for value in manifest["resolution"])
    encoded_min = float(manifest["height_range_cm"]["encoded_min"])
    encoded_max = float(manifest["height_range_cm"]["encoded_max"])
    heightmap_path = root / "Content/Generated/YarlungLandscape" / str(manifest["heightmap"])
    raw = heightmap_path.read_bytes()
    values = struct.unpack("<" + "H" * (len(raw) // 2), raw)
    heights_cm = [lerp(encoded_min, encoded_max, value / 65535.0) for value in values]
    bounds = manifest["world_bounds_cm"]
    return Heightfield(
        heights_cm=heights_cm,
        width=width,
        height=height,
        min_x=float(bounds["min_x"]),
        max_x=float(bounds["max_x"]),
        min_y=float(bounds["min_y"]),
        max_y=float(bounds["max_y"]),
        manifest=manifest,
    )


def write_track_csv(path: Path, points: list[TrackPoint]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="ascii") as handle:
        writer = csv.writer(handle)
        writer.writerow(["idx", "x", "y", "z", "roll_deg", "section", "terrain_z"])
        for point in points:
            writer.writerow(
                [
                    point.idx,
                    f"{point.x:.3f}",
                    f"{point.y:.3f}",
                    f"{point.z:.3f}",
                    f"{point.roll_deg:.3f}",
                    point.section,
                    f"{point.terrain_z:.3f}",
                ]
            )


def read_track_csv(path: Path) -> list[TrackPoint]:
    with path.open("r", newline="", encoding="ascii") as handle:
        reader = csv.DictReader(handle)
        points = []
        for row in reader:
            points.append(
                TrackPoint(
                    idx=int(row["idx"]),
                    x=float(row["x"]),
                    y=float(row["y"]),
                    z=float(row["z"]),
                    roll_deg=float(row["roll_deg"]),
                    section=row["section"],
                    terrain_z=float(row["terrain_z"]),
                )
            )
        return points


def polyline_length(points: list[tuple[float, float, float]]) -> float:
    return sum(distance(a, b) for a, b in zip(points, points[1:]))


def closed_polyline_length(points: list[tuple[float, float, float]]) -> float:
    if len(points) < 2:
        return 0.0
    return polyline_length(points) + distance(points[-1], points[0])


def distance(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return math.sqrt((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2)


def resample_polyline(points: list[tuple[float, float, float]], spacing_cm: float) -> list[tuple[float, float, float]]:
    if len(points) < 2:
        return points
    distances = [0.0]
    for a, b in zip(points, points[1:]):
        distances.append(distances[-1] + distance(a, b))
    total = distances[-1]
    sample_count = max(2, int(total / spacing_cm) + 1)
    result = []
    segment = 0
    for index in range(sample_count):
        target = min(total, index * total / (sample_count - 1))
        while segment + 1 < len(distances) and distances[segment + 1] < target:
            segment += 1
        span = max(1.0, distances[segment + 1] - distances[segment])
        t = (target - distances[segment]) / span
        a = points[segment]
        b = points[segment + 1]
        result.append((lerp(a[0], b[0], t), lerp(a[1], b[1], t), lerp(a[2], b[2], t)))
    return result


def moving_average_points(
    points: list[tuple[float, float, float]],
    radius: int,
    passes: int,
) -> list[tuple[float, float, float]]:
    current = list(points)
    for _ in range(passes):
        next_points = []
        for index in range(len(current)):
            start = max(0, index - radius)
            end = min(len(current), index + radius + 1)
            count = end - start
            sx = sum(point[0] for point in current[start:end])
            sy = sum(point[1] for point in current[start:end])
            sz = sum(point[2] for point in current[start:end])
            next_points.append((sx / count, sy / count, sz / count))
        current = next_points
    return current


def catmull_rom(
    p0: tuple[float, float, float],
    p1: tuple[float, float, float],
    p2: tuple[float, float, float],
    p3: tuple[float, float, float],
    t: float,
) -> tuple[float, float, float]:
    t2 = t * t
    t3 = t2 * t
    return tuple(
        0.5
        * (
            2.0 * p1[i]
            + (-p0[i] + p2[i]) * t
            + (2.0 * p0[i] - 5.0 * p1[i] + 4.0 * p2[i] - p3[i]) * t2
            + (-p0[i] + 3.0 * p1[i] - 3.0 * p2[i] + p3[i]) * t3
        )
        for i in range(3)
    )


def sample_closed_catmull(points: list[tuple[float, float, float]], samples_per_segment: int) -> list[tuple[float, float, float]]:
    samples = []
    count = len(points)
    for index in range(count):
        p0 = points[(index - 1) % count]
        p1 = points[index]
        p2 = points[(index + 1) % count]
        p3 = points[(index + 2) % count]
        for step in range(samples_per_segment):
            samples.append(catmull_rom(p0, p1, p2, p3, step / samples_per_segment))
    return samples
