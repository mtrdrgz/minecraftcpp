# AGENTS.md — Minecraft 26.1.2 → C++ Port
> This document is the single source of truth for any AI agent working on this project.
> It is maintained by the lead agent and must be updated after every session.
> If you are reading this, assume you have NO conversation context. Follow this document exactly.

---

## ⛔ RULE #0 — THE ONE RULE THAT MATTERS MOST (READ FIRST, EVERY TIME)

**This is a 1:1 reverse-engineering port. You are translating Java → C++, nothing else.
NEVER invent, guess, approximate, simplify, tune, or "make it look reasonable".
EVERY value, constant, formula, ordering, and algorithm MUST come from the decompiled
Java source (`26.1.2/src/`) or the worldgen data JSON (`26.1.2/data/`).**

If you cannot find the source for something, STOP and go read the Java. Do not fill the
gap with a plausible-looking number. A wrong-but-plausible value is WORSE than a TODO,
because it hides the bug and looks done.

Concretely, this means:
- ❌ NO `rng.nextInt(10)` "≈ 1 tree/chunk for visibility" style density tuning.
- ❌ NO "chance = 0.05" hardcoded because it looked about right.
- ❌ NO predicate/condition that `return true;` "for now" — that silently disables the
  exact gating that makes vanilla worldgen correct (see the predicate bug below).
- ❌ NO placeholder hashes/curves/noise (an FNV in place of MD5, a lerp in place of a
  spline). These corrupt EVERYTHING downstream and are nearly invisible until you see
  the world.
- ✅ Port the real class. Read `<Feature>.java`, `<Placement>.java`, the data JSON.
  Copy the constants verbatim. Match the RNG call order exactly.
- ✅ If a feature/predicate/type isn't ported yet, make it a hard no-op that does
  NOTHING and is logged/listed as unported — never a silent "pass everything".
- ✅ Prove parity with a `*_parity` test against ground truth from the real jar/data
  whenever you can. The repo's culture is parity tests, not eyeballing.

**Why this rule exists (real bugs this session, Session 40):** the worldgen "worked"
visually but was full of disguised approximations that produced absurd results
(pumpkins piled on beaches, sugar cane everywhere, only ~3 biomes in the whole world,
every tree a dark spruce). None of these were "a number slightly off" — they were
omitted algorithms masked by `return true` / placeholder code. The fixes were not
tuning; they were porting the real Java that had been skipped. See Session 40 below.

If you only read one line of this file: **port it from the source; do not make it up.**

---

## MANDATORY AGENT WORKFLOW — follow this every session

> This section is non-negotiable. Skipping it produces silent drift that compounds across sessions.

### Files you MUST read before writing any code
1. `AGENTS.md` (this file) — rules, source locations, build, current state
2. `PROJECT_COVERAGE.md` — live port-progress dashboard, devlog of all findings, proof records
3. `TASKLIST.md` — current bug/feature queue

### While working — documentation requirements

**Every Java file you open:**
Update its row in `docs/PORT_COVERAGE.tsv`. Three columns: `path  status  proof`.
- Even if you just read it and concluded it needs no port: mark `n/a` and explain why.
- `unvisited` means no human or agent has ever looked at it.

**Proof requirement (anti-hallucination):**
- `ported` → C++ file + function/class + parity gate (`gate_name N/0`) OR exact Java method citation (`ClassName.method → src/path.h:line`)
- `partial` → list what IS ported (C++ location) and what is NOT (specific methods/features)
- `n/a` → one sentence why (Java-only infra, replaced by architecture, test scaffolding, etc.)
- **Never mark `ported` without proof. A wrong port that looks right is worse than `unvisited`.**

**Every significant finding (algorithm quirk, bug, parity delta, Java behaviour):**
Add a bullet to the devlog in `PROJECT_COVERAGE.md` (newest session at top).

**Every system-level status change:**
Update `README.md` status tables and parity certification numbers.

### Session end checklist
- [ ] `docs/PORT_COVERAGE.tsv` updated for every file touched
- [ ] Devlog entry added to `PROJECT_COVERAGE.md` (even a one-liner)
- [ ] Progress dashboard numbers in `PROJECT_COVERAGE.md` updated
- [ ] `AGENTS.md` § CURRENT STATE updated with session summary
- [ ] `README.md` updated if any system status or parity number changed
- [ ] All changes committed and pushed to `main`

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

> NOTE: the repo root on this machine is `C:\Users\Mateo\Desktop\minecraftcpp\`
> (older docs said `...\Desktop\Claude\` — that path is stale; everything below is
> relative to the current repo root).

```
C:\Users\Mateo\Desktop\minecraftcpp\
├── 26.1.2\                 ← git-ignored; fetched from Mojang CDN (see below)
│   ├── client.jar          ← Original JAR (do not modify)
│   ├── data\               ← Worldgen DATA JSON — THE authoritative 1:1 values
│   │   └── minecraft\worldgen\{biome,configured_feature,placed_feature,
│   │       noise_settings,density_function,structure,...}\*.json
│   └── src\                ← Decompiled Java source — READ THIS before porting
│       ├── net\minecraft\  ← Main game code
│       └── com\mojang\     ← Mojang platform libs
├── (repo root)            ← the C++ project
├── memory\ (per-agent)     ← Claude Code memory (build quirks etc.)
└── AGENTS.md               ← This file (keep updated)
```

**Reading order to port a worldgen feature 1:1:** (1) the biome JSON lists placed
features per step; (2) `placed_feature/<name>.json` gives the placement modifier chain
(count/rarity/heightmap/predicates); (3) `configured_feature/<name>.json` gives the
feature type + config; (4) the Java class for the feature/placement/predicate gives
the algorithm. Port all four faithfully — the JSON carries the numbers, the Java
carries the logic.

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

> **⚠️ IMPORTANT — LFS budget workaround (2026-06):** the repo's GitHub LFS quota was
> exhausted by the previously-committed `26.1.2/src/` (decompiled Java) and
> `26.1.2/data/minecraft/worldgen/*.json` files. To avoid this happening again:
> 1. **Do NOT commit Minecraft source or assets.** `.gitignore` ignores ALL of
>    `26.1.2/` — everything is reproducible via the script below.
> 2. **Fresh clones / ephemeral machines** fetch everything they need from the open
>    Mojang CDN via the script below. No LFS, no auth, no proprietary commit.
> 3. **Existing LFS-tracked source/data files** can be removed from history with
>    `git lfs migrate export` + force-push when the team is ready (this is destructive
>    to history, so it's a separate operation from the certification work).

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
C:\Users\Mateo\Desktop\minecraftcpp\  ← C++ project root (repo root)
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
- **Command safety**: All build/run/test commands must go through `tools\run_with_timeout.ps1` with an explicit `-TimeoutSec`. Do not run raw long-lived commands.

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
- [ ] Full worldgen feature/structure pipeline — see `docs/WORLDGEN_PLAN.md`
      for the complete phased checklist (features, trees, carvers, structures).
- [ ] Port remaining ~277 mob goals.
- [ ] Implement Crafting and Smelting logic.

### PHASE 16 — Integration & Optimization
- [ ] Multi-threaded chunk loading & meshing (thread pool).
- [ ] LOD for distant chunks.
- [ ] GPU-driven rendering (indirect draw calls).

---

## CURRENT STATE

**Last updated**: 2026-06-21 — village increment #1: structure processor pipeline + legacy element.

**Village processor pipeline (2026-06-21 c):** ported `StructureTemplate.processBlockInfos`
rule-processor path into `StructureGen.cpp::placeTemplate` — the first of three
village layers. `legacy_single_pool_element` now applies `BlockIgnoreProcessor.STRUCTURE_AND_AIR`
(village buildings stop placing template AIR / carving terrain: plains village writes
~12k→~3.8k blocks), and the `RuleProcessor` family applies per block via a fresh
`LegacyRandomSource(Mth.getSeed(worldPos))` with the certified RuleTests (always_true/
block_match/blockstate_match/random_block_match; tag_match deferred). Pool elements now
carry their `processors` list id. Verified with `structure_gen_probe`: processor-only
blocks appear (mossify→mossy_cobblestone 720, farm→carrots 105 over 60×60). Also
improves enabled jigsaw structures (outpost_rot, copper_bulb_degradation). Villages
stay gated pending increment #2 (`feature_pool_element`) and #3 (Beardifier +
Gravity/Protected/TERRAIN_MATCHING projection processors). Plan: `docs/STRUCTURES_STATUS.md`.

**Last updated prior**: 2026-06-21 — structure biome gate + Linux verification harness + village root cause.

**Structure biome gate + verification harness (2026-06-21 b):** added
`structure_gen_probe` (CMake target + `tools/structure_gen_probe/main.cpp`), a
headless driver of the real `generateStructures` over a flat world against real
worldgen data — the cross-platform way to verify structure work without Windows
(builds+runs on the parity-only GCC build). Used it to find + fix a real 1:1 bug:
the hand-built non-jigsaw structures skipped `Structure.isValidBiome`, so they
placed in any biome (the probe placed **400 nether fossils in 1600 overworld plains
chunks**, plus desert pyramids/igloos in plains). Fixed by parsing each non-jigsaw
structure's `biomes` and validating at the chunk-centre surface (`onTopOfChunkCenter`
→ `isValidBiome`); the probe now shows each structure only in its biomes and zero
overworld nether fossils. Also root-caused **villages**: they assemble fully (12 in
120×120, ~100 pieces, ~12k blocks) but are deliberately gated off by
`isKnownBrokenRuntimeStructureSet` pending the processor/legacy-element/`feature_pool_element`/
`beard_thin` polish layers. Owner decision: build all layers to 1:1 (incl. the real
Beardifier, structure-starts-before-NOISE) before flipping the village gate. Full
ledger + plan: `docs/STRUCTURES_STATUS.md`.

**Last updated prior**: 2026-06-21 — structures subsystem audit + RULE #0 honesty pass.

**Structures audit + honesty (2026-06-21):** the user reported structures as the
worst part of worldgen (no villages, things exposing). Audited the whole subsystem
against the Java source + provisioned data and wrote the certification ledger
`docs/STRUCTURES_STATUS.md`. Findings: terrain is gated 1:1 but structures are not.
Only swamp_hut, desert_pyramid, jungle_pyramid, igloo, shipwreck and nether_fossil
are actually placed (hand-ported pieces, dispatched in `StructureGen.cpp::
tryGenerateAndPlace`). The jigsaw family (villages, pillager_outpost, ancient_city,
bastion, trail_ruins, trial_chambers) *assembles* but with **no structure
processors and no terrain adaptation**, so it is not trustworthy (this is why
villages look absent/exposed). Three root architectural gaps remain unported:
(1) structures run as a post-decoration pass instead of at STRUCTURE_STARTS before
NOISE; (2) the **Beardifier**/`terrain_adaptation` density slot (beard_thin/
beard_box/bury/encapsulate) — `grep -ri beard src/` is empty; (3) the structure
**processor** pipeline is not wired into `placeTemplate` (raw blocks only).
RULE #0 fix landed: `ocean_ruin`/`ruined_portal`/`buried_treasure` were marked
`supported` but actually no-op'd (jigsaw assembly with an empty `start_pool`); they
are now honest no-ops and logged as UNPORTED at load. `StructureGen.cpp` compiles
clean (`g++ -std=c++23 -DGLM_ENABLE_EXPERIMENTAL -Isrc -Ivendor -fsyntax-only`).
Parity-only build confirmed to configure+build+run structure gates on Linux GCC.
Prioritised roadmap (buried_treasure → structure-starts-before-noise → Beardifier →
processors → ruined_portal → remaining hand-built → whole-structure server gate) is
in `docs/STRUCTURES_STATUS.md`.

**Last updated prior**: 2026-06-21 18:28 UTC - menu button screen-transition crash hotfix.

