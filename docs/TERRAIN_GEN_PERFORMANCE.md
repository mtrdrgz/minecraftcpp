# Terrain Generation Performance Report

**Scope:** Why chunk terrain generation is slow in the C++ port, ranked by impact, with
file/line evidence and the corresponding Java behaviour it diverges from.

**Method:** Static read-through of the generation pipeline
(`NoiseBasedChunkGenerator`, `DensityFunction`, `Noise`, `RandomState`, `Aquifer`,
`SurfaceSystem`, `BiomeSource`/`BiomeManager`, `Climate`, `PalettedContainer`,
`LevelChunk`) and the runtime scheduler in `client/Minecraft.cpp`. No profiler run was
performed; cost estimates are derived from the call structure and iteration counts.

> This is an analysis-only document — no generation code was changed.

---

## TL;DR — the four things that dominate

| # | Bottleneck | Where | Rough cost | Java does instead |
|---|-----------|-------|-----------|-------------------|
| 1 | **A brand-new generator (full noise router + biome R-tree) is built per chunk**, then **`RandomState` is rebuilt twice more** | `Minecraft.cpp:901-908`, `NoiseBasedChunkGenerator.cpp:290-313,439,475` | Hundreds of MD5 hashes + tens of thousands of RNG draws **per chunk**, ×3 | Build `ChunkGenerator` + `RandomState` **once per world** and share them across every chunk |
| 2 | **Per-block density tree-walk through pointer-keyed, linear-search caches** | `NoiseBasedChunkGenerator.cpp:81-265`, `DensityFunction.cpp` | ~98 k block-evals/chunk × tree depth × O(n) cache scans | Precompiled **index-based** caches + bulk array fill over a cell |
| 3 | **Uncached per-column biome lookups** in surface build and carvers | `SurfaceSystem.cpp:125`, `BiomeSource.cpp:36-55` | 256+ lookups/chunk, each = 6 climate noise tree-walks + R-tree search, **no interpolation cache** | Climate samples reuse the chunk's `FlatCache`/`Cache2D` resolver |
| 4 | **Uncached preliminary-surface sampling hammered by the aquifer** | `NoiseBasedChunkGenerator.cpp:327-332`, `Aquifer.cpp:339-347,372-396` | `findTopSurface` (a Y-loop of full density walks) called on a 4-block grid + 13× per aquifer cell, all uncached | Preliminary surface is a cached density function sampled through the chunk's resolver |

Everything below expands these, then lists secondary issues.

---

## How generation is driven (baseline facts)

Runtime loop, every tick (`Minecraft.cpp:780-915`):

- Render distance `RADIUS = 6` → a 13×13 = **169-chunk** working set (`Minecraft.cpp:784`).
- Chunks outside the radius are **unloaded** (`Minecraft.cpp:786-798`) and there is **no
  save/cache** — re-entering an area **regenerates from scratch**.
- Generation runs on a `ThreadPool` **capped at 4 workers** (`ThreadPool.cpp:12`), at most
  `MAX_QUEUE_PER_TICK = 4` new chunks queued per tick (`Minecraft.cpp:895`).
- The worker task is the critical one:

```cpp
// Minecraft.cpp:901-908
m_threadPool->enqueue([pos = cand.pos, seed = m_worldSeed]() -> std::unique_ptr<LevelChunk> {
    auto chunk = std::make_unique<LevelChunk>(pos);
    levelgen::NoiseBasedChunkGenerator generator(seed);  // <-- rebuilt every chunk
    generator.fillFromNoise(*chunk);
    generator.buildSurface(*chunk);
    generator.applyCarvers(*chunk);
    return chunk;
});
```

- Decoration (trees) and structures run **on the main thread**, budgeted to
  `MAX_DECORATE_PER_TICK = 2` (`Minecraft.cpp:847-855`).
- Meshing runs **on the render thread**, 1 chunk/frame (`LevelRenderer.cpp` rebuild loop).

`fillFromNoise` visits **every block** of the column: with overworld
`height = 384`, `cellHeight = 8`, `cellWidth = 4`, that is `4×4×48 = 768` cells ×
`128` blocks = **98 304 density evaluations per chunk** (`NoiseBasedChunkGenerator.cpp:343-422`).

---

## Bottleneck 1 — the generator is rebuilt from scratch for every chunk

This is the single largest and most clear-cut cost.

`Minecraft.cpp:903` constructs a fresh `NoiseBasedChunkGenerator(seed)` **inside the
per-chunk worker task**. Its constructor (`NoiseBasedChunkGenerator.cpp:290-313`) does:

