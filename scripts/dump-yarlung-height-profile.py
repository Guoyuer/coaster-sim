#!/usr/bin/env python3
"""Dump steep Yarlung heightmap scanline profiles for terrain diagnostics."""

from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def read_heightmap(root: Path) -> tuple[list[float], dict[str, object]]:
    manifest_path = root / "Content/Generated/YarlungLandscape/manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    width, height = manifest["resolution"]
    encoded_min_m = float(manifest["height_range_cm"]["encoded_min"]) / 100.0
    encoded_max_m = float(manifest["height_range_cm"]["encoded_max"]) / 100.0
    heightmap_path = root / "Content/Generated/YarlungLandscape" / str(manifest["heightmap"])
    raw = heightmap_path.read_bytes()
    values = struct.unpack("<" + "H" * (len(raw) // 2), raw)
    heights = [
        encoded_min_m + (encoded_max_m - encoded_min_m) * (value / 65535.0)
        for value in values
    ]
    metadata = {
        "manifest": manifest,
        "width": int(width),
        "height": int(height),
        "heightmap_path": heightmap_path,
    }
    return heights, metadata


def profile_stats(points: list[float]) -> dict[str, float | int]:
    deltas = [points[index + 1] - points[index] for index in range(len(points) - 1)]
    second = [deltas[index + 1] - deltas[index] for index in range(len(deltas) - 1)]
    return {
        "min_m": min(points),
        "max_m": max(points),
        "relief_m": max(points) - min(points),
        "unique_cm_rounded": len(set(round(point, 2) for point in points)),
        "flat_pairs_lt_15cm": sum(1 for delta in deltas if abs(delta) < 0.15),
        "big_pairs_gt_3m": sum(1 for delta in deltas if abs(delta) > 3.0),
        "kinks_gt_1_5m": sum(1 for value in second if abs(value) > 1.5),
        "mean_abs_second_delta_m": statistics.mean(abs(value) for value in second),
    }


def write_csv(
    output: Path,
    points: list[float],
    x0: int,
    y: int,
    width: int,
    height: int,
    manifest: dict[str, object],
) -> None:
    bounds = manifest["world_bounds_cm"]
    deltas = [points[index + 1] - points[index] for index in range(len(points) - 1)]
    second = [deltas[index + 1] - deltas[index] for index in range(len(deltas) - 1)]
    with output.open("w", newline="", encoding="ascii") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "sample",
                "x_index",
                "y_index",
                "world_x_m",
                "world_y_m",
                "height_m",
                "delta_to_next_m",
                "second_delta_m",
            ]
        )
        for index, height_m in enumerate(points):
            x = x0 + index
            world_x_m = (
                float(bounds["min_x"])
                + (float(bounds["max_x"]) - float(bounds["min_x"])) * x / (width - 1)
            ) / 100.0
            world_y_m = (
                float(bounds["min_y"])
                + (float(bounds["max_y"]) - float(bounds["min_y"])) * y / (height - 1)
            ) / 100.0
            writer.writerow(
                [
                    index,
                    x,
                    y,
                    f"{world_x_m:.3f}",
                    f"{world_y_m:.3f}",
                    f"{height_m:.3f}",
                    f"{deltas[index]:.3f}" if index < len(deltas) else "",
                    f"{second[index]:.3f}" if index < len(second) else "",
                ]
            )


