"""Parity test: the Python pipeline must agree with the C++ runtime on the
shared Yarlung terrain config. The golden values below are mirrored in
Source/CoasterSim/Tests/YarlungTerrainConfigParityTests.cpp. Both sides read
Config/yarlung-terrain.json, so a drift in either implementation (or an edit to
the JSON) turns one of the two tests red instead of silently desyncing the
generated heightmap/textures from the runtime mesh.

Run: python scripts/test_yarlung_parity.py
"""

from __future__ import annotations

import sys

import yarlung_config as c


def _check(name: str, got, expected, tol: float = 1.0) -> bool:
    ok = abs(float(got) - float(expected)) <= tol
    print(f"  {'OK ' if ok else 'FAIL'} {name}: got {got!r} expected {expected!r}")
    return ok


def main() -> int:
    results = []

    results.append(_check("grid_size", c.SIZE, 1009, tol=0))
    results.append(_check("encoded_min", c.HEIGHT_MIN, 260000.0, tol=0))
    results.append(_check("encoded_max", c.HEIGHT_MAX, 730000.0, tol=0))
    results.append(_check("river_mask_half_width", c.RIVER_MASK_HALF_WIDTH_CM, 26000.0, tol=0))
    results.append(_check("min_x", c.MIN_X, -337778.4313411617))
    results.append(_check("max_x", c.MAX_X, 337778.4313411617))
    results.append(_check("min_y", c.MIN_Y, -416981.55087574443))
    results.append(_check("max_y", c.MAX_Y, 416981.55087574443))

    results.append(_check("height_value_to_cm(32768)", c.height_value_to_cm(32768), 495003.585870))

    if all(results):
        print("yarlung parity: PASS")
        return 0
    print("yarlung parity: FAIL")
    return 1


if __name__ == "__main__":
    sys.exit(main())
