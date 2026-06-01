# AGENTS.md — Minecraft 26.1.2 → C++ Port
> This document is the single source of truth for any AI agent working on this project.
> It is maintained by the lead agent and must be updated after every session.
> If you are reading this, assume you have NO conversation context. Follow this document exactly.

---

## WHAT THIS PROJECT IS

A faithful **port** of Minecraft Java Edition 26.1.2 to native C++ for Windows.

**RULE #1 — NON-NEGOTIABLE**: This is NOT a clone. Do NOT invent gameplay, features, or systems.
Every system must be reverse-engineered from the decompiled Java source and ported 1:1.
If the Java does X, the C++ does X. If you are unsure, read the Java source first.

**Goals:**
- Native Windows x64 executable, standalone (zero runtime dependencies shipped with it)
- All game assets embedded directly in the `.exe` binary
- Multi-backend rendering: OpenGL 4.6, Vulkan 1.3, DirectX 12
- High performance: target 500+ FPS on modern hardware at default settings
- Faithful to the original: same protocol, same world format, same behavior

---

## SOURCE MATERIAL LOCATIONS

```
C:\Users\Mateo\Desktop\Claude\
├── 26.1.2\
│   ├── client.jar          ← Original JAR (do not modify)
│   └── src\                ← Decompiled Java source (25,635 files) — READ THIS
│       ├── net\minecraft\  ← Main game code (6,596 files)
│       └── com\mojang\     ← Mojang platform libs (286 files)
├── assets\
│   ├── indexes\30.json     ← Asset index
│   └── objects\            ← 4,749 raw asset files (textures, sounds, etc.)
└── AGENTS.md               ← This file (keep updated)
```

**Java source package → purpose quick reference:**
| Java Package | Files | What it does |
|---|---|---|
| `net/minecraft/client/` | 1,813 | Client logic, rendering, GUI, input |
| `net/minecraft/world/` | 2,559 | Blocks, entities, items, physics |
| `net/minecraft/network/` | 411 | Network protocol, packets |
| `net/minecraft/server/` | 418 | Integrated server |
| `net/minecraft/nbt/` | 43 | Named Binary Tag format |
| `net/minecraft/util/` | 713 | Utilities, data fixers |
| `com/mojang/blaze3d/` | 149 | Graphics abstraction (OpenGL wrapper) |

---

## FETCHING SOURCE MATERIAL FROM THE MOJANG CDN

The Mojang CDN is open to `curl` (no auth) and is the canonical way to obtain
the inputs above on a fresh/ephemeral machine (e.g. a cloud session). Everything
lands under the git-ignored `26.1.2/` directory, so nothing proprietary is ever
committed. The version manifest pins `26.1.2` as the current release; its client
classes are Java 25 bytecode (v69) and libraries are downloaded separately.

```bash
# 1. Manifest -> version json -> client.jar (verify sha1) + asset index
curl -s https://piston-meta.mojang.com/mc/game/version_manifest_v2.json -o /tmp/vm.json
VJSON=$(jq -r '.versions[]|select(.id=="26.1.2").url' /tmp/vm.json)
curl -s "$VJSON" -o 26.1.2/26.1.2.json
mkdir -p 26.1.2
curl -s "$(jq -r .downloads.client.url 26.1.2/26.1.2.json)" -o 26.1.2/client.jar   # sha1 in json

# 2. Worldgen DATA json (authoritative 1:1 values) lives INSIDE client.jar
unzip -o -q 26.1.2/client.jar 'data/minecraft/worldgen/*' -d 26.1.2
#   -> noise_settings/, density_function/, biome/, noise/,
#      multi_noise_biome_source_parameter_list/ (just {"preset":...}), etc.

# 3. Decompiled Java source (read-only reference) via Vineflower
curl -s https://repo1.maven.org/maven2/org/vineflower/vineflower/1.12.0/vineflower-1.12.0.jar -o vineflower-1.12.0.jar
mkdir -p 26.1.2/classes_stage
unzip -o -q 26.1.2/client.jar 'net/minecraft/world/level/*' 'net/minecraft/world/level/biome/*' \
   'net/minecraft/util/*' 'net/minecraft/core/*' -d 26.1.2/classes_stage
java -jar vineflower-1.12.0.jar -e=26.1.2/client.jar 26.1.2/classes_stage 26.1.2/src

# 4. (Only to RUN the real code for parity ground truth) JDK 25 + libraries
curl -sL "https://api.adoptium.net/v3/binary/latest/25/ga/linux/x64/jdk/hotspot/normal/eclipse" -o /tmp/jdk25.tgz
mkdir -p 26.1.2/jdk25 && tar -xzf /tmp/jdk25.tgz -C 26.1.2/jdk25 --strip-components=1
mkdir -p 26.1.2/libs
for u in $(jq -r '.libraries[].downloads.artifact.url' 26.1.2/26.1.2.json); do
  curl -s "$u" -o "26.1.2/libs/$(basename "$u")"; done
```

