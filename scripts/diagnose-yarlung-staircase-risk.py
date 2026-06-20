#!/usr/bin/env python3
"""Report first-person corridor heightfield staircase risk along the full track."""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

from yarlung_track_lib import TrackPoint, catmull_rom, load_heightfield, read_track_csv


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def normalize_xy(x: float, y: float) -> tuple[float, float]:
    length = math.hypot(x, y)
    if length < 1.0e-6:
        return 1.0, 0.0
    return x / length, y / length


def sample_track(points: list[TrackPoint], samples_per_segment: int) -> list[tuple[float, float, float, str]]:
    base = [(point.x, point.y, point.z) for point in points]
    samples = []
    count = len(points)
    for index in range(count):
        p0 = base[(index - 1) % count]
        p1 = base[index]
        p2 = base[(index + 1) % count]
        p3 = base[(index + 2) % count]
        for step in range(samples_per_segment):
            t = step / samples_per_segment
            x, y, z = catmull_rom(p0, p1, p2, p3, t)
            samples.append((x, y, z, points[index].section))
    return samples


def profile_metrics(heightfield, center: tuple[float, float, float, str], forward: tuple[float, float]) -> dict[str, float]:
    fx, fy = forward
    rx, ry = normalize_xy(-fy, fx)
    max_risk = 0.0
    max_abs_second = 0.0
    max_slope = 0.0
    risk_side = 0.0
    for side in (-1.0, 1.0):
        offsets = [2200.0 + i * 6400.0 for i in range(26)]
        heights = [
            heightfield.sample_cm(center[0] + rx * side * offset, center[1] + ry * side * offset)
            for offset in offsets
        ]
        slopes = [
            abs((heights[i + 1] - heights[i]) / max(1.0, offsets[i + 1] - offsets[i]))
            for i in range(len(heights) - 1)
        ]
        seconds = [
            abs(heights[i - 1] - 2.0 * heights[i] + heights[i + 1])
            for i in range(1, len(heights) - 1)
        ]
        side_slope = max(slopes) if slopes else 0.0
        side_second = max(seconds) if seconds else 0.0
        # Penalize large C1 slope breaks on steep, near-track slopes. Values are empirical,
        # meant to rank corridor segments before visual validation, not replace screenshots.
        side_risk = min(1.0, side_slope / 1.15) * min(1.0, side_second / 240.0)
        if side_risk > max_risk:
            max_risk = side_risk
            max_abs_second = side_second
            max_slope = side_slope
            risk_side = side
    return {
        "risk": max_risk,
        "max_abs_second_cm": max_abs_second,
        "max_slope": max_slope,
        "risk_side": risk_side,
    }


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="ascii") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "sample",
                "distance_m",
                "section",
                "x_cm",
                "y_cm",
                "z_cm",
                "risk",
                "max_abs_second_cm",
                "max_slope",
                "risk_side",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--track", default="Content/Generated/YarlungLandscape/YarlungTrack.csv")
    parser.add_argument("--samples-per-segment", type=int, default=4)
    parser.add_argument("--out", default="Saved/Diagnostics/yarlung-staircase-risk.csv")
    parser.add_argument("--fail-risk", type=float, default=0.72)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    heightfield = load_heightfield(root)
    points = read_track_csv(root / args.track)
    samples = sample_track(points, args.samples_per_segment)
    rows: list[dict[str, object]] = []
    distance_m = 0.0
    worst: dict[str, object] | None = None
    failures = 0
    for index, sample in enumerate(samples):
        prev = samples[max(0, index - 1)]
        nxt = samples[min(len(samples) - 1, index + 1)]
        fx, fy = normalize_xy(nxt[0] - prev[0], nxt[1] - prev[1])
        if index > 0:
            last = samples[index - 1]
            distance_m += math.sqrt((sample[0] - last[0]) ** 2 + (sample[1] - last[1]) ** 2 + (sample[2] - last[2]) ** 2) / 100.0
        metrics = profile_metrics(heightfield, sample, (fx, fy))
        row = {
            "sample": index,
            "distance_m": f"{distance_m:.2f}",
            "section": sample[3],
            "x_cm": f"{sample[0]:.3f}",
            "y_cm": f"{sample[1]:.3f}",
            "z_cm": f"{sample[2]:.3f}",
            "risk": f"{metrics['risk']:.3f}",
            "max_abs_second_cm": f"{metrics['max_abs_second_cm']:.2f}",
            "max_slope": f"{metrics['max_slope']:.3f}",
            "risk_side": "right" if metrics["risk_side"] > 0 else "left",
        }
        rows.append(row)
        if metrics["risk"] >= args.fail_risk:
            failures += 1
        if worst is None or float(row["risk"]) > float(worst["risk"]):
            worst = row

    out_path = root / args.out
    write_csv(out_path, rows)
    print(f"[YARLUNG-STAIR] samples={len(rows)} fail_risk={args.fail_risk:.2f} failures={failures}")
    if worst:
        print(
            "[YARLUNG-STAIR] worst "
            f"sample={worst['sample']} distance_m={worst['distance_m']} section={worst['section']} "
            f"risk={worst['risk']} side={worst['risk_side']} second_cm={worst['max_abs_second_cm']} "
            f"slope={worst['max_slope']}"
        )
    print(f"[YARLUNG-STAIR] csv={out_path}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
