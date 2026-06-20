"""Single source of truth for Yarlung terrain constants and river math.

Loads Config/yarlung-terrain.json — the SAME file C++ (`YarlungTerrain::Config`)
reads — so the asset pipeline and the runtime mesh share one definition of the
world georeferencing, encoded height range, and river centerline. Nothing else
in the Python pipeline should hardcode these values.

Parity with the C++ implementation is enforced by scripts/test_yarlung_parity.py
and the CoasterSim.Yarlung.TerrainConfigParity automation test.
"""

from __future__ import annotations

import json
import math
from pathlib import Path

_CONFIG_PATH = Path(__file__).resolve().parent.parent / "Config" / "yarlung-terrain.json"

with _CONFIG_PATH.open("r", encoding="utf-8") as _f:
    _CONFIG = json.load(_f)

SIZE: int = int(_CONFIG["grid_size"])

_bounds = _CONFIG["world_bounds_cm"]
MIN_X: float = float(_bounds["min_x"])
MAX_X: float = float(_bounds["max_x"])
MIN_Y: float = float(_bounds["min_y"])
MAX_Y: float = float(_bounds["max_y"])

_height = _CONFIG["encoded_height_cm"]
HEIGHT_MIN: float = float(_height["min"])
HEIGHT_MAX: float = float(_height["max"])

_river = _CONFIG["river"]
RIVER_ANCHOR_X: float = float(_river["anchor_x_cm"])
RIVER_ANCHOR_Y: float = float(_river["anchor_y_cm"])
RIVER_Z: float = float(_river["z_cm"])
RIVER_MASK_HALF_WIDTH_CM: float = float(_river["mask_half_width_cm"])
_RIVER_TERMS = [
    (float(t["amp_cm"]), float(t["freq"]), float(t["phase"]))
    for t in _river["centerline_terms"]
]


def smooth01(value: float) -> float:
    t = min(1.0, max(0.0, value))
    return t * t * (3.0 - 2.0 * t)


def river_center_y(x: float) -> float:
    offset_x = x - RIVER_ANCHOR_X
    return RIVER_ANCHOR_Y + sum(
        amp * math.sin(offset_x * freq + phase) for amp, freq, phase in _RIVER_TERMS
    )


def height_value_to_cm(encoded: int) -> float:
    return HEIGHT_MIN + (HEIGHT_MAX - HEIGHT_MIN) * (encoded / 65535.0)


def normalize_encoded_height_cm(height_cm: float) -> float:
    return min(1.0, max(0.0, (height_cm - HEIGHT_MIN) / (HEIGHT_MAX - HEIGHT_MIN)))
