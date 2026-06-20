"""Fast offline preview of the Yarlung terrain mesh relief.

Mirrors the C++ relief synthesis in YarlungTerrainRelief.cpp in vectorized
numpy, then hillshades the result so we can judge smooth-vs-rugged-vs-crystalline
in seconds instead of a multi-minute UE Nanite mesh rebuild. It also writes a
separate macro-landform preview for talus/buttress/gully composition; that image
is a design candidate, not a runtime mirror until explicitly ported to C++.

Tune the constants in TUNABLES, run, eyeball the PNG, then port the chosen
values back into the C++ (the noise character is governed by these params; the
exact lattice values differ but the rugged character transfers 1:1).
"""

import numpy as np
from PIL import Image
import csv

# --- map constants: shared single source of truth (same as the commandlet) ---
from yarlung_config import (
    SIZE as SRC,
    MIN_X,
    MAX_X,
    MIN_Y,
    MAX_Y,
    HEIGHT_MIN as ENC_MIN_Z,
    HEIGHT_MAX as ENC_MAX_Z,
    RIVER_ANCHOR_X as RIVER_X0,
)

HEIGHTMAP = "Content/Generated/YarlungLandscape/YarlungTsangpo_1009.r16"
TRACK_CSV = "Content/Generated/YarlungLandscape/YarlungTrack.csv"
GRID = 2017                       # mesh grid resolution (4 m spacing) — preview-only

# --- TUNABLES (port the winners to C++ YarlungTerrainReliefCm) ---
SLOPE_A, SLOPE_B = 0.0, 0.10      # SlopeGate = smooth01((slope - A) / B)
NOISE_WL_CM = 24000.0             # base octave wavelength
WARP_WL_CM = 60000.0
WARP_AMP_CM = 13000.0
OCTAVES, LACUNARITY, GAIN = 5, 2.0, 0.5
RIDGED_BIAS, RIDGED_W = 0.42, 1.9
FBM_W, STRATA_W = 0.40, 0.0
STRATA_FREQ = 0.004              # keep above the ~8 m grid Nyquist (was 0.011 -> aliased moire)
AMP_MIN_CM, AMP_MAX_CM = 450.0, 1400.0
CLIFF_FOLD_START_CM, CLIFF_FOLD_FADE_CM = 36000.0, 120000.0
CLIFF_FOLD_WARP_WL_CM = 140000.0
CLIFF_FOLD_WARP_AMP_CM = 18000.0
CLIFF_FOLD_ALONG_WL_CM = 180000.0
CLIFF_FOLD_ACROSS_WL_CM = 30000.0
CLIFF_FOLD_OCTAVES = 4
CLIFF_FOLD_BIAS, CLIFF_FOLD_W = 0.36, 1.0
CLIFF_FOLD_FBM_W = 0.20
CLIFF_FOLD_MIN, CLIFF_FOLD_MAX = -1.25, 0.35
CLIFF_FOLD_AMP_MIN_CM, CLIFF_FOLD_AMP_MAX_CM = 450.0, 1200.0

MACRO_TALUS_RAISE_CM = 5200.0
MACRO_UPPER_BACK_CUT_CM = 4200.0
MACRO_BUTTRESS_RAISE_CM = 8200.0
MACRO_GULLY_CUT_CM = 7600.0
MACRO_WET_FACE_CUT_CM = 2400.0

# First-person camera corridor approximation. This is intentionally wider than a
# single exact frame: it marks terrain likely to pass through the on-rails camera
# frustum over the ride, so relief tuning follows the coaster, not the river.
VIEW_BACKWARD_FADE_CM = 12000.0
VIEW_FAR_START_CM = 160000.0
VIEW_FAR_FADE_CM = 120000.0
VIEW_SIDE_BASE_CM = 14000.0
VIEW_SIDE_FADE_CM = 26000.0
VIEW_MAX_SIDE_CM = 180000.0
VIEW_TAN_HALF_FOV = 0.84
HERO_TRACK_DISTANCE_CM = 195150.0
HERO_CROP_HALF_WIDTH_PX = 520
HERO_CROP_HALF_HEIGHT_PX = 420


