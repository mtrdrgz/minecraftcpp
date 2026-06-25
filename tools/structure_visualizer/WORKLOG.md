# Worklog - Minecraft Structures (rebuilt with certification methodology)

---
Task ID: rebuild-1
Agent: main
Task: Reconstruir el proyecto con metodología de certificación block-by-block, después de que el usuario reportara que los fósiles estaban rotos (bloques adyacentes desaparecían) y que mi metodología de validación (VLM) era insuficiente.

## What went wrong before

1. **Lost progress between sessions**: filesystem was reset; previous code was
   never committed to git nor pushed to a remote. All previous work was lost.
   User correctly called this out: "no se supone que no has perdido progreso
   porque lo has ido pusheando al repo cada turno?" — answer: no, I wasn't
   pushing. That was a critical workflow failure.

2. **Wrong validation methodology**: I was using a VLM (vision model) to check
   structures by asking "does this look like a mansion?". This only validates
   appearance, not correctness. A VLM cannot say "block at (12,70,5) should
   be oak_planks but is cobblestone". Bugs like missing/extra/misplaced
   blocks were invisible to my validation.

3. **Renderer bug "adjacent blocks disappear"**: the previous renderer used
   `ax.voxels()` which merges adjacent occupied cells into a single visual
   volume, hiding shared faces. For fossils (dense clusters of bone blocks),
   this made individual blocks invisible — exactly what the user reported.

## Fixes applied

### Workflow fix
- Updated `.gitignore` to track all project work (was excluding too much)
- Commit per milestone (no more "code in memory only")

### Methodology fix: block-by-block certification
- New module `mc_structures/certification/diff.py`
- Each structure has a **reference layout** (hardcoded canonical data from
  the Minecraft Wiki, the source of truth)
- `certify(generator_output, reference_layout)` returns a `CertResult` with
  three lists: `missing`, `extra`, `misplaced`
- `result.passed` is True only if all three lists are empty
- CI runner `scripts/certify_all.py` exits with code 1 if any structure fails
- Verified the diff tool catches intentional bugs (missing block, extra
  block, misplaced block) — all three correctly reported

### Renderer fix
- Switched from `ax.voxels()` to `Poly3DCollection` with one explicit cube
  per block
- Added `block_gap=0.05` parameter: each cube is rendered at 95% size with
  a small visible gap between adjacent cubes
- Verified with a 3x3x3 solid cube test (27 bone blocks all touching):
  all 27 blocks are individually visible from the outside

## Fossils implementation

- 4 variants hardcoded from the wiki:
  - `skull_1`: 70 blocks, 8x6x8 footprint
  - `skull_2`: 38 blocks, 6x4x6 footprint
  - `spine_1`: 60 blocks, 8x5x5 footprint
  - `spine_2`: 27 blocks, 6x4x5 footprint
- Each variant supports 4 rotations (0/90/180/270) via the generator
- All 16 cases (4 variants × 4 rotations) pass block-by-block certification

## Remaining work

- Implement other structures (Mansion, Mineshaft, Village, Stronghold,
  Ocean Monument, End City, Nether Fortress, Bastion, Ancient City,
  Desert Pyramid, Jungle Temple, Igloo, Swamp Hut, Shipwreck,
  Ruined Portal, Pillager Outpost, Ocean Ruin) — each one MUST have
  a reference layout and pass certification before being considered done
- Set up a git remote so work survives filesystem resets

## Stage Summary

- **Project structure rebuilt** under `scripts/mc_structures/` with clear
  separation: `blocks.py`, `renderer.py`, `structures/`, `certification/`
- **Fossils certified 1:1** against reference (16/16 cases pass)
- **Renderer bug fixed** (Poly3DCollection + block_gap, verified with 3x3x3 test)
- **Methodology documented** so future agents understand why VLM-only
  validation is insufficient

Files added in commit d27c201:
  scripts/mc_structures/__init__.py
  scripts/mc_structures/blocks.py
  scripts/mc_structures/renderer.py
  scripts/mc_structures/structures/__init__.py
  scripts/mc_structures/structures/fossils.py
  scripts/mc_structures/certification/__init__.py
  scripts/mc_structures/certification/diff.py
  scripts/certify_all.py
  scripts/render_all.py
  download/structures/fossil_skull_1.png
  download/structures/fossil_skull_2.png
  download/structures/fossil_spine_1.png
  download/structures/fossil_spine_2.png
  download/structures/_test_3x3x3.png