Notes:
- Game *assets* (textures/sounds) come from the asset index
  (`.assetIndex.url`, id `30`) and `https://resources.download.minecraft.net/<2hex>/<hash>`.
  They are not needed for terrain generation and are large (~450 MB), so skip
  unless a task requires them.
- In 26.x, `ResourceLocation` was renamed `Identifier` and
  `ResourceKey.location()` is now `ResourceKey.identifier()`. The C++ port keys
  biomes by their string id (`"minecraft:plains"`) so this rename is transparent.

---

## C++ PROJECT STRUCTURE

```
C:\Users\Mateo\Desktop\Claude\mcpp\     ← C++ project root
├── CMakeLists.txt
├── cmake\
│   ├── FindVulkan.cmake
│   └── CompilerFlags.cmake
├── tools\
│   └── asset_packer\                   ← Offline tool: packs assets/ → assets.bin
│       ├── CMakeLists.txt
│       └── main.cpp
├── src\
│   ├── main.cpp                        ← WinMain entry point
│   ├── core\                           ← Phase 1
│   │   ├── Math.h                      ← GLM re-exports + MC-specific types
│   │   ├── Log.h/.cpp                  ← Logger
│   │   ├── ResourceLocation.h          ← namespace:path identifier
│   │   ├── Registry.h                  ← Generic registry
│   │   └── StbImpl.cpp                 ← stb_image implementation
│   ├── platform\                       ← Phase 3
│   │   ├── Window.h/.cpp               ← Win32 window creation & message pump
│   │   ├── Input.h/.cpp                ← Keyboard + mouse state
│   ├── assets\                         ← Phase 2
│   │   ├── AssetPack.h/.cpp            ← Runtime reader of embedded assets.bin
│   │   ├── AssetManager.h/.cpp         ← Port of ResourceManager.java
│   │   └── TextureAtlas.h/.cpp         ← Block/Item texture management
│   ├── render\                         ← Phases 4, 13, 14
│   │   ├── IRenderDevice.h             ← Abstract GPU interface
│   │   ├── ICommandList.h              ← Abstract draw commands
│   │   ├── RenderBackend.h/.cpp        ← Backend selector (dynamic switching)
│   │   ├── opengl\
│   │   │   ├── DeviceGL.h/.cpp         ← OpenGL 4.6 backend
│   │   │   ├── CommandListGL.h/.cpp
│   │   ├── vulkan\
│   │   │   ├── DeviceVK.h/.cpp         ← Vulkan 1.3 backend
│   │   │   ├── SwapchainVK.h/.cpp
│   │   │   ├── BufferVK.h/.cpp
│   │   │   ├── TextureVK.h/.cpp
│   │   │   ├── PipelineVK.h/.cpp       ← GLSL to SPIR-V at runtime
│   │   │   └── CommandListVK.h/.cpp
│   │   ├── dx12\
│   │   │   ├── DeviceDX12.h/.cpp       ← DirectX 12 backend
│   │   │   ├── SwapchainDX12.h/.cpp
│   │   │   ├── BufferDX12.h/.cpp
│   │   │   ├── TextureDX12.h/.cpp
│   │   │   ├── PipelineDX12.h/.cpp
│   │   │   └── CommandListDX12.h/.cpp
│   │   ├── level\
│   │   │   ├── LevelRenderer.h/.cpp    ← World rendering
│   │   │   ├── ChunkMesh.h/.cpp        ← Greedy mesher
│   │   ├── entity\
│   │   │   ├── EntityRenderDispatcher.h/.cpp
│   │   └── gui\
│   │       ├── GuiGraphics.h/.cpp      ← HUD/UI rendering primitive
│   │       └── Font.h/.cpp             ← Bitmap font system
│   ├── nbt\                            ← Phase 5
│   │   ├── Tag.h/.cpp                  ← NBT structure
│   │   └── NbtIo.h/.cpp                ← NBT reading
│   ├── world\                          ← Phases 7, 15
│   │   ├── level\
│   │   │   ├── levelgen\
│   │   │   │   └── Noise.h/.cpp        ← Perlin noise generation
│   │   │   ├── chunk\
│   │   │   │   ├── LevelChunk.h/.cpp
│   │   │   │   └── PalettedContainer.h/.cpp
│   │   │   └── block\
│   │   │       └── Blocks.h/.cpp
│   │   ├── inventory\
│   │   │   ├── Container.h             ← Inventory interface
│   │   │   ├── SimpleContainer.h/.cpp
│   │   │   └── Slot.h/.cpp
│   │   ├── entity\
│   │   │   ├── Entity.h/.cpp           ← Base entity + Metadata + Equipment
│   │   │   ├── Mob.h/.cpp              ← Base mob class with AI
│   │   │   ├── ai\
│   │   │   │   ├── Goal.h/.cpp         ← AI Goal base and WrappedGoal
│   │   │   │   ├── GoalSelector.h/.cpp ← Priority-based AI execution
│   │   │   │   └── RandomStrollGoal.h/.cpp
│   │   │   └── player\
│   │   │       └── Player.h/.cpp
│   │   └── phys\
│   │       └── AABB.h/.cpp
│   ├── network\                        ← Phase 8
│   │   ├── Connection.h/.cpp           ← TCP connection
│   │   ├── PacketBuffer.h/.cpp         ← Protocol serialization
│   │   └── protocol\
│   │       └── game\
│   │           └── PlayPackets.h/.cpp  ← Game protocol packets
│   ├── client\                         ← Phase 1 + integration
│   │   ├── Minecraft.h/.cpp            ← Main game class
│   │   └── player\
│   │       └── LocalPlayer.h/.cpp      ← Local player controller
│   ├── gui\                            ← Phase 11
│   │   ├── components\
│   │   │   └── Button.h/.cpp           ← UI Button component
│   │   ├── screens\
│   │   │   ├── Screen.h                ← Screen base class
│   │   │   └── TitleScreen.h/.cpp      ← Main menu
│   │   └── Gui.h/.cpp                  ← HUD
│   └── audio\                          ← Phase 12
│       ├── SoundEngine.h/.cpp          ← XAudio2-based engine
│       ├── SoundManager.h/.cpp         ← Decoded Ogg cache, categories
│       ├── SoundSource.h               ← Sound categories enum
│       ├── SoundEvent.h                ← Sound name/range structure
│       └── OggDecoder.h/.cpp           ← stb_vorbis wrapper
```

