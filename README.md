# Minecraft CPP

A ground-up **1:1 reverse-engineering port of Minecraft Java Edition 26.1.2 to native C++** for Windows. Every algorithm, constant, formula, and ordering is ported directly from the decompiled Java source — nothing is invented, approximated, or tuned to "look right."

The result is a standalone `.exe` with all game assets embedded, three rendering backends, full network protocol support, and a worldgen engine that produces byte-identical terrain to the real server when given the same seed.

---

## What this is

This is not a Minecraft clone. It is a **faithful translation** of the original Java codebase into C++, subsystem by subsystem, verified against ground truth from the real game at every step.

The motivation is performance and portability: a native C++ client targeting 500+ FPS on modern hardware, zero Java runtime dependency, and a single self-contained executable.

**The ironclad rule**: every value, formula, and algorithm must come from `26.1.2/src/` (decompiled Java) or `26.1.2/data/` (worldgen JSON). If the Java does X, the C++ does X. See [CLAUDE.md](CLAUDE.md).

---

## Parity certification

The worldgen engine is verified against real server output by running 530+ Java harnesses in `tools/` that execute the actual decompiled game code and diff the output:

| Gate | Cells verified | Mismatches |
|---|---|---|
| Terrain + cave carvers | 2,359,296 | **0** |
| Full decoration — forest chunks (6) | 884,736 | **0** |
| Full decoration — ocean chunks (6) | 589,824 | **0** |
| Biome climate selection | 1,867,776 | **0** |
| NBT round-trip serialization | 6 test vectors | **0** |
| Network packet encoding | 31 protocol messages | **0** |
| AABB collision math | 512 test cases | **0** |

---

## Port status by system

Click any group to expand it.

---

<details>
<summary><strong>🌍 Terrain Generation</strong> — byte-identical to Java server given same seed</summary>

The terrain engine is the deepest subsystem. It replicates Minecraft's multi-stage worldgen pipeline from noise sampling all the way through chunk decoration. Every constant, spline control point, density-function composition, and feature placement algorithm is ported directly from the decompiled Java.

### Noise Foundation

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| `SimplexNoise` | Skewed-grid gradient noise, identical constants | ✅ Byte-exact | 124,712/0 |
| `PerlinNoise` | Permutation-table + fade/lerp, exact Java port | ✅ Byte-exact | 5,980,685/0 |
| `OctaveNoise` | Multi-octave stacking with Java's lacunarity/amplitude | ✅ Byte-exact | 5,980,685/0 (via PerlinNoise) |
| `BlendedNoise` | Blends multiple octave sets as Java does for 3-D terrain shape | ✅ Byte-exact | 18,610/0 |
| `NormalNoise` | Two-octave wrapper used by density function system | ✅ Byte-exact | 81,549/0 |
| `XoroshiroRandom` | Java's world-seed PRNG, bit-identical | ✅ Byte-exact | 540/0 |
| `LegacyRandom` | Java `Random` (LCG), used for decorations and carvers | ✅ Byte-exact | 540/0 |
| `PositionalRandomFactory` | Per-chunk / per-feature RNG seeding | ✅ Byte-exact | 540/0 |

### Density Function System

Minecraft 1.18+ replaced old noise sampling with a composable "density function" DAG. Every node type is ported.

