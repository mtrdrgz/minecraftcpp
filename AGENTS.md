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
├── mcpp\                   ← the C++ project (build here)
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

**Last updated**: Session 47 (fixed panorama 180° flip; real option widgets — Slider/
CycleButton + GameOptions store; Sound/Video/Controls sub-screens populated; FOV slider)

**DIRECTION (decided by the user, Session 43):** GPU/OpenCL worldgen is OFF the table —
it has no source to port (Minecraft generates on CPU) and would violate RULE #0. Optimize
the CPU generator instead. Current focus order: **(1) MENUS**, then (2) CPU worldgen perf,
then (3) real structures.

**Session 43:**
- placeTree now rejects an origin whose block-below is wood/leaves/log/stem (or air/fluid):
  the chunk heightmap rises onto a neighbour's leaves (cross-chunk decoration), so OCEAN_FLOOR
  placement could land a tree on another tree's foliage + drop a dirt block on the leaves.
- hugeMushroomPlacer requires dirt/mycelium below (HugeMushroomFeature ground gate) → no more
  huge mushrooms on the ocean surface.
- updateLocalChunks budgets the deferred-decoration pass to a few chunks/tick (nearest-first)
  to cut the movement stutter (decoration is heavy + main-thread). This is a stopgap; the real
  perf work (focus #2) is to reuse generators, make the feature caches thread-safe so decoration
  can run on workers, and stop re-reading worldgen JSON.

## MENU SYSTEM ROADMAP (focus #1 — port 1:1 from net.minecraft.client.gui.screens.*)

Why the menu was "all gray": the font + GUI textures live in client.jar, NOT assets.bin.
Fix is to embed them as Windows resources (extract from `26.1.2/client.jar`).

- [x] **Embed + render font/logo/button** (Session 43). `assets/resource_ids.h` +
  `assets.rc` now carry `ascii.png` (IDR_FONT_ASCII), `gui_logo.png` (IDR_GUI_LOGO),
  `gui_button.png`/`gui_button_hl.png`. `Minecraft::render` loads them via
  `loadResourceTex(IDR_*)` (RCDATA → stb). Font/logo/buttons are no longer null.
  EXTRACTION (gitignored, local only): unzip the four PNGs from client.jar into
  `mcpp/src/assets/` (see Session 43 PowerShell). NOTE: in 26.1.2's jar the title
  `panorama_0..5.png` are 1×1 STUBS — the real panorama isn't shippable from this jar,
  so use the dirt-tile MENU_BACKGROUND fallback (the game's own no-panorama background).
- [ ] **Font fidelity**: port real glyph advances (`FontManager`/`GlyphProvider` derive
  widths from the ascii.png non-empty columns; current Font.cpp uses hardcoded widths).
- [x] **Mouse-capture fix** (Session 45). `Window::onLButtonDown` captured the mouse on
  EVERY click (hid the cursor + clipped it to centre) → the title screen was unclickable.
  Removed it; `main.cpp` now decides: menu open → route click to the screen (cursor stays
  visible); in-game + no menu → click grabs the mouse for the camera.
- [x] **Splash text + icons** (Session 45). Embedded `splashes.txt` (IDR_SPLASHES) +
  `language.png`/`accessibility.png` (IDR_GUI_LANG/ACCESS). `pickSplash()` picks a random
  non-empty line; `TitleScreen::renderSplash` ports `SplashRenderer` (yellow, rotated
  -PI/9, pulsing scale `1.8 - |sin(...)*0.1|`, at `w/2+123, 69`). Added `GuiGraphics::rotate`.
  Icons drawn 16x16 on the 20x20 language/accessibility buttons.
- [x] **Background = dirt-tile** (the no-panorama fallback) — but see below: the REAL
  panorama art IS available.
- [x] **Panorama background** (Session 46). `render/gui/PanoramaRenderer.{h,cpp}` draws the
  rotating title panorama as a perspective cube (FOV 85, znear .05, zfar 10) of 6 textured
  faces (the GL-cubemap equivalent). Faces load from assets.bin (1.3 MB each; the jar has
  only 1x1 stubs). Face→PNG mapping from CubeMapTexture (`SUFFIXES={_1,_3,_5,_4,_0,_2}` into
  GL faces +X,-X,+Y,-Y,+Z,-Z): +X=pan1,-X=pan3,+Y=pan5,-Y=pan4,+Z=pan0,-Z=pan2. Spin ≈2°/s
  (`spin += dt*20*0.1`). `Minecraft::renderPanorama` draws it to the cmd in main.cpp's title
  branch (before the GUI); `TitleScreen` overlays panorama_overlay (dirt fallback if faces
  missing). CAVEAT: per-face UV ORIENTATION is best-effort (cube built from a standard
  inward skybox layout + CubeMap's rotationX(180); could not screenshot-verify) — if a face
  looks mirrored/upside-down, flip its UVs in PanoramaRenderer's CUBE table.
- [x] **Title screen 1:1** (Session 44). Rewrote `gui/screens/TitleScreen.cpp` straight
  from the decompiled `TitleScreen.java` + `LogoRenderer.java`: Singleplayer /
  Multiplayer / Minecraft Realms stacked (200x20, topPos=h/4+48, spacing 24), then the
  row [language 20] [Options... 98] [Quit Game 98] [accessibility 20] at topPos+36, logo
  256x44@(w/2-128,30), edition subtitle 128x14@(w/2-64,67), version bottom-left +
  copyright bottom-right, dirt-tile background. REMOVED the invented "Connect to
  Localhost" button. Unported-screen actions (Multiplayer/Realms/Options/lang/access)
  are hard no-ops; Singleplayer starts the local world (stand-in for SelectWorldScreen).
  Strings are the real en_us.json values. NOTE: the client GUI classes are NOT in the
  pre-decompiled `26.1.2/src` (only world/util/core were). Decompile what you need with
  Vineflower: extract the `.class` files from `client.jar` into a stage dir and run
  `jdk25/bin/java -jar vineflower-1.12.0.jar <stage> <out>` (output now in
  `26.1.2/gui_src/`, gitignored). GUI textures extracted from client.jar and embedded as
  RCDATA: ascii/logo/edition/button/button_hl/dirt (IDR_* in resource_ids.h).
  TODO next: real splash text (needs splashes file + diagonal yellow render); 9-slice
  button sprite; language/accessibility icon sprites.
- [ ] **Widget library**: `Button`/`SpriteIconButton`/`AbstractWidget` with the real
  9-slice sprite scaling (`button.png` is a NineSlice sprite) + hover/focus.
- [x] **Options navigation** (Session 46). `gui/screens/options/OptionsScreen.{h,cpp}`
  ports `OptionsScreen.java`'s layout: the 2-column grid of category buttons (Skin
  Customization / Music & Sounds / Video Settings / Controls / Language / Chat Settings /
  Resource Packs / Accessibility Settings / Telemetry Data / Credits & Attribution) + a
  Done button. Centralised screen creation in `Minecraft::openTitleScreen()` /
  `openOptionsScreen()` (GUI textures are owned by Minecraft and reused, so screens rebuild
  with their textures). Title "Options..." button → openOptionsScreen; each category opens
  an `OptionsSubScreen` (real title + Done → back); Done → back to title. Navigation works.
- [x] **Option widgets** (Session 47). `gui/components/OptionWidgets.{h,cpp}`: AbstractWidget
  + Slider + CycleButton (+ WidgetButton). `client/Options.h` GameOptions store (owned by
  Minecraft, `options()`; volumes 0..1, fov 30..110, sensitivity 0..1→0..200%, render
  distance 2..32, gamma 0..1). `OptionsSubScreen` is now a real base (add*/layout: big=1col
  310px, small=2col 150px + Done). Ported the option LISTS from the Java:
  SoundOptionsScreen (master big + 9 source sliders + subtitles/directional toggles, from
  SoundOptionsScreen.java), VideoSettingsScreen (graphics cycle / render distance / GUI
  scale / brightness / fullscreen / vsync / view bobbing), ControlsScreen (sensitivity +
  invert/auto-jump/discrete-scroll/touchscreen). FOV slider in the OptionsScreen subheader.
  CAVEATS / NEXT: sliders are click-to-set (drag = poll mouse-down each frame — needs a
  Window `isMouseDown()`); options are NOT persisted to disk; categories without a concrete
  screen yet (Skin/Language/Chat/Resource Packs/Accessibility/Telemetry/Credits) open the
  generic titled screen — port each `*.java` option list + the key-binding list (Controls).
- [ ] **Screen framework polish**: real `GridLayout`/`HeaderAndFooterLayout`, 9-slice
  button sprite, hover tooltips, keyboard/ESC handling.



**Session 42 — finish the tree-clipping fix + cave plants on surface:**

1. **Trees STILL clipped at borders (Session 41 didn't fully fix it).** Root cause:
   the tree placer writes through `TreeWorld` (TreeGen.h), NOT the ChunkWGL — and
   TreeWorld clamped to one chunk, so foliage spilling across a border was dropped
   regardless of the ChunkWGL change. Fixes: (a) `TreeWorld` now routes reads/writes
   to the owning chunk via the same `chunkAt` resolver (treePlacer sets it from the
   WGL); (b) **deferred decoration** — `LevelChunk::decorated` flag + `Minecraft::
   tryDecorate(cp)` which only decorates a chunk once ALL 8 neighbours are loaded, so
   cross-chunk writes always land. `startLocalGame` now gens a 5×5 and decorates the
   inner 3×3; `updateLocalChunks` defers decoration to a per-tick pass over loaded
   chunks. The frontier shows bare terrain (trees appear as you approach) instead of
   clipped trees — strictly correct. Verified: 2715 leaves in the decorated 3×3.

2. **glow_lichen / cave_vines spawning on the surface everywhere.** Wiring
   `height_range` (Session 41) spread their Y to 0..256, but their gating placements
   were unported → pass-through. Ported `surface_relative_threshold_filter`
   (keeps glow_lichen ≥13 below surface) and `environment_scan` (cave_vines scan up to
   a ceiling) + a `has_sturdy_face` predicate (approx: full solid+opaque cube).
   Verified: 0 cave-plants above y60. (They now need real caves to place at all —
   correct, since carvers aren't ported yet.)
   NOTE: their RENDER is still a cross (isCrossPlant), not flat-on-wall — the mesher
   has no block-model/multiface system. They're off the surface now so it's rarely
   seen; proper flat rendering needs the model port.

**Reminder for unported placement modifiers:** the default in `parseModifier` is to
SKIP (nullptr → pass-through). For FILTERS that gate cave/underground features this is
WRONG (it lets them onto the surface). When you wire a new positional modifier
(height_range, etc.), check whether sibling FILTER modifiers in the same placed_feature
are still unported — porting one without the other moved glow_lichen to the surface.



**Session 41 — more worldgen ports + fixes (port, don't tune):**

1. **Trees clipped at chunk borders / lone floating dirt — FIXED.** `BiomeDecorator`'s
   `ChunkWGL` dropped any write outside the active 16×16 chunk, so a tree near a
   border lost the foliage/trunk that belonged in the neighbour (the "half/quarter
   sphere of leaves" + a lone dirt block). It now routes reads/writes to the chunk
   that OWNS each world position via an optional `chunkAt(cx,cz)` resolver
   (`applyBiomeDecoration(..., chunkAt)`), so features write across borders into
   already-loaded neighbours and mark them dirty. `Minecraft::decorateChunk` passes
   `getChunk` (main-thread only). LIMITATION: a neighbour that isn't loaded yet is
   still skipped — the proper fix is Java's two-phase WorldGenRegion (features for a
   chunk run only once all chunks in its region reached the prior step). Same
   best-effort model as structures. Unit tests pass `chunkAt={}` → single-chunk.

2. **Ore generator ported 1:1.** `resolveConfigured` now handles `ore`/`scattered_ore`
   (`orePlacer`) — a faithful port of `OreFeature.place`+`doPlace`+`canPlaceOre`
   (ellipsoid vein along a random axis, RuleTest targets tag_match/block_match/
   blockstate_match/random_*, discard_chance_on_air_exposure, exact RNG order).
   Also wired the `height_range` placement (was unhandled → identity!) with a
   `HeightProvider`/`VerticalAnchor` parser (constant/uniform/[very_]biased_to_bottom/
   trapezoid; absolute/above_bottom/below_top). Verified in-engine: 3285 ore blocks in
   the spawn 3×3. CAVEAT: `Mth.sin` isn't ported project-wide; `orePlacer` reproduces
   it bit-exactly inline (SIN[i]=sin(i·2π/65536)); port a shared `Mth` for other users.

3. **More cutout plants don't occlude / aren't cubes.** Added small mushrooms
   (red/brown_mushroom), glow_lichen, sculk_vein, cave_vines(_plant), hanging_roots,
   dripleaf, spore_blossom, twisting/weeping vines, vine to `isCrossPlant`
   (ChunkMesh.cpp). Still a hardcoded list, not the block-model system.

**Open / deferred (next sessions — from the source, never tune):**
- **Savanna/biome grass+foliage tint.** `ChunkMesh.cpp getTextureTint` hardcodes the
  plains color for ALL grass/foliage. The 1:1 path: embed `assets/minecraft/textures/
  colormap/{grass,foliage}.png` (extend tools/build_atlas.py to pull them from
  client.jar), port `GrassColor.get(temp,downfall)`/`FoliageColor` (index = `((1-temp)*255)`,
  `((1-temp*downfall)*255)` into the colormap), `BiomeColors`, and the
  `grass_color_modifier` (none/dark_forest/swamp) + biome `grass_color`/`foliage_color`
  overrides; feed the per-block biome (temp/downfall from BiomeRegistry) into the mesher.
- **"Whole chunk surface becomes one block" when moving fast.** Reported as a likely
  data race during concurrent async chunk gen (4 worker threads). Could NOT reproduce
  from the info at hand — NEEDS a screenshot + seed/coords. Suspects to check first:
  any mutable state shared across the per-chunk generators built in the
  `updateLocalChunks` worker lambda; the `Climate::RTree m_lastResult` (mutable) if a
  BiomeSource is ever shared across threads; `getOrCreateNoise` if a RandomState is
  shared. Each worker currently builds its OWN generator, so confirm nothing is static.
- **`random_patch` feature still unported** (`resolveConfigured` → noop) — port
  `RandomPatchFeature` (tries / xz_spread / y_spread + inner placed feature). Flowers
  reaching the world densely (the giant flower diamond) is likely related.
- Structures: user will request real spawning next; current ones are approximations
  (see WORLDGEN_PLAN.md / Session structure notes).
- Certify terrain NormalNoise/DensityFunction parity (MD5 is correct now).

---

### Session 40 (previous)
**Original Session 40 fixes: real MD5 noise seeding, block predicates, plant rendering
— the worldgen got wired into the engine and runnable.**

**Session 40 — root-cause fixes for "it generates but it's wrong" (all were skipped
ports, NOT mis-tuned numbers):**

1. **MD5 noise seeding (the big one).** `RandomSource.cpp` `md5Bytes()` was an FNV
   placeholder, not MD5. The overworld uses Xoroshiro, whose
   `XoroshiroPositionalRandomFactory.fromHashOf(name)` seeds EVERY named noise
   (`Noises::TEMPERATURE/VEGETATION/CONTINENTALNESS/EROSION/RIDGE/SHIFT` + all terrain
   noises) via `Hashing.md5().hashString(name).asBytes()`. Wrong/correlated seeds →
   the climate noise collapsed → the whole world was ~3 biomes (taiga/snow/desert,
   i.e. only temperature varied) and non-1:1 with Java. Replaced with real RFC 1321
   MD5. Result verified in-engine: biome variety over a 1024×1024 sample went 3 → 12
   (beach, birch_forest, deep_ocean, forest, jungle, oceans, plains, river, savanna,
   sparse_jungle, …). MD5 now covered by `worldgen_random_parity` (standard vectors
   for "" and "abc" via `RandomSupport::seedFromHashOf`). This also makes ALL terrain
   noise 1:1, so do a NormalNoise/density parity pass next to certify it.

2. **Block predicates (pumpkins & sugar cane).** `BiomeDecorator.cpp parsePredicate`
   did not implement `matching_blocks` or `matching_fluids` (both fell through to
   `return true`), and NO predicate applied the `offset` field. Java's
   `StateTestingPredicate.test(level,origin)` = `test(level.getBlockState(origin.offset(offset)))`.
   So `patch_pumpkin` ("air here AND grass_block at offset [0,-1,0]") and
   `patch_sugar_cane` ("air, would_survive, AND water at [±1,-1,0]") had their gating
   silently disabled → pumpkins/sugar cane scattered onto sand/water in absurd counts.
   Ported `matching_blocks`, `matching_fluids`, and `offset` for all state-testing
   predicates + `would_survive`. (Fluids are modeled as the water/lava blocks.)

3. **Plant rendering (`ChunkMesh.cpp`).** Two duplicated hardcoded plant lists
   (cross-render + cull-bypass) were missing newer cutout blocks, so `leaf_litter`
   (and bush/firefly_bush/wildflowers/pink_petals/dry grass) rendered as full cubes
   AND culled neighbour faces (holes in the ground). Unified into one `isCrossPlant()`
   used by both sites and added the missing blocks. NOTE: this is still a hardcoded
   list standing in for the real block-model system — a proper port reads block models
   (JSON) to decide render shape + occlusion. Until then, keep `isCrossPlant` in sync
   with `Blocks.cpp`.

**Worldgen is now wired into the engine** (Sessions earlier this set): the local world
(`Minecraft::startLocalGame` + the streaming `updateLocalChunks` integration) runs
`applyBiomeDecoration` (trees/vegetation) and `generateStructures` on the MAIN thread
(the BiomeDecorator/feature caches are not thread-safe). `decorateChunk`/`runStructures`
load the data-driven JSON from the discovered `26.1.2/data` tree. Run from the REPO
ROOT so that tree resolves; `--quickPlaySingleplayer` enters the world.

**Open / next (attack from the source, do NOT tune):**
- Certify terrain NormalNoise/DensityFunction parity now that MD5 is correct (add a
  `noise`/`density_function` parity target vs the real jar). The biome *map* is varied
  now but full terrain-shape parity is unverified.
- `random_patch` feature type is still NOT handled in `resolveConfigured` (falls to
  noop). Grass/flowers currently reach the world via other paths; port `RandomPatchFeature`
  (tries / xz_spread / y_spread + the inner placed feature) properly.
- Structures are hand-built approximations, not the template/jigsaw port; dungeons need
  carvers (no caves yet). See WORLDGEN_PLAN.md.
- `leaf_litter`/cutout rendering is a hardcoded list, not the block-model system.

---

## PREVIOUS STATE (pre-Session-40)

**Last updated**: Session 39 (surface vegetation breadth: double plants + dry vegetation + canSurvive dispatch)

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
- The current visible singleplayer terrain scaffold must not be treated as final terrain generation. Replace it systematically with direct ports from the Java decompiled source; do not tune or invent noise behavior.
