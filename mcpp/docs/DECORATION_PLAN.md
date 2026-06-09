# Decoration / biome-features 1:1 port plan (toward server byte-match)

Goal: port ALL terrain decoration 1:1 (biome features, trees, tree decorators, biome
special characteristics) **except structures**, proven by full-chunk byte-match against a
real **no-structures** Minecraft server `.mca`. Terrain through carvers is already
full-chunk byte-exact (`full_chunk_parity`, 2.36M/0); the remaining gap is decoration.

## Measured ground truth (Session 63→64)
- Server run: `mcpp/tools/run_server_gen.ps1` (generate-structures=false, forceload a
  region to FULL). Dump: `mcpp/tools/ServerChunkDump.java` → TSV in `FullChunkParity`
  format. Compare with `full_chunk_parity --cases server_chunk_cases.tsv`.
- Terrain-only C++ vs server full chunks: **24905 / 589824 mismatches (4.2%)** over 6
  chunks. The 95.8% match validates the `.mca` decoder; the 4.2% is exactly un-ported
  decoration. Gap is dominated by the **ore family**: tuff, andesite, gravel, diorite,
  granite (the stony `ore_*` blobs) + iron/lapis ores → **ores are the #1 target**.

## Trustworthy-to-reuse C++ infra (verified, do not rebuild)
- `feature/FeatureSorter.h` — `buildFeaturesPerStep(featureSources, biomeFeatures)` (global
  per-step feature index = the `setFeatureSeed` index) + `selectFeatureIndicesForStep`.
- `feature/BiomeFeatures.{h,cpp}` — per-biome ordered placed-feature keys per step;
  `stepCountForBiome` preserves raw JSON length (matches Java `maxStep`).
- `feature/GenerationStep.h` — Decoration enum (RAW_GENERATION=0 … VEGETAL=9, TOP_LAYER=10,
  COUNT=11).
- `feature/DecorationDriver.h` / `DecorationPlan.h` — seed model: `WorldgenRandom(create(0))`
  → `setDecorationSeed(worldSeed, originX=C.x*16, originZ=C.z*16)` once → per feature
  `setFeatureSeed(decorationSeed, globalIndex, step)`; `featureSeed = decoSeed + index +
  10000*step` (u64 wrap).
- `placement/PlacedFeature.h` — depth-first modifier drive (matches Java lazy flatMap RNG).
  Modifiers ported: in_square, count, rarity_filter, random_offset, biome, block_predicate_filter.
- Harness `tools/BiomeDecorationParity.java` + `feature/BiomeDecorationParityTest.cpp` —
  per-feature certification (PRE/PUT). `loadFeature/loadModifier/loadStateProvider/
  loadPredicate` dispatch throws on unknown (fail-closed). Certified mismatches=0: grass
  patches, tall_grass/large_fern, flower_plains, flower_forest; trees oak/birch/jungle/spruce.

## Missing / to build
- `feature/BiomeDecorator.cpp::applyBiomeDecoration` is a NO-OP — must become the real loop.
- `WorldGenRegion.h` is disconnected from the placement `WorldGenLevel`; needs an adapter
  over a multi-chunk map with multi-heightmap (WG-live vs frozen) + radius-1 writable bounds.
- World-dependent placement modifiers not in the runtime registry: `height_range`,
  `environment_scan`, `surface_water_depth_filter`, `count_on_every_layer`, etc. (some exist
  in the harness `loadModifier` — promote them).
- Feature types (port from `26.1.2/src/.../feature/*` + configured_feature JSON), ordered by
  cross-biome coverage:
  1. **Ores** (`ore`/`scattered_ore` + `OreConfiguration` + `RuleTest` tag_match/block_match/
     random_block_match + `height_range`/HeightProvider). Highest coverage. `OreFeature.place`
     RNG order: `nextFloat()*PI`, `y0/y1 = originY + nextInt(3)-2`, per-segment
     `nextDouble()*size/16`, overlap cull, ellipsoid walk, `canPlaceOre` = `target.test(state,
     rng)` (+ air-exposure check consuming `nextFloat` only if 0<discard<1).
  2. Remaining placement modifiers + HeightProvider/IntProvider/FloatProvider.
  3. Vegetal patches beyond grass/flowers (random_patch/flower, vegetation_patch, seagrass,
     kelp, block_pile, sugar cane, pumpkin [real predicate — do NOT tune], cactus, dead bush,
     multiface_growth, block_column) + remaining state providers/predicates.
  4. Trees + tree decorators (fancy/forking/giant/mega_jungle/dark_oak/bending/cherry/
     upwards_branching trunks; acacia/dark_oak/mega_pine/pine/bush/random_spread/cherry
     foliage; alter_ground/beehive/cocoa/vine/attached_to_leaves/... decorators).
  5. lakes + fluid_springs.
  6. local_modifications (disk, ice_patch, forest_rock, geode, iceberg).
  7. underground_decoration (glow_lichen, dripstone) + top_layer (freeze_top_layer).

## ⚠ Critical finding (Session 64): edge-spill features need the 3×3 region, NOT the single-chunk harness
The single-chunk `BiomeDecorationParity` Proxy `getChunk` returns the center chunk for ALL
coords, and `OreFeature` writes via `section.setBlockState` (bypassing the out-of-chunk drop
that `level.setBlock` does for trees). So an ore blob extending past the chunk edge gets
**wrapped** into the center chunk — a harness artifact. Therefore **ore/lakes/big-trees
cannot be cleanly certified in isolation**; they must be certified in the real **3×3
WorldGenRegion against the server `.mca`** (where `getChunk` returns the correct neighbor and
writes land/read correctly). Surface features that write only via `level.setBlock` (grass,
flowers, small trees) remain fine in the single-chunk harness.

## The 3×3 byte-match design (the gateway = task 11)
A server chunk C's blocks = its own features' writes + features spilled in from its 8
neighbors (FEATURES step `blockStateWriteRadius(1)`, `WorldGenRegion.ensureCanWrite` gates
`max(|dx|,|dz|)<=1`). To byte-match C: generate terrain+carvers for a **5×5** block of chunks
centered on C (the inner 3×3 are decorated; the outer ring exists only so the 8 neighbors'
radius-1 reads are correct), prime the four WG heightmaps on each, build ONE shared region,
then run the full decoration loop (`ChunkGenerator.applyBiomeDecoration`, `26.1.2/src/.../
ChunkGenerator.java:314`) once per chunk in the 3×3 in fixed ascending (cz,cx) order, each
reseeded from its own `decorationSeed`, all writing into the shared map. Then compare C's
cells against the `.mca`. Implement as a `--decorate` mode of `FullChunkParityTest.cpp`; run
ONLY ported features, log+count UNPORTED keys (never silently skip-as-pass). Watch the
mismatch count fall as families are ported.

### Gotchas (honor or byte-match silently fails)
- Multi-heightmap: `WORLD_SURFACE_WG`/`OCEAN_FLOOR_WG` are LIVE during decoration; non-WG
  `OCEAN_FLOOR` is FROZEN at pre-decoration (the harness `ChunkLevel` already models this).
- Use quart-resolution `BiomeSource::getNoiseBiome`, not the block zoomer, for possibleBiomes
  + the biome filter.
- `WorldGenRegion.getHeight` returns stored height **+1**.
- FeatureSorter is GLOBAL over `biomeSource.possibleBiomes()`; the per-chunk 3×3 biome set only
  selects which indices fire.
- Every unported type is a logged, counted hard no-op (or throw in harness) — never pass-through.
- Skipping the structure sub-loop does NOT perturb feature seeds (they key off the per-step
  feature index, independent of structure indices).