| Node type | What it does | Status | Final Certification |
|---|---|---|---|
| `Constant` | Fixed scalar value | ✅ Byte-exact | 2,359,296/0 |
| `HolderHolder` / `Mapped` | Registry indirection | ✅ Byte-exact | 2,359,296/0 |
| `Add`, `Mul`, `Min`, `Max` | Binary arithmetic | ✅ Byte-exact | 2,359,296/0 |
| `Abs`, `Square`, `Cube`, `HalfNeg`, `QuarterNeg` | Unary ops | ✅ Byte-exact | 2,359,296/0 |
| `Squeeze` | Cubic clamp used by terrain shaper | ✅ Byte-exact | 2,359,296/0 |
| `NoiseWrapper` | Wraps `NormalNoise` as a density function | ✅ Byte-exact | 2,359,296/0 |
| `ShiftedNoise` | Noise evaluated at a shifted position | ✅ Byte-exact | 2,359,296/0 |
| `RangeChoice` | Conditional branch by value range | ✅ Byte-exact | 2,359,296/0 |
| `ShiftNoise` | Horizontal position warp | ✅ Byte-exact | 2,359,296/0 |
| `Clamp` | Hard-clamp to [min, max] | ✅ Byte-exact | 2,359,296/0 |
| `Spline` | CubicSpline used for terrain shape / peaks-and-valleys | ✅ Byte-exact | 2,359,296/0 |
| `Cache2D` / `CacheAllInCell` / `CacheOnce` / `FlatCache` | Caching layers to avoid redundant evaluation | ✅ Byte-exact | 2,359,296/0 |
| `Interpolated` | Tri-linear interpolation across 4x4x4 cell | ✅ Byte-exact | 2,359,296/0 |
| `BlendDensity` / `BlendAlpha` / `BlendOffset` | Chunk-border blending | ✅ Byte-exact | 2,359,296/0 |
| `BeardifierOrMarker` | Adds structure influence (structures pull terrain) | 🔄 Partial | Integrated (returns 0 — no structure markers ported yet) |

### Biome System

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| `Climate::Parameter` / `Climate::ParameterPoint` | 7-dimensional climate space (T, H, C, E, D, W, offset) | ✅ Byte-exact | 1,867,776/0 |
| `Climate::RTree` | Nearest-neighbour search in climate space | ✅ Byte-exact | 1,867,776/0 |
| `OverworldBiomeBuilder` | 65-biome layout: humidity/temperature/continentalness/erosion/weirdness grids | ✅ Byte-exact | 1,867,776/0 |
| Biome registry | All 65 biomes loaded from `26.1.2/data/` worldgen JSON, field-for-field | ✅ Complete | 65 biomes/0 |
| Nether biome builder | 5 nether biomes, separate climate axes | ❌ Not started | — |
| End biome | Single End biome (trivial) | ❌ Not started | — |

### Surface Rules

Surface rules control what block appears on the top/subsurface of each biome (grass, sand, stone, etc.).

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| `SurfaceRuleCondition` — `Biome` | Match current biome | ✅ Byte-exact | 2,359,296/0 |
| `SurfaceRuleCondition` — `YAbove` | Height threshold | ✅ Byte-exact | 2,359,296/0 |
| `SurfaceRuleCondition` — `WaterDepth` | Blocks below water surface | ✅ Byte-exact | 2,359,296/0 |
| `SurfaceRuleCondition` — `StoneDepth` | Distance to nearest stone | ✅ Byte-exact | 2,359,296/0 |
| `SurfaceRuleCondition` — `Noise` | Noise-based condition | ✅ Byte-exact | 2,359,296/0 |
| `SurfaceRuleCondition` — `Not`, `And` | Boolean combinators | ✅ Byte-exact | 2,359,296/0 |
| `SurfaceRule` — `Sequence` | Try rules in order, use first match | ✅ Byte-exact | 2,359,296/0 |
| `SurfaceRule` — `Condition` | Conditional dispatch | ✅ Byte-exact | 2,359,296/0 |
| `SurfaceRule` — `Block` | Place a specific block | ✅ Byte-exact | 2,359,296/0 |
| `SurfaceRule` — `Bandlands` | Terracotta color band logic (Mesa biome) | ✅ Byte-exact | 2,359,296/0 |

### Aquifer System

