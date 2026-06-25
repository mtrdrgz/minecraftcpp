# structure_visualizer — Python tool for visualizing + certifying Minecraft structure templates

> **Sister tool to `tools/structure_gen_probe/`.** Both tools share the goal of
> verifying structure generation, but approach it from different angles:
>
> - `structure_gen_probe` (C++): runs the **real C++ engine** structure generator
>   (`StructureGen.cpp`) on a synthetic flat world and reports placements. This is
>   the ground-truth observation tool for what the engine does today.
> - `structure_visualizer` (Python): loads the **same NBT template files** the
>   engine uses, applies the same rotation/translation logic, and renders PNG
>   previews + runs block-by-block certification against the canonical layout.
>
> Both tools load identical source data (the `.nbt` files extracted from
> Minecraft's `client.jar` by `cmake/PrepareRuntimeAssets.cmake`). The Python
> tool exists for fast iteration on rotation/translation logic and for producing
> human-readable visualizations of single templates; the C++ tool exists for
> end-to-end engine parity testing.

## What it does

1. **Loads NBT templates** from `data/minecraft/structure/{fossil,nether_fossils}/*.nbt`
   using a minimal hand-written NBT parser (`mc_structures/nbt.py`). No external
   NBT library needed.

2. **Applies rotation + translation** as a 1:1 port of `FossilFeature.h`:
   - `transform(pos, rotation)` — matches `fossil_detail::transform` exactly
     (rot 0=NONE, 1=CW_90, 2=CW_180, 3=CCW_90)
   - `zero_position_with_transform(origin, rotation, sizeX, sizeZ)` — matches
     `fossil_detail::zeroPositionWithTransform` exactly

3. **Certifies** every (variant, rotation) pair against its reference layout
   block-by-block. The certification reports `missing`, `extra`, and `misplaced`
   blocks with exact coordinates, and **aborts with exit code 1** if any
   discrepancy is found.

4. **Renders** each variant to a PNG using `Poly3DCollection` (one explicit cube
   per block, with a small gap so adjacent blocks remain visually distinct).
   This avoids the "adjacent blocks disappear" bug that `ax.voxels()` had.

## How to run

### Prerequisites

```bash
# Extract NBT templates from Minecraft's client.jar (same as the C++ build).
# The NBT files live at:
#   data/minecraft/structure/fossil/*.nbt         (16 overworld fossils)
#   data/minecraft/structure/nether_fossils/*.nbt (14 nether fossils)
#
# Either:
#   1. Run the C++ build (PrepareRuntimeAssets.cmake) which extracts them to
#      src/assets/data/minecraft/structure/, or
#   2. Download client.jar from Mojang and unzip the structure/ subdirectory.

export MCPP_NBT_DIR=/path/to/data/minecraft/structure
```

### Certify all fossils (block-by-block)

```bash
cd tools/structure_visualizer
python3 certify_all.py
# Expected: 120/120 pass (8 overworld bone + 8 overworld coal + 14 nether) × 4 rotations
```

### Render fossils to PNG

```bash
cd tools/structure_visualizer
python3 render_all.py fossils
# Output: download/structures/*.png
```

## Scope

This tool **does not** implement the full `FossilFeature.place()` flow. It implements the **deterministic placement path** (no `BlockRotProcessor` integrity randomness, no `ProtectedBlockProcessor`, no `RuleProcessor`). For fossils, the deterministic path is the entire visible structure — processors only affect which blocks survive placement in a real world, but the canonical layout (what a fossil looks like with integrity=1.0) is what this tool certifies.

For full FossilFeature parity (including RNG-drawn integrity), use
`tools/NetherFossilPiecesParity.java` + `src/.../NetherFossilPiecesParityTest.cpp`,
which test the C++ engine against the real Java classes via reflection.

## Files

```
mc_structures/
  __init__.py
  blocks.py              # Block dataclass + color palette (~150 block types)
  nbt.py                 # minimal gzip + binary NBT parser
  renderer.py            # Poly3DCollection-based voxel renderer
  structures/
    __init__.py
    fossils.py           # NBT loader + rotation/translation (1:1 with FossilFeature.h)
  certification/
    __init__.py
    diff.py              # block-by-block diff tool (missing/extra/misplaced)

certify_all.py           # CI runner: exits 1 if any structure fails certification
render_all.py            # renders PNGs to download/structures/
WORKLOG.md               # history of methodology changes and bug fixes
```

## Why this tool exists

The C++ engine's `structure_gen_probe` reports what the engine does — but for
iterating on template loading, rotation logic, and visualization, a Python tool
is faster to edit and run. The certification harness (`certification/diff.py`)
gives a hard yes/no answer for "does this generator match the reference", which
is what `docs/STRUCTURES_STATUS.md` calls the "block-level `.mca` diff" gap.

This tool does NOT close that gap (it doesn't run the C++ engine, doesn't test
processor chains, doesn't test biome gating or heightmap scanning), but it
provides a building block: a certified-correct NBT loader + rotation pipeline
that any future C++ parity test can be checked against.
