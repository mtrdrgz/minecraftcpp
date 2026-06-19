# CLAUDE.md — read this first

This repo is a **1:1 reverse-engineering port of Minecraft Java Edition 26.1.2 to C++**.
The full agent guide is [AGENTS.md](AGENTS.md) — read it before touching anything.
This file exists so the rule that matters most is never missed.

## ⛔ THE ONE RULE: port from the source, never make it up

You are translating Java → C++. Nothing else. **Every value, constant, formula,
ordering and algorithm MUST come from the decompiled Java source or the worldgen
data JSON. Never invent, guess, approximate, simplify, tune, or "make it look reasonable."**

- The Java source lives at `26.1.2/src/` **locally** — it is not committed to git.
  Fetch it with `tools/provision_parity_runtime.ps1` before reading it.
  The worldgen data JSON is in `26.1.2/data/` (same — fetch first).
- If you can't find the source for something, STOP and read the Java. Do not fill the
  gap with a plausible number — a wrong-but-plausible value hides the bug and looks done.
- A not-yet-ported feature must be a **hard no-op that does nothing** and listed as
  unported — NEVER a silent `return true;` / "pass everything", which disables gating.
- No placeholder hashes/curves/noise. They corrupt everything downstream.
- Prefer a `*_parity` test against Java ground truth over eyeballing.

**Do not tune frequencies/densities to "look right."** Wrong values come from a skipped
or incorrect port — find and fix the real algorithm.

## Repository layout

```
/                         ← repo root IS the C++ project
├── CMakeLists.txt        ← top-level build
├── src/                  ← all C++ source
├── cmake/                ← CMake modules (CompilerFlags, PrepareRuntimeAssets, …)
├── vendor/               ← third-party libraries (GLM, glad, glslang, …)
├── tools/                ← asset_packer, parity Java harnesses, helper scripts
├── docs/                 ← PORT_COVERAGE ledger, worldgen plan, decoration plan
├── profiling/            ← profiling infrastructure (Profiler.h/cpp + analysis tools)
├── AGENTS.md             ← master agent guide (read this)
├── TASKLIST.md           ← current bug/feature task list
└── 26.1.2/               ← LOCAL ONLY, never committed — fetch via tools/provision_parity_runtime.ps1
    ├── src/              ← decompiled Minecraft Java (Vineflower)
    └── data/             ← worldgen JSON extracted from client.jar
```

## Build / run (Windows, MSVC or llvm-mingw + Ninja)

```powershell
# Configure from repo root
cmake -S . -B build

# Build (always use the timeout wrapper to prevent hangs)
.\tools\run_with_timeout.ps1 -TimeoutSec 300 cmake --build build --target mcpp

# Run (from repo root so 26.1.2/data resolves if present)
build\mcpp.exe --quickPlaySingleplayer
```

Cross-compile from Linux with llvm-mingw (`/opt/llvm-mingw`):
```bash
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=... # see AGENTS.md § CROSS-COMPILE
cmake --build build-win --target mcpp -j$(nproc)
```