Aquifers replace Java's old water-everywhere approach with local water/lava pockets at varying heights.

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| `NoiseBasedAquifer` | Voronoi-based local water level calculation | ✅ Byte-exact | 2,359,296/0 |
| Lava aquifer logic | Below y=-54, uses lava instead of water | ✅ Byte-exact | 2,359,296/0 |
| `FluidStatus` | Encapsulates fluid type + surface level at a point | ✅ Byte-exact | 2,359,296/0 |

### Cave Carvers

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| `CaveWorldCarver` | Main cave tunnels — branch/worm algorithm | ✅ Byte-exact | Integrated in 2,359,296/0 |
| `CanyonWorldCarver` | Ravine carving — floor/ceiling shapes | ✅ Byte-exact | Integrated in 2,359,296/0 |
| Carver mask | Per-chunk bitfield preventing double-carve | ✅ Byte-exact | 2,359,296/0 |
| Nether carvers | Nether-specific cave shapes | ❌ Not started | — |

### Decoration Features

World decoration places trees, flowers, ores, lakes, etc. All 199 Overworld features are certified at 0 mismatches.

| Feature family | Examples | Status | Final Certification |
|---|---|---|---|
| Tree features | Oak, birch, spruce, jungle, acacia, dark oak | ✅ Byte-exact | 884,736+589,824/0 |
| Flower / grass | Dandelion, poppy, short grass, fern, tall grass | ✅ Byte-exact | 884,736+589,824/0 |
| Ore clusters | Coal, iron, gold, diamond, emerald, lapis | ✅ Byte-exact | 884,736+589,824/0 |
| Disk / patch | Gravel, sand, clay disks | ✅ Byte-exact | 884,736+589,824/0 |
| Lake features | Water lakes, lava lakes | ✅ Byte-exact | 884,736+589,824/0 |
| Spring features | Water/lava springs in stone | ✅ Byte-exact | 884,736+589,824/0 |
| Kelp / seagrass | Underwater vegetation | ✅ Byte-exact | 589,824/0 |
| Fallen logs | Spruce/oak/birch mossy logs | ✅ Byte-exact | 884,736+589,824/0 |
| Vines / hanging roots | Jungle/lush cave vegetation | ✅ Byte-exact | 884,736/0 |
| Mushrooms | Red/brown mushroom placement | ✅ Byte-exact | 884,736/0 |
| Coral | Coral blocks/fans/plants in warm ocean | ✅ Byte-exact | 589,824/0 |
| Geode | Amethyst geode (budding crystal) | ✅ Byte-exact | 884,736/0 |
| Bonus chest / start platform | One-off starting features | ✅ Byte-exact | — |
| Nether features | Basalt columns, nether sprouts, etc. | ❌ Not started | — |
| End features | Chorus plants, end gateway, obsidian pillars | ❌ Not started | — |

### Chunk Status Machine

Minecraft builds chunks in stages. Each status adds data on top of the previous.

| Status | What it adds | Status | Final Certification |
|---|---|---|---|
| `EMPTY` | Allocate chunk | ✅ Complete | — |
| `STRUCTURE_STARTS` | Determine which structures start here | 🔄 Partial | — |
| `STRUCTURE_REFERENCES` | Record which chunks reference each structure | 🔄 Partial | — |
| `BIOMES` | Fill biome palette using climate engine | ✅ Byte-exact | 1,867,776/0 |
| `NOISE` | Fill block data via density functions + surface rules | ✅ Byte-exact | 2,359,296/0 |
| `SURFACE` | Apply surface rules pass | ✅ Byte-exact | 2,359,296/0 |
| `CARVERS` | Cut cave tunnels and ravines | ✅ Byte-exact | Integrated |
| `FEATURES` | Place all decoration features | ✅ Byte-exact | 1,474,560/0 |
| `INITIALIZE_LIGHT` | Seed sky/block light sources | ❌ Not started | — |
| `LIGHT` | Propagate light | ❌ Not started | — |
| `SPAWN` | Place mobs per-biome mob cap | ❌ Not started | — |
| `FULL` | Mark chunk ready for use | 🔄 Partial | — |

