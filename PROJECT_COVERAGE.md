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

Newest entries first. Every agent adds an entry. Format: `### YYYY-MM-DD HH:MM UTC — Session name`.
Short is fine — one bullet per finding, no walls of text.

---

### 2026-06-20 02:00 UTC — Sin table, nextDouble bytecode, -O0, exhaustive carver verification

**Agent**: Super Z (GLM)

- **Sin table verified identical**: dumped ALL 65536 entries from both Java and C++ — 0 differences.
- **-O0 vs -O2 test**: same 7,673 mismatches with no optimization. GCC optimization is NOT the cause.
- **nextDouble bytecode discovery**: Vineflower decompiler generated INCORRECT source for `BitRandomSource.nextDouble()`. Decompiled source shows `combined * 1.110223E-16F` (float), but actual bytecode uses `l2d`+`dmul` (double×double) = exactly what C++ does.
- **Carver transition analysis**: 7,673 mismatches include BOTH "C++ missed carving" AND "C++ carved extra" → paths diverge, not just less output.
- **Carver change count**: Java carves 16,657 positions, C++ carves 8,984 correctly + misses 7,673 (54% of Java output).
- **CarverTraceParity tool**: NPEs on buildSurface (null structureManager). FullChunkParity avoids NPE through unknown mechanism.
- **EXHAUSTIVE VERIFICATION COMPLETE**: every component verified identical in isolation (RNG 867/867, sin 65536/65536, configs, math, aquifer 32/32, mask, canReach, shouldSkip, nextDouble bytecode, -O0). Issue requires combined-system trace.
- **Commits**: b70e8b0b, aac2d11c, 09f63794, c9094317, a59597db, 96c0b43a

---

### 2026-06-20 01:00 UTC — Carver isolated, aquifer verified, RNG verified

**Agent**: Super Z (GLM)

- **BREAKTHROUGH**: Added `full_chunk_noise_only_parity` and `full_chunk_surface_only_parity` diagnostic targets:
  - fillFromNoise only: **2,359,296 cells, 0 mismatches** ✅
  - fillFromNoise + buildSurface: **2,359,296 cells, 0 mismatches** ✅
  - + applyCarvers: **2,359,296 cells, 7,673 mismatches** ⚠️
- **Terrain density and surface system are byte-exact. ALL 7,673 mismatches are in applyCarvers.**
- **RNG verified**: 867/867 raw float bits identical, 20/20 `next(31)` values identical.
- **Aquifer verified**: `computeSubstance(ctx, 0.0)` returns identical results at all 32 mismatch positions.
- **Carver configs, math, mask, canReach, shouldSkip**: all verified identical line-by-line.
- **Commits**: da8d3134, 4e38a74c

---

### 2026-06-20 00:30 UTC — Interpolation order, CornerResolver, NoiseChunk port attempt

**Agent**: Super Z (GLM)

