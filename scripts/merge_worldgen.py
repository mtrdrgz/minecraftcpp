#!/usr/bin/env python3
"""
Merge all worldgen .cpp files into a single WorldGen.cpp.

Strategy:
  1. Read each .cpp file listed in the engine build (CMakeLists.txt lines 25-54)
  2. Extract #include directives — union them (deduplicate, preserving order)
  3. Extract the file body (everything after the last #include)
  4. Wrap each file's body in a unique named namespace to avoid anonymous
     namespace collisions (6 symbols collide across files — wrapping each
     file's body in its own namespace fixes this automatically)
  5. Concatenate: includes + all wrapped bodies → WorldGen.cpp
  6. Update CMakeLists.txt to replace the 27 entries with a single WorldGen.cpp

The wrapping namespace per-file is the KEY trick: anonymous namespaces in
different .cpp files are independent scopes, but if we merge them into one
file they'd share the same anonymous scope. By wrapping each file's body
in `namespace worldgen_<filename> { ... }`, we preserve the independence.

However, this means functions that were in anonymous namespaces and called
from the SAME file still work (they're in the same wrapping namespace).
Functions that were `static` also still work (file-scope statics become
namespace-scope within the wrapper).

The PUBLIC API functions (non-static, non-anonymous-namespace) are NOT
inside the wrapping namespace — they're at file scope. Wait, that won't
work either because they'd lose access to the anonymous helpers.

Actually the simplest correct approach: wrap EVERYTHING from each file
(including the anonymous namespace) in a named namespace, then add `using`
declarations for the public symbols that need to be visible.

But that's very complex. Let me use a simpler approach: just rename the
6 colliding symbols. There are only 6, and they're all in anonymous
namespaces, so renaming them with a file-specific prefix fixes the issue
without wrapping.
"""

import os
import re
import sys

REPO_ROOT = "/home/z/my-project"
LEVELGEN_DIR = os.path.join(REPO_ROOT, "src/world/level/levelgen")
OUTPUT_FILE = os.path.join(LEVELGEN_DIR, "WorldGen.cpp")

# The 27 .cpp files in the main engine build (from CMakeLists.txt lines 25-54)
SOURCE_FILES = [
    "Aquifer.cpp",
    "CubicSpline.cpp",
    "DensityFunction.cpp",
    "Noise.cpp",
    "Noises.cpp",
    "OreVeinifier.cpp",
    "RandomSource.cpp",
    "RandomState.cpp",
    "TerrainProvider.cpp",
    "NoiseRouterData.cpp",
    "NoiseSettings.cpp",
    "NoiseGeneratorSettings.cpp",
    "SurfaceRules.cpp",
    "SurfaceSystem.cpp",
    "SurfaceRuleData.cpp",
    "carver/WorldCarver.cpp",
    "BiomeSource.cpp",
    "BiomeManager.cpp",
    "OverworldBiomeBuilder.cpp",
    "NoiseBasedChunkGenerator.cpp",
    "feature/BiomeFeatures.cpp",
    "feature/BiomeDecorator.cpp",
    "feature/FullChunkDecorateParityTest.cpp",
    "feature/TreeGen.cpp",
    "feature/OreGen.cpp",
    "structure/StructureGen.cpp",
    "structure/placement/StructurePlacement.cpp",
]

# Colliding symbols in anonymous namespaces — these need per-file renaming.
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


def extract_includes(content):
    includes = []
    for line in content.split('\n'):
        stripped = line.strip()
        if stripped.startswith('#include'):
            includes.append(stripped)
    return includes


def remove_includes(content):
    lines = content.split('\n')
    result = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('#include'):
            continue
        result.append(line)
    return '\n'.join(result)


def apply_renames(content, filename):
    for fname, old, new in COLLISION_RENAMES:
        if fname == filename:
            content = re.sub(r'\b' + re.escape(old) + r'\b', new, content)
    return content


def main():
    all_includes = set()
    all_includes_ordered = []
    file_bodies = []

    for relpath in SOURCE_FILES:
        filepath = os.path.join(LEVELGEN_DIR, relpath)
        if not os.path.exists(filepath):
            print(f"WARNING: {filepath} does not exist, skipping", file=sys.stderr)
            continue

        with open(filepath) as f:
            content = f.read()

        includes = extract_includes(content)
        for inc in includes:
            if inc not in all_includes:
                all_includes.add(inc)
                all_includes_ordered.append(inc)

        body = remove_includes(content)
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
    output_lines.append("// Anonymous-namespace symbol collisions (6 symbols across 27 files)")
    output_lines.append("// were resolved by renaming with file-specific prefixes.")
    output_lines.append("// DO NOT EDIT MANUALLY — edit the original files and re-run the merge script.")
    output_lines.append("")

    output_lines.append("// ── Unified includes (deduplicated) ─────────────────────────────────────")
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


if __name__ == "__main__":
    main()