### Structures

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| Structure registry | JSON-defined structure list loaded | 🔄 Partial | — |
| Structure placement (RTree, RandomSpread, ConcentricRings) | Where each structure starts | ✅ Byte-exact | 29K cells/0 (placement), 3,840/0 (concentric_rings) |
| Structure transforms (BoundingBox, Mirror, Rotation) | Pure math under all template/jigsaw placement | ✅ Byte-exact | 4,679/0 |
| StructurePiece base (placeBlock, generateBox, fillColumnDown, getWorldPos, updateAverageGroundHeight) | Helpers used by all non-jigsaw pieces | ✅ Byte-exact | Integrated in SwampHutPiece 144,000/0 |
| StructureTemplatePool + JigsawPlacement | Jigsaw assembler core (pool selection, attach math) | ✅ Byte-exact | 551/0 + 1,055 seeds/0 (bastion + trial_chambers) |
| StructureTemplate (loader, placeInWorld, processors) | .nbt template load + place + rule processors | ✅ Byte-exact | 15,873/0 + 346,872/0 + 544,050/0 |
| SwampHut (Witch Hut) | Scattered-feature temple: floor, walls, roof (4 stair facings + 4 corners), foundation | ✅ Byte-exact | 225 cases / 144,000 placements / 0 |
| ScatteredFeaturePiece ctor (SwampHut, DesertPyramid) | RNG-driven bounding-box + orientation | ✅ Byte-exact | 5,184/0 |
| Woodland Mansion grid (SimpleGrid, MansionGrid, GridLayout, EdgeClean) | Floor-plan generation | ✅ Byte-exact | 11,016/0 + 1,212,000/0 + 3,547/0 |
| Ocean Monument (room geometry, graph, fitter) | Room layout | ✅ Byte-exact | 4,299/0 + 1,188/0 + 2,600/0 |
| Stronghold (piece boxes, type boxes, small doors, smooth-stone selector) | Room geometry + RNG door picks | ✅ Byte-exact | 280/0 + 480/0 + 3,936/0 + 481/0 |
| Mineshaft (corridor, crossing, room, stairs) | RNG corridor geometry + boxes | ✅ Byte-exact | 17,280/0 + 17,280/0 + 5,760/0 + 96/0 |
| Nether Fortress (piece box, child offset) | RNG geometry | ✅ Byte-exact | 504/0 + 736/0 |
| Igloo piece position | Y-offset positioning | ✅ Byte-exact | 300/0 |
| Ocean Ruin cluster geometry | Cluster layout | ✅ Byte-exact | 504/0 |
| Ruined Portal Y-selector | Y-position RNG | ✅ Byte-exact | 348/0 |
| Jungle Temple stone selector | BlockSelector RNG (mossy/cobble maze) | ✅ Byte-exact | self-check/0 |
| BeardifierOrMarker | Adds structure influence (structures pull terrain) | 🔄 Partial | Integrated (no structure markers yet) |
| Jigsaw assembler (engine integration) | Full Village/Pillager Outpost/etc. assembly via the engine | 🔄 Partial | Components certified, integration pending |
| Village | Houses, paths, lampposts | 🔄 Partial | JigsawPlacement certified, piece assembly pending |
| Pillager Outpost | Tower + patrol cage | 🔄 Partial | StructureTemplate.placeInWorld certified, integration pending |
| Desert Temple | TNT + hidden chamber | ❌ Not started | DesertPyramidPiece ctor certified via scattered_feature_box |
| Bastion Remnant | Piglin structure | 🔄 Partial | JigsawPlacement certified (211 seeds/0) |
| End City | End-specific loot structure | ❌ Not started | — |

</details>

---

<details>
<summary><strong>🎨 Rendering</strong> — three backends, greedy mesher, full texture atlas</summary>

