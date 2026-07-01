# STRUCTURES — port status & certification ledger

> Authoritative, source-grounded status of the structure subsystem. Written to
> "poner orden" (put order) on a subsystem that had many parity-tested *helpers*
> but a partial, partly-mislabelled in-game generation path.
>
> RULE #0 applies: an unported structure is a **hard no-op listed here**, never a
> silent `return true` / failed-assembly that looks done. This document is the
> single place that says, per structure, what is real and what is not.
>
> Last updated: 2026-07-01 (in-process assembly oracle; ALL implemented families
> byte-exact — see the 2026-07-01 section, which SUPERSEDES the per-family status
> in the older sections below).

---

## UPDATE 2026-07-01 — in-process oracle; 285/287 starts byte-exact; engine placement fixed

### The new gold standard: `tools/StructureStartsGenParity.java`

Runs the REAL `ChunkGenerator.createStructures` **in-process** (real
`NoiseBasedChunkGenerator` + overworld `MultiNoiseBiomeSource` + `RandomState`,
jar-backed `StructureTemplateManager`, biome tags bound from the shipped JSON)
and dumps every valid `StructureStart` through the real
`StructureStart.createTag` — so ids/BB/O/GD encoding is byte-identical to server
NBT. Unlike the `.mca`-based `StructureStartsDump`, it is **always
assembly-time**: the server's saved NBT mixes assembly boxes (pre-FEATURES
chunks) with placement-adjusted boxes (template structures move their
`templatePosition` in `postProcess`), which had silently corrupted the previous
gate. Validated against the dedicated server's world: 32/34 pre-features starts
byte-exact; the other 2 are shipwrecks whose Y a neighbouring chunk's FEATURES
pass had already projected (expected Java behaviour).

Compare with `tools/compare_starts.py` (order-insensitive, per-chunk):

```
26.1.2/jdk25/bin/java -cp "26.1.2/parity_classes:26.1.2/client.jar:26.1.2/libs/*" \
    StructureStartsGenParity 1 -60 -60 120 120 > /tmp/oracle.tsv
MCPP_BLOCK_STATES=src/assets/block_states.json ./build-linux/structure_starts_dump \
    --seed 1 --from -60 -60 --to 120 120 --out /tmp/cpp.tsv
python3 tools/compare_starts.py /tmp/oracle.tsv /tmp/cpp.tsv
```

### Certified assembly (seed 1, region (-60,-60)..(120,120), 287 oracle starts)

**285/285 starts of every implemented family are byte-exact** (all pieces:
id + BB + O + GD, in builder order):

| Family | Starts | Notes |
|---|---|---|
| mineshaft | 145 | stub = (midX, 50+yOffset, minZ); mesa = honest no-op |
| ocean_ruin cold/warm | 33+5 | assembly Y=90 (projection is placement-stage) |
| trial_chambers | 31 | jigsaw |
| shipwreck (+beached) | 27+3 | assembly Y=90; too-big corner adjust ported |
| **ruined_portal (+ocean)** | 10+7 | NEW: full findGenerationPoint incl. 4-corner noise-column scan |
| buried_treasure | 13 | assembly Y=90 |
| **monument** | 5 | NEW: surrounding-biome disc + MonumentBuilding BB (assembly only) |
| village plains/snowy | 2+1 | **all pieces byte-exact** (84+148+126 pieces) |
| trail_ruins | 2 | jigsaw |
| **igloo** | 1 | NEW: lab + ladders + top with exact PIVOTS/OFFSETS |
| **jungle_pyramid** | 1 | NEW: SinglePieceStructure (+ desert_pyramid, swamp_hut same path) |

**Missing (only remaining unported assembly): stronghold (1 start, 13 pieces —
recursive StrongholdPieces) and woodland mansion (1 start, 606 pieces — grid
assembly).** Both have byte-exact certified geometry helpers; what is missing is
the piece-recursion/assembly + placement, each a full-session port.

Multi-seed re-certification (same gate, 452 oracle starts total):

| Seed | Region | Result |
|---|---|---|
| 1 | (-60,-60)..(120,120) | 285/287 (missing: stronghold, mansion) |
| 12345 | (-40,-40)..(60,60) | **64/64 BYTE-EXACT** incl. 7 ancient_city (~90-102 pieces each) |
| -777 | (-40,-40)..(60,60) | 103/104 (missing: mineshaft_mesa — honest no-op) |

