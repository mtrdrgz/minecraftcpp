# Worldgen 1:1 foundation

Goal: Minecraft 26.1.2 seed-exact generation. Same seed + same dimension + same chunk coordinates should eventually produce the same blocks as vanilla, stage by stage.

This document records the current foundation after removing approximate runtime worldgen.

## Rule

Do not generate approximate terrain decoration, structures, ores or plants in runtime worldgen.

If a Java feature/placement/predicate/structure pipeline is not ported from `26.1.2/src/` and backed by `26.1.2/data/`, it must be skipped rather than approximated.

Visual placeholders are allowed only outside runtime world generation, or behind an explicit non-parity prototype path that is not used by the normal world generator.

## Current runtime policy

The runtime should currently keep only the parts that are suitable for parity auditing:

- base density/noise terrain;
- biome source/climate lookup;
- surface rules;
- aquifer/fluid terrain behavior where already source-backed;
- chunk storage.

The following runtime systems are intentionally disabled until rebuilt on vanilla foundations:

- biome decoration;
- ores through feature decoration;
- vegetation/trees/plants through `BiomeDecorator`;
- hand-built structures;
- low-level plant guards in `LevelChunk::setBlock()`.

## Foundation status

### 1. WorldGenRegion

Java features do not place through a single `LevelChunk`. They place through `WorldGenRegion` with neighbour reads and explicit writable bounds.

Status: **foundation added** in `mcpp/src/world/level/levelgen/WorldGenRegion.h`.

Next work:

- wire biome decoration to use `WorldGenRegion`;
- wire structures to use `WorldGenRegion`;
- wire carvers to use `WorldGenRegion` / carving masks;
- avoid direct single-chunk clamping in feature code.

### 2. FeatureSorter, global feature indices and no-op decoration driver

Java decoration uses `FeatureSorter.StepFeatureData` and global feature indices. The RNG call is sensitive to the feature index:

```text
setFeatureSeed(decorationSeed, globalFeatureIndex, step)
```

A local merged feature list is not sufficient.

Status: **foundation added** in:

- `mcpp/src/world/level/levelgen/feature/FeatureSorter.h`
- `mcpp/src/world/level/levelgen/feature/DecorationPlan.h`
- `mcpp/src/world/level/levelgen/feature/DecorationDriver.h`
- `mcpp/src/world/level/levelgen/feature/DecorationPlanDump.h`

The new code is data-only and does not place blocks. It prepares the Java-style feature order, per-step feature indices, decoration seed and feature seed calls that future decoration should execute.

Next work:

- add a Java exporter that dumps the same `decoration_seed` / `step` / `index` / `seed` / `feature` rows for fixed biome sets;
- compare C++ `DecorationDriver` output against that exporter;
- only after parity, connect the driver to runtime diagnostics;
- preserve indices for skipped unsupported features once runtime decoration is re-enabled.

### 3. Placement modifiers and block predicates

Unknown modifiers/predicates must not become identity/true in strict runtime generation. That hides missing ports and creates invalid blocks.

Next work:

- port missing placement modifiers from `26.1.2/src/net/minecraft/world/level/levelgen/placement/`;
- port missing block predicates;
- make unsupported modifiers/predicates fail closed in strict mode;
- add logging/diagnostics listing skipped features and missing dependency type.

Priority modifiers/predicates:

- `environment_scan`;
- `surface_relative_threshold_filter`;
- `count_on_every_layer`;
- `fixed_placement`;
- `matching_blocks`;
- `matching_fluids`;
- `has_sturdy_face`;
- offsets on predicates;
- true/solid/replaceable/inside_world_bounds variants as required by data.

### 4. Configured feature ports

Re-enable configured features one at a time only after direct Java source comparison.

Suggested order:

1. `OreFeature` plus vanilla ore placed/configured feature JSON.
2. `KelpFeature`.
3. `SeagrassFeature`.
4. `SeaPickleFeature`.
5. `TreeFeature`/tree decorators review.
6. `FallenTreeFeature` decorators and exact validity checks.
7. `MultifaceGrowthFeature`.
8. `CaveVinesFeature`.
9. `VegetationPatchFeature`.
10. `RootSystemFeature`.
11. Coral features.
12. Bamboo / vines / nether vegetation.

### 5. Structures

Hand-built structure shapes are disabled. They must not be used for parity worldgen.

Required vanilla pipeline:

- `StructureSet`;
- `StructurePlacement`;
- `RandomSpreadStructurePlacement`;
- `Structure`;
- `StructureStart`;
- `StructurePiece`;
- templates;
- processors;
- jigsaw;
- references;
- chunk writable area placement.

### 6. Carvers

Noise caves are not a replacement for the Java carver stage.

Required vanilla pipeline:

- `WorldCarver`;
- `ConfiguredWorldCarver`;
- `CarverConfiguration`;
- `CarvingContext`;
- `CarvingMask`;
- biome carver lists;
- source-chunk scan around the target chunk;
- exact `setLargeFeatureSeed` use.

### 7. Stage parity harness

Before adding large amounts of features, add a Java-vs-C++ comparator.

Suggested stages:

1. after `fillFromNoise`;
2. after `buildSurface`;
3. after `applyCarvers`;
4. after structure references/starts;
5. after `applyBiomeDecoration`;
6. final chunk.

Minimum initial output: per-section block hash. Later output: exact block diffs.

#### Harness setup (working)

Ground truth comes from the REAL decompiled 26.1.2 classes, never a reimplementation:

- Runtime (git-ignored under `26.1.2/`, fetched per AGENTS.md): `client.jar`
  (v69 bytecode), `libs/*.jar` (datafixerupper/guava/fastutil/… from the version
  manifest `libraries[]`, required at runtime — even `XoroshiroRandomSource`
  pulls in `com.mojang.serialization.Codec`), and a JDK 25 under `26.1.2/jdk25/`.
- A Java generator in `mcpp/tools/<Name>.java` prints a TSV (raw IEEE bits for
  floats/doubles so the comparison is exact).
- `mcpp/tools/run_groundtruth.ps1 -Tool <Name> -Out <tsv>` compiles + runs it
  against the correct classpath.
- A C++ `*_parity` target reads the TSV with `--cases <tsv>` and reports
  `cases=N mismatches=M`.

Verified working: `worldgen_random_parity` vs `WorldgenRandomParity` →
`cases=540 mismatches=0` (the WorldgenRandom seeding primitive every later stage
depends on is bit-exact).

Next ground-truth targets, in dependency order: final-density / noise-router
samples (validates the `RandomState` wiring in §6 — the deepest currently enabled
stage), climate sampler samples, then the §2 decoration seed-order plan.

## What not to do

Do not add new visual approximations to make the world look fuller.

Do not fix invalid placements by adding block-specific guards to `LevelChunk::setBlock()`.

Do not silently treat unsupported JSON types as identity/true in strict runtime generation.

Do not mark tasklist items complete unless the behavior was checked against 26.1.2 source/data or a parity test.