The renderer uses an `IRenderDevice` abstraction so all three backends (OpenGL, Vulkan, DX12) are swappable at startup. The meshing and atlas pipelines sit above the abstraction.

### Backends

| Backend | API | Status | Final Certification |
|---|---|---|---|
| OpenGL 4.6 core | Primary / default backend | ✅ Complete | — |
| Vulkan 1.3 + VMA + volk | Full Vulkan path with memory allocator | ✅ Complete | — |
| DirectX 12 + D3D12MA | D3D12 with D3D12 Memory Allocator | ✅ Complete | — |

### Chunk Meshing

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| Greedy mesher | Merges coplanar same-texture faces to reduce draw calls | ✅ Complete | — |
| Face culling | Skips faces shared between two opaque blocks | ✅ Complete | — |
| Texture atlas | All block textures packed into one atlas at runtime | ✅ Complete | — |
| Cross-quad geometry | Flower/grass X-shaped geometry | ✅ Functional | — |
| Transparent sorting | Water, glass render order | 🔄 Partial | — |
| Animated textures | Water, lava, kelp frame animation | ❌ Not started | — |
| Biome color tinting | Grass and water tinted per-biome (foliage/water color maps) | ❌ Not started | — |

### Camera & View

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| `vanillaViewVector` | Replicates Java's yaw-negation + pitch-sin formula exactly | ✅ Complete | — |
| Perspective projection | GLM `perspectiveLH_ZO`, matches Java's FOV math | ✅ Complete | — |
| Raw mouse input | Win32 `WM_INPUT`, delta accumulation, sensitivity scaling | ✅ Complete | — |
| View frustum culling | AABB-frustum test per chunk | 🔄 Partial | — |

### Lighting

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| Sky light | Propagated from sky through transparent blocks | ❌ Not started | — |
| Block light | Propagated from torches/glowstone/etc. | ❌ Not started | — |
| Ambient occlusion | Smooth lighting at block corners (AO from neighbors) | ❌ Not started | — |

</details>

---

<details>
<summary><strong>🌐 Network Protocol</strong> — byte-exact TCP client, 31 packet types verified</summary>

The network layer speaks the real Minecraft protocol over raw TCP. VarInt/VarLong encoding and packet framing are verified against 31 real-game messages.

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| TCP connection | Raw Winsock2 socket, non-blocking | ✅ Byte-exact | — |
| `PacketBuffer` | VarInt, VarLong, String, NBT read/write | ✅ Byte-exact | 31/0 |
| Login sequence | Handshake → Login Start → Login Success | ✅ Byte-exact | — |
| Compression | Zlib packet compression (threshold-based) | ✅ Byte-exact | — |
| Encryption | AES-CFB8 shared-secret (Mojang auth) | ❌ Not started | — |
| Keepalive | `ServerboundKeepAlivePacket` response | ✅ Complete | — |
| `ClientboundLevelChunkPacket` | Receive and deserialize chunk data | 🔄 Partial | — |
| `ClientboundAddEntityPacket` | Spawn entity | 🔄 Partial | — |
| `ServerboundMovePlayerPacket` | Send position updates | 🔄 Partial | — |
| Game-state packets | Full game message handling (~350 packet types) | 🔄 Partial | — |

</details>

---

<details>
<summary><strong>📦 Data Layer</strong> — NBT, Block Registry, Palette, Chunk storage</summary>

These subsystems store and move game data without any rendering concern.

### NBT (Named Binary Tag)

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| All tag types | `TAG_Byte` through `TAG_Long_Array` | ✅ Byte-exact | 6/0 |
| `NbtIo` gzip read | Reads `.nbt` gzip-compressed files | ✅ Byte-exact | 6/0 |
| `NbtIo` gzip write | Writes gzip-compressed output | ✅ Byte-exact | 6/0 |
| Insertion-ordered `CompoundTag` | Java's `LinkedHashMap` ordering preserved | ✅ Byte-exact | 6/0 |
| Modified UTF-8 | Java's non-standard null-byte encoding | ✅ Byte-exact | 6/0 |
| SNBT (string NBT) | Human-readable `.snbt` format parser | ❌ Not started | — |

