"""Single source of truth for Yarlung terrain constants.

Loads Config/yarlung-terrain.json — the SAME file C++ (`YarlungTerrain::Config`)
reads — so the asset pipeline and the runtime mesh share one definition of the
world georeferencing and encoded height range. Nothing else in the Python
pipeline should hardcode these values.

Parity with the C++ implementation is enforced by scripts/test_yarlung_parity.py
and the CoasterSim.Yarlung.TerrainConfigParity automation test.
"""

from __future__ import annotations

import json
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

RIVER_MASK_HALF_WIDTH_CM: float = float(_CONFIG["river_mask_half_width_cm"])


def smooth01(value: float) -> float:
    t = min(1.0, max(0.0, value))
    return t * t * (3.0 - 2.0 * t)


def height_value_to_cm(encoded: int) -> float:
    return HEIGHT_MIN + (HEIGHT_MAX - HEIGHT_MIN) * (encoded / 65535.0)


def normalize_encoded_height_cm(height_cm: float) -> float:
    return min(1.0, max(0.0, (height_cm - HEIGHT_MIN) / (HEIGHT_MAX - HEIGHT_MIN)))