1. `RandomState randomState(...)` — forks positional RNGs (`RandomState.cpp:16-22`).
2. `makeRouter(...)` → `NoiseRouterData::overworld(...)` (`NoiseRouterData.cpp:273-356`),
   which calls `randomState.getOrCreateNoise(...)` **~40 times**. Each call
   (`RandomState.cpp:24-38` → `Noises::instantiate` `Noises.cpp:123-127`) does:
   - `fromHashOf(name)` → a **real MD5 of the noise name string** (`RandomSource.cpp:95-145`).
   - `NormalNoise::create` → **two** `PerlinNoise` (`Noise.cpp:547-569`); each Perlin builds
     one `ImprovedNoise` **per octave** (up to 9–16 octaves; e.g. `JAGGED` has 16,
     `Noises.cpp:69`). Each `ImprovedNoise` ctor runs a **256-iteration Fisher–Yates
     shuffle** + 3 `nextDouble` (`Noise.cpp:267-280`), and each octave seed is **another
     MD5** via `fromHashOf("octave_N")` (`Noise.cpp:420-426`).
   - `overworldBase3d` builds a `BlendedNoise` = **3 legacy Perlin** of 16+16+8 octaves =
     **40 more `ImprovedNoise`** (`NoiseRouterData.cpp:38`, `Noise.cpp:586-596`).
3. `BiomeSource` (`NoiseBasedChunkGenerator.cpp:308`) builds a **Climate R-tree** over the
   **full overworld biome preset** (~100+ parameter points) — recursive bucketization with
   repeated 7-dimension `std::stable_sort`s (`Climate.h:267-333`, `BiomeSource.cpp:22-26`).
4. `BiomeManager` obfuscates the seed with **SHA-256** (`BiomeManager.cpp:63-69`).

Order-of-magnitude per chunk: **dozens of NormalNoise → ~120+ ImprovedNoise permutation
builds → hundreds of MD5/SHA-256 digests → a full R-tree rebuild** — none of which depends
on chunk position. In vanilla Java this is constructed **once per world** (the server holds
one `ChunkGenerator` and one `RandomState`); here it is paid **for every chunk**, and chunks
are even regenerated when the player walks back (Bottleneck 5).

It then gets paid **two more times**:

- `buildSurface` builds **another** `RandomState` (`NoiseBasedChunkGenerator.cpp:436-439`),
  and `SurfaceSystem`'s ctor instantiates **9 more noises** + generates clay bands
  (`SurfaceSystem.cpp:56-80`).
- `applyCarvers` builds **yet another** `RandomState` + `SurfaceSystem`
  (`NoiseBasedChunkGenerator.cpp:475-476`).

The comment at `NoiseBasedChunkGenerator.cpp:436-438` claims this is "cheap since noise
instances are just seeded Perlin trees" — that is exactly the expensive part, and it is the
top optimization target.

**Fix direction:** construct the generator (router, biome source/manager) and a single
`RandomState` **once per world**, store them, and pass them to each chunk task. The router's
density graph and the noise instances are immutable and read-only during sampling, so they
are safe to share across worker threads (the per-chunk mutable state is the
`CellInterpolationResolver`, the `Aquifer`, and the `OreVeinifier`, which are already created
per call). This alone should remove the majority of per-chunk CPU.

---

## Bottleneck 2 — per-block tree-walk with pointer-keyed linear-search caches

`fillFromNoise` evaluates `m_router.finalDensity->compute(blockContext)` **once per block**
(`NoiseBasedChunkGenerator.cpp:399`), i.e. ~98 k times/chunk. Each call walks the entire
density-function graph as a tree of **virtual `compute()` calls** over heap-allocated
`shared_ptr` nodes (`DensityFunction.cpp`; `DensityFunctionPtr` is
`std::shared_ptr<const DensityFunction>`, `DensityFunction.h:12`).

The caching layer that is supposed to make this affordable is implemented with **linear
searches keyed by raw function pointer** (`NoiseBasedChunkGenerator.cpp:81-265`). For every
cache/interpolation node, every block:

- `computeInterpolated` scans `m_cornerValues[0..count]` for a matching `func` pointer
  (`:90-95`).
- `computeCacheOnce` / `computeCacheAllInCell` → `computeCellCached` scans
  `entries[0..count]` for the pointer (`:242-247`).
- `computeCache2D` scans up to 32 entries, then up to 16 keys (`:138-161`).
- `computeFlatCache` scans up to 32 entries (`:172-177`).

So each block pays *(graph depth)* × *(O(n) pointer scan per cache marker)*. The
`cacheAllInCell` wrapper on `finalDensity` (`NoiseBasedChunkGenerator.cpp:297`) is *pure
overhead* in the fill loop: each of the 128 cell-local blocks is computed exactly once and
never re-read, so the cache only adds a store + linear scan with no reuse.

