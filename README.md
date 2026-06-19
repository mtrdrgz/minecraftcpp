# Minecraft CPP

1:1 reverse-engineering port of Minecraft Java Edition 26.1.2 to C++.
Single self-contained Windows executable with embedded assets, multi-backend
rendering (OpenGL 4.6 / Vulkan 1.3 / DirectX 12), and full worldgen parity.

**Before touching code:** read [CLAUDE.md](CLAUDE.md) (the one rule) and
[AGENTS.md](AGENTS.md) (full project state and working notes).

---

## Repository layout

```
/                         ← repo root IS the C++ project
├── CMakeLists.txt
├── src/                  ← all C++ source (world, render, network, audio, …)
├── cmake/                ← CMake modules (CompilerFlags, PrepareRuntimeAssets, …)
├── vendor/               ← third-party libs (GLM, glad, glslang, nlohmann/json, …)
├── tools/                ← asset_packer, 530+ parity Java harnesses, helper scripts
├── docs/                 ← PORT_COVERAGE ledger + worldgen / decoration plans
├── profiling/            ← profiling infrastructure (Profiler.h/cpp, analysis tools)
├── AGENTS.md             ← master guide: project state, session history, workflows
├── CLAUDE.md             ← the one rule (port from source, never invent)
├── TASKLIST.md           ← current bug/feature task list
└── 26.1.2/               ← LOCAL ONLY — never committed
    ├── src/              ← decompiled Minecraft Java (fetch via provision_parity_runtime.ps1)
    └── data/             ← worldgen JSON (same)
```

Local-only inputs (git-ignored, regenerated):
- `26.1.2/` — fetch with `tools/provision_parity_runtime.ps1`
- `assets/` — Mojang asset cache (textures, sounds), fetched by `cmake/PrepareRuntimeAssets.cmake`

---

## Build (Windows — MSVC 2022 or llvm-mingw)

```powershell
# Configure from repo root
cmake -S . -B build

# Build with timeout wrapper (prevents hangs)
.\tools\run_with_timeout.ps1 -TimeoutSec 300 cmake --build build --target mcpp

# Run (from repo root)
build\mcpp.exe --quickPlaySingleplayer
```

**Cross-compile from Linux (llvm-mingw):**
```bash
cmake -S . -B build-win \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=/opt/llvm-mingw/bin/x86_64-w64-mingw32-clang \
  -DCMAKE_CXX_COMPILER=/opt/llvm-mingw/bin/x86_64-w64-mingw32-clang++ \
  -DCMAKE_RC_COMPILER=/opt/llvm-mingw/bin/x86_64-w64-mingw32-windres \
  -G Ninja -DMCPP_SKIP_ASSET_PACK=ON
cmake --build build-win --target mcpp -j$(nproc)
```

CI skips the full asset pack (`-DMCPP_SKIP_ASSET_PACK=ON`) and uses placeholder
asset files. A full local build with real assets requires the Mojang CDN fetch
(CMake does this automatically via `cmake/PrepareRuntimeAssets.cmake`).

---

## Parity testing

The `tools/` directory contains 530+ Java harnesses (`*Parity.java`) for
ground-truth verification against the real game. Run them via the shell scripts:

```bash
tools/run_all_gates.sh          # all C++ parity targets (expects 0 mismatches)
tools/run_deco_parity.sh        # decoration / biome-feature gates
tools/run_groundtruth.sh        # Java ground-truth generation
```

Individual parity target example:
```bash
# C++ side
cmake --build build --target nether_fossil_pieces_parity
build/nether_fossil_pieces_parity

# Java side (needs 26.1.2/ local dump)
javac -cp "26.1.2/client.jar:26.1.2/libs/*" -d 26.1.2/parity/out tools/SomeParity.java
java  -cp "26.1.2/parity/out:26.1.2/client.jar:26.1.2/libs/*" SomeParity
```

See `docs/PORT_COVERAGE.md` for the full port ledger (6,882 Java files tracked).
