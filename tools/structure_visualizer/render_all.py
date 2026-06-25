"""Render all certified structures to PNG.

Usage:
    python /home/z/my-project/scripts/render_all.py
    python /home/z/my-project/scripts/render_all.py fossils
"""

from __future__ import annotations

import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

from mc_structures.structures import (
    fossil_overworld,
    fossil_overworld_coal,
    fossil_nether,
    OVERWORLD_FOSSILS,
    NETHER_FOSSILS,
    FOSSIL_REGISTRY,
)
from mc_structures.renderer import render_blocks


OUTPUT_DIR = "/home/z/my-project/download/structures"


def render_fossils():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print("\n=== Rendering fossils (1:1 with NBT) ===\n")

    # Render each entry in the registry (one PNG per variant, all in rot=0)
    for key, (name, variant, rotation, origin) in FOSSIL_REGISTRY.items():
        if variant.startswith("nether_fossils/"):
            blocks = fossil_nether(variant, rotation, origin)
        else:
            blocks = fossil_overworld(variant, rotation, origin)
        out_path = os.path.join(OUTPUT_DIR, f"{key}.png")
        render_blocks(
            blocks=blocks,
            title=name,
            subtitle=f"variant={variant} rot={rotation} blocks={len(blocks)}",
            output_path=out_path,
            elev=25,
            azim=-55,
            figsize=(10, 8),
        )
        print(f"  [OK] {name:25s} -> {out_path}  ({len(blocks)} blocks)")

    # Also render a 4-rotation comparison for skull_1
    print("\n=== Rendering rotation comparison: skull_1 rot=0/1/2/3 ===\n")
    for rot in (0, 1, 2, 3):
        blocks = fossil_overworld("fossil/skull_1", rot, (0, 0, 0))
        out_path = os.path.join(OUTPUT_DIR, f"fossil_skull_1_rot{rot}.png")
        rot_names = {0: "NONE", 1: "CW_90", 2: "CW_180", 3: "CCW_90"}
        render_blocks(
            blocks=blocks,
            title=f"Fossil Skull 1 — rotation {rot} ({rot_names[rot]})",
            subtitle=f"{len(blocks)} bone blocks",
            output_path=out_path,
            elev=25,
            azim=-55,
            figsize=(10, 8),
        )
        print(f"  [OK] rot={rot} ({rot_names[rot]:8s}) -> {out_path}  ({len(blocks)} blocks)")


def main():
    args = sys.argv[1:]
    if not args or "fossils" in args:
        render_fossils()


if __name__ == "__main__":
    main()
