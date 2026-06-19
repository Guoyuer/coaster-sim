#!/usr/bin/env python3
"""Render a legible P4 scenery diagnostic overlay for the Yarlung corridor."""

from __future__ import annotations

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageOps

from yarlung_track_lib import TrackPoint, closed_polyline_length, load_heightfield, read_track_csv


SECTION_COLORS = {
    "Station": (255, 255, 255),
    "Lift": (255, 210, 64),
    "Outbound": (40, 180, 255),
    "Turnaround": (255, 120, 40),
    "Return": (80, 230, 120),
    "Launch": (255, 70, 180),
    "Brake": (255, 50, 50),
}

RIVER_FILL = (0, 210, 255)
PANEL_BG = (18, 22, 24)
PANEL_FG = (232, 238, 238)
PANEL_MUTED = (155, 168, 168)


def quantile(values: list[float], ratio: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    pos = (len(ordered) - 1) * ratio
    low = int(math.floor(pos))
    high = int(math.ceil(pos))
    if low == high:
        return ordered[low]
    return ordered[low] + (ordered[high] - ordered[low]) * (pos - low)


def clamp_int(value: float, low: int, high: int) -> int:
    return max(low, min(high, int(round(value))))


def load_river_channel(path: Path) -> tuple[list[int], int, int]:
    image = Image.open(path).convert("RGB")
    width, height = image.size
    data = image.tobytes()
    red = list(data[0::3])
    return red, width, height


def river_centroid_by_column(river: list[int], width: int, height: int) -> list[float]:
    centroids: list[float | None] = []
    for x in range(width):
        total = 0.0
        weighted_y = 0.0
        for y in range(height):
            value = river[y * width + x]
            if value <= 0:
                continue
            total += value
            weighted_y += y * value
        centroids.append(weighted_y / total if total > 0.0 else None)

    last = None
    for x, value in enumerate(centroids):
        if value is None:
            centroids[x] = last
        else:
            last = value
    last = None
    for x in range(width - 1, -1, -1):
        if centroids[x] is None:
            centroids[x] = last
        else:
            last = centroids[x]
    return [float(value if value is not None else height * 0.5) for value in centroids]


def chamfer_distance_to_mask(mask: list[bool], width: int, height: int) -> list[float]:
    inf = 1.0e9
    diagonal = math.sqrt(2.0)
    dist = [0.0 if value else inf for value in mask]

    for y in range(height):
        row = y * width
        for x in range(width):
            index = row + x
            best = dist[index]
            if x > 0:
                best = min(best, dist[index - 1] + 1.0)
            if y > 0:
                best = min(best, dist[index - width] + 1.0)
                if x > 0:
                    best = min(best, dist[index - width - 1] + diagonal)
                if x + 1 < width:
                    best = min(best, dist[index - width + 1] + diagonal)
            dist[index] = best

    for y in range(height - 1, -1, -1):
        row = y * width
        for x in range(width - 1, -1, -1):
            index = row + x
            best = dist[index]
            if x + 1 < width:
                best = min(best, dist[index + 1] + 1.0)
            if y + 1 < height:
                best = min(best, dist[index + width] + 1.0)
                if x > 0:
                    best = min(best, dist[index + width - 1] + diagonal)
                if x + 1 < width:
                    best = min(best, dist[index + width + 1] + diagonal)
            dist[index] = best

    return dist


def make_overlay_base(hillshade_path: Path, river: list[int], width: int, height: int) -> Image.Image:
    hillshade = Image.open(hillshade_path).convert("L")
    hillshade = ImageOps.autocontrast(hillshade, cutoff=1)
    base = Image.merge("RGB", (hillshade, hillshade, hillshade))
    data = base.tobytes()
    blended = []
    for index, river_value in enumerate(river):
        pixel = data[index * 3 : index * 3 + 3]
        alpha = min(0.62, river_value / 255.0 * 0.58)
        blended.append(
            tuple(
                int(round(pixel[channel] * (1.0 - alpha) + RIVER_FILL[channel] * alpha))
                for channel in range(3)
            )
        )
    result = Image.new("RGB", (width, height))
    result.putdata(blended)
    return result


def to_scaled_pixel(point: TrackPoint, heightfield, scale: int) -> tuple[int, int]:
    gx, gy = heightfield.world_to_grid(point.x, point.y)
    return int(round(gx * scale)), int(round(gy * scale))


def draw_text_box(draw: ImageDraw.ImageDraw, xy: tuple[int, int], text: str, font: ImageFont.ImageFont) -> None:
    bbox = draw.textbbox(xy, text, font=font)
    pad = 3
    draw.rectangle(
        (bbox[0] - pad, bbox[1] - pad, bbox[2] + pad, bbox[3] + pad),
        fill=(0, 0, 0),
        outline=(255, 255, 255),
    )
    draw.text(xy, text, fill=(255, 255, 255), font=font)


def draw_track(draw: ImageDraw.ImageDraw, points: list[TrackPoint], heightfield, scale: int, font) -> None:
    scaled = [to_scaled_pixel(point, heightfield, scale) for point in points]
    for index, (a, b) in enumerate(zip(scaled, scaled[1:] + scaled[:1])):
        section = points[index].section
        draw.line((a, b), fill=(0, 0, 0), width=max(7, 7 * scale), joint="curve")
    for index, (a, b) in enumerate(zip(scaled, scaled[1:] + scaled[:1])):
        section = points[index].section
        draw.line((a, b), fill=SECTION_COLORS.get(section, (255, 255, 255)), width=max(3, 3 * scale), joint="curve")

    seen = set()
    for point, pixel in zip(points, scaled):
        if point.section in seen:
            continue
        seen.add(point.section)
        draw.ellipse(
            (
                pixel[0] - 4 * scale,
                pixel[1] - 4 * scale,
                pixel[0] + 4 * scale,
                pixel[1] + 4 * scale,
            ),
            fill=SECTION_COLORS.get(point.section, (255, 255, 255)),
            outline=(0, 0, 0),
            width=max(1, scale),
        )
        draw_text_box(draw, (pixel[0] + 6 * scale, pixel[1] - 6 * scale), point.section, font)

    for index in range(0, len(scaled), 20):
        a = scaled[index]
        b = scaled[(index + 2) % len(scaled)]
        dx = b[0] - a[0]
        dy = b[1] - a[1]
        length = math.hypot(dx, dy)
        if length < 1.0:
            continue
        ux = dx / length
        uy = dy / length
        left = (-uy, ux)
        tip = (a[0] + ux * 10 * scale, a[1] + uy * 10 * scale)
        p1 = (a[0] - ux * 6 * scale + left[0] * 5 * scale, a[1] - uy * 6 * scale + left[1] * 5 * scale)
        p2 = (a[0] - ux * 6 * scale - left[0] * 5 * scale, a[1] - uy * 6 * scale - left[1] * 5 * scale)
        draw.polygon((tip, p1, p2), fill=(0, 0, 0))


def track_pixel_bbox(points: list[TrackPoint], heightfield, scale: int, width: int, height: int) -> tuple[int, int, int, int]:
    pixels = [to_scaled_pixel(point, heightfield, scale) for point in points]
    margin = 110 * scale
    left = max(0, min(pixel[0] for pixel in pixels) - margin)
    top = max(0, min(pixel[1] for pixel in pixels) - margin)
    right = min(width, max(pixel[0] for pixel in pixels) + margin)
    bottom = min(height, max(pixel[1] for pixel in pixels) + margin)
    return left, top, right, bottom


def point_river_rows(
    points: list[TrackPoint],
    heightfield,
    river: list[int],
    river_centroids: list[float],
    mask_distance_px: list[float],
    width: int,
    height: int,
) -> list[dict[str, object]]:
    meters_per_x_px = heightfield.x_spacing_cm / 100.0
    meters_per_y_px = heightfield.y_spacing_cm / 100.0
    average_meters_per_px = (meters_per_x_px + meters_per_y_px) * 0.5
    rows = []
    for point in points:
        gx, gy = heightfield.world_to_grid(point.x, point.y)
        px = clamp_int(gx, 0, width - 1)
        py = clamp_int(gy, 0, height - 1)
        river_value = river[py * width + px]
        centerline_offset_m = abs(gy - river_centroids[px]) * meters_per_y_px
        mask_edge_distance_m = mask_distance_px[py * width + px] * average_meters_per_px
        rows.append(
            {
                "idx": point.idx,
                "section": point.section,
                "grid_x": f"{gx:.2f}",
                "grid_y": f"{gy:.2f}",
                "river_mask": river_value,
                "centerline_offset_m": f"{centerline_offset_m:.2f}",
                "mask_edge_distance_m": f"{mask_edge_distance_m:.2f}",
            }
        )
    return rows


def summarize(rows: list[dict[str, object]]) -> tuple[list[str], dict[str, list[float]]]:
    offsets = [float(row["centerline_offset_m"]) for row in rows]
    mask_distances = [float(row["mask_edge_distance_m"]) for row in rows]
    inside_count = sum(1 for row in rows if int(row["river_mask"]) > 24)
    lines = [
        f"river-mask samples: {inside_count}/{len(rows)} ({inside_count / max(1, len(rows)) * 100:.1f}%)",
        f"centerline offset m: med {quantile(offsets, 0.50):.1f} / p90 {quantile(offsets, 0.90):.1f} / max {max(offsets):.1f}",
        f"river-mask edge dist m: med {quantile(mask_distances, 0.50):.1f} / p90 {quantile(mask_distances, 0.90):.1f} / max {max(mask_distances):.1f}",
    ]

    by_section: dict[str, list[float]] = defaultdict(list)
    for row in rows:
        by_section[str(row["section"])].append(float(row["centerline_offset_m"]))
    return lines, by_section


def draw_panel(
    canvas: Image.Image,
    panel_x: int,
    points: list[TrackPoint],
    summary_lines: list[str],
    by_section: dict[str, list[float]],
    csv_path: Path,
) -> None:
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.load_default()
    title_font = ImageFont.load_default()
    x = panel_x + 24
    y = 28
    draw.text((x, y), "P4 scenery overlay", fill=PANEL_FG, font=title_font)
    y += 28
    draw.text((x, y), "Base: raw hillshade, autocontrast", fill=PANEL_MUTED, font=font)
    y += 18
    draw.text((x, y), "Cyan: river-mask red channel", fill=PANEL_MUTED, font=font)
    y += 18
    draw.text((x, y), "Track: YarlungTrack.csv by section", fill=PANEL_MUTED, font=font)
    y += 30

    length_m = closed_polyline_length([(p.x, p.y, p.z) for p in points]) / 100.0
    for line in [f"track length: {length_m:.1f} m", f"control points: {len(points)}", *summary_lines]:
        draw.text((x, y), line, fill=PANEL_FG, font=font)
        y += 18

    y += 14
    draw.text((x, y), "Section centerline offset (med/p90/max m)", fill=PANEL_FG, font=font)
    y += 22
    for section in SECTION_COLORS:
        values = by_section.get(section)
        if not values:
            continue
        color = SECTION_COLORS[section]
        draw.rectangle((x, y + 3, x + 12, y + 15), fill=color, outline=(0, 0, 0))
        draw.text(
            (x + 20, y),
            f"{section}: {quantile(values, 0.50):.0f}/{quantile(values, 0.90):.0f}/{max(values):.0f}",
            fill=PANEL_FG,
            font=font,
        )
        y += 18

    y += 18
    draw.text((x, y), "Review use:", fill=PANEL_FG, font=font)
    y += 18
    for line in [
        "1. Confirm track visibly hugs the river corridor.",
        "2. Confirm the hero corridor has river + canyon walls.",
        "3. If composition is weak, fix XY before art polish.",
    ]:
        draw.text((x, y), line, fill=PANEL_MUTED, font=font)
        y += 18

    y += 18
    draw.text((x, y), f"CSV: {csv_path.as_posix()}", fill=PANEL_MUTED, font=font)


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="ascii") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--track", default="Content/Generated/YarlungLandscape/YarlungTrack.csv")
    parser.add_argument("--hillshade", default="Content/Generated/YarlungLandscape/YarlungTsangpo_hillshade.png")
    parser.add_argument("--masks", default="Content/Generated/YarlungLandscape/YarlungTsangpo_masks.ppm")
    parser.add_argument("--out", default="Saved/Diagnostics/yarlung-scenery-overlay.png")
    parser.add_argument("--crop-out", default="Saved/Diagnostics/yarlung-scenery-overlay-crop.png")
    parser.add_argument("--csv", default="Saved/Diagnostics/yarlung-scenery-overlay.csv")
    parser.add_argument("--scale", type=int, default=3)
    parser.add_argument("--river-threshold", type=int, default=24)
    args = parser.parse_args()

    root = Path.cwd()
    heightfield = load_heightfield(root)
    points = read_track_csv(root / args.track)
    river, width, height = load_river_channel(root / args.masks)
    mask = [value > args.river_threshold for value in river]
    river_centroids = river_centroid_by_column(river, width, height)
    mask_distance_px = chamfer_distance_to_mask(mask, width, height)

    rows = point_river_rows(points, heightfield, river, river_centroids, mask_distance_px, width, height)
    csv_path = Path(args.csv)
    write_csv(root / csv_path, rows)
    summary_lines, by_section = summarize(rows)

    base = make_overlay_base(root / args.hillshade, river, width, height)
    resample = getattr(Image, "Resampling", Image).LANCZOS
    scaled = base.resize((width * args.scale, height * args.scale), resample)
    map_with_track = scaled.copy()
    map_draw = ImageDraw.Draw(map_with_track)
    draw_track(map_draw, points, heightfield, args.scale, ImageFont.load_default())

    panel_width = 760
    canvas = Image.new("RGB", (map_with_track.width + panel_width, map_with_track.height), PANEL_BG)
    canvas.paste(map_with_track, (0, 0))
    draw_panel(canvas, scaled.width, points, summary_lines, by_section, csv_path)

    output = root / args.out
    output.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(output)
    crop_output = root / args.crop_out
    crop_output.parent.mkdir(parents=True, exist_ok=True)
    map_with_track.crop(track_pixel_bbox(points, heightfield, args.scale, map_with_track.width, map_with_track.height)).save(crop_output)

    print(f"wrote {output}")
    print(f"wrote {crop_output}")
    print(f"wrote {root / csv_path}")
    for line in summary_lines:
        print(line)


if __name__ == "__main__":
    main()