def write_plot(output: Path, points: list[float], title: str) -> None:
    deltas = [points[index + 1] - points[index] for index in range(len(points) - 1)]
    width, height = 1200, 720
    left, right, top, bottom = 80, 40, 60, 80
    plot_width = width - left - right
    plot_height = height - top - bottom
    image = Image.new("RGB", (width, height), (248, 248, 246))
    draw = ImageDraw.Draw(image)
    try:
        font = ImageFont.truetype("arial.ttf", 16)
        small = ImageFont.truetype("arial.ttf", 12)
    except OSError:
        font = small = None

    min_h = min(points)
    max_h = max(points)
    x_axis = left
    y_axis = height - bottom
    draw.line((left, y_axis, width - right, y_axis), fill=(30, 30, 30), width=2)
    draw.line((left, y_axis, left, top), fill=(30, 30, 30), width=2)

    for index in range(6):
        y = y_axis - index * plot_height / 5
        value = min_h + index * (max_h - min_h) / 5
        draw.line((left, y, width - right, y), fill=(220, 220, 220))
        draw.text((8, y - 8), f"{value:.0f}m", fill=(50, 50, 50), font=small)

    for index in range(0, len(points), 12):
        x = x_axis + index * plot_width / (len(points) - 1)
        draw.line((x, y_axis, x, top), fill=(232, 232, 232))
        draw.text((x - 10, y_axis + 8), str(index), fill=(70, 70, 70), font=small)

    line_points = []
    for index, point in enumerate(points):
        x = x_axis + index * plot_width / (len(points) - 1)
        y = y_axis - (point - min_h) / max(max_h - min_h, 1.0) * plot_height
        line_points.append((x, y))
    draw.line(line_points, fill=(25, 92, 155), width=3)
    for x, y in line_points:
        draw.ellipse((x - 2, y - 2, x + 2, y + 2), fill=(25, 92, 155))

    bar_base = height - 32
    bar_height = 54
    max_delta = max(abs(delta) for delta in deltas)
    for index, delta in enumerate(deltas):
        x = x_axis + index * plot_width / (len(deltas) - 1)
        y = bar_base - abs(delta) / max_delta * bar_height
        draw.line((x, bar_base, x, y), fill=(178, 92, 35), width=2)

    draw.text((left, 20), title, fill=(20, 20, 20), font=font)
    draw.text(
        (left, 42),
        (
            f"No flat quantized plateaus: {len(set(round(point, 2) for point in points))}"
            f"/{len(points)} unique heights; relief {max_h - min_h:.1f}m"
        ),
        fill=(50, 50, 50),
        font=small,
    )
    draw.text(
        (left, height - 22),
        "orange bars = abs(delta to next sample); repeated lengths indicate bilinear piecewise slopes",
        fill=(70, 70, 70),
        font=small,
    )
    image.save(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", default="Saved/Diagnostics")
    parser.add_argument("--samples", type=int, default=96)
    parser.add_argument("--profile-count", type=int, default=3)
    args = parser.parse_args()

    root = Path.cwd()
    heights, metadata = read_heightmap(root)
    manifest = metadata["manifest"]
    width = int(metadata["width"])
    height = int(metadata["height"])
    bounds = manifest["world_bounds_cm"]
    x_spacing_m = (float(bounds["max_x"]) - float(bounds["min_x"])) / (width - 1) / 100.0
    y_spacing_m = (float(bounds["max_y"]) - float(bounds["min_y"])) / (height - 1) / 100.0

    def sample(x: int, y: int) -> float:
        return heights[y * width + x]

    rows: list[tuple[float, float, int]] = []
    for y in range(2, height - 2):
        gradients: list[float] = []
        for x in range(2, width - 2):
            dzdx = (sample(x + 1, y) - sample(x - 1, y)) / (2 * x_spacing_m)
            dzdy = (sample(x, y + 1) - sample(x, y - 1)) / (2 * y_spacing_m)
            gradients.append(math.hypot(dzdx, dzdy))
        sorted_gradients = sorted(gradients)
        rows.append(
            (
                statistics.mean(sorted_gradients[-80:]),
                sorted_gradients[int(len(sorted_gradients) * 0.95)],
                y,
            )
        )

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    candidates: list[tuple[float, float, int, int, dict[str, float | int]]] = []
    for _, _, y in sorted(rows, reverse=True)[:8]:
        for x0 in range(2, width - args.samples - 2):
            if x0 <= 820 and x0 + args.samples >= 780:
                continue
            points = [sample(x, y) for x in range(x0, x0 + args.samples)]
            stats = profile_stats(points)
            candidates.append(
                (
                    float(stats["relief_m"]),
                    float(stats["mean_abs_second_delta_m"]),
                    x0,
                    y,
                    stats,
                )
            )

    selected = sorted(candidates, reverse=True)[: args.profile_count]
    print(
        f"heightmap={metadata['heightmap_path']} size={width}x{height} "
        f"spacing={x_spacing_m:.3f}m,{y_spacing_m:.3f}m"
    )
    for index, (relief, roughness, x0, y, stats) in enumerate(selected, 1):
        points = [sample(x, y) for x in range(x0, x0 + args.samples)]
        csv_path = out_dir / f"yarlung-height-profile-{index}.csv"
        png_path = out_dir / f"yarlung-height-profile-{index}.png"
        write_csv(csv_path, points, x0, y, width, height, manifest)
        write_plot(
            png_path,
            points,
            f"Yarlung .r16 profile {index}: x={x0}-{x0 + args.samples - 1}, y={y}",
        )
        print(
            f"profile{index}: x={x0}-{x0 + args.samples - 1} y={y} "
            f"relief={relief:.1f}m unique={stats['unique_cm_rounded']}/{args.samples} "
            f"flats={stats['flat_pairs_lt_15cm']} big={stats['big_pairs_gt_3m']} "
            f"kinks={stats['kinks_gt_1_5m']} mean_abs_d2={roughness:.3f} "
            f"csv={csv_path} plot={png_path}"
        )


if __name__ == "__main__":
    main()