---

## DEPENDENCY POLICY (STRICT)

**Allowed runtime dependencies**: Win32, D3D12, Vulkan, OpenGL, WinSock2, XAudio2.
**Statically linked**: GLM, nlohmann/json, stb_image, stb_vorbis, GLAD, miniz (zlib), volk, glslang, D3D12MA.

---

## BUILD SYSTEM

- **Generator**: CMake 3.28+
- **Compiler**: MSVC 2022 or Clang-CL 17+
- **C++ standard**: C++23
- **Command safety**: All build/run/test commands must go through `mcpp\tools\run_with_timeout.ps1` with an explicit `-TimeoutSec`. Do not run raw long-lived commands.

---

## PHASE TRACKER

### PHASE 0 to 14 ✅ COMPLETE
- Core systems, assets, windowing, rendering (OpenGL, Vulkan, DX12), NBT, world structures, network, entities, GUI, and audio are fully functional.

### PHASE 15 — Game Logic (in progress)
- [x] Mob AI system (`world/entity/ai/Goal.h/.cpp`) — Base class, GoalSelector, and RandomStrollGoal implemented.
- [x] Inventory management (`world/inventory/`) — Container interface, SimpleContainer, and Slot implemented.
- [x] World generation foundations (`world/level/levelgen/`) — ImprovedNoise (Perlin) implemented.
- [x] Overworld biome climate selection is now 1:1: `Climate::RTree` ported (the
      production `findValue` search) and `OverworldBiomeBuilder` verified
      byte-identical to real Java (7593 entries) — see "biome selection" below.