def smooth01(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def lattice_hash(ix, iy):
    M = 0xFFFFFFFF
    h = ((ix & M) * 374761393 + (iy & M) * 668265263) & M
    h = ((h ^ (h >> 13)) * 1274126177) & M
    h = (h ^ (h >> 16)) & M
    return (h & 0x00FFFFFF).astype(np.float64) / float(0x01000000)


def coherent_noise(x, y):
    fx = np.floor(x); fy = np.floor(y)
    ix = fx.astype(np.int64); iy = fy.astype(np.int64)
    tx = smooth01(x - fx); ty = smooth01(y - fy)
    a = lattice_hash(ix, iy); b = lattice_hash(ix + 1, iy)
    c = lattice_hash(ix, iy + 1); d = lattice_hash(ix + 1, iy + 1)
    return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty


def fbm(x, y, octaves, lac, gain):
    s = np.zeros_like(x); amp = 0.5; freq = 1.0
    for _ in range(octaves):
        s += amp * (coherent_noise(x * freq, y * freq) * 2 - 1)
        freq *= lac; amp *= gain
    return s


def ridged(x, y, octaves, lac, gain):
    s = np.zeros_like(x); amp = 0.5; freq = 1.0; prev = np.ones_like(x)
    for _ in range(octaves):
        n = coherent_noise(x * freq, y * freq)
        n = 1.0 - np.abs(2.0 * n - 1.0); n = n * n
        s += amp * n * prev; prev = n
        freq *= lac; amp *= gain
    return s


def river_center_y(x):
    ox = x - RIVER_X0
    return -142330.0 + 9000.0 * np.sin(ox * 0.00009 + 0.25) + 4200.0 * np.sin(ox * 0.00021 - 0.6)


def load_track_points():
    with open(TRACK_CSV, newline="", encoding="utf-8") as handle:
        return [(float(row["x"]), float(row["y"])) for row in csv.DictReader(handle)]


def sample_track_xy(track_points, distance_cm):
    count = len(track_points)
    if count < 2:
        return (0.0, 0.0)
    lengths = []
    total = 0.0
    for index, current in enumerate(track_points):
        next_point = track_points[(index + 1) % count]
        segment = np.hypot(next_point[0] - current[0], next_point[1] - current[1])
        lengths.append(segment)
        total += segment
    wrapped = distance_cm % max(total, 1.0)
    accum = 0.0
    for index, segment in enumerate(lengths):
        if wrapped <= accum + segment:
            t = (wrapped - accum) / max(segment, 1.0)
            current = track_points[index]
            next_point = track_points[(index + 1) % count]
            return (
                current[0] * (1.0 - t) + next_point[0] * t,
                current[1] * (1.0 - t) + next_point[1] * t,
            )
        accum += segment
    return track_points[-1]


def save_hero_crop(array, track_points, out_path):
    x, y = sample_track_xy(track_points, HERO_TRACK_DISTANCE_CM)
    px = int(round((x - MIN_X) / (MAX_X - MIN_X) * (GRID - 1)))
    py_world = int(round((y - MIN_Y) / (MAX_Y - MIN_Y) * (GRID - 1)))
    py = GRID - 1 - py_world
    x0 = max(px - HERO_CROP_HALF_WIDTH_PX, 0)
    x1 = min(px + HERO_CROP_HALF_WIDTH_PX, array.shape[1])
    y0 = max(py - HERO_CROP_HALF_HEIGHT_PX, 0)
    y1 = min(py + HERO_CROP_HALF_HEIGHT_PX, array.shape[0])
    crop = array[y0:y1, x0:x1].copy()
    cx = px - x0
    cy = py - y0
    if 0 <= cy < crop.shape[0] and 0 <= cx < crop.shape[1]:
        crop[max(cy - 4, 0):min(cy + 5, crop.shape[0]), max(cx - 4, 0):min(cx + 5, crop.shape[1])] = 255
    Image.fromarray(crop).save(out_path)


def view_corridor_mask(x, y, track_points):
    mask = np.zeros_like(x, dtype=np.float64)
    count = len(track_points)
    if count < 3:
        return mask

    for index, current in enumerate(track_points):
        previous = track_points[(index - 1) % count]
        next_point = track_points[(index + 1) % count]
        tx = next_point[0] - previous[0]
        ty = next_point[1] - previous[1]
        length = max(np.hypot(tx, ty), 1.0)
        tx /= length
        ty /= length

        rel_x = x - current[0]
        rel_y = y - current[1]
        forward = rel_x * tx + rel_y * ty
        lateral = np.abs(tx * rel_y - ty * rel_x)
        side_limit = np.clip(VIEW_SIDE_BASE_CM + np.maximum(forward, 0.0) * VIEW_TAN_HALF_FOV, VIEW_SIDE_BASE_CM, VIEW_MAX_SIDE_CM)
        forward_mask = smooth01((forward + VIEW_BACKWARD_FADE_CM) / VIEW_BACKWARD_FADE_CM) * (
            1.0 - smooth01((forward - VIEW_FAR_START_CM) / VIEW_FAR_FADE_CM)
        )
        side_mask = 1.0 - smooth01((lateral - side_limit) / VIEW_SIDE_FADE_CM)
        mask = np.maximum(mask, forward_mask * side_mask)

    return np.clip(mask, 0.0, 1.0)


def macro_landform_delta(x, y, base_height, slope_gate, river_dist, view_mask):
    """Photo-directed broad forms: talus apron + buttress ridges + gullies.

    This deliberately acts at 80m-400m wavelengths. It is not detail noise; it is
    the fast preview for replacing the visible heightfield wall with a readable
    mountain composition before committing to a Nanite rebuild or asset pass.
    """

    corridor_gate = smooth01((view_mask - 0.12) / 0.68)
    wall_gate = smooth01((river_dist - 30000.0) / 38000.0) * (1.0 - smooth01((river_dist - 230000.0) / 110000.0))
    lower_wall = smooth01((river_dist - 26000.0) / 34000.0) * (1.0 - smooth01((river_dist - 118000.0) / 76000.0))
    mid_wall = smooth01((river_dist - 62000.0) / 50000.0) * (1.0 - smooth01((river_dist - 205000.0) / 90000.0))
    h01 = np.clip((base_height - ENC_MIN_Z) / (ENC_MAX_Z - ENC_MIN_Z), 0.0, 1.0)
    altitude_gate = 1.0 - 0.45 * smooth01((h01 - 0.70) / 0.18)
    gate = corridor_gate * wall_gate * slope_gate * altitude_gate

    talus_profile = np.exp(-np.square((river_dist - 52000.0) / 42000.0))
    upper_back_cut = np.exp(-np.square((river_dist - 124000.0) / 72000.0))

    along_warp = 28000.0 * fbm(x / 165000.0, river_dist / 190000.0, 3, LACUNARITY, GAIN)
    buttress_phase = (x + 0.10 * river_dist + along_warp) / 132000.0
    buttress_ridges = np.power(np.clip(ridged(buttress_phase, river_dist / 210000.0, 4, 2.0, 0.5), 0.0, 1.0), 1.65)
    buttress_cross = np.exp(-np.square((river_dist - 128000.0) / 86000.0))

    gully_phase = (x + 0.22 * river_dist - 26000.0 * fbm(x / 150000.0, river_dist / 125000.0, 3, 2.0, 0.5)) / 98000.0
    gully_lines = 1.0 - np.abs(np.sin(gully_phase * np.pi))
    gully_lines = np.power(np.clip(gully_lines, 0.0, 1.0), 3.2)
    gully_lines *= 0.55 + 0.45 * smooth01(ridged(x / 180000.0, river_dist / 170000.0, 3, 2.0, 0.5))
    gully_profile = smooth01((river_dist - 38000.0) / 60000.0) * (1.0 - smooth01((river_dist - 210000.0) / 98000.0))

    wet_face_texture = 0.5 + 0.5 * fbm((x + 7331.0) / 46000.0, (river_dist - 1229.0) / 52000.0, 4, 2.0, 0.55)

    talus = MACRO_TALUS_RAISE_CM * lower_wall * talus_profile
    back_cut = -MACRO_UPPER_BACK_CUT_CM * mid_wall * upper_back_cut
    buttress = MACRO_BUTTRESS_RAISE_CM * mid_wall * buttress_cross * buttress_ridges
    gully = -MACRO_GULLY_CUT_CM * gully_profile * gully_lines
    wet_face = -MACRO_WET_FACE_CUT_CM * mid_wall * (1.0 - buttress_ridges) * wet_face_texture

    return gate * (talus + back_cut + buttress + gully + wet_face)


def main():
    track_points = load_track_points()
    raw = np.fromfile(HEIGHTMAP, dtype="<u2").astype(np.float64).reshape(SRC, SRC)
    base_src = ENC_MIN_Z + (ENC_MAX_Z - ENC_MIN_Z) * raw / 65535.0

    # bilinear upsample 1009 -> GRID
    u = np.linspace(0, SRC - 1, GRID)
    v = np.linspace(0, SRC - 1, GRID)
    x0 = np.floor(u).astype(int); x1 = np.minimum(x0 + 1, SRC - 1); tx = (u - x0)[None, :]
    y0 = np.floor(v).astype(int); y1 = np.minimum(y0 + 1, SRC - 1); ty = (v - y0)[:, None]
    top = base_src[np.ix_(y0, x0)] * (1 - tx) + base_src[np.ix_(y0, x1)] * tx
    bot = base_src[np.ix_(y1, x0)] * (1 - tx) + base_src[np.ix_(y1, x1)] * tx
    base = top * (1 - ty) + bot * ty

    X = np.linspace(MIN_X, MAX_X, GRID)[None, :] * np.ones((GRID, 1))
    Y = np.linspace(MIN_Y, MAX_Y, GRID)[:, None] * np.ones((1, GRID))
    small_x = X[::2, ::2]
    small_y = Y[::2, ::2]
    small_view_mask = view_corridor_mask(small_x, small_y, track_points)
    view_mask = np.repeat(np.repeat(small_view_mask, 2, axis=0), 2, axis=1)[:GRID, :GRID]

    dx = (MAX_X - MIN_X) / (GRID - 1)
    dy = (MAX_Y - MIN_Y) / (GRID - 1)

    # base normal.z from base gradient (for slope gate)
    gby, gbx = np.gradient(base, dy, dx)
    base_nz = 1.0 / np.sqrt(1.0 + gbx * gbx + gby * gby)
    slope = 1.0 - base_nz
    slope_gate = smooth01((slope - SLOPE_A) / SLOPE_B)

    river_dist = np.abs(Y - river_center_y(X))
    river_gate = smooth01((river_dist - 22000.0) / 24000.0)

    h01 = np.clip((base - ENC_MIN_Z) / (ENC_MAX_Z - ENC_MIN_Z), 0.0, 1.0)
    height_gate = 1.0 - 0.30 * smooth01((h01 - 0.82) / 0.14)

    ws = 1.0 / WARP_WL_CM
    wx = X + WARP_AMP_CM * fbm(X * ws, Y * ws, 2, 2.0, 0.5)
    wy = Y + WARP_AMP_CM * fbm((X + 1733.0) * ws, (Y - 911.0) * ws, 2, 2.0, 0.5)

    ns = 1.0 / NOISE_WL_CM
    rg = ridged(wx * ns, wy * ns, OCTAVES, LACUNARITY, GAIN)
    fb = fbm(wx * ns * 1.7, wy * ns * 1.7, OCTAVES, LACUNARITY, GAIN)
    strata = np.sin(base * STRATA_FREQ + fb * 3.0)

    detail = np.clip((rg - RIDGED_BIAS) * RIDGED_W + FBM_W * fb + STRATA_W * strata, -1.25, 1.25)
    amp = AMP_MIN_CM + (AMP_MAX_CM - AMP_MIN_CM) * slope_gate
    disp = slope_gate * river_gate * height_gate * amp * detail

    cliff_fold_gate = view_mask * slope_gate * river_gate * height_gate * smooth01(
        (river_dist - CLIFF_FOLD_START_CM) / CLIFF_FOLD_FADE_CM
    )
    cfs = 1.0 / CLIFF_FOLD_WARP_WL_CM
    fold_x = X + CLIFF_FOLD_WARP_AMP_CM * fbm(
        X * cfs,
        river_dist * cfs,
        3,
        LACUNARITY,
        GAIN,
    )
    fold_y = river_dist + CLIFF_FOLD_WARP_AMP_CM * fbm(
        (X - 3911.0) * cfs,
        (river_dist + 2207.0) * cfs,
        3,
        LACUNARITY,
        GAIN,
    )
    fold_ridged = ridged(
        fold_x / CLIFF_FOLD_ALONG_WL_CM,
        fold_y / CLIFF_FOLD_ACROSS_WL_CM,
        CLIFF_FOLD_OCTAVES,
        LACUNARITY,
        GAIN,
    )
    fold_fbm = fbm(
        fold_x / CLIFF_FOLD_ALONG_WL_CM * 1.65,
        fold_y / CLIFF_FOLD_ACROSS_WL_CM * 1.35,
        CLIFF_FOLD_OCTAVES,
        LACUNARITY,
        GAIN,
    )
    fold_detail = np.clip(
        (fold_ridged - CLIFF_FOLD_BIAS) * CLIFF_FOLD_W + CLIFF_FOLD_FBM_W * fold_fbm,
        CLIFF_FOLD_MIN,
        CLIFF_FOLD_MAX,
    )
    fold_amp = CLIFF_FOLD_AMP_MIN_CM + (CLIFF_FOLD_AMP_MAX_CM - CLIFF_FOLD_AMP_MIN_CM) * slope_gate
    disp += cliff_fold_gate * fold_amp * fold_detail

    z = base + disp
    macro_delta = macro_landform_delta(X, Y, base, slope_gate, river_dist, view_mask)
    z_macro = z + macro_delta

    # hillshade with sun ~ FRotator(pitch=-55, yaw=-18) -> light travel dir
    p = np.radians(-55.0); yw = np.radians(-18.0)
    ld = np.array([np.cos(p) * np.cos(yw), np.cos(p) * np.sin(yw), np.sin(p)])
    to_light = -ld
    def hillshade(height):
        gy, gx = np.gradient(height, dy, dx)
        nz = 1.0 / np.sqrt(1.0 + gx * gx + gy * gy)
        nx = -gx * nz
        ny = -gy * nz
        shade = np.clip(nx * to_light[0] + ny * to_light[1] + nz * to_light[2], 0.0, 1.0)
        return 0.15 + 0.85 * shade  # ambient floor

    shade = hillshade(z)
    macro_shade = hillshade(z_macro)

    img = (np.clip(shade, 0, 1) * 255).astype(np.uint8)
    out = "Saved/Diagnostics/terrain-relief-preview.png"
    Image.fromarray(img[::-1], "L").save(out)  # flip Y so north is up

    macro_img = (np.clip(macro_shade, 0, 1) * 255).astype(np.uint8)
    macro_out = "Saved/Diagnostics/terrain-macro-landform-preview.png"
    Image.fromarray(macro_img[::-1], "L").save(macro_out)
    macro_crop_out = "Saved/Diagnostics/terrain-macro-landform-hero-crop.png"
    save_hero_crop(macro_img[::-1], track_points, macro_crop_out)

    rgb = np.repeat(img[:, :, None], 3, axis=2).astype(np.float64)
    overlay = view_mask[:, :, None]
    orange = np.array([255.0, 132.0, 32.0])[None, None, :]
    rgb = rgb * (1.0 - 0.45 * overlay) + orange * (0.45 * overlay)
    overlay_out = "Saved/Diagnostics/terrain-relief-preview-viewcorridor.png"
    Image.fromarray(np.clip(rgb[::-1], 0, 255).astype(np.uint8), "RGB").save(overlay_out)

    macro_rgb = np.repeat(macro_img[:, :, None], 3, axis=2).astype(np.float64)
    talus_color = np.array([120.0, 160.0, 110.0])[None, None, :]
    macro_overlay = smooth01((np.abs(macro_delta) - 1200.0) / 9000.0)[:, :, None]
    macro_rgb = macro_rgb * (1.0 - 0.42 * macro_overlay) + talus_color * (0.42 * macro_overlay)
    macro_overlay_out = "Saved/Diagnostics/terrain-macro-landform-preview-overlay.png"
    Image.fromarray(np.clip(macro_rgb[::-1], 0, 255).astype(np.uint8), "RGB").save(macro_overlay_out)

    print(
        f"wrote {out}, {overlay_out}, {macro_out}, {macro_overlay_out}, and {macro_crop_out}  "
        f"disp_cm[min/mean/max]={disp.min():.0f}/{np.abs(disp).mean():.0f}/{disp.max():.0f} "
        f"macro_cm[min/mean/max]={macro_delta.min():.0f}/{np.abs(macro_delta).mean():.0f}/{macro_delta.max():.0f} "
        f"view_mask_coverage={(view_mask > 0.01).mean() * 100:.1f}%"
    )


if __name__ == "__main__":
    main()
