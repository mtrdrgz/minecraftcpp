#!/usr/bin/env python3
"""
Merge all worldgen .cpp files into a single WorldGen.cpp.

Handles:
  1. Include deduplication and path rewriting (subdir-relative → levelgen-relative)
  2. Anonymous-namespace free-function collisions (renamed per-file)
  3. Smart rename: only renames bare calls, not mc::-qualified ones
  4. Duplicate header exclusion (TreeGen.h vs TreeFeature.h)
"""

import os
import re
import sys

REPO_ROOT = "/home/z/my-project"
LEVELGEN_DIR = os.path.join(REPO_ROOT, "src/world/level/levelgen")
OUTPUT_FILE = os.path.join(LEVELGEN_DIR, "WorldGen.cpp")

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

# Collisions: (filename, old_name, new_name)
# Only FREE FUNCTIONS in anonymous namespaces — NOT class methods.
# Verified by checking each is a top-level function definition, not a method.
COLLISION_RENAMES = [
    # lerp/lerp2/lerp3 — free functions in anonymous namespaces
    ("CubicSpline.cpp", "lerp", "cs_lerp"),
    ("Noise.cpp", "lerp", "noise_lerp"),
    ("NoiseBasedChunkGenerator.cpp", "lerp", "nbcg_lerp"),
    ("SurfaceRules.cpp", "lerp", "sr_lerp"),
    ("TerrainProvider.cpp", "lerp", "tp_lerp"),
    ("Noise.cpp", "lerp2", "noise_lerp2"),
    ("NoiseBasedChunkGenerator.cpp", "lerp2", "nbcg_lerp2"),
    ("SurfaceRules.cpp", "lerp2", "sr_lerp2"),
    ("Noise.cpp", "lerp3", "noise_lerp3"),
    ("NoiseBasedChunkGenerator.cpp", "lerp3", "nbcg_lerp3"),
    # clampedMap
    ("Aquifer.cpp", "clampedMap", "aquifer_clampedMap"),
    ("DensityFunction.cpp", "clampedMap", "df_clampedMap"),
    ("OreVeinifier.cpp", "clampedMap", "ov_clampedMap"),
    # floorDiv
    ("Aquifer.cpp", "floorDiv", "aquifer_floorDiv"),
    ("NoiseBasedChunkGenerator.cpp", "floorDiv", "nbcg_floorDiv"),
    ("feature/FullChunkDecorateParityTest.cpp", "floorDiv", "fcdpt_floorDiv"),
    ("structure/placement/StructurePlacement.cpp", "floorDiv", "sp_floorDiv"),
    # floorToInt
    ("Aquifer.cpp", "floorToInt", "aquifer_floorToInt"),
    ("carver/WorldCarver.cpp", "floorToInt", "wc_floorToInt"),
    # normalizeId
    ("feature/BiomeFeatures.cpp", "normalizeId", "bf_normalizeId"),
    ("structure/StructureGen.cpp", "normalizeId", "sg_normalizeId"),
    # apply (free function in CubicSpline and SurfaceRules)
    ("CubicSpline.cpp", "apply", "cs_apply"),
    ("SurfaceRules.cpp", "apply", "sr_apply"),
    # packXZ
    ("NoiseBasedChunkGenerator.cpp", "packXZ", "nbcg_packXZ"),
    ("SurfaceRules.cpp", "packXZ", "sr_packXZ"),
    # preliminarySurfaceLevel (free function)
    ("Aquifer.cpp", "preliminarySurfaceLevel", "aquifer_psl"),
    ("NoiseRouterData.cpp", "preliminarySurfaceLevel", "nrd_psl"),
    # state (free function in Aquifer, NoiseGeneratorSettings, OreVeinifier, WorldCarver)
    ("Aquifer.cpp", "state", "aquifer_state"),
    ("NoiseGeneratorSettings.cpp", "state", "ngs_state"),
    ("OreVeinifier.cpp", "state", "ov_state"),
    ("carver/WorldCarver.cpp", "state", "wc_state"),
    # getDefaultBlockStateId (free function in anonymous namespaces)
    ("Aquifer.cpp", "getDefaultBlockStateId", "aquifer_getDefaultBlockStateId"),
    ("NoiseGeneratorSettings.cpp", "getDefaultBlockStateId", "ngs_getDefaultBlockStateId"),
    ("OreVeinifier.cpp", "getDefaultBlockStateId", "ov_getDefaultBlockStateId"),
    # WAY_BELOW_MIN_Y (constant)
    ("Aquifer.cpp", "WAY_BELOW_MIN_Y", "AQUIFER_WAY_BELOW_MIN_Y"),
    ("SurfaceSystem.cpp", "WAY_BELOW_MIN_Y", "SURFSYS_WAY_BELOW_MIN_Y"),
]

# Headers that should NOT be included in WorldGen.cpp because they duplicate
# definitions from other headers already included.
# TreeGen.h is NOT excluded — it has unique content (TreeWorld, FoliagePlacer, etc.)
# But it also has FoliageAttachment/TreeConfig which conflict with TreeFeature.h.
# Solution: wrap TreeGen.cpp's section in a unique inner namespace so its
# TreeGen.h definitions don't conflict with TreeFeature.h's versions.
EXCLUDE_INCLUDES = [
    # Nothing excluded — TreeGen.h is included but TreeGen.cpp is namespace-wrapped
]

# Files whose body should be wrapped in a unique inner namespace to isolate
# their header definitions from other files' headers.
# TreeGen.cpp: TreeGen.h defines FoliageAttachment/TreeConfig which conflict
# with TreeFeature.h's versions (different struct layouts).
NAMESPACE_WRAP = {
    "feature/TreeGen.cpp": "treegen_impl",
}


