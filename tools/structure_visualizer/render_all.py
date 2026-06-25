"""Render all certified structures to PNG.

Usage:
    python /home/z/my-project/scripts/render_all.py
    python /home/z/my-project/scripts/render_all.py fossils
    python /home/z/my-project/scripts/render_all.py mineshaft
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
    generate_mineshaft,
    MineshaftType,
)
from mc_structures.renderer import render_blocks


OUTPUT_DIR = "/home/z/my-project/download/structures"


def render_fossils():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print("\n=== Rendering fossils (1:1 with NBT) ===\n")

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


def render_mineshaft(seed: int = 1, chunkX: int = 0, chunkZ: int = 0):
    """Render a mineshaft for visual verification.

    The mineshaft is procedurally generated using the same RNG sequence as
    the C++ worker build (LegacyRandomSource + WorldgenRandom.setLargeFeatureSeed).
    The pieces (rooms, corridors, crossings, stairs) are placed at the same
    world positions the worker build would place them.

    NOTE on postProcess RNG: the per-piece postProcess RNG is NOT byte-exact
    with the worker (it uses setFeatureSeed, not a per-piece hash). This means
    cobwebs, torches, and rails may appear in slightly different positions.
    The PIECE LAYOUT (rooms/corridors/crossings/stairs + their boxes and
    orientations) IS 1:1 with the worker, certified by tools/MineshaftAssemblyParity
    .java against the real Java classes.
    """
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print(f"\n=== Rendering mineshaft (seed={seed}, chunkX={chunkX}, chunkZ={chunkZ}) ===\n")

    blocks = generate_mineshaft(seed=seed, chunkX=chunkX, chunkZ=chunkZ,
                                 mineshaft_type=MineshaftType.NORMAL)

    # Filter out cave_air (transparent — would render as dark blocks)
    visible_blocks = [b for b in blocks if b.block_type != "cave_air"]

    out_path = os.path.join(OUTPUT_DIR, "mineshaft.png")
    render_blocks(
        blocks=visible_blocks,
        title=f"Mineshaft (NORMAL)",
        subtitle=f"seed={seed} chunk=({chunkX},{chunkZ}) visible={len(visible_blocks)} total={len(blocks)}",
        output_path=out_path,
        elev=22,
        azim=-55,
        figsize=(14, 10),
    )
    print(f"  [OK] mineshaft -> {out_path}  ({len(visible_blocks)} visible blocks, {len(blocks)} total)")

    # Also render a top-down view
    out_path_top = os.path.join(OUTPUT_DIR, "mineshaft_top.png")
    render_blocks(
        blocks=visible_blocks,
        title=f"Mineshaft (NORMAL) — top view",
        subtitle=f"seed={seed} chunk=({chunkX},{chunkZ})",
        output_path=out_path_top,
        elev=80,
        azim=0,
        figsize=(14, 10),
    )
    print(f"  [OK] mineshaft (top) -> {out_path_top}")


def main():
    args = sys.argv[1:]
    if not args:
        render_fossils()
        render_mineshaft()
    elif "fossils" in args:
        render_fossils()
    elif "mineshaft" in args:
        render_mineshaft()


if __name__ == "__main__":
    main()