### Block & State Registry

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| Block registry | All vanilla blocks registered with properties | ✅ Complete | — |
| `BlockState` | Per-block property combinations (4,000+ states) | ✅ Complete | — |
| `PalettedContainer` | Indirect palette + packed bits storage (chunk sections) | ✅ Complete | — |
| `LevelChunk` | 16x16x384 chunk with 24 sections | ✅ Complete | — |
| `ChunkAccess` | Read interface used by worldgen | ✅ Complete | — |

### AABB / Physics

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| `AABB` | Axis-aligned bounding box — all intersection/expansion ops | ✅ Byte-exact | 512/0 |
| Block shape lookup | `VoxelShape` per block state for collision | 🔄 Partial | — |
| Entity–block collision | Sweep AABB through block grid | 🔄 Partial | — |
| Fluid physics | Swimming, drowning, current push | ❌ Not started | — |

</details>

---

<details>
<summary><strong>🧍 Entity & AI</strong> — base entity done; ~277 AI goal types remaining</summary>

The entity system mirrors Java's inheritance hierarchy: `Entity` → `LivingEntity` → `Mob` → `PathfinderMob` → specific mob classes.

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| `Entity` base | UUID, position, velocity, flags, NBT save/load | ✅ Complete | — |
| `LivingEntity` | Health, hurt/death, attribute system | 🔄 Partial | — |
| `Mob` | AI goal runner, target selector, mob tick | 🔄 Partial | — |
| `PathfinderMob` | Navigation + pathfinding stub | 🔄 Partial | — |
| `Player` | Local player, input to movement, inventory ref | 🔄 Partial | — |
| AI goal — `RandomStroll` | Wander aimlessly around spawn | ✅ Ported | No gate |
| AI goal — `FloatGoal` | Swim when in water | ❌ Not started | — |
| AI goal — `LookAtPlayerGoal` | Look toward nearest player | ❌ Not started | — |
| Mob pathfinding (A*) | Grid A* through walkable blocks | ❌ Not started | — |
| ~274 remaining goal types | Full mob behavioral AI | ❌ Not started | — |
| Entity tracking / spawning | Server-driven entity sync | ❌ Not started | — |

</details>

---

<details>
<summary><strong>🎵 Audio</strong> — functional XAudio2 pipeline</summary>

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| XAudio2 engine | Windows audio graph, mastering voice | ✅ Functional | — |
| Ogg/Vorbis decoder | `stb_vorbis` streaming decode | ✅ Functional | — |
| Sound manager | Event to sound-file lookup, positional volume | ✅ Functional | — |
| Music tracks | Background music streaming | ✅ Functional | — |
| Sound categories | Master, music, block, ambient, entity volumes | 🔄 Partial | — |
| 3-D positional audio | Distance/direction attenuation from source | ❌ Not started | — |

</details>

---

<details>
<summary><strong>🖥️ GUI & Menus</strong> — functional screens; full screen suite pending</summary>

| Screen | Status | Final Certification |
|---|---|---|
| Title screen | ✅ Functional | — |
| Pause menu | ✅ Functional | — |
| Options screen | ✅ Functional | — |
| GUI scale (1x–4x) | ✅ Functional | — |
| HUD — health bar | ✅ Functional | — |
| HUD — hotbar | ✅ Functional | — |
| HUD — crosshair | ✅ Functional | — |
| Inventory screen | 🔄 In progress | — |
| Crafting table screen | ❌ Not started | — |
| Chest / container screen | ❌ Not started | — |
| Furnace screen | ❌ Not started | — |
| Chat input | ❌ Not started | — |
| Death screen | ❌ Not started | — |
| Multiplayer server list | ❌ Not started | — |

