#!/usr/bin/env python3
"""Score first-person screenshots with cheap visual-risk metrics.

These numbers do not replace looking at screenshots. They are a triage layer:
find washed-out terrain, missing forest mass, flat wall regions, and noisy/dark
foreground structure across many on-rails times before spending human attention.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
from dataclasses import dataclass, asdict
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageStat


@dataclass
class ShotMetrics:
    path: str
    width: int
    height: int
    mean_luma: float
    sky_like_frac: float
    washed_frac: float
    forest_green_frac: float
    wet_rock_frac: float
    flat_block_frac: float
    edge_density: float
    dark_structure_frac: float
    visual_risk: float


def iter_pixels(image: Image.Image, y0: int, y1: int):
    rgb = image.convert("RGB")
    pixels = rgb.load()
    width, _height = rgb.size
    for y in range(y0, y1):
        for x in range(width):
            yield pixels[x, y]


def luma(pixel: tuple[int, int, int]) -> float:
    r, g, b = pixel
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def saturation(pixel: tuple[int, int, int]) -> float:
    r, g, b = [v / 255.0 for v in pixel]
    hi = max(r, g, b)
    lo = min(r, g, b)
    if hi <= 1.0e-6:
        return 0.0
    return (hi - lo) / hi


def fraction(image: Image.Image, y0: int, y1: int, predicate) -> float:
    total = 0
    matched = 0
    for pixel in iter_pixels(image, y0, y1):
        total += 1
        if predicate(pixel):
            matched += 1
    return matched / total if total else 0.0


def flat_block_fraction(image: Image.Image, y0: int, y1: int, block: int = 24) -> float:
    gray = image.convert("L")
    width, height = gray.size
    total = 0
    flat = 0
    for y in range(y0, y1, block):
        for x in range(0, width, block):
            crop = gray.crop((x, y, min(width, x + block), min(height, y + block)))
            if crop.size[0] < block // 2 or crop.size[1] < block // 2:
                continue
            total += 1
            if ImageStat.Stat(crop).stddev[0] < 6.0:
                flat += 1
    return flat / total if total else 0.0


def edge_density(image: Image.Image, y0: int, y1: int) -> float:
    gray = image.convert("L")
    pixels = gray.load()
    width, _height = gray.size
    total = 0
    edges = 0
    for y in range(max(1, y0), y1 - 1):
        for x in range(1, width - 1):
            gx = abs(pixels[x + 1, y] - pixels[x - 1, y])
            gy = abs(pixels[x, y + 1] - pixels[x, y - 1])
            total += 1
            if gx + gy > 34:
                edges += 1
    return edges / total if total else 0.0


def score(path: Path) -> ShotMetrics:
    image = Image.open(path).convert("RGB")
    width, height = image.size
    top_end = int(height * 0.48)
    mid_start = int(height * 0.18)
    mid_end = int(height * 0.88)
    lower_start = int(height * 0.38)

    mean_luma = ImageStat.Stat(image.convert("L")).mean[0]
    sky_like = fraction(
        image,
        0,
        top_end,
        lambda p: (p[2] > p[0] * 1.03 and p[2] > p[1] * 0.93 and luma(p) > 105)
        or (luma(p) > 175 and saturation(p) < 0.22),
    )
    washed = fraction(
        image,
        mid_start,
        height,
        lambda p: luma(p) > 148 and saturation(p) < 0.24,
    )
    forest_green = fraction(
        image,
        mid_start,
        height,
        lambda p: p[1] > p[0] * 1.18 and p[1] > p[2] * 1.06 and 28 < luma(p) < 142,
    )
    wet_rock = fraction(
        image,
        mid_start,
        height,
        lambda p: 34 < luma(p) < 118 and saturation(p) < 0.30,
    )
    flat = flat_block_fraction(image, lower_start, height)
    edges = edge_density(image, mid_start, height)
    dark_structure = fraction(
        image,
        int(height * 0.30),
        height,
        lambda p: luma(p) < 42 and saturation(p) < 0.35,
    )

    # Risk is intentionally simple and monotonic: high washed/flat/dark support
    # plus low forest/edge detail means a screenshot likely reads as greybox.
    visual_risk = (
        washed * 2.2
        + flat * 1.7
        + dark_structure * 0.8
        + max(0.0, 0.10 - forest_green) * 2.0
        + max(0.0, 0.035 - edges) * 2.0
    )

    return ShotMetrics(
        path=str(path),
        width=width,
        height=height,
        mean_luma=round(mean_luma, 3),
        sky_like_frac=round(sky_like, 5),
        washed_frac=round(washed, 5),
        forest_green_frac=round(forest_green, 5),
        wet_rock_frac=round(wet_rock, 5),
        flat_block_frac=round(flat, 5),
        edge_density=round(edges, 5),
        dark_structure_frac=round(dark_structure, 5),
        visual_risk=round(visual_risk, 5),
    )


def load_font(size: int):
    try:
        return ImageFont.truetype("arial.ttf", size)
    except OSError:
        return ImageFont.load_default()


def write_contact_sheet(metrics: list[ShotMetrics], output: Path, thumb_width: int = 480) -> None:
    if not metrics:
        return
    font = load_font(16)
    small = load_font(13)
    thumbs: list[Image.Image] = []
    for item in metrics:
        image = Image.open(item.path).convert("RGB")
        ratio = thumb_width / image.size[0]
        thumb = image.resize((thumb_width, int(image.size[1] * ratio)))
        canvas = Image.new("RGB", (thumb_width, thumb.size[1] + 96), (22, 22, 22))
        canvas.paste(thumb, (0, 0))
        draw = ImageDraw.Draw(canvas)
        name = Path(item.path).name
        draw.text((8, thumb.size[1] + 6), name, fill=(235, 235, 235), font=font)
        draw.text(
            (8, thumb.size[1] + 32),
            f"risk={item.visual_risk:.2f} washed={item.washed_frac:.2f} green={item.forest_green_frac:.2f}",
            fill=(210, 210, 210),
            font=small,
        )
        draw.text(
            (8, thumb.size[1] + 54),
            f"flat={item.flat_block_frac:.2f} edge={item.edge_density:.3f} dark={item.dark_structure_frac:.2f}",
            fill=(210, 210, 210),
            font=small,
        )
        thumbs.append(canvas)

    columns = min(3, len(thumbs))
    rows = math.ceil(len(thumbs) / columns)
    width = columns * thumb_width
    height = rows * max(thumb.size[1] for thumb in thumbs)
    sheet = Image.new("RGB", (width, height), (12, 12, 12))
    cell_h = max(thumb.size[1] for thumb in thumbs)
    for index, thumb in enumerate(thumbs):
        x = (index % columns) * thumb_width
        y = (index // columns) * cell_h
        sheet.paste(thumb, (x, y))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("images", nargs="+", type=Path)
    parser.add_argument("--out-csv", type=Path, default=Path("Saved/Diagnostics/offscreen-visual-survey.csv"))
    parser.add_argument("--out-json", type=Path, default=Path("Saved/Diagnostics/offscreen-visual-survey.json"))
    parser.add_argument("--contact-sheet", type=Path, default=Path("Saved/Diagnostics/offscreen-visual-survey.png"))
    args = parser.parse_args()

    metrics = [score(path) for path in args.images]
    metrics.sort(key=lambda item: item.visual_risk, reverse=True)

    args.out_csv.parent.mkdir(parents=True, exist_ok=True)
    with args.out_csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(asdict(metrics[0]).keys()))
        writer.writeheader()
        for item in metrics:
            writer.writerow(asdict(item))

    args.out_json.write_text(json.dumps([asdict(item) for item in metrics], indent=2), encoding="utf-8")
    write_contact_sheet(metrics, args.contact_sheet)

    worst = metrics[0]
    print(
        f"analyzed={len(metrics)} worst={Path(worst.path).name} risk={worst.visual_risk:.3f} "
        f"washed={worst.washed_frac:.3f} green={worst.forest_green_frac:.3f} "
        f"flat={worst.flat_block_frac:.3f} edge={worst.edge_density:.3f} "
        f"csv={args.out_csv} sheet={args.contact_sheet}"
    )


if __name__ == "__main__":
    main()
