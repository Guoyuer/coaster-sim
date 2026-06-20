"""Fast offline preview of the Yarlung terrain mesh relief.

Mirrors the C++ relief synthesis in YarlungLandscapeImportCommandlet.cpp
(YarlungTerrainReliefCm + coherent noise / fbm / ridged) in vectorized numpy,
then hillshades the result so we can judge smooth-vs-rugged-vs-crystalline in
seconds instead of a ~5 min UE Nanite mesh rebuild. Pure geometry/normal
shading -- independent of UE material/exposure.

Tune the constants in TUNABLES, run, eyeball the PNG, then port the chosen
values back into the C++ (the noise character is governed by these params; the
exact lattice values differ but the rugged character transfers 1:1).
"""

import numpy as np
from PIL import Image

# --- map constants (must match the commandlet) ---
HEIGHTMAP = "Content/Generated/YarlungLandscape/YarlungTsangpo_1009.r16"
SRC = 1009
GRID = 2017                       # mesh grid resolution (4 m spacing)
MIN_X, MAX_X = -337778.431, 337778.431
MIN_Y, MAX_Y = -416981.551, 416981.551
ENC_MIN_Z, ENC_MAX_Z = 260000.0, 730000.0
RIVER_X0 = 95543.0

# --- TUNABLES (port the winners to C++ YarlungTerrainReliefCm) ---
SLOPE_A, SLOPE_B = 0.0, 0.10      # SlopeGate = smooth01((slope - A) / B)
NOISE_WL_CM = 24000.0             # base octave wavelength
WARP_WL_CM = 60000.0
WARP_AMP_CM = 13000.0
OCTAVES, LACUNARITY, GAIN = 5, 2.0, 0.5
RIDGED_BIAS, RIDGED_W = 0.42, 1.9
FBM_W, STRATA_W = 0.40, 0.0
STRATA_FREQ = 0.004              # keep above the ~8 m grid Nyquist (was 0.011 -> aliased moire)
AMP_MIN_CM, AMP_MAX_CM = 2500.0, 7000.0


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


def main():
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

    z = base + disp

    # hillshade with sun ~ FRotator(pitch=-55, yaw=-18) -> light travel dir
    p = np.radians(-55.0); yw = np.radians(-18.0)
    ld = np.array([np.cos(p) * np.cos(yw), np.cos(p) * np.sin(yw), np.sin(p)])
    to_light = -ld
    gy, gx = np.gradient(z, dy, dx)
    nz = 1.0 / np.sqrt(1.0 + gx * gx + gy * gy)
    nx = -gx * nz; ny = -gy * nz
    shade = np.clip(nx * to_light[0] + ny * to_light[1] + nz * to_light[2], 0.0, 1.0)
    shade = 0.15 + 0.85 * shade  # ambient floor

    img = (np.clip(shade, 0, 1) * 255).astype(np.uint8)
    out = "Saved/Diagnostics/terrain-relief-preview.png"
    Image.fromarray(img[::-1], "L").save(out)  # flip Y so north is up
    print(f"wrote {out}  disp_cm[min/mean/max]={disp.min():.0f}/{np.abs(disp).mean():.0f}/{disp.max():.0f}")


if __name__ == "__main__":
    main()