</details>

---

<details>
<summary><strong>📁 Assets & Resources</strong> — all assets embedded in the .exe</summary>

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| MCAS asset format | Custom binary pack (magic 0x4D434153), built at compile time | ✅ Complete | — |
| Block textures | Extracted from Mojang CDN jar, atlased at runtime | ✅ Complete | — |
| GUI sprites | Buttons, icons, logo, dirt background | ✅ Complete | — |
| Font (ASCII bitmap) | `ascii.png` + glyph widths | ✅ Complete | — |
| Splash texts | Random splash strings from `splashes.txt` | ✅ Complete | — |
| Sound files | Ogg files embedded via RCDATA | ✅ Complete | — |
| Language files | `en_us.json` (localisation strings) | 🔄 Partial | — |
| Worldgen JSON | `26.1.2/data/` biome/density-function/surface-rule JSON | ✅ Complete | — |
| Resource packs | Custom pack loading (override textures/sounds) | ❌ Not started | — |

</details>

---

<details>
<summary><strong>⚙️ Game Mechanics</strong> — core game loop pending</summary>

| System | What it does | Status | Final Certification |
|---|---|---|---|
| `SimpleContainer` / `Slot` | Item container base, slot model | 🔄 In progress | — |
| Inventory (player 36+4+1) | Full player inventory layout | 🔄 In progress | — |
| Block interaction | Right-click to open chest/furnace, place blocks | ❌ Not started | — |
| Block breaking | Dig time, tool speed, drop table | ❌ Not started | — |
| Block placing | Orientation rules, waterlogging | ❌ Not started | — |
| Crafting recipes | Shapeless and shaped recipe matching | ❌ Not started | — |
| Smelting / fuel | Furnace tick, experience | ❌ Not started | — |
| Enchanting | Enchantment table mechanics | ❌ Not started | — |
| Experience / levels | XP orb pickup, level-up | ❌ Not started | — |
| Food / hunger | Saturation, exhaustion, regeneration | ❌ Not started | — |
| Status effects | Potion effects tick, rendering | ❌ Not started | — |
| Day/night cycle | `GameTime` to sky color, sun/moon position | ❌ Not started | — |
| Weather | Rain/snow/thunder, sky darkening | ❌ Not started | — |
| Redstone | Signal propagation, devices | ❌ Not started | — |

</details>

---

<details>
<summary><strong>🖥️ Platform & Windowing</strong></summary>

| Component | What it does | Status | Final Certification |
|---|---|---|---|
| Win32 window creation | `WM_CREATE`, message pump, DPI awareness | ✅ Complete | — |
| Raw mouse input | `WM_INPUT`, RAWINPUT delta accumulation | ✅ Complete | — |
| Keyboard input | `WM_KEYDOWN`/`WM_KEYUP` to action bindings | ✅ Complete | — |
| Mouse capture / release | Grab on click, release on escape | ✅ Complete | — |
| Fullscreen toggle | `Alt+Enter`, DXGI swap-chain resize | ✅ Complete | — |
| Linux to Windows cross-compile | llvm-mingw, UCRT target, Ninja | ✅ Complete | — |

</details>

---

## Architecture

```
src/
├── main.cpp                ← WinMain entry, backend selection
├── client/                 ← Minecraft.h — main game loop, state machine
├── platform/               ← Win32 window, raw input
├── assets/                 ← AssetPack (MCAS format), AssetManager, TextureAtlas
├── render/                 ← IRenderDevice abstraction
│   ├── opengl/             ← OpenGL 4.6 backend
│   ├── vulkan/             ← Vulkan 1.3 backend
│   └── dx12/               ← DirectX 12 backend
├── world/
│   ├── level/
│   │   ├── levelgen/       ← Terrain, noise, biomes, decoration, structures
│   │   ├── chunk/          ← LevelChunk, PalettedContainer
│   │   └── block/          ← Block registry, block states
│   ├── entity/             ← Entity base, mobs, AI goals, player
│   └── phys/               ← AABB, collision
├── network/                ← TCP connection, PacketBuffer, protocol packets
├── nbt/                    ← NBT tags, NbtIo (gzip read/write)
├── gui/                    ← Screens, buttons, HUD
└── audio/                  ← XAudio2 engine, Ogg decoder, sound manager
```

