#!/usr/bin/env python3
"""
Find anonymous-namespace FUNCTION DEFINITIONS that collide across files.
Only looks for functions that have a body (definition), not declarations.

This is more precise than the previous scan — it only finds real link-time
collisions (functions defined in anonymous namespaces of multiple .cpp files
that, when merged, would be in the same anonymous scope).
"""

import os
import re
import sys
from collections import defaultdict

LEVELGEN_DIR = "/home/z/my-project/src/world/level/levelgen"

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

KEYWORDS = {
    'if', 'for', 'while', 'switch', 'return', 'sizeof', 'static_cast',
    'dynamic_cast', 'const_cast', 'reinterpret_cast', 'catch', 'throw',
    'new', 'delete', 'namespace', 'class', 'struct', 'enum', 'union',
    'typedef', 'using', 'template', 'operator', 'do', 'const', 'constexpr',
    'inline', 'static', 'virtual', 'unsigned', 'signed', 'auto', 'void',
    'int', 'char', 'double', 'float', 'bool', 'long', 'short', 'override',
    'final', 'default', 'case', 'break', 'continue', 'goto', 'else',
    'try', 'explicit', 'friend', 'mutable', 'volatile', 'register',
    'thread_local', 'extern', 'public', 'private', 'protected',
}


def find_anon_namespace_blocks(content):
    """Find all anonymous namespace blocks and return their content."""
    blocks = []
    for m in re.finditer(r'\bnamespace\s*\{', content):
        start = m.end()
        depth = 1
        pos = start
        while pos < len(content) and depth > 0:
            if content[pos] == '{':
                depth += 1
            elif content[pos] == '}':
                depth -= 1
            pos += 1
        blocks.append(content[start:pos - 1])
    return blocks


def find_function_definitions(block):
    """Find function DEFINITIONS (with body) in a block.

    A function definition looks like:
        return_type name(params) {
    or:
        return_type name(params) const {
    or:
        name(params) {  (constructor/destructor)

    We look for the pattern: word followed by (params) followed by optional
    qualifiers followed by { — this indicates a definition, not just a declaration.
    """
    funcs = set()

    # Pattern: identifier( ... ) [const] [noexcept] {
    # We need to find the function name, which is the identifier before the (
    for m in re.finditer(r'\b(\w+)\s*\(([^)]*)\)\s*(?:const\s*|noexcept\s*|override\s*|final\s*)*\{', block):
        name = m.group(1)
        if name in KEYWORDS:
            continue
        # Check it's not a control flow statement (if/for/while/switch already in KEYWORDS)
        # Check it's not a macro or function call (the { after ) means it's a definition)
        funcs.add(name)

    # Also find lambda assignments: auto name = [&](...) { — these are variables
    # but with function bodies that could collide
    for m in re.finditer(r'(?:auto|inline\s+auto)\s+(\w+)\s*=\s*\[&\d,=\s]*\([^)]*\)\s*(?:mutable\s*)?(?:->\s*[\w:<>*&\s]+)?\s*\{', block):
        name = m.group(1)
        if name not in KEYWORDS:
            funcs.add(name)

    return funcs


def find_static_functions(content):
    """Find static function definitions (file-scope static functions).

    These are NOT in anonymous namespaces but have internal linkage.
    When merged, they'd collide if multiple files define the same name.
    """
    funcs = set()
    # Pattern: static return_type name(params) {
    for m in re.finditer(r'\bstatic\s+[\w:<>&*\s]+?\s+(\w+)\s*\([^)]*\)\s*\{', content):
        name = m.group(1)
        if name not in KEYWORDS:
            funcs.add(name)
    return funcs


def main():
    # Map: symbol_name -> set of files that define it (in anonymous namespace)
    anon_symbols = defaultdict(set)
    # Map: symbol_name -> set of files that define it (static function)
    static_symbols = defaultdict(set)

    for relpath in SOURCE_FILES:
        filepath = os.path.join(LEVELGEN_DIR, relpath)
        if not os.path.exists(filepath):
            continue
        with open(filepath) as f:
            content = f.read()

        # Anonymous namespace functions
        for block in find_anon_namespace_blocks(content):
            for sym in find_function_definitions(block):
                anon_symbols[sym].add(relpath)

        # Static functions (outside anonymous namespaces)
        for sym in find_static_functions(content):
            static_symbols[sym].add(relpath)

    # Find collisions
    anon_collisions = {sym: files for sym, files in anon_symbols.items()
                       if len(files) > 1}
    static_collisions = {sym: files for sym, files in static_symbols.items()
                         if len(files) > 1}

    print(f"Anonymous-namespace function definitions: {len(anon_symbols)}")
    print(f"  Collisions across files: {len(anon_collisions)}")
    print(f"Static function definitions: {len(static_symbols)}")
    print(f"  Collisions across files: {len(static_collisions)}")
    print()

    # Generate the renames list
    all_collisions = {}
    all_collisions.update(anon_collisions)
    all_collisions.update(static_collisions)

    print(f"=== Total unique colliding symbols: {len(all_collisions)} ===")
    print()

    # Generate Python code
    print("# COLLISION_RENAMES for merge_worldgen.py:")
    print("COLLISION_RENAMES = [")
    for sym in sorted(all_collisions.keys()):
        files = sorted(all_collisions[sym])
        for f in files:
            basename = os.path.basename(f).replace('.cpp', '').upper()
            # Shorten long filenames
            basename = basename.replace('FULLCHUNKDECORATEPARITYTEST', 'FCDPT')
            basename = basename.replace('NOISEBASEDCHUNKGENERATOR', 'NBCG')
            basename = basename.replace('STRUCTUREPLACEMENT', 'STRPLACE')
            basename = basename.replace('STRUCTUREGEN', 'STRGEN')
            basename = basename.replace('OVERWORLDBIOMEBUILDER', 'OWBB')
            basename = basename.replace('NOISEGENERATORSETTINGS', 'NGS')
            basename = basename.replace('SURFACESYSTEM', 'SURFSYS')
            basename = basename.replace('SURFACERULES', 'SURFRULES')
            basename = basename.replace('SURFACERULEDATA', 'SURFRDATA')
            basename = basename.replace('DENSITYFUNCTION', 'DENFUNC')
            basename = basename.replace('NOISEROUTERDATA', 'NRDATA')
            basename = basename.replace('BIOMEMANAGER', 'BIOMGR')
            basename = basename.replace('BIOMESOURCE', 'BIOSRC')
            basename = basename.replace('OREVEINIFIER', 'OREVEIN')
            basename = basename.replace('RANDOMSOURCE', 'RANDSRC')
            basename = basename.replace('RANDOMSTATE', 'RANDSTATE')
            basename = basename.replace('TERRAINPROVIDER', 'TPROV')
            basename = basename.replace('BIOMEFEATURES', 'BIOFEAT')
            basename = basename.replace('BIOMEDECORATOR', 'BIODEC')
            basename = basename.replace('TREEGEN', 'TREEGEN')
            basename = basename.replace('WORLDCARVER', 'WCARVE')
            new_name = f"{basename}_{sym}"
            print(f'    ("{f}", "{sym}", "{new_name}"),')
    print("]")

    print()
    print("=== Collision details ===")
    for sym in sorted(all_collisions.keys()):
        files = sorted(all_collisions[sym])
        print(f"  {sym}: {files}")


if __name__ == "__main__":
    main()
