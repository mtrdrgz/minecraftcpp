#!/usr/bin/env python3
"""
Merge all worldgen .cpp files into a single WorldGen.cpp.

Fixes include paths: when a .cpp from a subdirectory (e.g. carver/WorldCarver.cpp)
includes a header relative to its own directory (e.g. "WorldCarver.h"), the
merged file (which lives in the parent levelgen/ directory) needs the include
adjusted to "carver/WorldCarver.h".

Also fixes "../" relative includes: a file in feature/ that includes
"../RandomSource.h" needs it changed to "RandomSource.h" (since WorldGen.cpp
is in the parent).
"""

import os
import re
import sys

REPO_ROOT = "/home/z/my-project"
LEVELGEN_DIR = os.path.join(REPO_ROOT, "src/world/level/levelgen")
OUTPUT_FILE = os.path.join(LEVELGEN_DIR, "WorldGen.cpp")

# (relpath, subdirectory_prefix) — subdirectory_prefix is prepended to
# relative includes that don't start with ../
SOURCE_FILES = [
    ("Aquifer.cpp", ""),
    ("CubicSpline.cpp", ""),
    ("DensityFunction.cpp", ""),
    ("Noise.cpp", ""),
    ("Noises.cpp", ""),
    ("OreVeinifier.cpp", ""),
    ("RandomSource.cpp", ""),
    ("RandomState.cpp", ""),
    ("TerrainProvider.cpp", ""),
    ("NoiseRouterData.cpp", ""),
    ("NoiseSettings.cpp", ""),
    ("NoiseGeneratorSettings.cpp", ""),
    ("SurfaceRules.cpp", ""),
    ("SurfaceSystem.cpp", ""),
    ("SurfaceRuleData.cpp", ""),
    ("carver/WorldCarver.cpp", "carver/"),
    ("BiomeSource.cpp", ""),
    ("BiomeManager.cpp", ""),
    ("OverworldBiomeBuilder.cpp", ""),
    ("NoiseBasedChunkGenerator.cpp", ""),
    ("feature/BiomeFeatures.cpp", "feature/"),
    ("feature/BiomeDecorator.cpp", "feature/"),
    ("feature/FullChunkDecorateParityTest.cpp", "feature/"),
    ("feature/TreeGen.cpp", "feature/"),
    ("feature/OreGen.cpp", "feature/"),
    ("structure/StructureGen.cpp", "structure/"),
    ("structure/placement/StructurePlacement.cpp", "structure/placement/"),
]

# Colliding symbols in anonymous namespaces
COLLISION_RENAMES = [
    ("Aquifer.cpp", "WAY_BELOW_MIN_Y", "AQUIFER_WAY_BELOW_MIN_Y"),
    ("SurfaceSystem.cpp", "WAY_BELOW_MIN_Y", "SURFACE_SYSTEM_WAY_BELOW_MIN_Y"),
    ("NoiseBasedChunkGenerator.cpp", "floorDiv", "NOISE_FLOOR_DIV"),
    ("StructurePlacement.cpp", "floorDiv", "STRUCTURE_FLOOR_DIV"),
    ("Aquifer.cpp", "getDefaultBlockStateId", "AQUIFER_GET_DEFAULT_BLOCK_STATE_ID"),
    ("NoiseGeneratorSettings.cpp", "getDefaultBlockStateId", "NOISE_GEN_GET_DEFAULT_BLOCK_STATE_ID"),
    ("OreVeinifier.cpp", "getDefaultBlockStateId", "ORE_VEIN_GET_DEFAULT_BLOCK_STATE_ID"),
    ("BiomeFeatures.cpp", "normalizeId", "BIOME_FEATURES_NORMALIZE_ID"),
    ("StructureGen.cpp", "normalizeId", "STRUCTURE_GEN_NORMALIZE_ID"),
    ("NoiseBasedChunkGenerator.cpp", "q", "NOISE_Q"),
    ("StructurePlacement.cpp", "q", "STRUCTURE_Q"),
    ("Aquifer.cpp", "state", "AQUIFER_STATE"),
    ("NoiseGeneratorSettings.cpp", "state", "NOISE_GEN_STATE"),
    ("OreVeinifier.cpp", "state", "ORE_VEIN_STATE"),
]