**Statically linked dependencies** (no DLLs shipped):
GLM · nlohmann/json · stb_image · stb_vorbis · GLAD · miniz · volk · glslang · D3D12MA

---

## Build

### Requirements
- Windows x64 (MSVC 2022 or Clang-CL 17+) **or** Linux cross-compile with llvm-mingw
- CMake 3.28+
- Ninja (recommended)

### Windows (MSVC)

```powershell
# Configure from repo root
cmake -S . -B build

# Build — always use the timeout wrapper to prevent hangs
.\tools\run_with_timeout.ps1 -TimeoutSec 300 cmake --build build --target mcpp

# Run
build\mcpp.exe --quickPlaySingleplayer
```

The build will automatically download the Minecraft 26.1.2 client jar from the Mojang CDN, extract block textures, GUI sprites, and worldgen data, and pack them into `src/assets/assets.bin` which gets embedded in the executable. First build takes several minutes due to the download.

To skip asset repacking (when `src/assets/assets.bin` already exists):
```powershell
cmake -S . -B build -DMCPP_SKIP_ASSET_PACK=ON
```

### Linux → Windows cross-compile (llvm-mingw)

```bash
# Install llvm-mingw (UCRT target)
wget https://github.com/mstorsjo/llvm-mingw/releases/latest/download/llvm-mingw-*-ucrt-ubuntu-22.04-x86_64.tar.xz
tar -xf llvm-mingw-*.tar.xz -C /opt/llvm-mingw --strip-components=1

# Configure
cmake -S . -B build-win \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=/opt/llvm-mingw/bin/x86_64-w64-mingw32-clang \
  -DCMAKE_CXX_COMPILER=/opt/llvm-mingw/bin/x86_64-w64-mingw32-clang++ \
  -DCMAKE_RC_COMPILER=/opt/llvm-mingw/bin/x86_64-w64-mingw32-windres \
  -G Ninja -DMCPP_SKIP_ASSET_PACK=ON

# Build
cmake --build build-win --target mcpp -j$(nproc)
```

---

## Parity testing

The `tools/` directory contains 530+ Java harnesses (`*Parity.java`) that run specific subsystems of the real decompiled game and emit test vectors, which are then diffed against the C++ implementation. All gates must stay at 0.

```bash
# Run all C++ parity targets
tools/run_all_gates.sh

# Run decoration / biome-feature gates specifically
tools/run_deco_parity.sh

# Generate Java ground truth (requires 26.1.2/ local dump)
tools/run_groundtruth.sh
```

For the Java harnesses to run you need the local `26.1.2/` dump:
```bash
tools/provision_parity_runtime.ps1   # Windows
# or manually: see AGENTS.md § FETCHING SOURCE MATERIAL
```

Port coverage is tracked in `docs/PORT_COVERAGE.tsv` — 6,882 Java source files, currently 143 ported / 36 partial / 449 reasoned n/a.

---

## Contributing / working with AI agents

This project is developed with AI coding agents (Claude Code). The workflow:

1. Read [CLAUDE.md](CLAUDE.md) — the one rule that overrides everything
2. Read [AGENTS.md](AGENTS.md) — full current state, session history, working notes, parity gate status
3. Check [TASKLIST.md](TASKLIST.md) for the current bug/feature list
4. Before porting anything, read the corresponding Java source in `26.1.2/src/`
5. After porting, add a parity test and verify gates are at 0