- [x] Biome registry data: all 65 biomes loaded 1:1 from `worldgen/biome/*.json`
      (`world/level/biome/Biome.h` + `BiomeRegistry`), verified field-for-field
      against an independent parser. This is the data hub for biome-specific
      blocks/trees/colours/carvers/spawns.
- [ ] Full worldgen feature/structure pipeline — see `mcpp/docs/WORLDGEN_PLAN.md`
      for the complete phased checklist (features, trees, carvers, structures).
- [ ] Port remaining ~277 mob goals.
- [ ] Implement Crafting and Smelting logic.

### PHASE 16 — Integration & Optimization
- [ ] Multi-threaded chunk loading & meshing (thread pool).
- [ ] LOD for distant chunks.
- [ ] GPU-driven rendering (indirect draw calls).

---

## CURRENT STATE

**Last updated**: Session 31 (Biome registry data layer — all 65 biomes 1:1 + WORLDGEN_PLAN.md)
**Current phase**: PHASE 15 (Game Logic) in progress; worldgen feature/structure port started
**Executable**: `C:\Users\Mateo\Desktop\Claude\mcpp\build\mcpp.exe` — built 2026-05-31

**Worldgen roadmap**: `mcpp/docs/WORLDGEN_PLAN.md` is the master 1:1 checklist for
the full feature/tree/structure port (Phases A–G, with real data counts). Phase A
(biome registry) is done; Phases B–G (decoration framework, feature types, trees,
carvers, structures, engine integration) are the remaining work. Do NOT reintroduce
the removed hand-authored approximate generators — port from Java + data only.

**Next action**: PHASE 15 — Game Logic
1. ~~Audit `OverworldBiomeBuilder.cpp` line-by-line against `OverworldBiomeBuilder.java`.~~
   DONE (Session 30): proven byte-identical to the real Java output (7593 entries,
   all quantized longs + biome ids + order match exactly). See `overworld_biome_parity`.
2. Split/rename `BiomeSource` into the Java-shaped roles (`BiomeSource`, `MultiNoiseBiomeSource`, parameter-list preset provider) instead of leaving the current combined wrapper.
3. Biome *selection* parity is now covered for the overworld preset (Session 30:
   `Climate::RTree` ported and verified against the real Java RTree over 300k
   targets incl. all 4155 distance-tie cases). Still TODO: wire the real noise
   router so the *sampled* climate (temperature/humidity/... density functions)
   is also verified end-to-end, and add the same coverage for nether/end presets.
4. Continue the Java-faithful surface pipeline: verify/finish `SurfaceRules`, `SurfaceSystem`, and `SurfaceRuleData` against the decompiled Java before claiming them complete.
5. Port placed/configured features and structures only from Java/data definitions. The approximate tree/ore/surface-decoration/structure generators added by another LLM were removed from the build and must not be re-enabled as-is.