Vanilla Java compiles the density graph **once** into a flat list and assigns each cached
function an **integer slot**; `NoiseChunk` fills the interpolated functions at the
**2×2×2 cell corners** and then does pure-arithmetic trilinear interpolation for interior
blocks, with all cache hits being **O(1) array indexing**. The expensive `BlendedNoise`
*is* correctly limited to the 8 corners here (via the `interpolated` node,
`NoiseRouterData.cpp:437-443`), but everything around it is paid per block through the
slow path.

**Fix direction:** replace the pointer-keyed linear caches with index-based slots assigned
at graph-build time (mirror Java's `NoiseChunk` wrapping pass), or at minimum store the
cache slot directly on the marker node instead of searching a per-resolver array.

---

## Bottleneck 3 — uncached biome lookups during surface build and carving

`SurfaceSystem::buildSurface` loops over all **256 columns** and calls
`biomeGetter(blockX, startingHeight, blockZ)` per column (`SurfaceSystem.cpp:112-125`).
That routes to `BiomeManager::getBiome` → `debugSelectQuart` (8-corner LCG fiddle,
`BiomeManager.cpp:71-111`) → `BiomeSource::getNoiseBiome`.

`getNoiseBiome` builds a `DensityFunctionContext` with **no interpolation resolver**
(`BiomeSource.cpp:45`) and computes **six** climate density functions
(`temperature, vegetation, continents, erosion, depth, ridges`, `:46-52`). Each climate
function is a `shiftedNoise2d` whose `shiftX`/`shiftZ` are themselves noise functions
(`NoiseRouterData.cpp:276-288`), so a single biome lookup is on the order of **~18
`NormalNoise::getValue` evaluations** (each = 2 Perlin trees), followed by a **Climate
R-tree search** (`Climate.h:194-200`). Because the context carries no resolver, **none of
this is cached** — the `flatCache`/`cache2d` markers fall straight through to recompute
(`DensityFunction.cpp:84-87`).

The same uncached `getBiome` is wired into the carvers' `topMaterial` path
(`NoiseBasedChunkGenerator.cpp:472-492`) and `buildSurface` again triggers biome-dependent
extensions for badlands/frozen-ocean columns (`SurfaceSystem.cpp:126-128,175-177`).

**Fix direction:** sample climate through the chunk's cached resolver (Java reuses the
chunk noise caches for biome lookup), and/or cache biome results per quart-column for the
chunk instead of recomputing per surface column and per carver query.

---

## Bottleneck 4 — preliminary-surface sampling is uncached and called in bulk

`samplePreliminarySurfaceLevel` (`NoiseBasedChunkGenerator.cpp:327-332`) computes
`m_router.preliminarySurfaceLevel->compute(...)` with **no resolver**, so it is fully
uncached. That density function is `findTopSurface` (`DensityFunction.cpp:415-443`), which
**loops in `cellHeight` steps from the top down**, doing a full density `compute()` at each
step — an inner loop of expensive walks per call.

The aquifer then calls this getter heavily for every chunk
(`fillFromNoise` enables aquifers by default, `NoiseBasedChunkGenerator.cpp:362-372`):

- `maxPreliminarySurfaceLevel` scans a grid **every 4 blocks** over the padded chunk
  (`Aquifer.cpp:339-347`).
- `computeFluid` samples **13 surface offsets** per aquifer cell (`Aquifer.cpp:372-396`),
  and `computeSubstance` runs the 2×3×2 neighbour search per solid block
  (`Aquifer.cpp:186-235`).

Each of those surface samples re-runs the uncached `findTopSurface` Y-loop.