- **NoiseChunk port attempt**: created `NoiseChunk.h`/`NoiseChunk.cpp` to replace `CellInterpolationResolver`. Reduced mismatches from 216K to 10,884, but WORSE than 7,673 baseline. Reverted.
- **CornerResolver**: inner Interpolated markers return 0.0 during corner computation (matching Java's stale value behavior). Cache markers compute normally.
- **Lerp order**: changed from X→Y→Z to Y→X→Z to match Java's NoiseInterpolator incremental path.
- **BaseTerrainColumn parity**: 21,504 cases, 0 mismatches — density computation is byte-exact for sampled positions.
- **Commits**: eb641af6, 7e206cdc, 2d0871d5

---

### 2026-06-20 00:00 UTC — BlendedNoise fix, GCC support, terrain parity baseline

**Agent**: Super Z (GLM)

- **BlendedNoise ctor arg-eval-order fix** (commit b23410db): delegating-ctor args evaluated right-to-left on GCC, causing `mainNoise` to consume random FIRST. Fix: direct member init in DECLARATION order. BlendedNoise parity: 18,610/18,610 byte-exact (was 100% wrong on GCC).
- **ImprovedNoise 1.0E-7F fix** (commit b23410db): `1.0E-7` → `static_cast<double>(1.0E-7F)` to match Java's float-literal widening. ImprovedNoise parity: 956,530/0.
- **GCC support** (commit b23410db): added GCC branch to `cmake/CompilerFlags.cmake` with `-ffp-contract=off`. Wrapped `biome_manager_parity` bcrypt link in `if(WIN32)`.
- **Terrain parity**: `full_chunk_parity` shows 7,673 mismatches / 2,359,296 cells (99.67% byte-exact) on GCC. Previous "0 mismatches" claim was likely on MSVC (left-to-right arg eval).
- **All noise/density subsystems verified byte-exact**: ImprovedNoise (956K), PerlinNoise (6M), BlendedNoise (18.6K), NormalNoise (81K), SimplexNoise (125K), PerlinSimplexNoise (141K), WorldgenRandom (540), OverworldBiome (300K), ClimateBiome (56), StructurePlacement (29K), Mth (5.4K), MthExtra (6K), DensityRouter (488).
- **Commits**: b23410db, 61a0e5c9

---

### 2026-06-19 22:00 UTC — Repo restructure + README overhaul

**Agent**: Claude (claude-sonnet-4-6)

- **Repo cleanup**: Moved C++ project from `mcpp/` subdirectory to repo root. Deleted all stale/outdated MD files. Updated `.gitignore`, CMake paths, CI workflow, `CLAUDE.md`, `AGENTS.md` path references.
- **Mouse controls fixed**: `LevelRenderer.cpp` — yaw `+=dx` → `-=dx`; pitch `-=dy` → `+=dy`.
- **Texture system fixed**: `Blocks.cpp` — added `assignKnownBlockTextures()` for explicit top/side/bottom differentiation.
- **README rewritten** with collapsible `<details>` dropdowns per system group, Final Certification column.

---

### 2026-06-11 — Session 66+ — Engine integration + full-port kickoff

**Agent**: Claude (previous session)

- **Full-port goal set**: user expanded scope to full 1:1 port of ALL Java files. Created `docs/PORT_COVERAGE.tsv` (6,882 files).
- **Decoration engine integration**: decoration TU compiled into engine. `mcpp.exe --quickPlaySingleplayer` decorates in-game: 65 biomes, 199 placed features, 300+ chunks, zero failures.
- **NBT audit**: `CompoundTag` NOT insertion-ordered, modified-UTF-8 missing, gzip truncating. Fixed. Gate: `nbt_parity 6/0`.
- **PacketBuffer audit**: VarLong 5-byte (should be 10), UTF-16 wrong, unnamed NBT wrong. Fixed. Gate: `packet_buffer_parity 31/0`.
- **AABB audit**: byte-exact, 0 findings. Gate: `aabb_parity 512/0`.
- **PalettedContainer corruption**: resize/direct-mode bug scrambling sections past 16 palette entries. Fixed.
- **Key finding**: `NoiseChunk.cachedClimateSampler` ≠ `randomState.sampler()` — vanilla uses CACHED sampler.

---

### 2026-06-10 — Session 66 — C++ full-cell parity on 6 forest + 6 ocean chunks

**Agent**: Claude (previous session)

- **Forest certification**: `DecorateAll cells=884736 mismatches=0` on all 6 forest chunks.
- **Ocean certification**: `DecorateAll cells=589824 mismatches=0` on all 6 ocean chunks.
- **Java GT = server byte-match**: `FullChunkDecorateParity.java` reproduces real `server.jar` chunks byte-for-byte.

---

### Sessions 63–65 — Terrain byte-match, decoration gap quantification, ore porting

**Agent**: Claude (previous sessions)

- **Session 63**: `full_chunk_parity 2359296/0` — C++ terrain+carvers byte-matches real generator on 6 chunks. Found+fixed `OreVeinifier` bug.
- **Session 64**: Quantified decoration gap: 24905/589824 (4.2%) — all decoration. Proved `.mca` decode correct.
- **Session 65**: Built `full_chunk_decorate_parity` 3×3 driver. Ported `OreFeature.h` + RuleTest + height_range. Ore result: ~92.5%.

---

### Sessions 40–62 — Worldgen foundation, biome climate, noise, density functions

**Agent**: Claude (previous sessions)

- **Session 40 key lesson**: worldgen "worked visually" but was full of disguised approximations. Fixes were not tuning but porting real Java that had been skipped.
- **Biome climate**: `Climate::RTree` + `OverworldBiomeBuilder` — 7,593-entry verification, scaled to 1,867,776-cell gate.
- **Density function DAG**: all 15+ node types ported. `Spline` CubicSpline control points from `TerrainProvider.java`.
- **Surface rules, aquifer system, cave carvers, biome registry**: all ported from Java source.