**Known issues / limitations:**
- Online mode auth not implemented.
- Multipart block state counts may be off.
- Entity models lack textures (colored boxes).
- LocalPlayer physics not implemented yet.
- The current local singleplayer terrain now runs through a Java-shaped `NoiseRouter.finalDensity` path using ported Java RNG, `BlendedNoise`, Java `TerrainProvider` offset/factor/jaggedness splines, and the Java overworld caves/noodle/pillars density composition. `NoiseChunk` marker semantics now include `interpolated`, `cacheOnce`, `cacheAllInCell`, `cache2D`, and `flatCache` via the C++ cell resolver; blend wrappers remain identity because old-chunk blending is not ported yet.
- Use `--quickPlaySingleplayer` to auto-enter the local generated world for harnessed smoke tests.
- `Blocks` now exposes all loaded block names and default state IDs from embedded `block_states.json` (`16,584` states). This is the required base for Java-faithful `Blocks.X.defaultBlockState()` usage.
- Ported terrain-generation records/foundations now include `NoiseSettings`, `NoiseGeneratorSettings`, `RandomSource` (`LegacyRandomSource`, `SingleThreadedRandomSource`, `XoroshiroRandomSource`, positional factories), `SimplexNoise`, `PerlinSimplexNoise`, `ImprovedNoise`, `PerlinNoise`, `NormalNoise`, `BlendedNoise`, `Noises`/`NoiseData` parameters, `RandomState`, `CubicSpline`, `TerrainProvider`, Java overworld cave/noodle/pillar density helpers, `DensityFunction`, core/noise-backed `DensityFunctions` nodes, `EndIslandDensityFunction`, `FindTopSurface`, `NoiseRouter`, `OreVeinifier`, and partial `NoiseRouterData` routers.
- `SurfaceSystem::frozenOceanExtension()` now ports `Biome.shouldMeltFrozenOceanIcebergSlightly()` for frozen/deep-frozen ocean using Java's `TemperatureModifier.FROZEN` logic and the `FROZEN_TEMPERATURE_NOISE`/`BIOME_INFO_NOISE` seeds, replacing the earlier hardcoded no-melt approximation.
- `Aquifer` is now ported as its own C++ module with Java's global fluid picker, disabled aquifer path, and noise-based aquifer sampling/pressure/fluid-level logic. `NoiseBasedChunkGenerator::fillFromNoise()` now uses `aquifer.computeSubstance(context, density)` and falls back to `defaultBlock` when Java would return `null`.
- `OreVeinifier` is now ported and wired after Aquifer, matching Java's `NoiseChunk` material rule order when `settings.oreVeinsEnabled()` is true.
- `SurfaceRules`, `SurfaceSystem`, and `SurfaceRuleData` exist but are not certified complete. Treat them as partial until each condition/rule is checked against Java source.
- The previous LLM added `BiomeSource`, `feature/TreeGen`, `feature/OreGen`, `feature/SurfaceDecoration`, and `structure/StructureGen` as approximate systems. They contained hand-authored biome points, per-chunk random scattering, hardcoded chances, and simplified structures. Session 23 removed them from `src/CMakeLists.txt` and removed their calls from `NoiseBasedChunkGenerator::buildSurface()`. The files may remain on disk for reference, but they are intentionally out of the executable.
- `NoiseBasedChunkGenerator::buildSurface()` now passes an empty biome key to `SurfaceSystem` until the Java `Climate` / `MultiNoiseBiomeSource` pipeline is ported. This keeps biome-dependent rules inactive instead of feeding them fake biomes.
- Session 24 replaced the fake `BiomeSource` implementation with a Java-shaped foundation: `Climate.h` now ports `quantizeCoord`, `Parameter`, `TargetPoint`, `ParameterPoint::fitness`, and `ParameterList::findValue` using the same distance metric as Java.
- Session 25 ported the full `OverworldBiomeBuilder` preset shape: off-coast biomes, all `addInlandBiomes()` weirdness slices (`addPeaks`, `addHighSlice`, `addMidSlice`, `addLowSlice`, `addValleys`), underground biomes, and the Java `pick*` helper decisions. `NoiseBasedChunkGenerator::buildSurface()` now passes this biome lookup into `SurfaceSystem` for overworld terrain. This is a direct C++ port scaffold and must be audited line-by-line before claiming final 1:1 biome behavior.
- Session 26 mechanically split the Overworld preset builder out of `BiomeSource.cpp` into `OverworldBiomeBuilder.h/.cpp`. `BiomeSource.cpp` is now only the router-sampling wrapper that builds the preset and calls `Climate::ParameterList::findValue()`. Build and quick singleplayer smoke passed after the split.
- Session 27 added `BiomeManager.h/.cpp`, porting Java's block-position biome zoom: subtract-2 block offset, parent quart cell selection, 8-corner fiddled distance, `LinearCongruentialGenerator.next`, and `getFiddle`. `BiomeManager::obfuscateSeed()` uses Windows BCrypt SHA-256 over little-endian seed bytes and reads the first 8 digest bytes little-endian to match Guava `Hashing.sha256().hashLong(seed).asLong()`. `NoiseBasedChunkGenerator::buildSurface()` now asks `BiomeManager::getBiome(blockX, blockY, blockZ)` instead of directly sampling `BiomeSource`.
- Session 28 added `tools/BiomeManagerExpected.java` to generate independent Java reference values and `biome_manager_parity` CMake target (`BiomeManagerParityTest.cpp`). The test verifies `obfuscateSeed`, `getFiddle`, one fiddled-distance sample, and selected quart coordinates for several block positions against Java output. Harnessed target build and run passed, followed by full Release build and quick singleplayer smoke.
- Session 30 fetched the real `26.1.2` `client.jar` from the Mojang CDN (sha1
  verified), extracted the worldgen DATA json, and decompiled the worldgen Java
  with Vineflower (see "FETCHING SOURCE MATERIAL"). Two terrain-gen facts were
  established against that ground truth:
  - `OverworldBiomeBuilder.cpp` is a verified 1:1 port: its parameter list is
    byte-for-byte identical to the real Java `OverworldBiomeBuilder` (7593
    entries, same order, same quantized longs, same biome ids). Float
    quantization (`(long)(coord*10000f)`) matches exactly.
  - `Climate::ParameterList::findValue` previously used a brute-force linear
    scan, but Java production uses `Climate.RTree` (an exact branch-and-bound
    6-ary tree with a `lastResult` seed). These disagree at distance ties: over
    300k sampled targets the two diverged on 4155 (~1.4%), i.e. the old C++ picked
    the wrong biome there. `Climate.RTree` is now ported into `Climate.h`
    (build/bucketize/sort with `std::stable_sort` to match Java `List.sort`,
    branch-and-bound `search`, `lastResult` seed). Verified: C++ matches the real
    Java RTree on all 300k targets, including every tie case.
  - New: `tools/OverworldBiomeParity.java` (reference generator that runs the
    real decompiled code) and the `overworld_biome_parity` CMake target
    (`OverworldBiomeParityTest.cpp`). The test is pure standard C++ (no bcrypt),
    so it builds and runs on Linux too; `--cases <tsv>` does the full jar-backed
    comparison, default mode does self-contained invariant checks.
  - CAVEAT: the C++ `RTree` `lastResult` is a plain `mutable` pointer (single
    instance), matching Java's per-thread `ThreadLocal` only for single-threaded
    sampling. If biome sampling is ever parallelised, make `lastResult`
    thread-local to avoid a data race and to match Java's per-thread semantics.
    `lastResult` only affects which leaf is returned among exact distance ties.
