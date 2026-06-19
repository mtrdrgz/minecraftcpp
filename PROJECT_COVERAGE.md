# PROJECT_COVERAGE.md — Port Progress Dashboard & Devlog

> **Every agent reads this file at session start. Every agent updates it at session end.**
> It is the living proof that the port is 1:1 — not "looks right", not "probably correct", 
> but documented, cited, and gated.

---

## Agent Protocol (mandatory)

### Session start checklist
1. Read `AGENTS.md` (the rules, source locations, build system)
2. Read this file (current state, recent findings, what's in flight)
3. Check `TASKLIST.md` for the current bug/feature queue
4. Fetch `26.1.2/src/` if the task requires reading Java (see AGENTS.md § FETCHING SOURCE MATERIAL)

### While working
- **Every Java file you open**: update its row in `docs/PORT_COVERAGE.tsv` — even if you just read it and concluded "n/a". Unvisited means no one has looked at it.
- **Every file you port**: add a proof entry in column 3 of the TSV (see proof format below).
- **Every significant finding**: add a devlog entry at the bottom of this file.
- **Every system-level change**: update `README.md` status tables accordingly.

### Session end checklist
1. Update `docs/PORT_COVERAGE.tsv` for every file touched this session
2. Add a devlog entry in this file (even a short one — "visited X, concluded n/a because Y")
3. Update the progress dashboard section below (just update the numbers)
4. Update `AGENTS.md` § CURRENT STATE with a summary of what changed
5. Update `README.md` if any system's status or certification numbers changed
6. Commit and push everything to main

### Proof format (anti-hallucination rule)

The TSV has three columns: `path  status  proof`. The proof field must be non-empty for any non-unvisited file.

| Status | What the proof must contain |
|---|---|
| `ported` | C++ file path + function/class name + either a parity gate (`gate_name N/0`) or exact algorithm citation (`Java method X → C++ src/path:line`) |
| `partial` | What is ported (cite C++ location) + what is NOT ported (list specifically) |
| `n/a` | One sentence: why this file has no C++ equivalent (replaced by architecture, Java-only infrastructure, test scaffolding, etc.) |
| `unvisited` | Leave blank — means no agent has opened this file yet |

**Never mark `ported` without a parity gate or explicit algorithm citation. A wrong port that "looks right" is worse than unvisited.**

---

## Progress Dashboard

*Last updated: 2026-06-20*

### Overall

```
Total Java files tracked : 6,882
Ported (full)            : 502   ( 7.3%)
Partial                  : 95    ( 1.4%)
Reasoned N/A             : 449   ( 6.5%)
Unvisited                : 5,836 (84.8%)

Actionable files (excl. N/A): 6,433
Weighted progress            : 549.5 / 6,433

[████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░] 8.5%
```

### By major package

| Package | Total | Ported | Partial | N/A | Progress |
|---|---|---|---|---|---|
| `net/minecraft/world/` | 2,559 | 242 | 53 | 0 | `[██░░░░░░░░]` 10.5% |
| `net/minecraft/client/` | 1,813 | 28 | 16 | 0 | `[░░░░░░░░░░]` 2.0% |
| `net/minecraft/util/` | 713 | 43 | 0 | 386 | `[█░░░░░░░░░]` 13.1% of actionable |
| `net/minecraft/server/` | 418 | 0 | 1 | 0 | `[░░░░░░░░░░]` 0.1% |
| `net/minecraft/network/` | 411 | 162 | 3 | 0 | `[████░░░░░░]` 39.8% |
| `com/mojang/blaze3d/` | 149 | 4 | 1 | 0 | `[░░░░░░░░░░]` 3.0% |
| `net/minecraft/nbt/` | 43 | 0 | 18 | 0 | `[██░░░░░░░░]` 20.9% |
| `com/mojang/math/` | 10 | 7 | 0 | 0 | `[███████░░░]` 70.0% |
| Other packages | 776 | 18 | 4 | 63 | `[░░░░░░░░░░]` ~2.9% |

### world/ subpackage detail

| Subpackage | Total | Ported | Partial | Progress |
|---|---|---|---|---|
| `world/level/` | 1,297 | 178 | 37 | 14.9% |
| `world/entity/` | 708 | 24 | 6 | 3.8% |
| `world/item/` | 313 | 11 | 6 | 4.5% |
| `world/phys/` | 27 | 20 | 2 | 78.0% |
| `world/inventory/` | 64 | 0 | 2 | 1.6% |
| Other world/ | 150 | 9 | 0 | 6.0% |

### Parity gates (certified at 0 mismatches)

These are the hard guarantees. Every gate must stay green after every commit.

| Gate | Cells / vectors | Status |
|---|---|---|
| Terrain + cave carvers | 2,359,296 | ⚠️ 7,673 mismatches (99.67%) — see devlog 2026-06-20 |
| Full decoration — forest 6 chunks | 884,736 | ✅ 0 mismatches |
| Full decoration — ocean 6 chunks | 589,824 | ✅ 0 mismatches |
| Biome climate selection | 1,867,776 | ✅ 0 mismatches |
| NBT round-trip | 6 vectors | ✅ 0 mismatches |
| Network packet encoding | 31 messages | ✅ 0 mismatches |
| AABB collision | 512 cases | ✅ 0 mismatches |

---

## What needs a port (big-picture)

For a full 1:1 port every actionable Java file must reach `ported` or `partial` with proof. The biggest unvisited blocs, in priority order:

1. **`net/minecraft/world/entity/`** (708 files, 3.8% done) — all mob classes, AI goals, attributes, effects
2. **`net/minecraft/client/`** (1,813 files, 2.0% done) — rendering pipeline, GUI screens, input handling, sound
3. **`net/minecraft/world/level/`** (1,297 files, 14.9% done) — remaining worldgen: structures, light engine, chunk loading
4. **`net/minecraft/world/item/`** (313 files, 4.5% done) — all item types, use behaviours
5. **`net/minecraft/server/`** (418 files, 0.1% done) — integrated server, commands
6. **`net/minecraft/util/`** (713 files, 13.1% of actionable) — utilities, most are n/a candidates

---

## Devlog

Newest entries first. Every agent adds an entry. Short is fine — one bullet per finding, no walls of text.

---

### 2026-06-20 — Session: Terrain parity investigation + BlendedNoise fix

**Agent**: Super Z (GLM)

- **BlendedNoise ctor arg-eval-order fix** (commit b23410db): the delegating-ctor form passed 3× `PerlinNoise::createLegacyForBlendedNoise(*random, ...)` as args. C++ evaluates delegating-ctor args in UNSPECIFIED order; GCC evaluates right-to-left, causing `mainNoise` to consume random FIRST instead of LAST. All 40 ImprovedNoise instances ended up in wrong slots. Fix: direct member init in DECLARATION order. After fix: BlendedNoise parity 18,610/18,610 byte-exact (was 100% wrong on GCC).
- **ImprovedNoise 1.0E-7F fix** (commit b23410db): `1.0E-7` (double) → `static_cast<double>(1.0E-7F)` to match Java's float-literal widening. ImprovedNoise parity: 956,530/0.
- **GCC support** (commit b23410db): added GCC branch to `cmake/CompilerFlags.cmake` with `-ffp-contract=off` for FP-strict parity. Wrapped `biome_manager_parity` bcrypt link in `if(WIN32)`.
- **Terrain parity investigation**: `full_chunk_parity` shows 7,673 mismatches / 2,359,296 cells (99.67% byte-exact) on GCC. Previous claim of "0 mismatches" was likely on MSVC where the BlendedNoise arg-eval-order bug doesn't manifest (MSVC evaluates left-to-right).
- **Root cause of remaining 7,673 mismatches**: the C++ `CellInterpolationResolver` does not replicate Java's `NoiseChunk` architecture. Java's `NoiseChunk` maintains independent `NoiseInterpolator` instances (one per `interpolated()` marker) with slice arrays. During `fillSlice`, inner interpolators return `this.value` (stale from previous cell, 0.0 for first cell). During `fillAllDirectly` (fillingCell=true), ALL interpolators simultaneously return `Mth.lerp3(factors, theirCorners)`. The C++ resolver computes corners on-demand with direct computation (no stale values), producing different FP results for nested `interpolated()` markers.
- **All noise/density subsystems verified byte-exact**: ImprovedNoise (956K), PerlinNoise (6M), BlendedNoise (18.6K), NormalNoise (81K), SimplexNoise (125K), PerlinSimplexNoise (141K), WorldgenRandom (540), OverworldBiome (300K), ClimateBiome (56), StructurePlacement (29K), Mth (5.4K), MthExtra (6K), DensityRouter (488). All at 0 mismatches.
- **Next step**: port `NoiseChunk` from Java (`26.1.2/src/net/minecraft/world/level/levelgen/NoiseChunk.java`, ~800 lines) to replace `CellInterpolationResolver` in `NoiseBasedChunkGenerator.cpp`. This requires: (1) `NoiseInterpolator` class with slice arrays, (2) `fillSlice`/`selectCellYZ`/`fillAllDirectly`/`updateForY/X/Z` methods, (3) `mapAll(wrap)` visitor to wrap density tree markers.
- **Files examined**: `src/world/level/levelgen/Noise.cpp`, `src/world/level/levelgen/NoiseBasedChunkGenerator.cpp`, `26.1.2/src/net/minecraft/world/level/levelgen/NoiseChunk.java`, `26.1.2/src/net/minecraft/world/level/levelgen/NoiseBasedChunkGenerator.java`, `26.1.2/src/net/minecraft/world/level/levelgen/synth/BlendedNoise.java`, `26.1.2/src/net/minecraft/world/level/levelgen/synth/ImprovedNoise.java`

---

### 2026-06-19 — Session: Repo restructure + README overhaul

**Agent**: Claude (claude-sonnet-4-6)

- **Repo cleanup**: Moved C++ project from `mcpp/` subdirectory to repo root. Deleted all stale/outdated MD files. Updated `.gitignore`, CMake paths, CI workflow, `CLAUDE.md`, `AGENTS.md` path references (29 occurrences via sed).
- **Mouse controls fixed**: `LevelRenderer.cpp` — yaw was `+=dx` (inverted due to `vanillaViewVector` negating yaw in view matrix); fixed to `-=dx`. Pitch was `-=dy` (Win32 Y increases downward, so positive dy = look down should increase pitch); fixed to `+=dy`.
- **Texture system fixed**: `Blocks.cpp` — `block_states.json` intentionally ships with empty texture fields. Added `assignKnownBlockTextures()` called after JSON load: explicit top/side/bottom differentiation for dirt-like and log blocks, generic `setFallbackTexture()` for all others (strips `minecraft:` prefix, maps to atlas name).
- **README rewritten** with collapsible `<details>` dropdowns per system group, Final Certification column, granular terrain gen breakdown into 8 subsections.
- **Files examined this session**: `src/render/level/LevelRenderer.cpp`, `src/world/level/block/Blocks.cpp`, `CMakeLists.txt`, `cmake/PrepareRuntimeAssets.cmake`, `.github/workflows/build-release.yml`, `AGENTS.md`, `README.md`, `CLAUDE.md`, `.gitignore`

---

### 2026-06-11 — Session 66+ — Engine integration + full-port kickoff

**Agent**: Claude (previous session)

- **Full-port goal set**: user expanded scope to full 1:1 port of ALL Java files. Created `docs/PORT_COVERAGE.tsv` (6,882 files) and `docs/PORT_COVERAGE.md` roadmap. Tool: `tools/port_coverage.sh`.
- **Decoration engine integration** (commit 9035e916): decoration TU compiled into engine as library (`MCPP_DECORATE_NO_MAIN`). `mcpp.exe --quickPlaySingleplayer` decorates in-game: 65 biomes, 199 placed features (0 no-ops), 300+ chunks, zero failures.
- **NBT audit** (7c53c091): found real infidelity — `CompoundTag` was NOT insertion-ordered (Java uses `LinkedHashMap`), modified-UTF-8 null-byte encoding was missing, gzip write was silent-truncating. Fixed. Gate: `nbt_parity 6/0`.
- **PacketBuffer audit** (25bade31): VarLong was only 5-byte (Java is 10-byte), `Utf8String` used wrong UTF-16 semantics + missing replacement decode, unnamed network NBT was wrong. Fixed. Gate: `packet_buffer_parity 31/0`.
- **AABB audit** (1c3949ac): byte-exact, 0 findings. Gate: `aabb_parity 512/0`.
- **Biome breadth**: 9 → 25 biome classes certified vs server (1,867,776 cells).
- **PalettedContainer corruption** (1321bc56): resize/direct-mode bug scrambling any section whose palette grew past 16 entries. Fixed.
- **Key finding on `NoiseChunk.cachedClimateSampler`**: vanilla fills biomes via the CACHED sampler, NOT `randomState.sampler()` — they disagree at shore boundaries and flip surface rules. This was a real 1:1 bug.
- **Key finding on `WorldGenRegion.getRandom()`**: it is a deterministic positional random (worldgen_region_random @ chunk world pos), NOT time-seeded. Proxy implementation was wrong.

---

### 2026-06-10 — Session 66 — C++ full-cell parity on 6 forest + 6 ocean chunks

**Agent**: Claude (previous session)

- **Forest certification** (8c1ab5be): `DecorateAll cells=884736 mismatches=0` on all 6 forest chunks.
  - Root cause 1: seed-1 spawn IS chunk (10,10); world creation decorates spawn 3×3 BEFORE any forceload in xz order (x outer asc, z inner asc).
  - Root cause 2: biome fill must use `NoiseChunk.cachedClimateSampler` not raw sampler (see above).
  - Root cause 3: `WorldGenRegion.getRandom()` is positional, not time-seeded.
- **Ocean certification**: `DecorateAll cells=589824 mismatches=0` on all 6 ocean chunks.
  - Fixes: fluid tags bound (TallSeagrassBlock.canSurvive uses FluidTags.WATER — unbound tag silently returns false, kills tall seagrass, desyncs RNG); biome manager reads CACHED noise biome (quartY clamp); `fillFromNoise` vs `getBaseColumn` (aquifer width); post-processing runs at FULL promotion even when tick-frozen (bubble columns consume random.nextLong).
- **Java GT = server byte-match**: `FullChunkDecorateParity.java` reproduces real `server.jar` saved chunks byte-for-byte on all 6 primary chunks (98304/98304 cells each).

---

### Sessions 63–65 — Terrain byte-match, decoration gap quantification, ore porting

**Agent**: Claude (previous sessions)

- **Session 63** (worldgen 1:1 — full-chunk terrain): built `provision_parity_runtime.ps1`, provisioned real Java runtime. `full_chunk_parity 2359296/0` — C++ terrain+carvers byte-matches real generator on 6 chunks.
  - Found+fixed real port bug: `OreVeinifier` (see commit history).
- **Session 64**: built server ground truth pipeline (`run_server_gen.ps1` + `ServerChunkDump.java`). Quantified decoration gap: 24905/589824 (4.2%) — all decoration. Proved `.mca` decode is correct (95.8% match is terrain).
  - KEY FINDING: edge-spilling features cannot be certified in single-chunk harness — ore blobs wrap artifact. Must use 3×3 WorldGenRegion for ore family.
- **Session 65**: built `full_chunk_decorate_parity` 3×3 driver. Ported `OreFeature.h` + RuleTest + height_range 1:1. Ore result: `DecorateOre ore_cells=26185 ore_mismatches=1965` (~92.5%). Residual: FeatureSorter global index ordering vs Java `possibleBiomes()` order.

---

### Sessions 40–62 — Worldgen foundation, biome climate, noise, density functions

**Agent**: Claude (previous sessions)

- **Session 40 key lesson**: worldgen "worked visually" but was full of disguised approximations — pumpkins on beaches, sugar cane everywhere, only ~3 biomes. Fixes were not tuning but porting real Java that had been skipped (`return true` predicates, placeholder noise). This is why Rule #0 exists.
- **Biome climate** (`Climate::RTree` + `OverworldBiomeBuilder`): 7,593-entry verification then scaled to 1,867,776-cell gate. Exact port of Java's production `findValue` search.
- **Density function DAG**: all 15+ node types ported. `Spline` CubicSpline control points extracted verbatim from `TerrainProvider.java`. `Interpolated` tri-linear cell caching exact.
- **Surface rules**: all condition + rule types ported. `Bandlands` terracotta color bands.
- **Aquifer system**: `NoiseBasedAquifer` Voronoi water level, lava below y=-54, `FluidStatus`.
- **Cave carvers**: `CaveWorldCarver` branch/worm, `CanyonWorldCarver` floor/ceiling shapes, carver mask bitfield.
- **Biome registry**: all 65 biomes loaded from `26.1.2/data/minecraft/worldgen/biome/*.json`.

---

*Add new entries above this line. Format: `### YYYY-MM-DD — Session name` followed by bullets.*

---

### 2026-06-20 — Session: NoiseChunk port attempt (reverted)

**Agent**: Super Z (GLM)

- **Attempted NoiseChunk port**: created `NoiseChunk.h`/`NoiseChunk.cpp` to replace `CellInterpolationResolver` with a faithful port of Java's `NoiseChunk` (independent `NoiseInterpolator` instances with slice arrays, `fillSlice`/`selectCellYZ`/`updateForY/X/Z` lifecycle).
- **Added `prepareResolver` virtual method** to `DensityFunction` base class, with overrides in `Interpolated`, `CacheMarker`, `TwoArgument`, `Mapped`, `Clamp`, `RangeChoice`, `ShiftedNoiseFunction`, `WeirdScaledSamplerFunction`, `FindTopSurfaceFunction`, `PeaksAndValleysFunction` — traverses the density tree to pre-register all markers before `fillSlice`.
- **Result**: NoiseChunk port reduced mismatches from 216K (initial broken state) to 10,884, but this is WORSE than the 7,673 baseline. The port introduced new mismatches while fixing others.
- **Reverted**: restored all files to the 7,673-mismatch baseline. NoiseChunk.h/.cpp removed. `prepareResolver` infrastructure kept in DensityFunction.h for future use.
- **Root cause of remaining 10,884 mismatches in the port**: likely the `CacheOnce` implementation is missing the `lastArray`/`arrayInterpolationCounter` optimization path, or the `FlatCache`/`Cache2D` implementations have incorrect key computation. Needs further investigation.
- **Files examined**: `26.1.2/src/net/minecraft/world/level/levelgen/NoiseChunk.java` (809 lines), `src/world/level/levelgen/DensityFunction.{h,cpp}`, `src/world/level/levelgen/NoiseBasedChunkGenerator.cpp`

---

### 2026-06-20 — Session: Interpolation order + CornerResolver + key finding

**Agent**: Super Z (GLM)

- **CornerResolver**: during corner computation, inner Interpolated markers now return 0.0 (matching Java's NoiseInterpolator.value stale behavior during fillSlice). Cache markers compute normally.
- **Lerp order**: changed computeInterpolated from X→Y→Z (lerp3) to Y→X→Z to match Java's NoiseInterpolator incremental path (updateForY → updateForX → updateForZ).
- **Key finding**: `BaseTerrainColumnParity` (fillFromNoise, 14 sampled columns × 4 seeds = 21,504 cases) passes with **0 mismatches**. This proves the density computation is byte-exact for sampled positions. The 7,673 full-chunk mismatches are at specific cell boundary positions that the column test doesn't sample.
- **Root cause refined**: the CellInterpolationResolver is correct for most positions but has subtle FP differences at specific cell boundaries. A full NoiseChunk port with slice arrays is needed to resolve these — the per-cell resolver approach cannot exactly replicate Java's chunk-persistent NoiseInterpolator state.
- **All noise/density subsystems remain byte-exact**: ImprovedNoise (956K), PerlinNoise (6M), BlendedNoise (18.6K), NormalNoise (81K), SimplexNoise (125K), PerlinSimplexNoise (141K), WorldgenRandom (540), OverworldBiome (300K), ClimateBiome (56), StructurePlacement (29K), Mth (5.4K), MthExtra (6K), DensityRouter (488), BaseTerrainColumn (21.5K).