Extra root cause found by the seed-12345 run: a referenced template that is
absent from the shipped jars (ancient_city's intact_horizontal_wall_stairs_5)
must resolve to an EMPTY template (`StructureTemplateManager.getOrCreate`
semantics); the engine threw instead and a silent `catch(...)` killed every
ancient_city assembly in-game with no trace. Fixed in `ensureTemplate`.

### Root causes fixed (these were THE user-visible bugs)

1. **`getBaseHeight` was not Java's.** It scanned `finalDensity` at block
   resolution; Java's `iterateNoiseColumn` samples a single-cell NoiseChunk
   (cell-interpolated density + real aquifer + ore veins) and tests the
   heightmap predicate per state — WORLD_SURFACE_WG counts fluids (sea level
   over oceans), OCEAN_FLOOR_WG skips them. Ported 1:1 (`iterateNoiseColumn`,
   `getBaseHeight`, `getOceanFloorHeight`, `getBaseColumn`). This alone fixed
   every village piece mismatch (mixed ±1 child Y → RNG divergence → different
   piece lists) and, in-game, stops land structures validating on ocean chunks
   (villages-in-the-sea).
2. **Biome gate sampled the wrong source.** `Structure.isValidBiome` uses the
   quart **noise biome**; the dump used the seed-fuzzed BiomeManager zoomer →
   false positives/negatives at biome borders (shipwreck at (4,8),
   missing buried_treasure at (36,11)).
3. **Assembly vs placement Y conflated.** Template structures assemble at Y=90;
   the heightmap projection happens in `postProcess`. The dump now emits
   assembly semantics; the ENGINE now projects at placement exactly like
   `postProcess` (ocean_ruin footprint scan through air/water/ice, shipwreck
   footprint mean with first-AVAILABLE heights, ruined_portal 4-corner
   noise-column scan).
4. **placeTemplate ignored Mirror + pivot.** Shipwrecks rotate around PIVOT
   (4,0,15) and half of all ruined portals are FRONT_BACK mirrored — both were
   silently dropped in placement.

### Still open (structure subsystem)

- **Block-level placement gate**: diff engine-placed chunks against the
  server's structures-on `.mca` (ServerChunkDump). The assembly layer is now
  certified; placement Y logic is ported 1:1 but its inputs are the WG
  heightmaps — post-carver heightmap differences are possible and only the
  block gate can certify them.
- **stronghold + mansion assembly** (see above).
- Chest loot / block entities (chests place empty), monument/mansion/fortress
  block placement, `concentric_rings` eager ring generation for locate.
- `WorldGen.cpp` still carries a stale mirror of the structure Runtime used by
  the decorate-parity harness — it does NOT have these fixes; the authoritative
  runtime is `StructureGen.cpp` (used by the game and the dump).

---

## UPDATE 2026-06-30 — structure-starts parity gate: 2 structures BYTE-EXACT vs server.jar

Built the full structure-starts parity pipeline and certified two structure
families as byte-exact against the real Minecraft 26.1.2 dedicated server. This
closes the "no structures-on block-level .mca diff has passed" proof gap noted
in the 2026-06-22 (c) entry.

### Pipeline

1. `tools/run_server_gen_structures.sh` — runs server.jar headless with
   `generate-structures=true`, forceloads a 91×31-chunk region around the
   spawn map, saves to `world_structures/*.mca`.
2. `tools/StructureStartsDump.java` — reads the server's .mca files and dumps
   every `structures.starts` entry as TSV: `S <id> <cx> <cz> <refs> <count>`
   followed by `C <pieceId> <bb...> <O> <GD>` rows.
3. `tools/structure_starts_dump/main.cpp` (NEW) — the C++ counterpart: calls
   the engine's real assembly functions (no block placement) and dumps the
   same TSV format. Uses `dumpStructureStarts()` (new public API in
   `StructureGen.h`) + `NoiseBasedChunkGenerator` for the real biome gate.
4. `diff server.tsv cpp.tsv` — byte-exact comparison.

### Certified structures (seed=1, region -5..90 × -5..30)

| Structure | Server starts | Server pieces | C++ starts | C++ pieces | diff lines |
|---|---|---|---|---|---|
| **minecraft:mineshaft** | 38 | 4691 | 38 | 4691 | **0** ✅ |
| **minecraft:trial_chambers** | 5 | 1183 | 5 | 1183 | **0** ✅ |
| **minecraft:ocean_ruin_cold** | 8 | 60 | 8 | 60 | **8** ⚠️ (7/8 byte-exact; 1 has Y mismatch) |
| minecraft:shipwreck | 7 | 7 | 0 | 0 | (needs OCEAN_FLOOR_WG heightmap) |
| minecraft:shipwreck_beached | 1 | 1 | 0 | 0 | (needs WORLD_SURFACE_WG heightmap) |
| minecraft:ruined_portal | 4 | 4 | 0 | 0 | (needs terrain height for Y projection) |
| minecraft:buried_treasure | 3 | 3 | 0 | 0 | (needs OCEAN_FLOOR_WG + anchor block scan) |
| minecraft:monument | 1 | 1 | 0 | 0 | (UNPORTED — hard no-op, RULE #0) |

**mineshaft**: all 38 StructureStarts match exactly — piece IDs (msroom /
mscorridor / mscrossing / msstairs), bounding boxes, orientations (2D data
value), and genDepth values are byte-identical to the server's NBT.

**trial_chambers**: all 5 StructureStarts match exactly — this is a complex
jigsaw structure (235 pieces per start). The entire jigsaw pipeline (pool
loading, template loading, jigsaw placement, piece assembly, bounding box
computation, piece ordering) is byte-exact vs the server.

### Bugs found and fixed in the dump tool (NOT in the engine)

1. **Orientation encoding**: Java's StructurePiece NBT writes the "O" field as
   `Direction.get2DDataValue()` (SOUTH=0, WEST=1, NORTH=2, EAST=3), NOT the 3D
   ordinal (DOWN=0, UP=1, NORTH=2, ...). The C++ engine uses the 3D Direction
   enum internally — the dump tool now converts 3D→2D for the NBT format. The
   engine's assembly was always correct; only the dump output was wrong.

2. **Jigsaw piece "O" field**: Java's `PoolElementStructurePiece` does NOT set
   the StructurePiece orientation (it stays null → O=-1). The rotation is
   stored in a separate "rotation" NBT field. The dump tool now outputs O=-1
   for all jigsaw pieces.

3. **Multi-structure set dispatch**: mineshafts (`mineshaft` + `mineshaft_mesa`)
   and shipwrecks (`shipwreck` + `shipwreck_beached`) are multi-structure sets
   requiring weighted-random selection via `setLargeFeatureSeed`. The dump tool
   now replicates this selection logic (same as `Runtime::generate`).

4. **Chunk iteration order**: in mirror mode, the dump tool now iterates chunks
   in the EXACT order they appear in the server TSV (not sorted) so the output
   is byte-diffable without sorting.

### Remaining work

The 5 remaining structure families (shipwreck, ocean_ruin, ruined_portal,
buried_treasure, + the unported monument/stronghold/mansion/fortress) need
their assembly functions exposed in `dumpStructureStarts`. This requires:

- **Template size lookup** (`Runtime::sizeOf`) for bounding box computation.
- **Terrain height integration** for Y adjustment:
  - `buried_treasure`: Y = `getBaseHeight(midX, midZ, OCEAN_FLOOR_WG)` — the
    piece is created at Y=90 but projected to the ocean floor heightmap.
  - `shipwreck` (ocean): Y = mean ocean floor height over the template footprint.
  - `shipwreck_beached`: Y = `minY - templateH/2 - nextInt(3)`.
  - `ruined_portal`: Y depends on placement type (on_land_surface / underground /
    ocean / mountain).
  - `ocean_ruin`: Y=90 (fixed) — does NOT need terrain height, but needs the
    cluster assembly logic (16 random draws + 4-8 child pieces).
- **Real biome source**: the dump tool now uses `NoiseBasedChunkGenerator` for
  the biome gate, so structures that require specific biomes (swamp_hut →
  swamp, desert_pyramid → desert, etc.) will be correctly gated.

The `NoiseBasedChunkGenerator` is already linked into the dump tool. The
`getBaseHeight(x, z)` method returns the WORLD_SURFACE height; the
OCEAN_FLOOR_WG heightmap needs a separate computation (first non-fluid block
from top). This is the main remaining technical blocker for the height-dependent
structures.

### Spawn map (seed=1, C++ probe with forced biomes)

| Structure | Biome | Chunk (x,z) | Pieces |
|---|---|---|---|
| minecraft:trial_chambers | plains | (36,2) | 235 |
| minecraft:village_plains | plains | (14,3) | 120 |
| mineshaft | plains | (54,4) | 133 (total) |
| ruined_portal | plains | (60,17) | 1 |
| minecraft:pillager_outpost | plains | (3,19) | 18 |
| desert_pyramid | desert | (46,3) | 1 |
| minecraft:village_desert | desert | (11,6) | 68 |
| swamp_hut | swamp | (9,2) | 1 |
| minecraft:trail_ruins | jungle | (40,1) | 20 |
| jungle_temple | jungle | (64,9) | 1 |
| igloo | snowy_plains | (65,1) | 1 |
| minecraft:village_snowy | snowy_plains | (13,3) | 95 |
| minecraft:village_taiga | taiga | (79,3) | 144 |
| buried_treasure | beach | (48,3) | 1 |
| shipwreck_beached | beach | (4,8) | 1 |
| shipwreck | ocean | (4,8) | 1 |
| minecraft:village_savanna | savanna | (15,2) | 119 |

---

## UPDATE 2026-06-26 — verification pass + RULE #0 reverts (fabricated structures)

Re-audited the in-game structure dispatch directly in the post-merge monolith
(`WorldGen.cpp::tryGenerateAndPlace`, mirrored in the probe's
`structure/StructureGen.cpp`). Since the 2026-06-22 ledger the `supportedTypes`
set had grown to include `mineshaft`, `ocean_ruin`, `buried_treasure`,
`ruined_portal`, `ocean_monument`, `woodland_mansion`, `fortress`, `stronghold`,
`end_city`. Three of those were **fabricated placeholders**, not piece ports:

- **`woodland_mansion`** (`tryPlaceWoodlandMansion`) — writes a hand-built
  **52×52 cobblestone slab + dark-oak-log box**. Comment admits "Simplified",
  "GAP: interior rooms … deferred". Not from `WoodlandMansionPieces`.
- **`fortress`** (`tryPlaceNetherFortress`) — writes a **5×10 nether-brick
  bridge with fences**. Comment: "we only place a basic bridge". Invented.
- **`stronghold`** (`tryPlaceStronghold`) — writes a **16×16×8 stone-brick box
  and then fills its 14×14×6 interior with AIR**. This is the literal
  "square pocket of air carved into the terrain" the owner reported. Invented.

All three were `supported = true` with **no gating** (`isKnownBrokenRuntimeStructureSet`
returns false for everything), so they generated in-game in any biome that passed
the centre-column biome check — exactly the "structures generate where they
shouldn't / leave a square of air" symptom. **Removed from `supportedTypes` in
both `WorldGen.cpp` and `StructureGen.cpp`**; they now report UNPORTED and place
nothing (RULE #0). Their `tryPlace*` bodies are left as unreachable dead code to
be replaced by real ports.

**`ocean_monument` — also reverted (owner decision 2026-06-26).** It placed the
**real Java outer-shell geometry** (correct `createTopPiece` coords/blocks) but
**deferred all child rooms** and **did not align the RNG stream**
(`placeOceanMonument` comment: "RNG state … does NOT match Java's"). A lone shell
with no interior in deep ocean is a "looks reasonable" partial that still returns
true, so it is removed from `supportedTypes` in both files → UNPORTED no-op.

**`end_city` — kept enabled.** Places only the `end_city/base_floor` template
(child pieces deferred), but it is reachable only in the End dimension, which is
not generated during overworld play. Left as a partial pending a real port; it
does not affect the overworld world the owner is finishing.

**Fossils (overworld "dinosaur bones") — investigated, body is faithful.** The
owner suspected absurd frequency + air carving. `FossilFeature.h` is a faithful
1:1 of `FossilFeature.java`: rarity is `RarityFilter.onAverageOnceEvery(64)` for
both `FOSSIL_UPPER`/`FOSSIL_LOWER` (`CavePlacements`), registered only in
`desert`/`swamp`/`mangrove_swamp` (`BiomeDefaultFeatures.addFossilDecoration`),
placed 15–24 blocks below the OCEAN_FLOOR_WG surface, and it writes only
`bone_block`/ore — **never air**. The per-feature seeding
(`setDecorationSeed`+`setFeatureSeed`) and `rarity_filter` parsing are correct,
and the engine shares the exact `decorateOneChunk` path. So the square-air the
owner attributed to fossils is the fabricated **stronghold** above; any genuine
over-frequency is most likely the known **re-decoration / race** issue
(chunks decorated more than once amplifies rare distinctive features) or a
runtime biome-filter mismatch — both need the Windows engine to confirm and were
NOT reproducible by static read of the faithful feature.

> Environment note for this pass: run on remote Linux; the fictional `26.1.2`
> client/data is not fetchable here, so the data-driven probes
> (`structure_gen_probe`, `full_chunk_decorate_parity`) and the Windows engine
> could not be run. Findings above are grounded in the decompiled Java source
> (present) + the C++ dispatch code.

---

## UPDATE 2026-06-22 (c) — in-game village smoke + postprocess blockers cleared

Follow-up verification narrowed the prior blockers:

- `structure_gen_probe --seed 1 --radius 56 --biome minecraft:plains --surface 68`
  now reports the target `minecraft:village_plains` start `(40,51)` with `pieces=84`,
  matching the server GT start dump (`S minecraft:village_plains 40 51 0 84`). The
  earlier observed 89-piece delta is not reproduced by this headless forced-plains probe.
- `mcpp.exe --quickPlaySingleplayer --seed 1 --spawn 640 816 --backend opengl` reaches
  the main loop and places a village start at `(40,51)` without `runStructures failed`,
  `decorateChunk failed`, or `updateShape not ported` after this session's fixes.
- Important distinction: the true in-game biome context at the smoke location resolves
  the start as `minecraft:village_snowy` with 31 pieces, not the forced-plains
  `minecraft:village_plains` server-GT target. Do not merge those two as one certificate.
- Fixed postprocess/updateShape blockers found in the in-game path by porting only Java
  behaviour: `DirtPathBlock` tick-only conversion, `BaseTorchBlock` floor-torch support,
  `BedBlock` paired-half validation from real state-id properties, and
  `StairBlock.getStairsShape` property recomputation. Static `BlockBehaviour` identity
  cases such as `stripped_spruce_wood` and `tuff_bricks` are now explicit no-ops.

Remaining proof gap: no structures-on block-level `.mca` diff has passed. Next work should
add a dedicated structure start/piece-list parity gate for seed 1 chunk `(40,51)` in the
true server biome context, then run the block dump diff against `world_structures`.

---

## UPDATE 2026-06-22 (b) — server-GT village certification attempt, blocked

Full certification was attempted and deliberately stopped without marking success.
Ground truth now exists for the next agent:

- Real server seed: `1`.
- Vanilla locate result: nearest `minecraft:village_plains` at block `[640, ~, 816]`,
  chunk `(40,51)`.
- Generated structures-on server world around chunks `(32..47,43..58)`.
- `StructureStartsDump` result includes `S minecraft:village_plains 40 51 0 84`.
- `ServerChunkDump` now supports `--region <dir>` so block dumps can target
  `world_structures/dimensions/minecraft/overworld/region`.

Important result: villages are **not** fully certified. During the attempted full block
diff, the C++ structure path around `(40,51)` assembled `minecraft:village_plains` with
89 pieces while server GT stores 84 children. The same attempt hit a crash while routing a
`FeaturePoolElement` for `minecraft:oak`, which appears in village pool JSON but is not a
plain `worldgen/placed_feature/oak.json` sidecar. Next work must first build a focused
structure-start/piece-list parity gate for `(seed=1, structure=village_plains, chunk=40,51)`
before any block-level `.mca` certification claim.

One runtime fix is kept: structure block placement now projects starts from
`getBaseHeight(x,z)-1`, the same pre-NOISE base-column height vanilla uses at
STRUCTURE_STARTS. That removes the earlier documented post-Beardifier heightmap mismatch
between Beardifier assembly and block placement.

---

## UPDATE 2026-06-22 — FeaturePoolElement wired + per-chunk FEATURES structure pass

The previous status correctly said `feature_pool_element` could not be guessed: it
needed Java's per-decorating-chunk FEATURES random and ordering. That architecture
is now wired instead of faked.

- `Minecraft::tryDecorate` now begins the Java FEATURES turn before structures,
  then runs structures, then runs biome features in the same turn. The engine
  decoration context primes non-WG heightmaps once at turn start.
- Jigsaw structures now place per decorating chunk: starts within the maximum
  jigsaw reach are assembled, intersecting pieces are visited in piece order,
  template block writes are clipped to the decorating chunk box, and the random is
  seeded as Java does with `setDecorationSeed(seed, chunkMinX, chunkMinZ)` then
  `setFeatureSeed(decorationSeed, structureIndexInStep, stepIndex)`.
- `FeaturePoolElement` is now a real delegated call to the already-certified
  `PlacedFeature` runtime (`enginePlaceStructurePoolFeature`). It receives the
  caller's seeded structure random and must not reseed it.
- Windows verification: MSVC/Ninja build links `build-vs\mcpp.exe`; headless
  `structure_gen_probe --radius 4 --seed 1` runs; `mcpp.exe --quickPlaySingleplayer`
  loads embedded worldgen, enters the main loop, and generates/decorates chunks
  until the intentional timeout.

Remaining proof gaps: use the server-GT target above to close the 89-vs-84 piece delta,
fix/port the `minecraft:oak` structure feature reference path, then run a block-level
`.mca` diff. Do not mark whole-village structure output fully certified until those pass.

---

## UPDATE 2026-06-21 (d) — VILLAGES ENABLED: all three layers ported + integrated

Villages are now turned on (the `isKnownBrokenRuntimeStructureSet` gate is removed).
All three layers that were missing are ported and verified on Linux (the engine's
in-game *visual* render is the only step that needs the Windows build):

- **#1 Processor pipeline** (RuleProcessor + legacy `STRUCTURE_AND_AIR` + TERRAIN_MATCHING
  GravityProcessor) — village buildings stop carving air, streets follow terrain,
  mossify/farm/street rules apply. Verified with `structure_gen_probe` (mossy_cobblestone,
  carrots appear; paths follow a sloped world).
- **#2 / the Beardifier (#3) — terrain adaptation, CERTIFIED.** Ported
  `net.minecraft...Beardifier` byte-exact (`beardifier_parity` cases=400 checks=8000
  mismatches=0 vs the real class). Junctions recorded during jigsaw assembly;
  `Beardifier.forStructuresInChunk` ported as `buildBeardifier`; wired into
  `fillFromNoise` as `add(finalDensity, beardifier)`. The no-structure terrain stays
  byte-identical: `full_chunk_parity` cases=98304 mismatches=0 with the hook present.
  Engine wiring: the per-chunk Beardifier is built on the main thread (the structure
  runtime is single-threaded) and handed to the worker by shared_ptr for both the
  startup and async `fillFromNoise` paths.

Verified end-to-end on Linux: villages assemble with the full pipeline; the per-chunk
Beardifier is non-empty over the chunks a village reaches and is deterministic
(same seed → same beardifier); its density contribution near villages is non-zero
(it WILL adapt terrain); terrain elsewhere is unchanged.

**Verification suite (all green, Linux GCC + JDK25 GT):**
- `jigsaw_placement_parity` — 5 structures × 1055 seeds, **0 piece mismatches** (the
  junction recording added for the Beardifier does NOT move any piece).
- `beardifier_parity` — 400 cases / 8000 checks, **0 mismatches** vs the real class.
- `full_chunk_parity` — 98304 cells, **0 mismatches** (no-structure terrain byte-identical
  with the beardifier hook present).
- `structure_gen_probe` — villages enabled, per-chunk Beardifier non-empty + deterministic
  + non-zero density near villages.

**Resolved 2026-06-22 b:** vanilla computes the
structure start (piece positions) ONCE at STRUCTURE_STARTS from the *base column* height
(`getFirstFreeHeight` via `getBaseColumn`, no beardifier) and reuses it at NOISE
(beardifier) and FEATURES (block placement). `runStructures` now uses
`getBaseHeight(x,z)-1` for `StructureWorld.heightAt`, matching the Beardifier assembly
height source instead of the post-Beardifier chunk heightmap. This is integrated but
still needs the full server-GT piece/block parity gate above.

**Superseded 2026-06-22:** `feature_pool_element` is no longer an omitted layer; it is
wired through the per-chunk FEATURES pass described above. It still needs a
structures-on server GT diff before the full village output can be called certified.

**What still needs the Windows engine (cannot be done in this Linux session):** the
in-game visual pass and a server `generate-structures=true` `.mca` GT diff. Everything
else is ported and gated above.

---

---

## UPDATE 2026-06-21 (c) — village increment #1: processor pipeline + legacy element

Ported the structure **processor pipeline** into the placement path
(`StructureGen.cpp`), the first of the three village layers. This is
`StructureTemplate.processBlockInfos` for the rule-based processors that every
jigsaw pool element references:

- **`legacy_single_pool_element` semantics** — `BlockIgnoreProcessor.STRUCTURE_AND_AIR`:
  legacy elements (all village buildings) no longer place template AIR, so they stop
  carving holes in the terrain. Verified: plains village block writes dropped from
  ~12k to ~3.8k per village (the spurious air carving is gone). Single elements keep
  `BlockIgnoreProcessor.STRUCTURE_BLOCK` (air still carves — vanilla-correct).
- **`RuleProcessor`** — per block a fresh `LegacyRandomSource(Mth.getSeed(worldPos))`,
  first matching rule wins via the certified short-circuit `input && location`
  RuleTests (always_true / block_match / blockstate_match / random_block_match;
  tag_match deferred → only zombie villages). Output state rotated at place time.
  Verified end-to-end with the probe: blocks that can ONLY exist via a processor
  appear — mossify→`mossy_cobblestone` (720), farm→`carrots` (105) over a 60×60 scan.
- **Pool element** now carries its `processors` list id (`StructureTemplatePool.h`).

This also improves the already-enabled jigsaw structures (pillager_outpost
`outpost_rot`, trial_chambers `copper_bulb_degradation`) — they now apply their rule
processors and drop structure-block markers.

**Still deferred for villages (kept gated):** GravityProcessor / ProtectedBlockProcessor
and the TERRAIN_MATCHING projection processors (street terrain-following) →
terrain-adaptation phase; `feature_pool_element` pieces (increment #2);
`beard_thin` Beardifier (increment #3). Villages stay disabled until 1:1.

---

## UPDATE 2026-06-21 (b) — empirical findings, biome gate, village root cause

A headless verification harness now exists: **`structure_gen_probe`** (target +
`tools/structure_gen_probe/main.cpp`). It drives the real `generateStructures`
over a flat world against the real worldgen data and reports what places — so
structure work is verifiable on Linux/CI without Windows. Build with the
parity-only GCC build; run e.g.
`MCPP_BLOCK_STATES=src/assets/block_states.json ./build-linux/structure_gen_probe
--biome minecraft:plains --radius 80 2>/dev/null | grep placed`.

**Biome gate FIXED (real 1:1 bug, verified by the harness).** The hand-built
non-jigsaw structures placed in *any* biome because their dispatch skipped the
`Structure.isValidBiome` check (the jigsaw path already had it). Symptom: the probe
placed **400 nether fossils in 1600 overworld plains chunks**, plus desert pyramids
and igloos in plains. Fix: parse each non-jigsaw structure's `biomes` set and
validate it at the chunk-centre surface column (exactly `onTopOfChunkCenter` →
`isValidBiome`). After the fix the probe shows each structure only in its own
biomes: swamp→swamp_hut, snowy→igloo, desert→desert_pyramid, jungle→jungle_temple +
trail_ruins, beach→shipwreck_beached, and **zero** nether fossils in the overworld.
This directly addresses "structures exposing where they shouldn't."

**Villages — exact root cause found (and they DO assemble).** With the probe,
temporarily clearing the gate shows plains villages assemble fully: **12 villages in
120×120 chunks, 66–122 pieces and ~8k–13k blocks each**. They are *deliberately*
disabled by `isKnownBrokenRuntimeStructureSet` in `StructurePlacement.cpp` (sets
`generationEnabled=false` for `minecraft:villages`) because the polish layer is
incomplete — NOT because assembly fails. The jigsaw skeleton is proven good
(pillager_outpost/trial_chambers place in-game today). Village-specific remaining
work to reach 1:1: (1) the **structure processor pipeline** wired into placement
(`legacy_single_pool_element` air-ignore via `BlockIgnoreProcessor.STRUCTURE_AND_AIR`,
`JigsawReplacementProcessor`, then the element's processor list: street/farm/mossify/
zombie, then the projection's processors); (2) **`feature_pool_element`** pieces (35
in village pools — lamps/decorations); (3) **`beard_thin`** terrain adaptation.

**Agreed plan (owner decision this session):** build all three layers to 1:1 FIRST,
keep villages gated until they are byte-faithful, and port the **real Beardifier**
(rearchitect so structure starts are known before NOISE) rather than any
approximation. Sequence: processors+legacy element → feature_pool_element →
Beardifier → flip the gate.

---

## TL;DR for the world generator

- **Terrain looks 1:1; structures do not.** Confirmed by the existing terrain gates
  (`full_chunk_parity 2359296/0`, carvers `0`). The structure subsystem is the
  least-finished part of worldgen.
- **What actually places in-game today** (hand-ported pieces, dispatched in
  `StructureGen.cpp::tryGenerateAndPlace`): swamp hut, desert pyramid, jungle
  pyramid, igloo, shipwreck (+beached), nether fossil. Plus the **jigsaw family**
  (villages, pillager outpost, ancient city, bastion, trail ruins, trial chambers)
  via `assembleJigsaw`, **but** without structure processors or terrain adaptation
  (see gaps below), so jigsaw output is *not* trustworthy yet.
- **What is NOT placed at all** (helpers/math ported, no in-game placement —
  previously some of these were mislabelled "supported"): ocean ruin, ruined
  portal, buried treasure, ocean monument, woodland mansion, mineshaft, stronghold,
  nether fortress, end city.
- **The "structures expose where they shouldn't" complaint** is concentrated in the
  **jigsaw families**, which in vanilla use `terrain_adaptation` (`beard_thin`,
  `beard_box`, `encapsulate`) implemented by the **Beardifier** density function —
  **not ported**. The hand-ported temples (desert/jungle/igloo/swamp/shipwreck) use
  `terrain_adaptation: none` in vanilla, so the missing Beardifier does **not**
  explain any exposing on those; if they look wrong it is a piece-port bug, not the
  beardifier.

---

## The three architectural gaps that block a 1:1 structure world

These are the root causes behind the user-visible mess. None can be fixed by
tuning; each is a real Java system that is not ported.

### 1. Generation ordering — structures run as a *post-pass*, not as STRUCTURE_STARTS
Vanilla pipeline per chunk: `STRUCTURE_STARTS` (decide starts + assemble pieces) →
`STRUCTURE_REFERENCES` → `NOISE` (terrain, with the Beardifier reading nearby
structure pieces) → `SURFACE` → `CARVERS` → `FEATURES` (place structure blocks +
decorate) → `FULL`. In this port, structures are generated **after** terrain *and*
decoration, in `Minecraft::tryDecorate → runStructures` (`Minecraft.cpp:455`). That
ordering makes the Beardifier impossible (terrain is already final) and changes
structure↔feature interaction order. **A faithful port must compute structure
starts before `fillFromNoise`** and store them on the chunk.

### 2. Beardifier / terrain adaptation — NOT ported
`terrain_adaptation` is read from the structure JSON by *nobody*: `grep -ri beard
src/` → none. Values that matter: villages/pillager_outpost/nether_fossil
`beard_thin`, ancient_city `beard_box`, trial_chambers `encapsulate`,
stronghold/trail_ruins `bury`. Java: `Beardifier.java` +
`StructurePiece.terrainAdaptation()` + the `minecraft:beardifier` density function
slot in the noise router. Until this exists, beard/encapsulate structures sit on
raw terrain and **expose / float**.

### 3. Structure processors — NOT applied at placement
`Runtime::placeTemplate` (`StructureGen.cpp:1141`) writes raw template blocks with
only block-state rotation. It ignores the element's `processors` entirely
(gravity-snap, `rule`, `block_age`/mossify, `block_rot`, `protected_blocks`,
jigsaw→`final_state`/`pos` replacement, street/path processors). Village houses in
particular rely on the **gravity** and **street** processors to meet the ground.
Note: a *standalone* `StructureProcessor` parity test exists
(`StructureProcessorParityTest.cpp`) but the pipeline is not wired into the
placement path.

Secondary gaps: no block-entity / loot-table writes in `LevelChunk` (temple/village
chests place as empty chest blocks), and `concentric_rings` placement is unported
(stronghold positioning / eye-of-ender locate).

---

## Per-structure ledger

Legend — **Placed**: real, dispatched in-game placement. **Jigsaw***: assembles via
the generic jigsaw path but unverified (no processors/beard). **Helpers**: only
sub-components (boxes/geometry/grids/selectors) ported, no in-game placement.
**No-op**: recognised but deliberately not placed (RULE #0 honest).

| Structure set | Type | In-game status | Notes / what's missing |
|---|---|---|---|
| swamp_huts | swamp_hut | **Placed** | `SwampHutPiece` ported; `terrain_adaptation: none`. |
| desert_pyramids | desert_pyramid | **Placed** | `DesertPyramidPiece` ported; none. |
| jungle_temples | jungle_temple | **Placed** | `JungleTemplePiece` + stone selector ported; none. |
| igloos | igloo | **Placed** | top + optional lab/ladders; needs `igloo/*.nbt`. |
| shipwrecks | shipwreck | **Placed** | template-list pick + place at y=90; needs shipwreck `.nbt`. |
| nether_fossils | nether_fossil | **Placed (approx.)** | ⚠ Y hardcoded to 32 (`StructureGen.cpp`) instead of the noise-column scan — a RULE #0 approximation to remove. Nether-only; now correctly excluded from the overworld by the biome gate (was spamming 400/1600 plains chunks). |
| villages | jigsaw | **Jigsaw*** | pools present; **no processors, no `beard_thin`** → exposes/floats; biome tag `#has_structure/village_*` must resolve. This is why villages look absent/wrong. |
| pillager_outposts | jigsaw | **Jigsaw*** | + watchtower; `beard_thin`; same gaps. |
| ancient_cities | jigsaw | **Jigsaw*** | `beard_box`; deep placement; same gaps. |
| nether_complexes (bastion) | jigsaw | **Jigsaw*** | bastion pool present; nether-only. |
| trail_ruins | jigsaw | **Jigsaw*** | `bury`; same gaps. |
| trial_chambers | jigsaw | **Jigsaw*** | `encapsulate`; same gaps. |
| ocean_ruins | ocean_ruin | **No-op** | `OceanRuinClusterGeometry` helper only. Was mislabelled supported. |
| ruined_portals | ruined_portal | **No-op** | only `RuinedPortalYSelector` helper. No piece/processor/lava-magma. Was mislabelled supported. |
| buried_treasures | buried_treasure | **No-op** | placement layer ready (`frequency 0.01`, `legacy_type_2`, `locate_offset` all supported); only the 1-piece `BuriedTreasurePiece` + chest loot missing. Smallest next win. |
| ocean_monuments | ocean_monument | **Helpers** | room geometry/graph/fitter ported; no assembly+placement. |
| woodland_mansions | woodland_mansion | **Helpers** | grid/layout/edge-clean ported; no assembly+placement. |
| mineshafts | mineshaft | **Helpers** | corridor/crossing/room/stairs boxes ported; no assembly+placement. |
| strongholds | stronghold | **Helpers** | piece boxes/doors ported; **`concentric_rings` placement unported** too. |
| (fortress) | fortress | **Helpers** | `NetherFortressPieceBox`/child-offset ported; no assembly. Nether-only. |
| end_cities | end_city | **No-op** | End dimension; not ported. |

---

## What IS certified (parity gates, build on Linux GCC `parity-only` build)

These gates lock the math/sub-component ports against Java ground truth and still
build + run (`cmake -S . -B build-linux -G Ninja -DCMAKE_C_COMPILER=gcc
-DCMAKE_CXX_COMPILER=g++`). They certify *pieces*, not whole-structure placement:

`structure_placement_parity` (random_spread + frequency + exclusion), bounding box
+ aggregate, structure piece math, jigsaw attach, jigsaw height limit, structure
template pool/loader/transforms, structure place-in-world, structure processor,
igloo piece position, ruined portal Y selector, stronghold piece box/type/door,
nether fortress piece box/child offset, mineshaft stairs box, ocean monument
room/graph/fitter, woodland mansion grid/layout/edge-clean, ocean ruin cluster,
structure pieces builder math, structure connected position.

> Caveat: these prove the helper is byte-identical to its Java counterpart. They do
> **not** prove a whole structure generates 1:1 — that needs the server `.mca`
> ground-truth with `generate-structures=true` (not yet built; the existing
> decoration GT runs with structures **off**).

---

## Prioritised roadmap to a 1:1 structure world

1. **Honesty (DONE this session).** Stop marking ocean_ruin / ruined_portal /
   buried_treasure as "supported" when they no-op; log unported types at load.
2. **Buried treasure** — smallest real structure (1 piece + chest). Placement layer
   already supported; port `BuriedTreasurePieces.BuriedTreasurePiece.postProcess`
   1:1. Needs minimal block-entity/loot support (or place chest, defer loot).
3. **Structure starts before NOISE** — re-architect so starts/pieces are computed
   at chunk STRUCTURE_STARTS and stored, unblocking #4 and fixing ordering.
4. **Beardifier** — port `Beardifier` + wire the `minecraft:beardifier` density slot
   so `beard_thin/beard_box/bury/encapsulate` adapt terrain. Fixes exposing.
5. **Processor pipeline in `placeTemplate`** — gravity, rule, block_rot/age,
   protected_blocks, jigsaw final_state. Fixes village houses meeting the ground.
6. **Ruined portal** — port `RuinedPortalPiece` + `BlockAgeProcessor`/`LavaSubmerged`
   + the per-type placement (underground/ocean/mountain); add lava/magma.
7. **Remaining hand-built** — ocean monument, woodland mansion, mineshaft,
   stronghold (+concentric_rings), nether fortress assembly.
8. **Whole-structure parity gate** — server `.mca` with structures ON, dumped and
   diffed like the decoration GT, to certify 1:1 end-to-end.

## Honest bottom line on "same seed → same world"

For **terrain, caves, water, surface and trees** the port is at or near byte parity
(gated). For **structures** it is **not** 1:1 today: a handful of surface temples
place from faithful piece ports, the jigsaw family assembles but without the two
systems (Beardifier + processors) that make it match, and roughly half of the
structure families are not placed at all. Reaching "same seed → same structures"
requires items 3–8 above; it is multi-session work, not a tuning pass.

## UPDATE 2026-06-30 (b) — ocean_ruin_cold: 7/8 byte-exact, engine Y bug found

Extended `dumpStructureStarts()` to handle `minecraft:ocean_ruin` (cold + warm).
Replicates the exact RNG sequence from `OceanRuinStructure.generatePieces` +
`OceanRuinPieces.addPieces`:
- `setLargeFeatureSeed(seed, cx, cz)` + `nextInt(4)` for rotation
- `nextFloat() <= largeProbability` for isLarge gate
- `nextFloat() <= clusterProbability` for cluster spawn
- Cluster: 16 raw `nextInt` draws for positions + 4-8 child ruins

Piece BBs are computed from template sizes via `structureGetBoundingBox` (the
1:1 port of `StructureTemplate.getBoundingBox`), using the same
`StructureTemplate.transform` formula the server uses. This is the first
template-based structure (non-jigsaw, non-recursive) to be certified at the
piece level.

### Results

| Start chunk | Server TPY | C++ Y | Match? |
|---|---|---|---|
| (-9,-17) | 90 | 90 | ✅ |
| (-13,1) | 90 | 90 | ✅ |
| (-12,22) | 90 | 90 | ✅ |
| (22,-18) | 90 | 90 | ✅ |
| (9,-16) | 90 | 90 | ✅ |
| **(3,22)** | **47** | **90** | ❌ |
| (43,-9) | 90 | 90 | ✅ |
| (69,-18) | 90 | 90 | ✅ |

7/8 starts are byte-exact. The 1 mismatch (chunk 3,22) has Y=47 (server) vs
Y=90 (C++). Investigation via `DumpRawNbt.java` confirmed the server stores
`TPY=47` for this chunk — the piece is projected to the OCEAN_FLOOR_WG
heightmap (Y=47 at this chunk's center), not hardcoded Y=90.

### Engine bug found

`Runtime::tryPlaceOceanRuin` (StructureGen.cpp:1869) hardcodes `BlockPos
offset{active.x * 16, 90, active.z * 16}`. The Java code's
`onTopOfChunkCenter(context, OCEAN_FLOOR_WG, ...)` projects the start to the
OCEAN_FLOOR_WG heightmap, and the piece's TPY is set to that height. When the
ocean floor is at Y=90 (shallow water or land), the C++ matches. When it's
below 90 (deep ocean, like chunk 3,22 where floor=47), the C++ leaves the piece
floating at Y=90 while the server buries it at Y=47.

**Fix needed**: `tryPlaceOceanRuin` should call `getBaseHeight(midX, midZ,
OCEAN_FLOOR_WG)` and use that Y instead of hardcoded 90. This requires adding
an OCEAN_FLOOR_WG heightmap method to `NoiseBasedChunkGenerator` (currently
only WORLD_SURFACE is available via `getBaseHeight`).

### Remaining structures blocker

All remaining structures (shipwreck, ruined_portal, buried_treasure) need
terrain height for Y adjustment:
- **shipwreck** (ocean): Y = mean OCEAN_FLOOR_WG over template footprint
- **shipwreck_beached**: Y = minY(WORLD_SURFACE_WG) - templateH/2 - nextInt(3)
- **ruined_portal**: Y depends on placement (on_land_surface / underground / etc.)
- **buried_treasure**: Y = first anchor block below OCEAN_FLOOR_WG height

The C++ `NoiseBasedChunkGenerator::getBaseHeight(x, z)` returns WORLD_SURFACE
height only. An OCEAN_FLOOR_WG method (first non-fluid block from top) needs
to be added. This is the main technical blocker for the remaining structures.