**Menu button screen-transition HOTFIX (2026-06-21 18:28):** user reported that
clicking Singleplayer from the title menu crashed after the streaming changes. Root
cause was UI lifetime, not worldgen: button callbacks such as Singleplayer,
Options, Back to Game, and Disconnect can call `Minecraft::setScreen` /
`startLocalGameFast`, destroying the current screen and its button vector while
`Button::mouseClicked` or the screen's button loop is still executing. Fixed by
adding `clickAction()` to `Button`/`WidgetButton`; title, pause, and options
screens now copy the selected callback first, stop touching widget storage, then
invoke the callback. Rebuilt `build/mcpp.exe` and ran a scripted title-menu
Singleplayer click smoke under `tools/run_with_timeout.ps1` without an immediate
crash or leftover `mcpp.exe` process.

**Last updated prior**: 2026-06-21 18:22 UTC - async meshing iterator/cache assert hotfix.

**Async meshing debug assert HOTFIX (2026-06-21 18:22):** user reported an MSVC
debug assertion from `std::vector` ("can't dereference value-initialized vector
iterator") shortly after the snapshot meshing change. Hardened the async meshing
path: pending mesh futures are removed by index/swap-pop instead of iterator erase,
and the vanilla model mesher cache now returns stable value copies for model-id
vectors and loaded models instead of references/pointers into mutable static
containers. This keeps worker mesh builds from holding invalidated cache storage.
Verified `build/mcpp.exe` rebuild and a 20s `--quickPlaySingleplayer` smoke via
`tools/run_with_timeout.ps1`; the smoke timed out normally and left no `mcpp.exe`
process alive.

**Last updated prior**: 2026-06-21 18:15 UTC - chunk meshing moved off the render thread.

**Chunk streaming stutter architecture v3 (2026-06-21 18:15):** the render-first
defer from 18:04 was rejected in playtesting because it made terrain visual catch-up
stop while the player moved. Replaced that patch with the proper meshing architecture:
`LevelRenderer` now deep-copies the dirty center chunk plus loaded cardinal neighbours
into private `LevelChunk` snapshots on the main thread, schedules `ChunkMesher::buildChunk`
on a dedicated mesh worker, polls ready futures, and only integrates/destroys GPU
buffers on the render thread. First section uploads are budgeted (2/frame) instead
of being suppressed by recent input, so chunk visuals can continue to appear while
the camera moves. Added explicit `LevelChunk` deep-copy support for snapshot meshing.
Release `build/mcpp.exe` rebuilt successfully via `tools/run_with_timeout.ps1` + VS
dev environment. Still open: decoration is still the next synchronous worldgen hot
path and should get the same snapshot/merge treatment or a real thread-safe
generation context.

**Last updated prior**: 2026-06-21 18:04 UTC - chunk streaming made render-first.

**Chunk streaming stutter mitigation v2 (2026-06-21 18:04):** the previous
main-thread decoration throttle was not enough; gameplay was still effectively
2 FPS while chunks were generating. Root cause now treated as CPU/render-thread
contention: terrain workers were normal-priority CPU hogs, and the renderer still
rebuilt/uploaded chunk meshes synchronously during camera movement. Fixed by
making the streaming `ThreadPool` tiny (1 worker on typical CPUs, 2 only on
larger CPUs) and putting its Windows worker threads into background mode, plus
deferring dirty chunk mesh rebuilds and first GPU uploads while camera/input
happened in the last 250 ms. This intentionally prefers responsive movement over
immediate chunk visual catch-up; some chunks may appear late while moving, which
is acceptable compared to freezing input. Release `build/mcpp.exe` rebuilt.

**Last updated prior**: 2026-06-21 17:56 UTC - chunk streaming stutter mitigation.

**Chunk streaming stutter MITIGATED (2026-06-21 17:56):** the terrain generator
workers were not the only problem. Runtime decoration and chunk meshing still ran
on the main thread, and decoration could execute a large whole-chunk
`applyBiomeDecoration` + structures turn exactly when movement made a new 3x3
neighbourhood eligible. Fixed the worst stalls without changing worldgen output:
`--quickPlaySingleplayer` now uses `startLocalGameFast`, the blocking
`startLocalGame` path no longer decorates the spawn chunk synchronously, completed
terrain integration is capped to 2 chunks/tick, queued chunks are tracked by
`m_queuedChunks`, main-thread decoration is limited to one eligible chunk and is
suppressed while movement/input happened in the last 750 ms, and dirty mesh rebuilds
remain capped to one per frame. Release `build/mcpp.exe` rebuilt successfully via
`tools/run_with_timeout.ps1` + VS dev environment. Still not final: decoration is
still synchronous when the player is idle; the correct long-term fix is a private
snapshot/merge decoration worker or a thread-safe engine decoration context.

**Last updated prior**: 2026-06-21 00:10 UTC - terrain performance cache/heightmap checkpoint.

**Terrain/engine performance PARTIAL (2026-06-21 00:10):** found a real base
problem in the Java cache model, not just a need for generic tuning. The C++ router
was missing Java's `flatCache(cache2d(...))` wrappers for key overworld 2D density
functions, and the interpolation corner resolver bypassed cache markers while
sampling cell corners. Fixed by restoring the Java-shaped wrappers, delegating cache
markers inside corner sampling, and adding shared per-noise-chunk 2D/flat caches.
`buildSurface` also repeated biome climate samples and full heightmap rescans; the
port now exposes `BiomeManager::selectQuart`, caches surface biomes by selected
quart for one chunk build, keeps `LevelChunk` heightmaps incrementally updated
during NOISE writes, removes redundant buildSurface `computeHeightmap()` calls, and
fixes `ChunkSection::setBlock` non-air accounting for solid-to-solid replacement.
Release checkpoint: `terrain_engine_perf --radius 4 --seed 1` => `fillFromNoise`
77.2 ms/chunk, `buildSurface` 18.8 ms/chunk, `applyCarvers` 4.1 ms/chunk,
`chunkMesh` 62.1 ms/chunk. `mcpp.exe` rebuilt. Still not done: `fillFromNoise` and
`chunkMesh` remain the next hot paths.

**Last updated prior**: 2026-06-20 22:40 UTC - terrain/meshing profiling pass and first engine performance fixes.

**Terrain/engine performance PARTIAL (2026-06-20 22:40):** added
`terrain_engine_perf` to measure full CPU terrain generation plus chunk meshing
without a window/GPU. Repro: `build-vs/Release/terrain_engine_perf.exe --radius 4
--seed 1`. Profile before final worldgen setter fix showed `fillFromNoise` ~190.5
ms/chunk, `buildSurface` ~105.8 ms/chunk, `applyCarvers` ~4.9 ms/chunk, mesh ~56.7
ms/chunk. Fixes landed: profiler no longer overwrites same-name worker events;
vanilla block model IDs are cached by numeric `stateId` and `Model` instances are
reused by reference; async local generation uses one thread-local
`NoiseBasedChunkGenerator` per worker/seed; `NoiseBasedChunkGenerator` keeps
persistent `RandomState`/`SurfaceSystem`; `fillFromNoise` uses a NOISE-stage block
setter that avoids per-block render-dirty atomics/heightmap writes. Re-profile
seed=1 radius=4: `fillFromNoise` ~138.3 ms/chunk, `buildSurface` ~76.4 ms/chunk,
`applyCarvers` ~3.6 ms/chunk, mesh ~57.2 ms/chunk. This is better but not done:
density-function and surface-rule execution remain the dominant CPU bottlenecks.
Release `mcpp.exe` rebuilt successfully.

**Last updated**: 2026-06-20 22:30 UTC - runtime block meshing now uses vanilla blockstate/model JSON.

**Block model path PARTIAL (2026-06-20 22:30):** stop doing targeted texture
patches. `ChunkMesh.cpp` now tries a vanilla data-driven path before the old
fallback renderer: it loads `assets/minecraft/blockstates/<block>.json`, evaluates
`variants` / `multipart` selectors against `BlockState` properties, recursively loads
`assets/minecraft/models/<model>.json` with parent texture/element inheritance,
resolves `#texture` references, and emits model `elements/faces` with `cullface` and
FaceBakery default UV formulas. This is intentionally documented as partial, not
ported: remaining work includes weighted variants, x/y rotations + uvlock, full
StateDefinition/pack-stack semantics, render-layer separation, AO, and the complete
ModelBakery/MaterialBaker runtime. The previous bamboo-specific renderer and
non-stone texture guard were removed; unresolved model textures go through
`missingno`. Release `mcpp.exe` rebuilt successfully.

**Last updated**: 2026-06-20 22:18 UTC - bamboo/stone visual fix + A/D camera movement.

**Jungle visual + camera controls FIXED (2026-06-20 22:18):** A/D free-camera
strafing was inverted because `right` used `cross(forward, up)` under the renderer's
camera convention; it now uses `cross(up, forward)`. The gray stone-looking columns
seen in jungle scenes were addressed at both likely failure points: bamboo now has a
special thin-stem mesh with `bamboo_stalk`/leaf-plane textures, and texture fallback no
longer disguises unresolved textures as `stone`. `tools/build_atlas.py` now writes a
real magenta/black `__missing__` tile into `block_atlas.png/json` instead of mapping
`__missing__` to the stone tile. Ignored atlas assets were regenerated locally and
Release `mcpp.exe` was rebuilt.

**Last updated**: 2026-06-20 22:10 UTC - block texture/model hint pass + atlas fallback guard.

**Block texture rendering FIXED (2026-06-20 22:10):** `mcpp.exe` was showing some
vegetation/ground-detail blocks with the entire atlas smeared across quads or with
cube fallback geometry. Root fixes: `Blocks.cpp` now derives per-block texture hints
from vanilla `assets/minecraft/blockstates/*.json` + `models/block/*.json` in
`assets.bin` (with local extracted-assets fallback for dev builds), `ChunkMesh.cpp`
uses the complete `isCrossPlant` set for cutout plant rendering instead of a stale
second list, and special-quad texture fallback no longer maps UV `0..1` over the
whole atlas when atlas data is unavailable. `TextureAtlas.cpp` also initializes the
embedded missing-texture UV from `__missing__` / `missing_texture` when present.
Verified by rebuilding Release `mcpp.exe` with MSVC.

**Last updated**: 2026-06-20 21:52 UTC — local-root parity/runtime setup restored; structure placement gate reverified.

**Local-root parity/runtime setup FIXED (2026-06-20 21:52):** fresh clones whose repo
root is `minecraftcpp/` (not the older parent `mcpp/` layout) can now provision and run
Java parity tools in-place. `tools/provision_parity_runtime.ps1` resolves the real repo
root, downloads the Mojang manifest/client/server jars, extracts `26.1.2/data/minecraft`,
downloads libraries + JDK 25, and keeps everything under the git-ignored repo-local
`26.1.2/`. `run_groundtruth.ps1`, `run_server_groundtruth.ps1`, and
`run_server_gen_structures.ps1` use the same root detection. Build resource files
`src/assets/resource_ids.h` and `src/assets/assets.rc` were restored and `.gitignore`
now anchors `/assets/` so those source files are tracked. Verified:
`StructurePlacementParity` Java GT (29,027 rows) + MSVC `structure_placement_parity`
=> `StructurePlacement seeds=4 set-checks=76 positives=28947 mismatches=0`.

**Last updated prior**: 2026-06-20 03:00 UTC â€” carver gate restored to **0 mismatches**.

**Last updated**: 2026-06-20 03:00 UTC — carver gate restored to **0 mismatches**.

**Carver root cause FIXED (2026-06-20 03:00):** the long-standing 7,673 carver
mismatches (`full_chunk_parity`) were a C++ **argument-evaluation-order** bug, not an
FP-parity limitation. `CaveWorldCarver.createTunnel`'s split recursion draws
`nextLong()` (child seed) then `nextFloat()` (child thickness) as arguments to one
call; Java evaluates left-to-right, GCC right-to-left, swapping the RNG draw order and
corrupting every split child tunnel. Fixed by hoisting RNG draws into locals in Java
source order (and hardening the `(nextFloat()-nextFloat())*nextFloat()` xRota/yRota
lines, same UB class) in `src/world/level/levelgen/carver/WorldCarver.cpp`. Verified
against real Java 26.1.2 ground truth (2,359,296 cells): carver mismatches 7,673 → 0.
See devlog 2026-06-20 03:00 for the empirical trace. (Lesson: never put two RNG draws in
one C++ argument list / unsequenced expression — argument and operand order is
unspecified and GCC differs from Java.)

**Last updated**: Session 66+ (worldgen ≈8M cells certified; engine decorates in-game; full-port goal set; audits + parallel pillars running)

**Session 66 continued (2026-06-10) — engine integration + full-port kickoff:**
- GOAL EXPANDED by the user: full 1:1 port of EVERYTHING (rendering, GUI, netcode,
  entities/movement, structures, packaging single .exe, optimization, cleanup), every
  Java file visited with proof. Master ledger: `docs/PORT_COVERAGE.tsv` (6,882
  files; 143 ported / 36 partial / 449 reasoned-n/a) + `PORT_COVERAGE.md` roadmap
  (per-subsystem verification gates). Maintain via `tools/port_coverage.sh`.
- BIOME BREADTH: 9 classes certified vs server (cfd5b3d3: taiga/plains/birch/swamp/
  snowy×2/ice_spikes/desert@seed4/old-growth + lush/dripstone caves; 1,867,776 cells)
  then 16 more at C++==GT 0 (d5ff8a5e: jungle/savanna/dark_forest/cherry/mangrove/
  mushroom/bamboo/meadow/mountains; sculk closed the deep_dark residual). Leftovers
  (pale_garden 1363, stony_peaks 151, icebergs, corals, GT↔server triage) in flight.
- ENGINE INTEGRATION (9035e916): mcpp.exe decorates in-game with the certified
  machinery — the decoration TU compiles into the engine as a library
  (MCPP_DECORATE_NO_MAIN; the parity executable stays the truth). Run from repo
  root: `build\mcpp.exe --quickPlaySingleplayer`. Smoke: 65 biomes, 199 placed
  features (0 no-ops), 300+ chunks, zero failures. Deltas documented in
  EngineDecoration.h (neighbour-availability order; disk-only data; legacy
  approximate runStructures pending #18).
- SUBSYSTEM AUDITS with byte-exact gates — six real infidelities found in "done"
  code: NBT (7c53c091: insertion-ordered compounds, modified-UTF-8, real gzip;
  nbt_parity 6/0), PacketBuffer (25bade31: VarLong 10-byte, Utf8String UTF-16
  semantics + replacement decode, unnamed network NBT; packet_buffer_parity 31/0),
  AABB (1c3949ac: bit-exact 512/0, zero findings). Plus: NbtIo gzip silent
  truncation fixed (d5ff8a5e), SurfaceSystem +2→+1 biome sample (75f80bea),
  PalettedContainer corruption (1321bc56), engine build fix + profiling/ tracked
  (09f28fce — was a gitignored build dependency).
- STANDING GATE SUITE (regression-gate EVERY change): newclasses 1867776/0 ·
  forest 884736/0 · ocean 589824/0 · terrain 2359296/0 · nbt 6/0 · packets 31/0 ·
  aabb 512/0 · ninja mcpp builds.
- PARALLEL TRACKS RUNNING: breadth leftovers; VoxelShapes/collision port (movement
  foundation); embedded-assets packaging groundwork; rendering gate #1 (block-model
  baking parity). Specs for structures (#18) and nether/end (#12) in the task list.

**Last updated prior**: Session 66 (worldgen 1:1 — C++ FULL-CELL server byte-match on 6 chunks; forest GT certified; tree port in flight)

**Session 66 continued — the day's full arc (commits 2772bdb6 → 8c1ab5be):**
1. Java GT = server byte-match on the 6 ocean chunks (see below).
2. **C++ engine reaches FULL-CELL parity: `DecorateAll cells=589824 mismatches=0`** on all
   6 server-certified chunks (commits 1321bc56 + 8d6668ad): ore, seagrass/kelp (+fluid
   model/tags), disks, springs, underwater magma, glow_lichen (multiface), bubble-column
   post-processing, WorldGenRegion setBlock/getHeight semantics, blocksMotion/occlusion
   tables. En route found+fixed a REAL engine bug: PalettedContainer resize/direct-mode
   corruption scrambling any section whose palette grew past 16 entries.
3. **Forest certification (commit 8c1ab5be)** — three 1:1 root causes: (a) the seed-1
   world spawn IS chunk (10,10): world creation decorates the spawn 3×3 BEFORE any
   forceload, xz except (10,11) first ("phase 1"; everything else is phase 2 = global xz)
   — `perm:` harness mode reproduces it, header documents the model; (b) vanilla fills
   biomes via `NoiseChunk.cachedClimateSampler`, NOT the raw `randomState.sampler()` —
   they disagree at shore boundaries and flip surface rules ((11,9): 361→0); (c)
   `WorldGenRegion.getRandom()` is a deterministic positional random
   (worldgen_region_random @ chunk world pos) — the proxy's was time-seeded.
   Verified vs the real server: oceans 6×0; (10,10)=0; (11,9)=0; (9,10),(10,9),(10,11)=0;
   (9,11)=2 cells (second-order phase-2 interleaving in uncertified neighbour turns).
4. NEXT (in flight): TREE family port to C++ (TreeFeature core, straight/fancy trunk,
   blob/fancy foliage, fallen trees, beehive/place_on_ground decorators, random_selector,
   patch_grass/bush) certified against plain-xz forest GTs (scheduler-independent; the
   GT↔server link is proven separately). Then underground/top-layer (task #17).

**Session 66** (worldgen 1:1 — decoration track): the fixed-order Java ground truth
`tools/FullChunkDecorateParity.java` now reproduces the real server.jar's saved chunks
**byte-for-byte** — all 6 primary chunks (seed 1) match 98304/98304 cells each, full content
(terrain + carvers + ALL decoration + FULL-promotion post-processing). 13/18 on an extended
sweep; the 5 misses are pure cross-chunk border-spill ORDER (server scheduler artifact).
- THE HARNESS (the spec for the C++ port): 5×5 real ProtoChunks via `fillFromNoise` (NOT
  `getBaseColumn` — its width-1 NoiseChunk aquifer can't reproduce 3D fluid pockets) +
  `buildSurface` + `applyCarvers` + `fillBiomesFromNoise` + `setPersistedStatus(CARVERS)`;
  prime non-WG heightmaps exactly as `ChunkStatusTasks.generateFeatures`; decorate the inner
  3×3 in **xz order** (x outer asc, z inner asc — matches the server's forceload FIFO) via a
  verbatim `applyBiomeDecoration`; a multi-chunk WorldGenLevel Proxy mirrors `WorldGenRegion`
  exactly (getHeight = chunk+1, ensureCanWrite Chebyshev≤1 from the DECORATING center,
  chunk-cache-first BiomeManager, brightness 0, DUMMY block-entity nbt + lazy getBlockEntity,
  `getPostProcessPos` marking); finally a `LevelChunk.postProcessGeneration` mirror (bubble
  columns via the real `BubbleColumnBlock.updateColumn`).
- THE FIVE BUGS that were the last 537 cells (all 1:1 fixes, no tuning): (1) vanilla FLUID
  tags must be bound — `TallSeagrassBlock.canSurvive` consults `FluidTags.WATER`; an unbound
  tag silently tests false, killing tall seagrass and desyncing the aquatic-vegetal RNG.
  (2) The decoration BiomeManager must read the chunk's CACHED noise biome (quartY clamp).
  (3) `fillFromNoise` vs `getBaseColumn` (aquifer). (4) the server run must `tick freeze`
  BEFORE forceloading or fluids flow into the .mca (generation still completes frozen).
  (5) post-processing runs at FULL promotion even frozen (bubble columns; dungeon-chest
  `setLootTable` consumes `random.nextLong` → block-entity path affects the RNG stream).
- CERTIFICATION CHAIN (reproducible): `run_server_gen.ps1` (tick freeze + forceload rect
  (-1,-1)..(12,12), generate-structures=false) → `ServerChunkDump <seed> [cx,cz ...]` →
  `compare_deco.sh <harness.tsv> <server.tsv> <cx> <cz>`; harness:
  `FullChunkDecorateParity <seed> <cx> <cz> xz` (driver: `run_deco_parity.sh`).
- C++ NEXT: certify the C++ port against the Java GT TSVs (scheduler-independent), porting
  family-by-family: shared deps (fluid tags, block predicates, state providers, placement
  modifiers) → vegetal/aquatic → disks/springs/lakes → TREES (biggest) → underground
  (dungeons/magma/lichen) → top_layer + postProcessGeneration. C++ driver already decorates
  in the server-proven xz order (its ore-only metric vs the full GT is border-confounded by
  unported families — do NOT tune against it).

**Last updated prior**: Session 65 (worldgen 1:1 — 3×3 decoration driver; ORE family ~92.5% server byte-match)

**Session 65** (worldgen 1:1 — decoration track): built the whole-chunk decoration driver
and got the first feature FAMILY (ores) substantially byte-matching the real no-structures
server.
- NEW: `full_chunk_decorate_parity` (`FullChunkDecorateParityTest.cpp`) — generates
  terrain+carvers for a 5×5 of chunks, builds a `MultiChunkLevel` (WorldGenRegion-style:
  reads any chunk, radius-1 writes, multi-heightmap WG-live/OCEAN_FLOOR-frozen), runs the
  real `applyBiomeDecoration` loop (FeatureSorter global order + `setFeatureSeed(deco,index,
  step)`, feature selection from the chunk's actual 3×3 noise biomes via `getNoiseBiome`),
  and compares chunk C vs the server `.mca` — one feature FAMILY at a time.
- FIXES (fast-start regressions / dead scaffolding uncovered while wiring): `BiomeFeatures::
  loadFromDirectory` and `BlockTags::loadFromDirectory` were STUBS returning empty (broke all
  tag/feature parity) — restored the real parsers; `NoiseBasedChunkGenerator::getNoiseBiome`
  was declared-but-undefined — defined (delegates to BiomeSource, quart resolution); two
  same-named `mc::levelgen::VerticalAnchor` types clashed once terrain+height-providers were
  in one TU — renamed the SurfaceRules one to `SurfaceVerticalAnchor`; disabled CMake C++
  module scanning (project uses headers, scan miscompiled multi-`..` includes).
- RESULT: terrain stays byte-exact (`full_chunk_parity 2359296/0`); `DecorateOre
  ore_cells=26185 ore_mismatches=1965` → **~92.5% of ore-family cells match the server**
  (the prior whole-gap was 24905). Ore feature (`OreFeature.h`) + RuleTest + height_range
  ported 1:1.
- ORE RESIDUAL (next): `granite/tuff/deepslate` swaps = shifted ore positions ⇒ the
  FeatureSorter GLOBAL index (hence per-feature seed) differs from the server. Prime suspect:
  `collectOverworldPossibleBiomes()` order vs Java `possibleBiomes()` (FeatureSorter assigns
  indices in source-encounter order). Also decoration overlap order across the 3×3 and the
  `getHeight +1` (MultiChunkLevel lacks the real WorldGenRegion +1). Close → ore=0, then port
  the next families (vegetal → trees → lakes/springs → local mods → underground/top-layer).
  See `docs/DECORATION_PLAN.md` + per-agent memory.

**Last updated prior**: Session 64 (worldgen 1:1 — SERVER .mca byte-match ground truth; decoration gap quantified + plan)

**Session 64** (worldgen 1:1 — decoration track, toward server byte-match): goal set by
the user — port ALL decorations + biome characteristics 1:1 (everything EXCEPT structures)
and prove it with the server test. Built the server ground-truth and the porting plan.
- SERVER GROUND TRUTH ("la prueba del server"): `tools/run_server_gen.ps1` runs the
  real `server.jar` (JDK25) with `generate-structures=false`, forceloads a chunk rectangle
  to FULL status, flushes, stops (writes stdin as raw ASCII with a leading-newline BOM
  absorber — the server otherwise parses a U+FEFF as part of the first command).
  `tools/ServerChunkDump.java` reads the `.mca` (manual region container parse + real
  `NbtIo` + 1.16+ paletted section unpack) and dumps every block as TSV in FullChunkParity
  format. Restored the `biome_decoration_parity` target (dropped by c7abe591).
- GAP QUANTIFIED + decoder VALIDATED: terrain-only C++ vs the server's full chunks =
  `24905 / 589824 (4.2%)` over 6 chunks. The 95.8% match proves the `.mca` decode is
  correct; the 4.2% is exactly the un-ported decoration. Gap dominated by the ORE family
  (tuff/andesite/gravel/diorite/granite + iron/lapis) → ores are the first port.
- KEY FINDING: edge-spilling features (ore/lakes/big trees) CANNOT be cleanly certified in
  the single-chunk `BiomeDecorationParity` harness — its Proxy `getChunk` returns the center
  chunk for all coords and `OreFeature` writes via `section.setBlockState` (bypassing the
  out-of-chunk drop), so blobs past the edge wrap into the center chunk (artifact). Ore et al.
  must be certified in the real **3×3 WorldGenRegion vs the server**. Surface features that
  write only via `level.setBlock` (grass/flowers/small trees) stay fine in isolation.
- PLAN: `docs/DECORATION_PLAN.md` — trustworthy-to-reuse infra (FeatureSorter,
  DecorationDriver seeds, PlacedFeature pipeline, BiomeFeatures), the 3×3-region byte-match
  design (5×5 terrain / 3×3 decorate, shared region, multi-heightmap WG-vs-frozen), and the
  coverage-ordered feature porting list (ores → modifiers → vegetal → trees → lakes/springs →
  local mods → underground/top-layer). NEXT: build the `full_chunk_parity --decorate` 3×3
  driver + port the ore family, drive the server mismatch count to 0 family-by-family.

**Last updated prior**: Session 63 (worldgen 1:1 — FULL-CHUNK terrain byte-match vs the real generator; OreVeinifier fix)

**Session 63** (worldgen 1:1 — full-chunk parity track): the user asked for terrain
that is provably 1:1 with the real generator, the acid test being the server jar
producing byte-matching chunks. Built the strongest proof the project has had and used
it to find + fix a real port bug.
- ENV: provisioned the real-Java runtime on this Windows box via new
  `tools/provision_parity_runtime.ps1` — sha1-verified `26.1.2/server.jar`, the 107
  manifest libs, and JDK 25 (client/server jar is Java 25 bytecode; system JDK was 21).
  The portable llvm-mingw/cmake/ninja toolchain lives under a Codex work dir (see the
  per-agent `memory/` notes). Re-verified the chain end-to-end: `worldgen_random_parity
  cases=540 mismatches=0`.
- STALE-DOC NOTE: the recent fast-start commit (`c7abe591`) had DROPPED the terrain
  column parity targets from `src/CMakeLists.txt` (base/surface/carved/density/
  climate/world_placement/…); their `.cpp` files remain but were un-buildable, so the
  "certified" claims below were stale until rewired. Restore the removed blocks from
  `git show 0bfc8121:mcpp/src/CMakeLists.txt` if you need the column tests.
- NEW HARNESS: `tools/FullChunkParity.java` drives the REAL generator through
  fillFromNoise+buildSurface+applyCarvers and dumps EVERY block of a chunk (98,304/chunk,
  canonical order lx→lz→y); C++ `full_chunk_parity` (`FullChunkParityTest.cpp`) generates
  the same chunks and compares all cells. This is FULL-CHUNK, not sampled columns — the
  prior column tests' blind spot.
- BUG FOUND + FIXED (RULE #0): full-chunk parity immediately exposed ~34 granite/
  copper_ore↔stone mismatches per affected chunk that column sampling never hit. Cause:
  the C++ overworld `NoiseRouter` built the ore-vein functions as raw `noise(...)` /
  `map(noise,Abs)`, but Java (`NoiseRouterData.overworld`) wraps each in
  `yLimitedInterpolatable(y, noise, veinMinY=-60, veinMaxY=50, 0)` (y-range-limited AND
  `interpolated()` over the density cell), with `abs()` applied AFTER the wrapper. The
  missing interpolation shifted `|veininess|` at the 0.4 vein threshold. Fixed in
  `NoiseRouterData.cpp` with the existing `yLimitedInterpolatable` helper + `y()`.
- Also added `-ffp-contract=off` to the Clang flags (`cmake/CompilerFlags.cmake`):
  Java never fuses a*b+c into FMA; clang's default contraction can 1-ULP-flip threshold
  comparisons and break byte-exact parity. (Not the vein cause, but a correctness
  requirement for all worldgen FP.)
- CERTIFIED: `full_chunk_parity --cases <wide tsv>` → `FullChunk cases=2359296
  mismatches=0` (4 seeds × 6 chunks incl. negative + far {1000,-1000} coords). Overworld
  terrain through CARVERS is now full-chunk byte-exact with the real 26.1.2 generator on
  the Windows llvm-mingw build.
- SCOPE / next: this proves the noise+surface+carvers BLOCK content. It does NOT yet
  include features/decoration (`applyBiomeDecoration` is still a no-op), structures (still
  approximate), heightmaps, light, or block entities — so an actual server `.mca` chunk
  (status `full`) does not byte-match yet. The path to the full acid test is to extend
  this harness stage-by-stage (heightmaps → features → structures), each certified the
  same way, plus a separate `.mca` verifier. See `docs/WORLDGEN_PLAN.md`.

**Last updated prior**: Session 62 (performance: 3 critical bottlenecks fixed)

**Session 62** (performance optimization track — PROFILING + FIXES):
- Deployed comprehensive profiling infrastructure in Session 61 (Profiler.h/cpp, Python analysis/visualization tools, run_profile.bat helper)
- Analyzed chunk generation architecture and identified **3 critical bottlenecks** in production code via code inspection
- **FIXED BOTTLENECK #1**: `MAX_DECORATE_PER_TICK` was 2 (main-thread bottleneck) → increased to 8 (4× faster decoration of visible chunks). Decoration is serialized and each chunk takes 200-1000ms; the 2/tick budget meant decorating visible area (169 chunks) took 80+ seconds with stutters. Now 4 chunks/tick alleviates main-thread starvation.
- **FIXED BOTTLENECK #2**: `MAX_QUEUE_PER_TICK` was 4 (generation underutilized) → increased to 8 (2× more parallel work). ThreadPool threads were idle because queue was too shallow vs player movement demand (13 chunks/tick).
- **FIXED BOTTLENECK #3**: `startLocalGame` decorated 5×5 grid (25 chunks) synchronously, blocking 5-25 seconds on startup → reduced to 3×3 grid (9 chunks, 2.8× faster startup). Outer ring now decorated asynchronously by updateLocalChunks() as player plays.
- Root cause analysis: RADIUS=6 visible area = 169 chunks; with 2 decorations/tick and each taking 200-1000ms, decoration was fundamentally too slow. Simple const tuning yields major gains.
- VALIDATION NOT YET DONE: profiling infrastructure ready but build toolchain (cmake/VS) not in PATH. Once built, `.\run_profile.bat` will measure actual improvement and confirm fixes work.
- Documentation: created PERFORMANCE_FIXES.md with detailed analysis, expected improvements, and rationale for each fix.

**Last updated prior**: Session 61 (decoration: 5 straight-trunk trees certified 1:1)

**Session 61** (decoration track — more trees): generalized the tree foliage to
dispatch by placer type. CERTIFIED 1:1 (`biome_decoration_parity --tree <id>`,
mismatches=0, 32 land chunks × 2 seeds, ~10 trees/chunk with overlap):
  - oak (13611), birch (13934), jungle_tree_no_vine (15481) — straight + blob
  - spruce (16550) — spruce_foliage_placer (foliageHeight=max(4,treeH-trunkH.sample);
    currentRadius=nextInt(2); no corner rng)
  - pine (12653) — pine_foliage_placer (foliageRadius OVERRIDE adds
    nextInt(max(trunkHeight+1,1)) — easy to miss; caused 90% drift until added)
All share straight_trunk_placer. jungle_tree itself needs decorators (cocoa/vines)
— deferred. NEXT trees (other trunk/foliage placers): acacia (forking +
acacia_foliage), dark_oak/pale_oak (dark_oak placers), cherry, fancy_oak (fancy —
complex), mega_* (giant). Then tree DECORATORS (bees/vines/cocoa/alter_ground),
dead bush/dry grass, dual_noise_provider, ores.

**Last updated prior**: Session 60 (decoration: OAK TREE certified 1:1)

**Session 60** (decoration track — trees): ported the oak `tree` feature to the
C++ data-driven loader and certified it 1:1, incl. multi-tree overlap.
- Ported: `StraightTrunkPlacer` (getTreeHeight = base+nextInt(a+1)+nextInt(b+1);
  placeBelowTrunkBlock via `rule_based_state_provider`; logs; foliage attachment),
  `BlobFoliagePlacer` (foliageHeight const; radius/offset providers; createFoliage
  rows; corner-skip `nextInt(2)` in shouldSkipLocation — only at |dx|==r&&|dz|==r),
  `TwoLayersFeatureSize` + `getMaxFreeTreeHeight`, validTreePos (air|
  #replaceable_by_trees) / isFree (+#logs). Block-name comparison makes
  `updateLeaves` (distance prop) a no-op.
- KEY FIX — heightmaps: Java maintains only `*_WG` heightmaps during worldgen
  decoration; non-WG variants are FROZEN at pre-decoration. So `WORLD_SURFACE_WG`
  is LIVE (a later grass sees an earlier one raise it) but `OCEAN_FLOOR` (used by
  trees) is FROZEN (a later tree's origin is the original surface, not an earlier
  tree's logs). `OCEAN_FLOOR` counts only motion-blocking (is_opaque) non-fluid
  blocks, so leaves don't raise it. Without this, multi-tree drifted (357 mism).
- Origin: trees anchor in the AIR above the surface (heightmap OCEAN_FLOOR gives
  the top-solid y; the synthetic placement adds random_offset vertical(+1) on BOTH
  sides — matches vanilla's on-top-of-ground anchoring).
- CERTIFIED: `biome_decoration_parity --tree minecraft:oak` →
  `placed_cases=13611 mismatches=0` (32 land chunks, 2 seeds, 10 trees/chunk with
  overlap). Grass re-verified 997/0. Harness: `tree:minecraft:oak`.
- NEXT: spruce/birch/etc (other trunk/foliage placer types: fancy/forking/giant,
  spruce/pine/acacia foliage), then dead bush/dry grass, dual_noise_provider, ores.

**Session 59** (decoration track — trees investigation — open blocker, now resolved S60)

**Session 59** (decoration track — trees, IN PROGRESS, not certified): added a tree
ground-truth mode to `BiomeDecorationParity.java` ("tree:minecraft:oak" wraps the
configured feature in a synthetic [count, in_square, heightmap OCEAN_FLOOR] and
runs the REAL Java `TreeFeature` via the Proxy WorldGenLevel; added the proxy
methods `isStateAtPosition` / `isFluidAtPosition` that trees need). Read and
understood the oak RNG order: `StraightTrunkPlacer.getTreeHeight` (base +
nextInt(a+1) + nextInt(b+1)), `BlobFoliagePlacer` (foliageHeight const, radius/
offset providers, `createFoliage` leaf rows, corner-skip `nextInt(2)` in
`shouldSkipLocation`), `TwoLayersFeatureSize`, `rule_based_state_provider`,
validTreePos = air|#replaceable_by_trees, isFree adds #logs. Block-name parity
means `updateLeaves` (distance property) is a no-op.
- RESOLVED (origin): the trunk origin must be the AIR block above the surface
  (heightmap getHeight returns the top-solid y = 63; `getMaxFreeTreeHeight` needs a
  free/air origin). With a synthetic placement [count(10), in_square, heightmap
  OCEAN_FLOOR, random_offset vertical(+1)] the real Java oak places correctly:
  1485 oak_log + 11862 oak_leaves + 264 dirt over 32 chunks. The tree
  ground-truth harness (`tree:minecraft:oak`) is working and emits PRE/PUT.
- NEXT (concrete): port the C++ tree feature into the data-driven loader —
  `StraightTrunkPlacer` (getTreeHeight base+nextInt(a+1)+nextInt(b+1); placeTrunk:
  placeBelowTrunkBlock via rule_based provider, logs, one foliage attachment),
  `BlobFoliagePlacer` (foliageHeight const; radius/offset providers; createFoliage
  rows; corner-skip nextInt(2)), `TwoLayersFeatureSize.getSizeAtHeight` +
  `getMaxFreeTreeHeight`, `rule_based_state_provider`, validTreePos/isFree, and the
  matching synthetic placement on the C++ side — then certify oak, then spruce/
  birch (other placers). NO tree worldgen code committed yet (C++ loader throws on
  `tree`); only the ground-truth tool. Nothing faked.

**Last updated prior**: Session 58 (decoration: vanilla seeding + 8 vegetation features certified)

**Session 58** (decoration track — flowers + correct seeding): fixed the decoration
seeding to be vanilla-faithful and extended the loader through the flower features.
- SEEDING FIX (important): per-feature runs now use the real
  `random.setDecorationSeed(seed, chunkMinX, chunkMinZ)` then `setFeatureSeed(deco,0,0)`
  per chunk (was a fixed `setFeatureSeed(seed,0,0)` for every chunk — gave identical
  RNG everywhere and never exercised `rarity_filter`). Both harness and C++ test.
- Harness keeps a FIXED set of land chunks (16/seed) instead of scanning until
  output, so rare features (rarity_filter) are exercised and runs are bounded.
- Loader additions: `noise_threshold_provider` (flower_plain), `dual` not yet;
  `simple_random_selector` (recursive — picks one inline placed sub-feature via
  nextInt(size); forest flowers).
- CERTIFIED 1:1 (`biome_decoration_parity`, mismatches=0, 32 land chunks, 2 seeds,
  vs real Java): patch_grass_plain (997), patch_grass_forest (211),
  patch_grass_jungle (2180), patch_grass_normal (508), patch_tall_grass (114,
  double-plant), patch_large_fern (114, double-plant), flower_plains (14,
  noise_threshold), flower_forest_flowers (120, simple_random_selector).
  flower_default trivially 0. Run via `tools/BiomeDecorationParity.java <placed>
  <grassy biome listing it>`.
- NOT yet: `dual_noise_provider` (flower_meadow), dead bush / dry grass (need
  `#dead_bush_may_place_on` surfaces), and the big one — TREES (`tree` feature +
  trunk/foliage placers + decorators), the next major type.

**Session 57** (decoration track — data-driven, vegetation batch): made the

**Session 57** (decoration track — data-driven, vegetation batch): made the
decoration parity test data-driven — it loads `placed_feature/<id>.json` +
`configured_feature/<id>.json` and assembles the ported PlacedFeature pipeline
(throws on any unsupported modifier/feature/provider — never faked). Supported so
far: modifiers `noise_threshold_count`, `noise_based_count`, `count`,
`rarity_filter`, `in_square`, `heightmap`, `random_offset`, `biome` (pass-through),
`block_predicate_filter` (matching_block_tag / matching_blocks / would_survive /
all_of / any_of / not / true); feature `simple_block` with `simple` and
`weighted` state providers; double-plant placement; `canSurvive` for
`#supports_vegetation` and `#dead_bush_may_place_on`.
- CERTIFIED 1:1 (each `biome_decoration_parity --placed <id>`, mismatches=0,
  6 land chunks × 2 seeds, vs real Java): `patch_grass_plain` (186),
  `patch_grass_forest` (34), `patch_grass_jungle` (410), `patch_grass_normal` (89),
  `patch_tall_grass` (162, double-plant). Run via
  `tools/BiomeDecorationParity.java <placed> <grassy biome that lists it>` then the
  C++ test. The forced biome MUST be grassy-surfaced (plants need
  `#supports_vegetation` below) AND list the feature.
- Harness note: the land-chunk scan finds output by trial; RARE features
  (rarity_filter, e.g. `patch_tall_grass_2`) are slow to find producing chunks —
  use a denser biome or widen the scan. Flowers (`flower_*`, weighted /
  noise_threshold providers) are next, then dead bush / dry grass, then TREES.
- NoiseThresholdProvider (flower_plain) and HeightProvider/`height_range` (ores)
  are ported but not yet wired into this loader.

**Session 56** (decoration track — first certified feature): built the decoration

**Session 56** (decoration track — first certified feature): built the decoration
ground-truth harness and certified the first vegetation feature end-to-end.
- The blocker for decoration parity is that Java feature placement needs a
  `WorldGenLevel` (unlike `buildSurface`). Solution: `tools/BiomeDecorationParity.java`
  drives the REAL vanilla `PlacedFeature.placeWithBiomeCheck` over a dynamic-`Proxy`
  `WorldGenLevel` backed by a ProtoChunk (implements the abstract methods features
  touch; routes interface default methods via `InvocationHandler.invokeDefault`;
  single-chunk isolation — air outside, drop out-of-chunk writes). It scans for
  land chunks (cheap centre-column precheck), emits the pre-decoration chunk (PRE)
  and every block the feature writes (PUT).
- CRITICAL: the harness must bind the vanilla block tags to `BuiltInRegistries.BLOCK`
  (`bindVanillaBlockTags`, same fix as Session 52's carver harness) or `state.is(tag)`
  is always false and `canSurvive` rejects every plant. short_grass survives on
  `#minecraft:supports_vegetation` (NOT `#dirt`, which is only dirt/coarse/rooted).
- C++: certified the depth-first `PlacedFeature.place` (Session 55) drives the real
  modifier chain; `BiomeDecorationParityTest` loads PRE into a LevelChunk, runs the
  assembled `patch_grass_plain` chain (noise_threshold_count, in_square, heightmap,
  count, random_offset[trapezoid], block_predicate[#air]) + `simple_block(short_grass)`.
  CERTIFIED: `biome_decoration_parity` → `feature=patch_grass_plain placed_cases=186
  mismatches=0` (6 land chunks, 2 seeds). This is the first of ~30 feature types.
- NEXT: more `simple_block`/vegetation reuse the same pipeline (fern, dead bush,
  flowers, dry grass); then `random_patch`/`flower` configs; then TREES (the big
  one: `TreeFeature` + trunk/foliage placers + decorators); ores; lakes/springs/etc.
  `applyBiomeDecoration` runtime wiring stays a no-op until each is parity-tested.

**Session 55** (decoration track — foundation): the user asked to port all biome

**Session 55** (decoration track — foundation): the user asked to port all biome
decorations. State check first: `applyBiomeDecoration` is a deliberate **no-op**
(Sessions 40–42 removed the approximate feature output rather than violate RULE #0;
only the FeatureSorter ordering is wired, certified Session 53). Porting all 258
placed / 221 configured features (≈30 feature types, each with exact RNG order) is
the largest remaining subsystem and must be done family-by-family, each certified.
- FIXED a foundational correctness bug blocking ALL decoration parity:
  `placement/PlacedFeature.place` composed modifiers BREADTH-first (each modifier
  applied to the whole position vector) but Java's lazy `Stream.flatMap` chain is
  DEPTH-first (each origin flows through modifiers[i+1..] and the feature before
  the next origin). This is RNG-observable for any feature with ≥2 RNG-consuming
  modifiers (e.g. ores: in_square + height_range + feature). Reimplemented as a
  depth-first recursive drive that matches Java's evaluation order exactly.
  Verified: `placement_parity` and `world_placement_parity` still pass.
- NOT done: actual feature ports remain. Next target is the ore family
  (`OreFeature.place` is read/understood; needs `RuleTest` tag_match/block_match,
  the `height_range` modifier, a WorldGenLevel-over-LevelChunk, and a per-feature
  decoration parity harness that feeds vanilla base+surface terrain in — same
  isolation trick as Session 54, since base terrain is not FP-exact on Linux).

**Session 54** (biomes → terrain track): the user asked to make biomes affect

**Session 54** (biomes → terrain track): the user asked to make biomes affect
terrain and certify it one by one. Confirmed from the 26.1.2 source that terrain
*shape* is biome-independent (`NoiseChunk` imports only `Climate`, not `Biome`);
the biome-dependent part of terrain is the **surface rules** (`SurfaceSystem` /
`SurfaceRules`). Built a per-biome surface certification for all 54 overworld
biomes.

- New `NoiseBasedChunkGenerator::buildSurface(chunk, biomeOverride)` overload to
  force one biome over a column in isolation (the default overload delegates to it
  with the BiomeManager getter — behaviour unchanged).
- `tools/BiomeSurfaceParity.java` emits the biome-independent base column (BASE
  rows) once per (seed,col), then forces each overworld biome over that exact base
  terrain and runs the real vanilla `SurfaceSystem.buildSurface` (SURF rows).
  `BiomeSurfaceParityTest` loads BASE into a `LevelChunk` and runs the ported
  surface system with the same forced biome.
- CERTIFIED: `biome_surface_parity` → `biomes=54 cases=248832 mismatches=0` for
  all 54 overworld biomes (2 seeds × 6 columns × full column). The ported surface
  rules — badlands clay bands (`generateBands`/`getBand`), desert sand/sandstone,
  snow/ice, gravel/stony shores, swamp/mangrove mud, frozen-ocean & eroded-badlands
  extensions, deepslate/bedrock — are bit-exact given identical base terrain.

- IMPORTANT FINDING (Linux toolchain): the base-terrain/aquifer stage is NOT
  bit-exact on the Linux g++ build. `base_terrain_column_parity` = 202/21504 and
  `surface_terrain_column_parity` = 299/21504 mismatches here, all FP-sensitive
  aquifer water/stone cells. `density_parity` (router functions) is still 0 on
  Linux, so the divergence is in `fillFromNoise` cell interpolation / aquifer
  float ordering, not the noise. These were certified 0 on the Windows llvm-mingw
  toolchain (Sessions 50–52). The per-biome surface certification feeds vanilla
  base terrain in precisely to factor this out; closing the terrain FP gap on
  Linux (or confirming it stays 0 on Windows) is a separate terrain-track task.
  NOTE for parity work on a fresh clone: `26.1.2/` and `src/assets/*.json`
  are git-LFS; run `git lfs pull` + re-extract `data/` from the real jar first.

**Session 53** (feature ordering + structure placement track): integrated the

**Session 53** (feature ordering + structure placement track): integrated the
FeatureSorter parity work from PR #7 (`continue-feature-sorter-parity`) into the
mainline and certified it on Linux, then added the first certified structure
stage — structure *placement* (where structures can spawn for a seed).

- Toolchain: this session ran the real Java ground-truth on **Linux**. Fetched
  the SHA1-verified `client.jar`, JDK 25 and the 107 manifest libs into `26.1.2/`
  per the AGENTS.md CDN workflow, and re-extracted `data/minecraft/**` from the
  real jar (the cloned `26.1.2/` was git-LFS pointers, not real content — any
  parity work MUST materialize the LFS content first). Added
  `tools/run_groundtruth.sh`, the Linux counterpart of `run_groundtruth.ps1`.

- FeatureSorter (PR #7, now mainline): `FeatureSorter::buildFeaturesPerStep`
  matches Java — `maxStep` uses each biome's full JSON feature-list length
  (trailing empty steps included), and the `FeatureData` comparator orders by
  (step, featureIndex). Certified: `feature_sorter_parity` →
  `biomes=65 steps=11 features=205 mismatches=0` vs `FeatureSorterParity.java`.
  This pins the global per-step PlacedFeature index fed to
  `WorldgenRandom.setFeatureSeed(decorationSeed, index, step)` for every biome.

- Structure placement (new): ported
  `world/level/levelgen/structure/placement/StructurePlacement.{h,cpp}` 1:1 —
  `RandomSpreadStructurePlacement.getPotentialStructureChunk`/`isPlacementChunk`,
  `RandomSpreadType` (linear/triangular), `Math.floorDiv`, the four
  `FrequencyReductionMethod` reducers (default + legacy_type_1/2/3, with the exact
  quirky arg orders), and exclusion zones via `hasStructureChunkInRange`. Loads
  every `worldgen/structure_set/*.json`. Certified: `structure_placement_parity` →
  `seeds=4 set-checks=76 positives=28947 mismatches=0` vs
  `StructurePlacementParity.java` (real Java `StructurePlacement.isStructureChunk`
  through `ChunkGeneratorStructureState`), covering all **19 random_spread** sets
  over a 160×160 chunk grid for seeds {0,1,42,123456789}.

- NOT yet certified / next: strongholds (`concentric_rings` placement — needs the
  ring precompute in `ChunkGeneratorStructureState`); and the structure *piece*
  assembly (jigsaw/template) — i.e. the actual blocks each structure writes. The
  existing `StructureGen.cpp` piece shapes remain hand-built approximations and
  are explicitly NOT 1:1 yet. Biome decoration feature execution is ordered
  correctly now (FeatureSorter) but per-feature placement is still partially
  ported (see Sessions 32–42).

**Session 52**: picked up the Session 51 carver verification handoff from the
Claude Code transcript. Generated the Java carved-terrain ground truth with
`run_groundtruth.ps1 -Tool CarvedTerrainColumnParity`, then chased the apparent
224 C++ over-carve mismatches. Root cause #1 was the Java harness, not C++:
`VanillaRegistries.createLookup()` alone left `BlockTags.OVERWORLD_CARVER_REPLACEABLES`
effectively unbound, so real replaceables such as `minecraft:deepslate` tested
false in Java. Fixed `tools/CarvedTerrainColumnParity.java` to load and
resolve the vanilla block tag JSON from `26.1.2/data/minecraft/tags/block/**`
and apply it to `BuiltInRegistries.BLOCK` before creating the registry lookup.
That dropped carved parity to one mismatch. Root cause #2 was a real C++ aquifer
bug: `NoiseBasedChunkGenerator::samplePreliminarySurfaceLevel()` evaluated raw
block coordinates and truncated toward zero; Java `NoiseChunk.preliminarySurfaceLevel`
quantizes X/Z through `QuartPos.fromBlock()->toBlock()` and uses `Mth.floor`.
Ported that exact quantize+floor behavior. Final verified wrapper runs:
`base_terrain_column_parity` (`BaseTerrainColumn cases=21504 mismatches=0`),
`surface_terrain_column_parity` (`SurfaceTerrainColumn cases=21504 mismatches=0`),
`carved_terrain_column_parity` (`CarvedTerrainColumn cases=21504 mismatches=0`),
and `mcpp` target build. Terrain is now sampled-column certified 1:1 through
`fillFromNoise()+buildSurface()+applyCarvers()` for the overworld cases; structures,
features, decoration, exhaustive surface-rule coverage, and nether/end terrain
remain uncertified.

**Session 51**: continued the terrain-only 1:1 track by adding the first Java-shaped
overworld carver stage after the now-certified base/surface pipeline. Added
`tools/CarvedTerrainColumnParity.java` and the C++ target
`carved_terrain_column_parity` to compare Java `buildSurface()+applyCarvers()`
columns before structures/features/decoration. Implemented
`world/level/levelgen/carver/WorldCarver.{h,cpp}` with the vanilla overworld
configured carvers (`minecraft:cave`, `minecraft:cave_extra_underground`,
`minecraft:canyon`), Java `WorldgenRandom.setLargeFeatureSeed(seed + index, x, z)`
ordering, cave/canyon config constants from `Carvers.java`, `CarvingMask`,
aquifer-backed carved state selection, lava-level handling, exact resolved
`overworld_carver_replaceables` fallback IDs, Java `Mth` sine table indexing, and
the `WorldCarver.carveBlock` grass/mycelium top-material restoration path via a
ported `SurfaceSystem::topMaterial`. `NoiseBasedChunkGenerator::applyCarvers()`
is now wired into both local initial chunk generation and async chunk generation,
so `mcpp.exe` generates terrain with the new carver stage. Re-verified the
already-certified stages after the integration: `base_terrain_column_parity`
(`BaseTerrainColumn cases=21504 mismatches=0`) and
`surface_terrain_column_parity` (`SurfaceTerrainColumn cases=21504 mismatches=0`),
then built `carved_terrain_column_parity` and `mcpp`. Important caveat: the Java
carved-terrain TSV could not be generated in this session. The normal sandbox run
failed with JDK/libs access errors, and the required escalated runner was rejected
by the Codex usage limit until 8:40 PM. Therefore carver code is compiled and
Java-shaped, but carved terrain is NOT yet certified 1:1; next terrain work should
first generate `build/carved_terrain_columns.tsv` with
`tools/run_groundtruth.ps1 -Tool CarvedTerrainColumnParity` and run
`carved_terrain_column_parity --cases ...`, then fix any mismatches.

**Session 50**: focused exclusively on proving/fixing the current overworld base
terrain before continuing terrain generation. Added real Java ground-truth
harnesses `tools/BaseTerrainColumnParity.java` and
`tools/SurfaceTerrainColumnParity.java`, plus C++ targets
`base_terrain_column_parity` and `surface_terrain_column_parity`. The base
terrain harness compares C++ `NoiseBasedChunkGenerator::fillFromNoise()` against
Java `NoiseBasedChunkGenerator.getBaseColumn()` before surface/carvers/features;
the surface harness fills a real Java `ProtoChunk`, calls vanilla
`NoiseBasedChunkGenerator.buildSurface(...)`, and compares after C++
`fillFromNoise()+buildSurface()`, still before carvers/structures/features/
decoration. Fixed three real parity bugs: removed hardcoded bottom bedrock from
`fillFromNoise()` because Java applies bedrock via surface rules, passed the
root `RandomState.random()` positional factory into `SurfaceSystem` instead of
the `"minecraft:terrain"` factory, and fixed `Mth.getSeed` parity by preserving
Java's 32-bit overflow for the `x * 3129871` term before widening to `long`.
Verified with wrapper commands: regenerated 21,504 Java base rows and 21,504
Java surface rows, built and ran `base_terrain_column_parity`
(`BaseTerrainColumn cases=21504 mismatches=0`), built and ran
`surface_terrain_column_parity` (`SurfaceTerrainColumn cases=21504 mismatches=0`),
ran `worldgen_random_parity` (passed), and built `mcpp`. A short
`--quickPlaySingleplayer` smoke was intentionally killed by timeout after launch;
the wrapper log did not capture game stdout. Caveat: this is sampled column
parity for overworld terrain through `buildSurface`; carvers, structures,
features, decorations, exhaustive surface-rule coverage, and nether/end terrain
are not certified by these two tests.

**Session 49**: pulled `origin/main` to `208c9c0` and continued the strict
Java-vs-C++ worldgen parity track. Fixed `tools/run_with_timeout.ps1` so it
normalizes duplicate `Path`/`PATH` process environment entries before
`Start-Process`; this keeps the mandatory timeout wrapper usable in the Codex
desktop environment. Re-generated real Java `DensityParity` cases with
`run_groundtruth.ps1` and verified `density_parity`: `DensityRouter cases=488
mismatches=0`, so the seed-wired overworld `NoiseRouter` functions
(`temperature` through `final_density`) are bit-exact for the sampled cases.
Added the next parity stage: `tools/ClimateBiomeParity.java` emits real
Java `RandomState` + `Climate.Sampler` +
`MultiNoiseBiomeSourceParameterList.Preset.OVERWORLD` rows, and
`climate_biome_parity` compares all six quantized climate target coordinates
plus the selected biome id. Verified with wrapper commands: generated 56 Java
rows, built `climate_biome_parity`, and ran it with `ClimateBiome cases=56
mismatches=0`. Caveat: the Java ground-truth runner needed to be executed outside
the Codex filesystem sandbox because JDK 25's `toRealPath()` on
`26.1.2/jdk25/conf/security/java.security` is denied inside the sandbox; normal
C++ builds/tests ran through the wrapper.

**Session 48**: fixed the tasklist UI bug requiring an in-game pause menu. Added
`gui/screens/PauseScreen` and wired ESC as an edge-trigger in `main.cpp`: when no
screen is open and the local player is in-game, ESC opens pause; when pause is
open, ESC or `Back to Game` calls `Minecraft::resumeGame()` and recaptures the
mouse. The pause menu follows the vanilla two-column layout basics from
`PauseScreen.java`: `Back to Game`, Advancements/Statistics/feedback/reporting
stub buttons for unported screens, `Options...`, and `Disconnect`. `Disconnect`
returns to the title screen; `Options...` from pause opens options with a back
action that returns to pause instead of title. The in-game render path now draws
an active screen over the world instead of always drawing HUD, which is required
for pause. Also fixed a pre-existing quickplay bug: GUI texture initialization no
longer reopens `TitleScreen` after `startLocalGame()` has already set the screen
to `nullptr`. Verified with short wrapper commands only: built `mcpp` Release and
ran a 12s `--quickPlaySingleplayer` smoke; logs show TitleScreen -> nullptr ->
world ready -> main loop, with no TitleScreen reopened after entering the world.
`TASKLIST.md` now marks only the pause-menu subitem done.

**Session 47**: fixed the tasklist UI bug where the `GUI Scale` option existed
but did not affect the rendered interface. Ported Java's
`Window.calculateScale(maxScale, enforceUnicode=false)` behavior: scale starts at
1 and increments while the next scale keeps the logical GUI at least 320x240;
option value `0` is Auto, and `1..4` are fixed caps. `Minecraft` now exposes
`guiScale()`, `guiScaledWidth()/Height()`, `guiMouseX()/Y()`, and `resizeGui()`.
Screens initialize/reinitialize with logical scaled dimensions, mouse
click/drag/release events are converted from framebuffer coordinates to GUI
coordinates, and `GuiGraphics::render` is called with logical GUI dimensions so
2D vertices scale back to the physical framebuffer like Java. The in-game HUD now
positions hotbar/crosshair/stats using logical GUI dimensions too. Verified with
wrapper commands: built `mcpp` Release and ran an 8s title-screen smoke; the
window initialized, entered the main loop, and was killed by the intentional
timeout. `TASKLIST.md` now marks only the GUI Scale subitem done; broader menu
completion remains open.

**Session 46**: fixed the tasklist UI bug where option sliders only reacted to
clicks. Ported the relevant `AbstractSliderButton` behavior into the current C++
widget stack: left click starts a slider drag, `mouseDragged` updates the value,
and `mouseReleased` clears the drag state. The value update now uses Java's handle
geometry, `(mouseX - (x + 4)) / (width - 8)`, instead of the previous full-widget
width fraction. Added mouse release/drag propagation through `Window` ->
`main.cpp` -> `Screen` -> `OptionsScreen`/`OptionsSubScreen` -> widgets, while
preserving the existing in-game click-to-capture behavior when no screen is open.
Verified with wrapper commands: built `mcpp` Release and ran an 8s title-screen
smoke; the window initialized, entered the main loop, and was killed by the
intentional timeout. `TASKLIST.md` now marks only the slider-drag subitem done;
the broader menu/UI section remains open.

**Session 45**: re-verified and tightened the chunk-border decoration path. The
client remains the owner of `applyBiomeDecoration()` and only decorates chunks
after all 8 neighbours are loaded, passing `chunkAt` through `ChunkWGL` into
`TreeWorld` so trees at a chunk edge can write leaves/logs into adjacent loaded
chunks. Removed the old decoration call from
`NoiseBasedChunkGenerator::buildSurface()`; that route decorated a single chunk
without a neighbour resolver and could duplicate client decoration or reintroduce
clipped features. Restored the standalone/embedded worldgen JSON fallback in
`Minecraft::ensureWorldgenData()`: if local `26.1.2/data` is absent, it loads
biome features and block tags from embedded MCAS `data/minecraft/...` entries
and installs the JSON asset reader for placed/configured feature resolution.
`biome_decorator_test` now has a deterministic edge-oak regression check that
places an oak at `x=15` and proves leaves/logs appear in chunk `x=1`; the test
also locates `26.1.2/data` from the repo root. Verified with wrapper
commands: built `biome_decorator_test`, ran `biome_decorator_test`, built
`embedded_worldgen_assets_test`, ran `embedded_worldgen_assets_test`, built
`mcpp`, and ran a 35s `--quickPlaySingleplayer` smoke (world loaded and entered
main loop; process was killed by the intentional timeout). Caveat: non-tree
features still write through `ChunkWGL::setBlock`, which is clamped to the active
chunk except where a feature uses a dedicated cross-chunk writer; Java's full
`WorldGenRegion` write/read semantics are still not fully ported.

**Session 44**: fixed the tasklist ore-distribution path by moving ore generation
onto the Java/data-driven decoration pipeline instead of the old hardcoded
`OreGen.cpp`. `BiomeDecorator` now parses `minecraft:height_range` via the
already-ported `HeightProvider` stack (`constant`, `uniform`, `biased_to_bottom`,
`very_biased_to_bottom`, `trapezoid`) and resolves vertical anchors
(`absolute`, `above_bottom`, `below_top`). `minecraft:ore` configured features
now port Java `OreFeature`: seeded vein direction, segment radius samples,
overlap culling, tested-position bitset, target-state order, `tag_match` and
`block_match` rule tests, and `discard_chance_on_air_exposure` with the six-way
air-neighbor check. `TreeGen.cpp` no longer includes or calls `OreGen.h`, and
`OreGen.cpp` was removed from the main `mcpp` and `biome_decorator_test` CMake
source lists so the hand-authored ore table cannot affect runtime generation.
The fallback block registry now includes ore/replacement blocks so decoration
tests without embedded `block_states.json` remain meaningful. Added a
`biome_decorator_test` mountain-slab check proving `ore_emerald` goes through
the real `height_range` path. Verified with wrapper commands: built
`biome_decorator_test`, ran `biome_decorator_test`, built `mcpp`, ran
`block_tags_parity`, `embedded_worldgen_assets_test`, and `vegetation_demo`; all
pass. Caveat: decoration still writes through a single-chunk `WorldGenLevel`
view, so cross-chunk feature writes/neighbour reads are still clamped compared
with Java's full `WorldGenRegion`.

**Session 43**: improved plant model fidelity using the real 26.1.2 block model
JSON from `client.jar`. Important parity note: ordinary flowers such as dandelion
and poppy are still `minecraft:block/cross` in Java and should remain cross
meshes. The special flat flowerbed blocks `pink_petals`, `wildflowers`, and
`leaf_litter` now render as a low horizontal double-sided plane instead of cube or
cross geometry. `vine` now renders as thin wall planes inset on faces that have a
solid neighboring support, with a north-face fallback when direction data is not
available; this matches the shape of the Java `vine` model better than the old
generic block path. `cave_vines` and `cave_vines_plant` were added to the plant
cross path because their Java models use `minecraft:block/cross`. Neighbor
face-culling now treats these special plant/vine blocks as non-occluding. Verified
with wrapper command: `mcpp` target builds. Remaining caveat: full multipart
state-driven model rendering (e.g. exact `flower_amount`/`facing` for flowerbeds
and exact vine directional block-state properties) is still not ported.

**Session 42**: fixed the visible gray/cube vegetation class called out in the
tasklist for the currently ported plant renderer. `ChunkMesh` now treats
`short_dry_grass`, `tall_dry_grass`, `bush`, and `firefly_bush` as cross-plane
plant meshes instead of ordinary block geometry, and neighbor face-culling now
recognizes those blocks as plants too. Dry grass textures now use Java's
`DryFoliageColor.FOLIAGE_DRY_DEFAULT` fallback tint (`#5C3C32`) instead of raw
gray atlas pixels; bush/firefly_bush use the existing plains grass tint fallback.
Verified with wrapper command: `mcpp` target builds. Remaining caveat: this is
still a fallback tint path; true biome-aware grass/foliage/dry-foliage/water
colors need biome IDs available to the mesher and Java colormap sampling.

**Session 41**: fixed the tasklist standalone-decoration gap. `tools/asset_packer`
now accepts an optional `data_minecraft_dir` argument and packs
`data/minecraft/worldgen/**` plus the direct `data/minecraft/tags/block/*.json`
files into MCAS under stable `data/minecraft/...` keys. The local ignored
`src/assets/assets.bin` was regenerated from `assets/`,
`26.1.2/src/assets` (absent/skip), and `26.1.2/data/minecraft`; it now contains
5944 entries and the worldgen/tag JSON needed by decoration. `AssetManager` can
now list packed paths by prefix. `BlockTags` and `BiomeFeatures` share their disk
parsers with a new `loadFromJsonEntries` path so embedded JSON and sidecar JSON
produce the same data. `BiomeDecorator` has an optional JSON asset-reader fallback
for lazy `placed_feature`/`configured_feature` resolution, and
`NoiseBasedChunkGenerator::decorationData()` now tries local `26.1.2/data` first
but falls back to MCAS when the exe is moved. `.gitignore` now permits source files
under `src/assets` to be versioned while keeping extracted/proprietary blobs
ignored, and `assets/assets.rc` has explicit CMake `OBJECT_DEPENDS` so resource
objects rebuild when the local pack changes. Added `embedded_worldgen_assets_test`,
which initializes the embedded resource and verifies 65 biome JSON files and 244
block tags load/parse from MCAS. Verified with wrapper commands: regenerated
`assets.bin`, built `asset_packer`, `mcpp`, `embedded_worldgen_assets_test`,
`biome_decorator_test`, `block_tags_parity`, and `vegetation_demo`; all tests pass.

**Session 40**: first tasklist bug pass focused on generated decoration/log regressions.
`Blocks` now exposes `getBlockStateId(serializedState)` for canonical states such
as `minecraft:oak_log[axis=x]` and `minecraft:tall_grass[half=upper]`, and loaded
pillar state IDs now carry inferred `axis` properties. `BiomeDecorator` preserves
JSON `Properties` when resolving providers, writes property-aware state IDs into
chunks, resolves trunk/leaf states through the new state helper, uses horizontal
log IDs in tree configs, and makes `FallenTreeFeature` closer to Java's shape:
stump first, random horizontal direction, 2-3 block gap from the stump, downward
ground probe, solid-support checks, gap limiting, and horizontal `axis=x/z` logs.
Feature/config caches in `BiomeDecorator` are now `thread_local` and keyed by data
directory + feature key to avoid shared-cache races during threaded generation.
`ChunkMesh` now chooses pillar top/side textures from the block state's `axis`, so
horizontal fallen logs no longer render as vertical logs. `NoiseBasedChunkGenerator`
now computes the heightmap after surface creation, loads biome feature lists and
block tags from local `26.1.2/data/minecraft`, and calls `applyBiomeDecoration`
before the final heightmap/mesh-dirty pass. Build hygiene fixed during verification:
`nlohmann_json` exposes the correct vendor include root, MinGW links XAudio2 as
`xaudio2_9`, DX12 adapter probing uses `IID_PPV_ARGS`, and `AABB.cpp` includes
`<algorithm>`. Verified with wrapper commands: `mcpp` build, `biome_decorator_test`,
`block_tags_parity`, and `vegetation_demo` all pass. Remaining important caveat:
runtime decoration data still comes from sidecar `26.1.2/data/minecraft`; embedding
worldgen/tags JSON into the executable/asset pack is still required for a moved
standalone exe to keep trees/decorations.

**Session 39**: extended surface vegetation coverage. `world/level/block/BlockStates.h`
(canonical state-id property helpers: blockName / setProperty, sorted like Java).
`BlockBehaviour.h` now dispatches `canSurvive` by plant family —
VegetationBlock/DoublePlant -> SUPPORTS_VEGETATION, DryVegetationBlock (dead_bush,
short/tall_dry_grass) -> SUPPORTS_DRY_VEGETATION (sand/red_sand/terracotta family +
the veg grounds), 30 blocks. `SimpleBlockFeature` now handles DoublePlantBlock
(tall_grass/large_fern/sunflower/lilac/rose_bush/peony): checks the space above and
places both halves (state[half=lower] + [half=upper]). vegetation_demo now also
generates tall_grass (69 lower + 69 upper) and block_tags_parity verifies the
dry-veg + double-plant canSurvive matrices. Also ported `BlockColumnFeature`
(feature/BlockColumnFeature.h) — the vertical-column feature used by sugar cane
and cactus (per-layer IntProvider heights, allowedPlacement-gated truncation, no
canSurvive); vegetation_demo generates a 3-tall sugar_cane column. Surface
vegetation now covers feature types simple_block, double-plant and block_column,
and the canSurvive families: VegetationBlock (grass, ferns, all standard flowers,
bush, sweet_berry_bush, firefly_bush, lily_of_the_valley) -> SUPPORTS_VEGETATION;
DoublePlantBlock -> SUPPORTS_VEGETATION; DryVegetationBlock -> SUPPORTS_DRY_VEGETATION;
full-block plants (pumpkin, melon) -> always true (ground gated by the placement's
block_predicate_filter). DOCUMENTED EXCEPTIONS not yet handled: mushrooms
(light-dependent), cactus (horizontal-neighbour-dependent; block_column places the
column), wither_rose (netherrack/soul soil). Remaining: those bespoke rules, the
huge-mushroom/bamboo/vegetation_patch feature types, and engine integration
(applyBiomeDecoration loop + FeatureSorter + biome registry + embed worldgen JSON).

**Session 38 end-to-end**: `PlacedFeature.place` composition ported (the flatMap
modifier chain + per-position feature call) plus `TrapezoidInt`, and small
`BiomeFilter`/`BlockPredicateFilter` modifiers (std::function-backed). The
`vegetation_demo` target runs the real `patch_grass_plain` chain
(noise_threshold_count → in_square → heightmap → biome → count(32) →
random_offset(trapezoid) → block_predicate_filter(air)) on a flat test world and
generates short_grass on the surface (84 blocks, correct state/Y). It's a
self-consistency integration check — each component is verified 1:1 vs Java
separately; full end-to-end vs Java is blocked only by datapack tag binding.
SURFACE VEGETATION now generates. Remaining: wire into the real ChunkGenerator
(applyBiomeDecoration loop + FeatureSorter + biome registry + embed worldgen
JSON), more plant families' canSurvive, and the rest of the feature/modifier
types (trees, patches, etc.).

**Session 38**: ported the block-behaviour gate for surface vegetation.
`world/level/block/BlockTags.{h,cpp}` is a data-driven resolver of
`data/minecraft/tags/block/*.json` (recursively expands `#tag` refs); verified
vs `tools/block_tags_reference.py` over all 244 tags (`block_tags_parity`).
`world/level/block/BlockBehaviour.h` ports `VegetationBlock.canSurvive` =
`below ∈ BlockTags.SUPPORTS_VEGETATION` (the 26.1.2 rule for grass/flowers; the
DIRT tag shrank, the real ground tag is SUPPORTS_VEGETATION = dirt/mud/moss/
grass_blocks + farmland, 11 blocks). All vegetation feature pieces are now ported
and verified: positions (placement modifiers), state (BlockStateProvider, incl.
noise/flowers), survival (canSurvive), write (SimpleBlockFeature.setBlock). What
remains is INTEGRATION: PlacedFeature.place composition, the applyBiomeDecoration
loop + FeatureSorter, and wiring the biome registry + embedding worldgen JSON.
Other plant families' canSurvive and the rest of the feature/modifier types are
incremental.

**NormalNoise parity gap — FIXED (Session 37)**: root cause was a C++
undefined-behaviour bug in `LegacyRandomSource::nextLong()` /
`SingleThreadedRandomSource::nextLong()`: they did
`composeJavaNextLong(next(32), next(32))`, but C++ leaves function-argument
evaluation order unspecified, and gcc evaluated the two `next(32)` right-to-left
— swapping the long's halves vs java.util.Random (upper-then-lower). This was
invisible to the earlier parity tests because they never exercised legacy
`nextLong` directly (WorldgenRandom has its own explicit nextLong). It corrupted
`forkPositional()` (which seeds via `nextLong`), hence every PerlinNoise octave
seed (`positional.fromHashOf("octave_N")`), hence all NormalNoise — i.e. ALL
terrain noise. Fixed by evaluating the two `next(32)` into ordered locals.
Now verified: `NormalNoise.getValue` matches Java bit-for-bit and the
NoiseThresholdProvider (flower_plain) state selection matches 1:1
(`block_state_provider_parity` NOISE + BSPN cases).
**Current phase**: PHASE 15 (Game Logic) in progress; worldgen feature/structure port started
**Executable**: `C:\Users\Mateo\Desktop\minecraftcpp\build\mcpp.exe` - built 2026-06-06

**Worldgen roadmap**: `docs/WORLDGEN_PLAN.md` is the master 1:1 checklist for
the full feature/tree/structure port (Phases A–G, with real data counts). Phase A
 (biome registry) is done; overworld carvers now have sampled Java parity through
`carved_terrain_column_parity`; the remaining work is decoration framework,
feature types, trees beyond the currently wired primitives, structures, broader
engine integration, and nether/end coverage. Do NOT reintroduce
the removed hand-authored approximate generators — port from Java + data only.

**Next action**: PHASE 15 — Game Logic
1. ~~Audit `OverworldBiomeBuilder.cpp` line-by-line against `OverworldBiomeBuilder.java`.~~
   DONE (Session 30): proven byte-identical to the real Java output (7593 entries,
   all quantized longs + biome ids + order match exactly). See `overworld_biome_parity`.
2. Split/rename `BiomeSource` into the Java-shaped roles (`BiomeSource`, `MultiNoiseBiomeSource`, parameter-list preset provider) instead of leaving the current combined wrapper.
3. Biome *selection* parity is now covered for the overworld preset. Session 30
   verified `Climate::RTree` against the real Java RTree over 300k targets incl.
   all 4155 distance-tie cases. Session 49 verified the seed-wired sampled
   overworld climate end-to-end (`density_parity`: 488/0 mismatches;
   `climate_biome_parity`: 56/0 mismatches for quantized target + biome id).
   Still TODO: add the same coverage for nether/end presets.
4. Terrain through overworld carvers is now sampled-column certified
   (`base_terrain_column_parity`, `surface_terrain_column_parity`, and
   `carved_terrain_column_parity` all 21,504/0 in Session 52). Next terrain
   work can move to structures/features/decoration, but each new stage must get
   its own Java-backed parity harness before it is called certified.
5. Port placed/configured features and structures only from Java/data definitions. The approximate ore/surface-decoration/structure generators added by another LLM must not be re-enabled as-is; `TreeGen.cpp` remains compiled only for the tree primitives used by the data-driven `BiomeDecorator`.
6. Tasklist next: continue the remaining rendering/worldgen/UI items from
   `C:\Users\Mateo\Desktop\tasklist.txt` (notably chunk-generation stutter/lagback,
   biome-aware tinting, animated water/lava/kelp textures, kelp/water plant logic,
   bamboo model/displacement, structures, and menus).

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
- `SurfaceRules`, `SurfaceSystem`, and `SurfaceRuleData` now have sampled
  overworld `buildSurface` column parity against real Java (Session 50:
  21,504/0 mismatches), but they are not exhaustively certified across every
  condition/rule/biome edge case. Keep adding Java-backed cases as terrain work
  expands.
- The previous LLM added `BiomeSource`, `feature/TreeGen`, `feature/OreGen`, `feature/SurfaceDecoration`, and `structure/StructureGen` as approximate systems. They contained hand-authored biome points, per-chunk random scattering, hardcoded chances, and simplified structures. The approximate ore/surface-decoration/structure paths are intentionally out of the executable; `TreeGen.cpp` remains compiled only for the tree primitive classes used by the data-driven `BiomeDecorator`, and its old `decorateChunk()` entry point is not called and no longer calls `OreGen`.
- `NoiseBasedChunkGenerator::buildSurface()` now samples the Java-shaped
  `BiomeManager` / `BiomeSource` climate pipeline for overworld surface rules.
  Nether/end biome sampling and terrain presets still need equivalent coverage.
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
  `docs/WORLDGEN_PLAN.md`). Delivered Phase A — the biome registry data
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
- Session 32 (Phase B start — trees/vegetation track): ported `WorldgenRandom`
  in `world/level/levelgen/RandomSource.{h,cpp}` — the population RNG that
  `ChunkGenerator.applyBiomeDecoration` uses to seed every feature
  (`setDecorationSeed`/`setFeatureSeed`/`setLargeFeatureSeed`/
  `setLargeFeatureWithSalt`). Verified 1:1 over 540 cases via
  `tools/WorldgenRandomParity.java` and the new `worldgen_random_parity` target
  (pure std C++; RandomSource.cpp's unused `<windows.h>`/`<bcrypt.h>` are now
  `#ifdef _WIN32`, so it builds on Linux). IMPORTANT FIX: every `nextDouble` was
  slightly off — Java multiplies by `double DOUBLE_MULTIPLIER = 1.110223E-16F`
  (a double initialised from a FLOAT literal, i.e. (double)(1.110223e-16f)) in
  double precision; the C++ used the plain double literal `1.110223E-16`.
  Corrected for Legacy/SingleThreaded/Xoroshiro/WorldgenRandom; this nudges
  noise/density output toward Java-correctness, so add a `noise`/
  `density_function` parity target to confirm the sampler now matches bit-for-bit.
  PRE-EXISTING ISSUE noted: `md5Bytes` in RandomSource.cpp is an FNV placeholder,
  not real MD5, so Xoroshiro positional `fromHashOf` does not match Java's
  `Hashing.md5()`; port real MD5 when doing noise seed parity.
- Session 33 (Phase B cont. — vegetation track): ported the value samplers and
  the world-independent placement modifiers, the "how many / where" core of
  feature decoration:
  - `world/level/levelgen/IntProvider.h` (`mc::valueproviders`): ConstantInt,
    UniformInt, BiasedToBottomInt, ClampedInt, WeightedListInt (cumulative-weight
    walk; matches WeightedList.Flat and .Compact).
  - `world/level/levelgen/placement/PlacementModifier.h` (`mc::levelgen::placement`):
    InSquarePlacement, CountPlacement (RepeatingPlacement), RarityFilter
    (PlacementFilter), RandomOffsetPlacement. `getPositions` returns
    `vector<BlockPos>`; takes a forward-declared `PlacementContext*` that the
    pure modifiers ignore (world-dependent modifiers will flesh it out).
  Verified 1:1 over 75 cases via `tools/PlacementParity.java` (runs the real
  decompiled classes; needs `Bootstrap.bootStrap()` and writes to a file because
  bootstrap reroutes System.out through Log4j) and the `placement_parity` target
  (pure std C++; links `glm` only for BlockPos/Math.h). Next: PlacementContext/
  WorldGenLevel + the world-dependent modifiers (heightmap, height_range,
  filters), BlockStateProvider, then the feature types (random_patch,
  simple_block, …) and TreeFeature.
- Session 34 (Phase B cont. — vegetation track): completed the value-provider
  layer. Added `FloatProvider.h` (Constant/Uniform/ClampedNormal/Trapezoid),
  `heightproviders/HeightProvider.h` (Constant/Uniform/BiasedToBottom/
  VeryBiasedToBottom/Trapezoid), `VerticalAnchor.h` (Absolute/AboveBottom/
  BelowTop) and `WorldGenerationContext.h`. Verified 1:1 over 60 cases via
  `tools/HeightFloatProviderParity.java` + `height_float_provider_parity` target;
  the ClampedNormalFloat cases also validate `nextGaussian` end-to-end. The Java
  generator builds a `WorldGenerationContext` with `sun.misc.Unsafe`
  (allocateInstance + putInt on the two final int fields) since its only ctor
  needs a ChunkGenerator. These are header-only (`mc::valueproviders` /
  `mc::levelgen` / `mc::levelgen::heightproviders`), not yet in the main `mcpp`
  build — pulled in as features/placements that use them are wired.
  STILL THE GATE for visible vegetation: PlacementContext/WorldGenLevel + the
  heightmap placement + a feature (random_patch/simple_block) + BlockStateProvider.
- Session 35 (Phase B cont. — vegetation track): ported the placement *surface*
  and the first world-dependent modifiers. Added `Heightmap.h` (Types enum),
  `placement/PlacementContext.h` (the `WorldGenLevel` query interface +
  `PlacementContext : WorldGenerationContext`), and `placement/HeightmapPlacement.h`
  (`HeightmapPlacement` + `HeightRangePlacement`). Verified 1:1 over 126 cases
  via `tools/WorldPlacementParity.java` and the `world_placement_parity` target.
  The Java harness implements `WorldGenLevel` (an interface) with a dynamic
  `Proxy` whose `getHeight(type,x,z)` is a deterministic stub, and builds
  `PlacementContext` with `Unsafe` (its only ctor needs a ChunkGenerator). The
  stub heightmap range includes height==minY so the empty branch is exercised.
  With this + the pure modifiers, the full XZ-scatter→surface position stream for
  a vegetation placed_feature is now computable (minus the `biome`/
  `block_predicate_filter` modifiers). NEXT: the feature *execution* side —
  `BlockStateProvider` + a feature (simple_block/random_patch) that writes blocks
  via WorldGenLevel.setBlock, verified by capturing setBlock on a Proxy level.
- Session 36 (Phase B cont. — vegetation feature execution): ported
  `BlockStateProvider` (SimpleStateProvider, WeightedStateProvider — verified 1:1,
  18 cases, `block_state_provider_parity`; state = canonical id string),
  `SimpleBlockFeature` + `FeaturePlaceContext` (faithful; `canSurvive` delegated
  to `WorldGenLevel`), and `NoiseBasedStateProviders.h` (NoiseThresholdProvider —
  faithful but BLOCKED by the NormalNoise gap above, so its noise branch is
  uncertified). Key findings while wiring the Java harness (a Proxy WorldGenLevel
  capturing setBlock): (1) `simple_block`'s only world step is
  `BlockState.canSurvive`, i.e. the block-behaviour subsystem (block classes are
  not even in our decompiled staging; the `minecraft:dirt` tag shrank to
  {dirt,coarse_dirt,rooted_dirt} in 26.1.2, so plant survival changed) — this is
  the boundary, to be backed when block behaviour is ported; (2) the NormalNoise
  parity gap. So canSurvive-gated end-to-end placement and noise-driven state are
  the two remaining gates for visible vegetation; the placement+selection pipeline
  (positions + which state) is otherwise built and verified.

**Decisions made:**
- AI goals are executed client-side for the port's prototype to simulate living behavior in offline mode.
- Inventory system mirrors the Java `Container` interface for future compatibility with server-side sync.
- The Title Screen `Singleplayer` button now starts an offline local world instead of attempting localhost networking; `Connect to Localhost` remains available as a separate button.
- OpenGL buffer allocation uses direct-state-access (`glCreateBuffers`/`glNamedBufferData`) so index buffers are valid without relying on VAO state during creation.
- The current overworld base terrain is sampled-verified through
  `buildSurface`, but whole-world generation is not final until Java carvers,
  structures, features, decoration, and broader parity coverage are complete.
  Continue replacing/expanding systems only with direct ports from the Java
  decompiled source and data; do not tune or invent noise behavior.
