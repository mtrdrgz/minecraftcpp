# Minecraft CPP

A ground-up **1:1 reverse-engineering port of Minecraft Java Edition 26.1.2 to native C++** for Windows. Every algorithm, constant, formula, and ordering is ported directly from the decompiled Java source — nothing is invented, approximated, or tuned to "look right."

The result is a standalone `.exe` with all game assets embedded, three rendering backends, full network protocol support, and a worldgen engine that produces byte-identical terrain to the real server when given the same seed.

---

## What this is

This is not a Minecraft clone. It is a **faithful translation** of the original Java codebase into C++, subsystem by subsystem, verified against ground truth from the real game at every step.

The motivation is performance and portability: a native C++ client targeting 500+ FPS on modern hardware, zero Java runtime dependency, and a single self-contained executable.

**The ironclad rule**: every value, formula, and algorithm must come from `26.1.2/src/` (decompiled Java) or `26.1.2/data/` (worldgen JSON). If the Java does X, the C++ does X. See [CLAUDE.md](CLAUDE.md).

---

## Current status

### What works today

| System | Status | Notes |
|---|---|---|
| **Windowing / input** | ✅ Complete | Win32, raw mouse, keyboard |
| **Rendering — OpenGL** | ✅ Complete | OpenGL 4.6 core, primary backend |
| **Rendering — Vulkan** | ✅ Complete | Vulkan 1.3, VMA, volk meta-loader |
| **Rendering — DirectX 12** | ✅ Complete | D3D12MA, multiple-backend switching |
| **Chunk meshing** | ✅ Complete | Greedy mesher, texture atlas |
| **Texture atlas** | ✅ Complete | Built at runtime from embedded assets |
| **Asset pipeline** | ✅ Complete | All assets embedded in `.exe` via RCDATA |
| **NBT** | ✅ Byte-exact | Insertion-ordered compounds, modified UTF-8, gzip — 6/0 parity gate |
| **Network protocol** | ✅ Byte-exact | TCP, VarInt/VarLong, PacketBuffer — 31/0 parity gate |
| **AABB / physics** | ✅ Byte-exact | 512/0 parity gate |
| **Biome climate selection** | ✅ Byte-exact | `Climate::RTree` + `OverworldBiomeBuilder` — 7,593-entry verification |
| **Biome registry** | ✅ Complete | All 65 biomes loaded from worldgen JSON, field-for-field verified |
| **Terrain generation** | ✅ Byte-exact | 2,359,296-cell parity gate at 0 mismatches |
| **Cave carvers** | ✅ Byte-exact | Integrated into terrain gate |
| **World decoration** | ✅ Byte-exact | 8M+ cells certified; 199 placed features, 0 no-ops |
| **GUI / menus** | ✅ Functional | Title screen, pause menu, options, GUI scale |
| **HUD** | ✅ Functional | Health, hotbar, crosshair |
| **Audio** | ✅ Functional | XAudio2, Ogg/Vorbis streaming |
| **Entity system** | 🔄 In progress | Base entity, mobs, AI goals (RandomStroll ported) |
| **Inventory** | 🔄 In progress | Container interface, SimpleContainer, Slot |
| **Structures** | 🔄 Partial | Foundation laid; full pipeline pending |
| **Crafting / smelting** | ❌ Not started | |
| **Mob AI (full)** | ❌ Not started | ~277 goal types remaining |

### Parity certification

The worldgen engine is verified against real server output:

- **Terrain + carvers**: 2,359,296 cells — 0 mismatches
- **Full decoration (all 6 primary seed-1 chunks)**: 98,304 cells each — 0 mismatches
- **Biome climate**: 1,867,776 cells across 9 biome classes — 0 mismatches
- **NBT serialization**: 6 round-trip test vectors — 0 mismatches  
- **Network packet encoding**: 31 protocol messages — 0 mismatches
- **AABB collision math**: 512 test cases — 0 mismatches

Parity is enforced via 530+ Java harnesses in `tools/` that run the real decompiled game code and diff the output against the C++ implementation.

### Known visual bugs

See [TASKLIST.md](TASKLIST.md) for the full tracked list. Active issues include:
- Flowers and vines render as crossed quads (should be flat planes)
- Grass / water colour biome-tinting not implemented (sabana, swamp look grey-green)
- Animated textures (water, lava, kelp) not implemented
- Fallen log direction placement not working (all segments face up)
- Several biomes missing or incomplete (only ~partial coverage of 65 total)

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

## Repository layout

```
/
├── CMakeLists.txt          ← top-level build
├── src/                    ← all C++ source
├── cmake/                  ← CMake modules
├── vendor/                 ← third-party libraries (vendored, no submodules)
├── tools/                  ← asset_packer + 530+ parity Java harnesses + scripts
├── docs/                   ← PORT_COVERAGE ledger, worldgen and decoration plans
├── profiling/              ← Profiler.h/cpp + analysis scripts
├── AGENTS.md               ← master agent guide: full project state, session history
├── CLAUDE.md               ← the one rule (read first)
├── TASKLIST.md             ← tracked bugs and pending features
└── 26.1.2/                 ← LOCAL ONLY, never committed
    ├── src/                ← decompiled Minecraft Java (Vineflower)
    └── data/               ← worldgen JSON from client.jar
```

---

## Contributing / working with AI agents

This project is developed with AI coding agents (Claude Code). The workflow:

1. Read [CLAUDE.md](CLAUDE.md) — the one rule that overrides everything
2. Read [AGENTS.md](AGENTS.md) — full current state, session history, working notes, parity gate status
3. Check [TASKLIST.md](TASKLIST.md) for the current bug/feature list
4. Before porting anything, read the corresponding Java source in `26.1.2/src/`
5. After porting, add a parity test and verify gates are at 0

The `docs/PORT_COVERAGE.md` file is the master roadmap. `docs/WORLDGEN_PLAN.md` and `docs/DECORATION_PLAN.md` are the detailed worldgen porting plans.