def fix_include(line, subdir_prefix):
    """Fix an #include line for the merged file's directory (levelgen/).

    The merged file lives at levelgen/WorldGen.cpp. Original files lived at:
      levelgen/Foo.cpp              (depth 0 — same as WorldGen.cpp)
      levelgen/feature/Foo.cpp      (depth 1 — one level deeper)
      levelgen/structure/placement/Foo.cpp (depth 2 — two levels deeper)

    Rules for path rewriting:
    - #include <...> (system/external) → leave as-is
    - #include "../X" from a depth-N file → strip N "../" prefixes
      (e.g. "../../block/Blocks.h" from feature/ → "../block/Blocks.h")
    - #include "Foo.h" (bare, from a subdir file at depth>0) → prepend subdir
      (e.g. "WorldCarver.h" from carver/ → "carver/WorldCarver.h")
    - #include "subdir/Foo.h" (already pathed) → leave as-is
    """
    m = re.match(r'(\s*#include\s+)"([^"]+)"(.*)', line)
    if not m:
        return line  # <...> or malformed — leave as-is

    prefix, header, suffix = m.group(1), m.group(2), m.group(3)

    # Count how many "../" the original header had
    levels_up = 0
    temp = header
    while temp.startswith('../'):
        levels_up += 1
        temp = temp[3:]

    # The subdir depth = number of "/" in subdir_prefix
    subdir_depth = subdir_prefix.count('/') if subdir_prefix else 0

    # From WorldGen.cpp (at levelgen/ depth 0), we need levels_up - subdir_depth
    # "../" prefixes. This is because the original file was subdir_depth levels
    # deeper, so levels_up - subdir_depth is the number of "../" needed from
    # levelgen/ to reach the same target.
    if levels_up > 0:
        remaining_up = levels_up - subdir_depth
        if remaining_up > 0:
            new_header = '../' * remaining_up + temp
        else:
            new_header = temp
        return f'{prefix}"{new_header}"{suffix}'

    # No "../" — bare header or already-pathed
    # If it has a "/" already, it's already a path (e.g. "world/level/block/Blocks.h")
    if '/' in header:
        return line  # leave as-is

    # Bare header from a subdirectory file: "Foo.h" → "subdir/Foo.h"
    if subdir_prefix:
        return f'{prefix}"{subdir_prefix}{header}"{suffix}'

    return line


def extract_includes(content):
    includes = []
    for line in content.split('\n'):
        stripped = line.strip()
        if stripped.startswith('#include'):
            includes.append(stripped)
    return includes


def process_includes(content, subdir_prefix):
    """Remove #include lines and return (includes, body)."""
    includes = []
    body_lines = []
    for line in content.split('\n'):
        stripped = line.strip()
        if stripped.startswith('#include'):
            fixed = fix_include(stripped, subdir_prefix)
            includes.append(fixed)
        else:
            body_lines.append(line)
    return includes, '\n'.join(body_lines)


def apply_renames(content, filename):
    for fname, old, new in COLLISION_RENAMES:
        if fname == filename:
            content = re.sub(r'\b' + re.escape(old) + r'\b', new, content)
    return content


def main():
    all_includes = set()
    all_includes_ordered = []
    file_bodies = []

    for relpath, subdir_prefix in SOURCE_FILES:
        filepath = os.path.join(LEVELGEN_DIR, relpath)
        if not os.path.exists(filepath):
            print(f"WARNING: {filepath} does not exist, skipping", file=sys.stderr)
            continue

        with open(filepath) as f:
            content = f.read()

        includes, body = process_includes(content, subdir_prefix)
        for inc in includes:
            if inc not in all_includes:
                all_includes.add(inc)
                all_includes_ordered.append(inc)

        body = apply_renames(body, os.path.basename(relpath))
        body = body.strip('\n')

        file_bodies.append((relpath, body))
        print(f"  Processed {relpath} ({len(body.splitlines())} lines)")

    print(f"\nBuilding {OUTPUT_FILE}...")

    output_lines = []
    output_lines.append("// WorldGen.cpp — Unified world generation compilation unit")
    output_lines.append("//")
    output_lines.append("// This file was generated by scripts/merge_worldgen.py.")
    output_lines.append("// It merges the following .cpp files into a single compilation unit:")
    for relpath, _ in file_bodies:
        output_lines.append(f"//   - {relpath}")
    output_lines.append("//")
    output_lines.append("// Include paths have been adjusted: relative includes from subdirectory")
    output_lines.append("// files (e.g. carver/WorldCarver.cpp's \"WorldCarver.h\") are prefixed")
    output_lines.append("// with the subdirectory name (\"carver/WorldCarver.h\"). Parent-relative")
    output_lines.append("// includes (\"../Foo.h\") are collapsed to bare (\"Foo.h\").")
    output_lines.append("//")
    output_lines.append("// Anonymous-namespace symbol collisions (6 symbols across 27 files)")
    output_lines.append("// were resolved by renaming with file-specific prefixes.")
    output_lines.append("// DO NOT EDIT MANUALLY — edit the original files and re-run the merge script.")
    output_lines.append("")

    output_lines.append("// ── Unified includes (deduplicated, paths adjusted) ────────────────────")
    for inc in all_includes_ordered:
        output_lines.append(inc)
    output_lines.append("")

    for relpath, body in file_bodies:
        output_lines.append("")
        output_lines.append(f"// ═════════════════════════════════════════════════════════════════════════")
        output_lines.append(f"// BEGIN {relpath}")
        output_lines.append(f"// ═════════════════════════════════════════════════════════════════════════")
        output_lines.append(body)
        output_lines.append(f"// ═════════════════════════════════════════════════════════════════════════")
        output_lines.append(f"// END {relpath}")
        output_lines.append(f"// ═════════════════════════════════════════════════════════════════════════")

    output_content = '\n'.join(output_lines) + '\n'

    with open(OUTPUT_FILE, 'w') as f:
        f.write(output_content)

    line_count = output_content.count('\n')
    print(f"\nWritten {OUTPUT_FILE}: {line_count} lines")
    print(f"Merged {len(file_bodies)} files")
    print(f"Unique includes: {len(all_includes_ordered)}")


if __name__ == "__main__":
    main()