- Session 31 began the full worldgen feature/structure port (master plan:
  `mcpp/docs/WORLDGEN_PLAN.md`). Delivered Phase A — the biome registry data
  layer (`world/level/biome/Biome.h` + `BiomeRegistry.{h,cpp}`): a faithful
  loader of all 65 `worldgen/biome/*.json` (climate, effects colours, the new
  26.1.2 `attributes` map incl. value-modifier form for water_fog_end_distance,
  carvers, the 11 ordered placed-feature steps, 8 spawner categories, spawn
  costs). Verified by `biome_registry_parity` (CMake target; pure std C++ +
  nlohmann, builds on Linux) and `tools/biome_registry_reference.py`, which
  agree on all 65 biomes / 1525 normalised fields (independent parsers). NOTE:
  the registry is not yet wired into the engine — the runtime needs the
  `worldgen/*` JSON embedded in the asset pack and `BiomeSource` resolving real
  `Biome` objects instead of id strings (Phase A "remaining" + Phase G). The
  loader currently reads a directory; `BiomeRegistry.cpp` is built only by the
  parity target, not yet added to the main `mcpp` sources.

**Decisions made:**
- AI goals are executed client-side for the port's prototype to simulate living behavior in offline mode.
- Inventory system mirrors the Java `Container` interface for future compatibility with server-side sync.
- The Title Screen `Singleplayer` button now starts an offline local world instead of attempting localhost networking; `Connect to Localhost` remains available as a separate button.
- OpenGL buffer allocation uses direct-state-access (`glCreateBuffers`/`glNamedBufferData`) so index buffers are valid without relying on VAO state during creation.
- The current visible singleplayer terrain scaffold must not be treated as final terrain generation. Replace it systematically with direct ports from the Java decompiled source; do not tune or invent noise behavior.