**Fix direction:** wrap/precompute the preliminary surface as a cached 2-D field for the
chunk (Java's `preliminarySurfaceLevel` is sampled through the cached router) so the aquifer
reads memoized values instead of re-walking the density tree.

---

## Bottleneck 5 — no chunk persistence; constant regeneration churn

Chunks live in `m_chunks` and are dropped when they leave `RADIUS = 6`
(`Minecraft.cpp:786-798`). There is no disk save and no LRU retention band, so **walking
back into an area pays the full generation cost (Bottlenecks 1–4) again**. Combined with
the per-chunk generator rebuild, back-and-forth movement is pathological.

**Fix direction:** keep a larger retention/eviction margin than the load radius, and/or
serialize generated chunks so re-entry is a load, not a regenerate.

---

## Bottleneck 6 — limited parallelism and main-thread serialization

- The pool is capped at **4 workers** (`ThreadPool.cpp:7-12`) regardless of core count, and
  only **4 chunks are queued per tick** (`Minecraft.cpp:895`). On an 8–16 core machine the
  generator is leaving most cores idle while each task is far heavier than it should be.
- **Decoration/structures run on the main thread** at 2/tick (`Minecraft.cpp:839-855`),
  gated on all 8 neighbours being present, so the visible fill rate is throttled by the
  main thread even when workers are free.
- **Meshing runs on the render thread**, 1 section-set/frame, re-iterating
  16×16×(section span) blocks (`LevelRenderer.cpp` rebuild loop, `ChunkMesh.cpp` block
  iteration). Newly loaded chunks also mark all 8 neighbours `meshDirty`
  (`Minecraft.cpp:816-824`), multiplying remesh work along a moving frontier.

Note these are *secondary*: with Bottleneck 1 fixed, each worker task shrinks dramatically,
so 4 workers go much further. Raising the worker cap only helps once the per-task cost is
reduced (otherwise more workers just contend and still starve the render thread, which is
exactly what the `ThreadPool.cpp:8-11` comment is working around).

---

## Secondary / micro costs (real but smaller)

- **Heightmap computed twice per chunk** in `buildSurface` — once at the start and once at
  the end (`NoiseBasedChunkGenerator.cpp:434,458`), each a full 16×16×384 top-down scan
  (`LevelChunk.cpp:38-59`). `setBlock` also maintains a partial heightmap and flips
  `meshDirty` on **every** block write (`LevelChunk.cpp:27-36`).
- **`PalettedContainer::set` does a linear `std::find` over the palette** on every block
  write, plus occasional full-section rebuilds on bit-width growth / direct-mode switch
  (`PalettedContainer.cpp:58-90`). For ~98 k writes/chunk this adds up.
- **`std::function` type-erasure in hot loops** — `FluidPicker`, `PreliminarySurfaceGetter`,
  `biomeGetter`, `topMaterial` are `std::function`/`std::function`-backed lambdas invoked
  per block/per sample (`Aquifer.h`, `NoiseBasedChunkGenerator.cpp:358-372,452-456,480-492`),
  defeating inlining.
- **`SimplexNoise::getValue(x,y)` recomputes `std::sqrt(3.0)` and the F2/G2 constants on
  every call** (`Noise.cpp:114-118`) — used by the surface/frozen-temperature Perlin-simplex
  noises.
- **`getBaseHeight` scans Y top-down calling `sampleFinalDensity` per block** with no
  resolver (`NoiseBasedChunkGenerator.cpp:334-341`) — an uncached full-tree walk per Y;
  cheap if rarely called, expensive if used for spawn/queries in a loop.
- **`shared_ptr` graph churn** — rebuilding the density graph per chunk (Bottleneck 1) also
  means thousands of atomic refcounted allocations per chunk; sharing the graph removes this
  too.

---

## Recommended order of attack

1. **Build the generator + `RandomState` once per world and share them** across chunk tasks;
   stop rebuilding in `buildSurface`/`applyCarvers`. *(Bottleneck 1 — by far the biggest win,
   and low-risk since the sampled graph is immutable.)*
2. **Replace pointer-keyed linear caches with index-based slots** and bulk-fill interpolated
   functions at cell corners. *(Bottleneck 2.)*
3. **Sample climate/biome and preliminary-surface through cached resolvers** so surface
   build, carvers, and the aquifer stop re-walking noise trees. *(Bottlenecks 3 & 4.)*
4. **Add a chunk retention margin / persistence** to stop regeneration churn. *(Bottleneck 5.)*
5. Only then revisit worker count, per-tick budgets, double heightmap passes, palette
   `find`, and `std::function` hot-path calls. *(Bottleneck 6 + micro costs.)*

A profiler pass (e.g. sampling `fillFromNoise` vs the generator constructor vs
`buildSurface`) is worth running to confirm the 1→4 ordering on the target hardware, but the
call-structure evidence above points strongly at the per-chunk generator rebuild as the
dominant cost.

---

## ⚠️ 1:1-parity guardrail

Per `CLAUDE.md`/`AGENTS.md`, none of these are tuning changes. Every fix above is a
**structural/caching** change that must produce **bit-identical** block output to the
current code and to Java — e.g. sharing one `RandomState` per world is actually *closer* to
vanilla than rebuilding per chunk, and index-based caches must reproduce the exact same
values (and Java's `lastResult`-seeded R-tree tie behaviour, `Climate.h:204`) as today.
Validate each change against the existing `*ParityTest` suite before landing.
