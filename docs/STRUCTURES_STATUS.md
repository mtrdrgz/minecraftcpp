# STRUCTURES — port status & certification ledger

> Authoritative, source-grounded status of the structure subsystem. Written to
> "poner orden" (put order) on a subsystem that had many parity-tested *helpers*
> but a partial, partly-mislabelled in-game generation path.
>
> RULE #0 applies: an unported structure is a **hard no-op listed here**, never a
> silent `return true` / failed-assembly that looks done. This document is the
> single place that says, per structure, what is real and what is not.
>
> Last updated: 2026-06-21 (session: structures audit + honesty pass + biome gate +
> village root-cause + Linux verification harness).

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
