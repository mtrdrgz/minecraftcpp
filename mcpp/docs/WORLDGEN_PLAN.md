# World Generation Port Plan (Minecraft 26.1.2 → C++)

> Faithful **1:1** port. Every value/algorithm comes from the decompiled Java
> (`26.1.2/src/...`) and the worldgen registry JSON shipped in `client.jar`
> (`26.1.2/data/minecraft/worldgen/...`). **No invented gameplay, no approximations.**
> Hand-authored "approximate" feature/structure generators were removed in
> Session 23 and must not be reintroduced.
>
> Status legend: ✅ done & verified · 🚧 in progress · ⬜ not started

This is the master checklist for "all biomes + every naturally-spawning block,
all trees, biome-specific blocks, and structures (villages, mineshafts, …)".
It is large and intentionally phased; each phase is portable and verifiable on
its own.

---

## Data inventory (from the real 26.1.2 registry)

| Registry | Count | Registry | Count |
|---|---|---|---|
| biome | 65 | configured_feature | 221 |
| placed_feature | 258 | configured_carver | 4 |
| structure | 34 | structure_set | 20 |
| template_pool | 188 | processor_list | 40 |
| density_function | 42 | noise_settings | 8 |

Feature interpreters needed: **~56 `Feature` types**, 38 configurations,
**9 trunk placers**, **11 foliage placers**, 13 tree decorators, 6 root placers,
**15 placement-modifier types**, ~12 block-state-provider types, 3 carver types.

---

## Phase A — Biome registry (data) ✅ DONE

All 65 biomes loaded 1:1 from `worldgen/biome/*.json`.

- ✅ `world/level/biome/Biome.h` — climate, special effects (colours), the
  26.1.2 `attributes` map (visual/audio/gameplay, incl. value-modifier form),
  generation settings (carvers + 11 feature steps), mob spawn settings.
- ✅ `world/level/biome/BiomeRegistry.{h,cpp}` — JSON loader (nlohmann).
- ✅ Verified: `biome_registry_parity` + `tools/biome_registry_reference.py`
  agree on **all 65 biomes / 1525 normalised fields** (independent parsers).

This gives, per biome, the exact ordered lists of carvers and placed-features
per decoration step — i.e. *what is allowed to spawn where*. Placing those
blocks is Phases B–F.

**Remaining for A:** wire the registry into the engine — embed `worldgen/*`
JSON into the asset pack and load via `AssetManager` (today the loader reads a
directory; the runtime path/embedding is a deliberate integration step).

---

## Phase B — Feature / decoration framework 🚧

The engine that turns a biome's placed-feature lists into block placements.

- ✅ `WorldgenRandom` population RNG (`setDecorationSeed` / `setFeatureSeed` /
  `setLargeFeatureSeed` / `setLargeFeatureWithSalt`) — verified 1:1 against the
  real decompiled code over 540 cases (`worldgen_random_parity`). This also
  fixed a pre-existing `nextDouble` precision bug shared by all RandomSources
  (Java multiplies by a `double`-field-from-`float`-literal `1.110223E-16F`),
  which feeds the noise system too.
- ⬜ `GenerationStep.Decoration` (11) + `Carving` (2) enums.
- ⬜ `FeatureSorter` — global per-step feature ordering (the index passed to
  `setFeatureSeed`); pure logic over the biome registry.
- ⬜ `WorldGenLevel` / `WorldGenRegion` placement surface over `LevelChunk`
  (getBlockState/setBlockState/getHeight within the 3×3 chunk region).
- ⬜ `ConfiguredFeature` + `PlacedFeature` data model + JSON loaders.
- ⬜ `PlacementContext` + the per-chunk decoration loop
  (`ChunkGenerator.applyBiomeDecoration`) using the population seed above.
- ⬜ **PlacementModifier** types (15): `count`, `rarity_filter`, `in_square`,
  `height_range`, `heightmap`, `random_offset`, `block_predicate_filter`,
  `surface_water_depth_filter`, `environment_scan`, `count_on_every_layer`,
  `noise_threshold_count`, `noise_based_count`,
  `surface_relative_threshold_filter`, `fixed_placement`, `biome`.
- ⬜ **BlockStateProvider** types (~12): `simple_state_provider`,
  `weighted_state_provider`, `noise_threshold_provider`, `noise_provider`,
  `dual_noise_provider`, `randomized_int_state_provider`,
  `rotated_block_provider`, etc.
- ⬜ `BlockPredicate` types (for `block_predicate_filter`).
- ⬜ `IntProvider` / `FloatProvider` / `HeightProvider` value providers.

---

## Phase C — Feature types (non-tree) ⬜

Port each `Feature` subclass + its configuration (counts from the inventory):

- ⬜ Ores & blobs: `ore`, `scattered_ore`, `block_blob`, `glowstone_blob`,
  `netherrack_replace_blobs`, `geode`, `dripstone_cluster`, `large_dripstone`,
  `pointed_dripstone`.