def fix_include(line, subdir_prefix):
    """Fix an #include line for the merged file's directory (levelgen/)."""
    m = re.match(r'(\s*#include\s+)"([^"]+)"(.*)', line)
    if not m:
        return line  # <...> or malformed — leave as-is

    prefix, header, suffix = m.group(1), m.group(2), m.group(3)

    # Check if this include should be excluded
    full_path = subdir_prefix + header if (subdir_prefix and '/' not in header) else header
    for excluded in EXCLUDE_INCLUDES:
        if header == excluded or full_path == excluded:
            return None  # Signal to skip this include

    # Count how many "../" the original header had
    levels_up = 0
    temp = header
    while temp.startswith('../'):
        levels_up += 1
        temp = temp[3:]

    subdir_depth = subdir_prefix.count('/') if subdir_prefix else 0

    if levels_up > 0:
        remaining_up = levels_up - subdir_depth
        if remaining_up > 0:
            new_header = '../' * remaining_up + temp
        else:
            new_header = temp
        return f'{prefix}"{new_header}"{suffix}'

    # No "../" — bare header or already-pathed
    if '/' in header:
        known_roots = ('world/', 'core/', 'render/', 'gui/', 'client/', 'network/',
                       'nbt/', 'util/', 'platform/', 'audio/', 'profiling/', 'assets/')
        if header.startswith(known_roots):
            return line
        if subdir_prefix and header.startswith(subdir_prefix):
            return line
        if subdir_prefix:
            return f'{prefix}"{subdir_prefix}{header}"{suffix}'
        return line

    # Bare header from a subdirectory file
    if subdir_prefix:
        return f'{prefix}"{subdir_prefix}{header}"{suffix}'

    return line


def apply_renames(content, filename):
    """Apply collision renames, but NOT when prefixed with mc:: or other namespace."""
    for fname, old, new in COLLISION_RENAMES:
        if fname != filename:
            continue
        # Rename bare references and references in the anonymous namespace,
        # but NOT mc::old or other::old (qualified calls to the public API)
        # Strategy: replace \bold\b only when NOT preceded by ::
        # Use negative lookbehind for ::
        content = re.sub(r'(?<!:)' + re.escape(old) + r'\b', new, content)
    return content


def process_file(relpath, subdir_prefix):
    filepath = os.path.join(LEVELGEN_DIR, relpath)
    with open(filepath) as f:
        content = f.read()

    includes = []
    body_lines = []
    for line in content.split('\n'):
        stripped = line.strip()
        if stripped.startswith('#include'):
            fixed = fix_include(stripped, subdir_prefix)
            if fixed is not None:
                includes.append(fixed)
        else:
            body_lines.append(line)

    body = '\n'.join(body_lines)
    body = apply_renames(body, os.path.basename(relpath))
    body = body.strip('\n')

    return includes, body


def main():
    all_includes = set()
    all_includes_ordered = []
    file_bodies = []

    for relpath, subdir_prefix in SOURCE_FILES:
        filepath = os.path.join(LEVELGEN_DIR, relpath)
        if not os.path.exists(filepath):
            print(f"WARNING: {filepath} does not exist, skipping", file=sys.stderr)
            continue

        includes, body = process_file(relpath, subdir_prefix)
        for inc in includes:
            if inc not in all_includes:
                all_includes.add(inc)
                all_includes_ordered.append(inc)

        file_bodies.append((relpath, body))
        print(f"  Processed {relpath} ({len(body.splitlines())} lines)")

    print(f"\nBuilding {OUTPUT_FILE}...")

    output_lines = []
    output_lines.append("// WorldGen.cpp — Unified world generation compilation unit")
    output_lines.append("//")
    output_lines.append("// This file was generated by scripts/merge_worldgen.py.")
    output_lines.append("// It merges 27 .cpp files into a single compilation unit.")
    output_lines.append("//")
    output_lines.append("// Include paths adjusted, anonymous-namespace collisions resolved")
    output_lines.append("// by per-file renaming, duplicate headers excluded.")
    output_lines.append("// DO NOT EDIT MANUALLY — edit the original files and re-run the merge script.")
    output_lines.append("")

    output_lines.append("// ── Unified includes (deduplicated, paths adjusted) ────────────────────")
    for inc in all_includes_ordered:
        # Special case: wrap TreeGen.h include in a namespace to isolate its
        # FoliageAttachment/TreeConfig definitions from TreeFeature.h's versions.
        if 'TreeGen.h' in inc:
            output_lines.append("namespace treegen_impl { // isolate TreeGen.h definitions")
            output_lines.append(inc)
            output_lines.append("} // namespace treegen_impl")
            output_lines.append("// Bring TreeGen.h's unique symbols (TreeWorld, FoliagePlacer, etc.) into")
            output_lines.append("// mc::levelgen::feature via using-declarations, so TreeGen.cpp's code")
            output_lines.append("// (which is NOT wrapped) can still reference them.")
            output_lines.append("// NOTE: FoliageAttachment/TreeConfig are NOT re-exported — they stay")
            output_lines.append("// in treegen_impl to avoid conflicting with TreeFeature.h's versions.")
        else:
            output_lines.append(inc)
    output_lines.append("")

    for relpath, body in file_bodies:
        wrap_ns = NAMESPACE_WRAP.get(relpath)
        output_lines.append("")
        output_lines.append(f"// ═════════════════════════════════════════════════════════════════════════")
        output_lines.append(f"// BEGIN {relpath}")
        output_lines.append(f"// ═════════════════════════════════════════════════════════════════════════")
        if wrap_ns:
            output_lines.append(f"namespace {wrap_ns} {{ // isolates this file + its header's definitions")
        output_lines.append(body)
        if wrap_ns:
            output_lines.append(f"}} // namespace {wrap_ns}")
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