- ⬜ Surface scatter / vegetation: `simple_block`, `random_patch`/
  `vegetation_patch`, `waterlogged_vegetation_patch`, `flower`, `block_pile`,
  `block_column`, `sea_pickle`, `seagrass`, `kelp`, `bamboo`, `multiface_growth`,
  `glow_lichen`, `vines`, `twisting_vines`, `weeping_vines`, `chorus_plant`.
- ⬜ Terrain edits: `lake`, `spring_feature`, `disk`, `iceberg`, `blue_ice`,
  `freeze_top_layer`, `underwater_magma`, `delta_feature`, `basalt_columns`,
  `basalt_pillar`, `fossil`, `desert_well`, `sculk_patch`.
- ⬜ Selectors: `random_selector`, `simple_random_selector`,
  `random_boolean_selector` (compose the above).
- ⬜ Mushrooms & nether/end: `huge_red_mushroom`, `huge_brown_mushroom`,
  `huge_fungus`, `nether_forest_vegetation`, `end_island`, `end_spike`,
  `end_gateway`, `end_platform`, `spike`, `void_start_platform`.
- ⬜ Misc: `monster_room`, `bonus_chest`, `root_system`.

---

## Phase D — Trees ⬜  (39 tree configs)

- ⬜ `TreeConfiguration` + `TreeFeature.place` (trunk → foliage → decorators →
  roots, with the leaf/space checks).
- ⬜ **Trunk placers (9):** straight, forking, giant, mega_jungle, fancy,
  dark_oak, bending, upwards_branching, cherry.
- ⬜ **Foliage placers (11):** blob, spruce, pine, acacia, bush, fancy,
  jungle, mega_pine, dark_oak, random_spread, cherry.
- ⬜ **Tree decorators (13):** beehive, cocoa, leave_vine, trunk_vine,
  alter_ground, attached_to_leaves, creaking_heart, leaf_litter, pale_moss, …
- ⬜ **Root placers (6):** mangrove root system, etc.
- ⬜ `fallen_tree` feature (5 configs).
- ⬜ Concrete trees: oak/fancy_oak/swamp_oak, birch(+tall/bees variants),
  spruce/pine/mega_spruce/mega_pine, jungle(+mega/bush), acacia, dark_oak,
  cherry, mangrove(+tall), pale_oak(+creaking), azalea, + the per-biome
  `trees_*` random selectors (plains, taiga, savanna, badlands, jungle,
  flower_forest, grove, snowy, windswept_hills, old_growth_*, birch_and_oak…).

---

## Phase E — Carvers ⬜

- ⬜ `WorldCarver` base + `CarvingContext` + `CarverDebugSettings`.
- ⬜ `CaveWorldCarver` (configs `cave`, `cave_extra_underground`).
- ⬜ `CanyonWorldCarver` (config `canyon`).
- ⬜ `NetherWorldCarver` (config `nether_cave`).
- ⬜ Carver application pass + carving mask, wired before surface build.

---

## Phase F — Structures ⬜  (16 structure types)

- ⬜ Framework: `Structure`, `StructureStart`, `StructurePiece`,
  `StructureSet` + `StructurePlacement` (random_spread / concentric_rings),
  `StructureTemplate`/`StructureManager` (NBT templates), `StructureProcessor`
  (40 processor lists), `PoolElement`/`StructureTemplatePool` (188 pools).
- ⬜ **Jigsaw** (10): village (plains/desert/savanna/snowy/taiga),
  pillager_outpost, bastion_remnant, ancient_city, trail_ruins, trial_chambers.
- ⬜ **Mineshaft** (2: normal, mesa).
- ⬜ Stronghold, fortress (nether bridge), ocean_monument, woodland_mansion,
  end_city, nether_fossil.
- ⬜ Single-piece: igloo, swamp_hut, desert_pyramid, jungle_temple,
  buried_treasure, shipwreck (2), ocean_ruin (2), ruined_portal (7).
- ⬜ Structure placement gating in the chunk generator (`createStructures`,
  `generateStructure`) + `Beardifier` terrain adaptation (already decompiled).

---

## Phase G — Integration ⬜

- ⬜ Embed `worldgen/*` JSON in the asset pack (`tools/asset_packer`).
- ⬜ `ChunkGenerator` pipeline order (Java): structures → noise fill → aquifer/
  ore-veins → surface → carvers → features → structure post → spawns.
- ⬜ Wire BiomeSource→biome registry so each column resolves a real `Biome`
  (effects/features/carvers/spawns) instead of a bare id string.

---

## Verification strategy

- Pure-logic subsystems (placement RNG, modifiers, trunk/foliage placers,
  carver math) are parity-tested against the real decompiled code run on
  JDK 25 — the technique proven for `Climate::RTree` and the biome registry.
- Data loaders are cross-checked against an independent parser over the full
  registry.
- The full client can only be built/run on Windows (MSVC/Vulkan/DX12); new
  subsystems are landed as standalone, Linux-buildable parity targets first,
  then wired into the (untestable-here) main build deliberately.
